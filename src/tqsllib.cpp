/***************************************************************************
                          tqsllib.c  -  description
                             -------------------
    begin                : Mon May 20 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include "tqsllib.h"
#include "tqslerrno.h"
#include "adif.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/evp.h>

int tQSL_Error = 0;
ADIF_GET_FIELD_ERROR tQSL_ADIF_Error;
const char *tQSL_BaseDir = 0;
char tQSL_ErrorFile[256];
char tQSL_CustomError[256];

#define TQSL_NAME_ITEM_CALLSIGN "1.3.6.1.4.1.12348.1.1"
#define TQSL_NAME_ITEM_QSO_NOT_BEFORE "1.3.6.1.4.1.12348.1.2"
#define TQSL_NAME_ITEM_QSO_NOT_AFTER "1.3.6.1.4.1.12348.1.3"

static char *custom_objects[][3] = {
	{ TQSL_NAME_ITEM_CALLSIGN, "AROcallsign", NULL },
	{ TQSL_NAME_ITEM_QSO_NOT_BEFORE, "QSONotBeforeDate", NULL },
	{ TQSL_NAME_ITEM_QSO_NOT_AFTER, "QSONotAfterDate", NULL }
};

static char *error_strings[] = {
	"Memory allocation failure",                        /* TQSL_ALLOC_ERROR */
	"Unable to initialize random number generator",     /* TQSL_RANDOM_ERROR */
	"Invalid argument",                                 /* TQSL_ARGUMENT_ERROR */
	"Operator aborted operation",						/* TQSL_OPERATOR_ABORT */
};

int
tqsl_init() {
	static char semaphore = 0;
	unsigned int i;
	static char path[256];

	tqsl_getErrorString();	/* Clear the error status */
	if (semaphore)
		return 0;
	ERR_load_crypto_strings();
	OpenSSL_add_all_algorithms();
	for (i = 0; i < (sizeof custom_objects / sizeof custom_objects[0]); i++) {
		if (OBJ_create(custom_objects[i][0], custom_objects[i][1], custom_objects[i][2]) == 0) {
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 1;
		}
	}
	if (tQSL_BaseDir == NULL) {
		char *cp;
		if ((cp = getenv("TQSLDIR")) != NULL && *cp != '\0')
			strncpy(path, cp, sizeof path);
		else {
#ifdef WINDOWS
			strcpy(path, "C:\\TQSL");
#else
#ifdef LOTW_SERVER
			strcpy(path, "/var/lotw/tqsl");
#else
			if (getenv("HOME") != NULL) {
				strncpy(path, getenv("HOME"), sizeof path);
				strncat(path, "/", sizeof path - strlen(path));
				strncat(path, ".tqsl", sizeof path - strlen(path));
			} else
				strcpy(path, ".tqsl");
#endif	/* LOTW_SERVER */
#endif  /* WINDOWS */
		}
		if ((i = mkdir(path, 0700)) != 0 && errno != EEXIST) {
			strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
			tQSL_Error = TQSL_SYSTEM_ERROR;
			return 1;
		}
		tQSL_BaseDir = path;
	}
	semaphore = 1;
	return 0;
}

const char *
tqsl_getErrorString_v(int err) {
	static char buf[256];
	unsigned long openssl_err;
	int adjusted_err;

	if (err == 0)
		return "NO ERROR";
	if (err == TQSL_CUSTOM_ERROR) {
		if (tQSL_CustomError[0] == 0)
			return "Unknown custom error";
		else {
			strncpy(buf, tQSL_CustomError, sizeof buf);
			return buf;
		}
	}
	if (err == TQSL_SYSTEM_ERROR) {
		strcpy(buf, "System error: ");
		if (strlen(tQSL_ErrorFile) > 0) {
			strncat(buf, tQSL_ErrorFile, sizeof buf - strlen(buf));
			strncat(buf, ": ", sizeof buf - strlen(buf));
		}
		strncat(buf, strerror(errno), sizeof buf - strlen(buf));
		return buf;
	}
	if (err == TQSL_OPENSSL_ERROR) {
		openssl_err = ERR_get_error();
		strcpy(buf, "OpenSSL error: ");
		if (openssl_err)
			ERR_error_string_n(openssl_err, buf + strlen(buf), sizeof buf - strlen(buf));
		else
			strncat(buf, "[error code not available]", sizeof buf - strlen(buf));
		return buf;
	}
	if (err == TQSL_ADIF_ERROR) {
		buf[0] = 0;
		if (strlen(tQSL_ErrorFile) > 0) {
			strncpy(buf, tQSL_ErrorFile, sizeof buf);
			strncat(buf, ": ", sizeof buf - strlen(buf));
		}
		strncat(buf, adifGetError(tQSL_ADIF_Error), sizeof buf - strlen(buf));
		return buf;
	}
	adjusted_err = err - TQSL_ERROR_ENUM_BASE;
	if (adjusted_err < 0 || adjusted_err >= (int)(sizeof error_strings / sizeof error_strings[0])) {
		sprintf(buf, "Invalid error code: %d", err);
		return buf;
	}
	return error_strings[adjusted_err];
}

const char *
tqsl_getErrorString() {
	const char *cp;
	cp = tqsl_getErrorString_v(tQSL_Error);
	tQSL_Error = TQSL_NO_ERROR;
	tQSL_ErrorFile[0] = 0;
	tQSL_CustomError[0] = 0;
	return cp;
}
