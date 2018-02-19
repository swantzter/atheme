/*
 * Copyright (c) 2011 William Pitcock <nenolod@dereferenced.org>.
 * Rights to this code are as documented in doc/LICENSE.
 *
 * UID provider stuff.
 */

#ifndef ATHEME_UID_H
#define ATHEME_UID_H

struct uid_provider
{
	void (*uid_init)(const char *sid);
	const char *(*uid_get)(void);
};

extern const struct uid_provider *uid_provider_impl;

#endif
