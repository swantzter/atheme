/*
 * Copyright (c) 2005-2007 Atheme Development Group
 * Rights to this code are as documented in doc/LICENSE.
 *
 * This file contains code for the Memoserv SENDGROUP function
 *
 */

#include "atheme.h"
#include "../groupserv/groupserv.h"

DECLARE_MODULE_V1
(
	"memoserv/sendgroup", false, _modinit, _moddeinit,
	PACKAGE_STRING,
	"Atheme Development Group <http://www.atheme.org>"
);

static void ms_cmd_sendgroup(sourceinfo_t *si, int parc, char *parv[]);

command_t ms_sendgroup = { "SENDGROUP", N_("Sends a memo to all members on a group."),
                           AC_NONE, 2, ms_cmd_sendgroup, { .path = "memoserv/sendgroup" } };

void _modinit(module_t *m)
{
        service_named_bind_command("memoserv", &ms_sendgroup);
}

void _moddeinit(module_unload_intent_t intent)
{
	service_named_unbind_command("memoserv", &ms_sendgroup);
}

static void ms_cmd_sendgroup(sourceinfo_t *si, int parc, char *parv[])
{
	/* misc structs etc */
	myuser_t *tmu;
	mowgli_node_t *n, *tn;
	mymemo_t *memo;
	mygroup_t *mg;
	int sent = 0, tried = 0;
	bool ignored, operoverride = false;
	service_t *memoserv;

	/* Grab args */
	char *target = parv[0];
	char *m = parv[1];
	
	/* Arg validation */
	if (!target || !m)
	{
		command_fail(si, fault_needmoreparams, 
			STR_INSUFFICIENT_PARAMS, "SENDGROUP");
		
		command_fail(si, fault_needmoreparams, 
			_("Syntax: SENDGROUP <group> <memo>"));
		
		return;
	}
	
	/* user logged in? */
	if (!si->smu)
	{
		command_fail(si, fault_noprivs, _("You are not logged in."));
		return;
	}

	if (si->smu->flags & MU_WAITAUTH)
	{
		command_fail(si, fault_notverified, _("You need to verify your email address before you may send memos."));
		return;
	}

	/* rate limit it -- jilles */
	if (CURRTIME - si->smu->memo_ratelimit_time > MEMO_MAX_TIME)
		si->smu->memo_ratelimit_num = 0;
	if (si->smu->memo_ratelimit_num > MEMO_MAX_NUM && !has_priv(si, PRIV_FLOOD))
	{
		command_fail(si, fault_toomany, _("You have used this command too many times; please wait a while and try again."));
		return;
	}

	/* Check for memo text length -- includes/common.h */
	if (strlen(m) >= MEMOLEN)
	{
		command_fail(si, fault_badparams, 
			"Please make sure your memo is less than %d characters", MEMOLEN);
		
		return;
	}

	/* Check to make sure the memo doesn't contain hostile CTCP responses.
	 * realistically, we'll probably want to check the _entire_ message for this... --nenolod
	 */
	if (*m == '\001')
	{
		command_fail(si, fault_badparams, _("Your memo contains invalid characters."));
		return;
	}
	
	mg = group(myentity_find(target));
	if (mg == NULL)
	{
		command_fail(si, fault_nosuch_target, "Group \2%s\2 is not registered.", target);
		return;
	}

	si->smu->memo_ratelimit_num++;
	si->smu->memo_ratelimit_time = CURRTIME;

	MOWGLI_ITER_FOREACH(tn, mg->acs.head)
	{
		groupacs_t *ga = (groupacs_t *) tn->data;
		tmu = ga->mu;

		if (!(ga->flags & GA_MEMOS) || tmu == NULL || tmu == si->smu)
			continue;

		tried++;

		/* Does the user allow memos? --pfish */
		if (tmu->flags & MU_NOMEMO)
			continue;

		/* Check to make sure target inbox not full */
		if (tmu->memos.count >= me.mdlimit)
			continue;

		/* As in SEND to a single user, make ignore fail silently */
		sent++;

		/* Make sure we're not on ignore */
		ignored = false;
		MOWGLI_ITER_FOREACH(n, tmu->memo_ignores.head)
		{
			mynick_t *mn;
			myuser_t *mu;

			if (nicksvs.no_nick_ownership)
				mu = myuser_find((const char *)n->data);
			else
			{
				mn = mynick_find((const char *)n->data);
				mu = mn != NULL ? mn->owner : NULL;
			}
			if (mu == si->smu)
				ignored = true;
		}
		if (ignored)
			continue;

		/* Malloc and populate struct */
		memo = smalloc(sizeof(mymemo_t));
		memo->sent = CURRTIME;
		memo->status = MEMO_CHANNEL;
		strlcpy(memo->sender,entity(si->smu)->name,NICKLEN);
		snprintf(memo->text, MEMOLEN, "%s %s", entity(mg)->name, m);

		/* Create a linked list node and add to memos */
		n = mowgli_node_create();
		mowgli_node_add(memo, n, &tmu->memos);
		tmu->memoct_new++;

		/* Should we email this? */
		if (tmu->flags & MU_EMAILMEMOS)
		{
			sendemail(si->su, EMAIL_MEMO, tmu, memo->text);
		}

		memoserv = service_find("memoserv");
		if (memoserv == NULL)
			memoserv = si->service;

		/* Is the user online? If so, tell them about the new memo. */
		if (si->su == NULL || !irccasecmp(si->su->nick, entity(si->smu)->name))
			myuser_notice(memoserv->nick, tmu, "You have a new memo from %s (%zu).", entity(si->smu)->name, MOWGLI_LIST_LENGTH(&tmu->memos));
		else
			myuser_notice(memoserv->nick, tmu, "You have a new memo from %s (nick: %s) (%zu).", entity(si->smu)->name, si->su->nick, MOWGLI_LIST_LENGTH(&tmu->memos));
		myuser_notice(memoserv->nick, tmu, _("To read it, type /%s%s READ %zu"),
					ircd->uses_rcommand ? "" : "msg ", memoserv->disp, MOWGLI_LIST_LENGTH(&tmu->memos));
	}

	/* Tell user memo sent, return */
	if (sent > 4)
		command_add_flood(si, FLOOD_HEAVY);
	else if (sent > 1)
		command_add_flood(si, FLOOD_MODERATE);
	if (operoverride)
		logcommand(si, CMDLOG_ADMIN, "SENDGROUP: to \2%s\2 (%d/%d sent) (oper override)", entity(mg)->name, sent, tried);
	else
		logcommand(si, CMDLOG_SET, "SENDGROUP: to \2%s\2 (%d/%d sent)", entity(mg)->name, sent, tried);
	command_success_nodata(si, _("The memo has been successfully sent to %d members on \2%s\2."), sent, entity(mg)->name);
	return;
}	

/* vim:cinoptions=>s,e0,n0,f0,{0,}0,^0,=s,ps,t0,c3,+s,(2s,us,)20,*30,gs,hs
 * vim:ts=8
 * vim:sw=8
 * vim:noexpandtab
 */
