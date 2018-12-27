AC_DEFUN([ATHEME_DECIDE_DIGEST_FRONTEND], [

	DIGEST_FRONTEND_VAL="ATHEME_DIGEST_FRONTEND_INTERNAL"
	DIGEST_FRONTEND="Internal"
	DIGEST_API_LIBS=""

	AS_IF([test "x${DIGEST_FRONTEND}x${LIBMBEDCRYPTO}" = "xInternalxYes"], [
		DIGEST_FRONTEND_VAL="ATHEME_DIGEST_FRONTEND_MBEDTLS"
		DIGEST_FRONTEND="ARM mbedTLS"
		DIGEST_API_LIBS="${LIBMBEDCRYPTO_LIBS}"
	])

	AS_IF([test "x${DIGEST_FRONTEND}x${LIBNETTLE}" = "xInternalxYes"], [
		DIGEST_FRONTEND_VAL="ATHEME_DIGEST_FRONTEND_NETTLE"
		DIGEST_FRONTEND="Nettle"
		DIGEST_API_LIBS="${LIBNETTLE_LIBS}"
	])

	AS_IF([test "x${DIGEST_FRONTEND}x${LIBCRYPTO}" = "xInternalxYes"], [
		DIGEST_FRONTEND_VAL="ATHEME_DIGEST_FRONTEND_OPENSSL"
		DIGEST_FRONTEND="OpenSSL"
		DIGEST_API_LIBS="${LIBCRYPTO_LIBS}"
	])

	AC_DEFINE_UNQUOTED([ATHEME_DIGEST_FRONTEND], [${DIGEST_FRONTEND_VAL}], [Front-end for digest interface])
	AC_SUBST([DIGEST_API_LIBS])
])
