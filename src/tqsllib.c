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
#ifdef __WIN32__
#include <io.h>
#endif
#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/evp.h>

#ifdef __WIN32__
#define MKDIR(x,y) mkdir(x)
#else
#define MKDIR(x,y) mkdir(x,y)
#endif

int tQSL_Error = 0;
ADIF_GET_FIELD_ERROR tQSL_ADIF_Error;
const char *tQSL_BaseDir = 0;
char tQSL_ErrorFile[256];
char tQSL_CustomError[256];

#define TQSL_NAME_ITEM_CALLSIGN "1.3.6.1.4.1.12348.1.1"
#define TQSL_NAME_ITEM_QSO_NOT_BEFORE "1.3.6.1.4.1.12348.1.2"
#define TQSL_NAME_ITEM_QSO_NOT_AFTER "1.3.6.1.4.1.12348.1.3"
#define TQSL_NAME_ITEM_DXCC_ENTITY "1.3.6.1.4.1.12348.1.4"

static char *custom_objects[][3] = {
	{ TQSL_NAME_ITEM_CALLSIGN, "AROcallsign", NULL },
	{ TQSL_NAME_ITEM_QSO_NOT_BEFORE, "QSONotBeforeDate", NULL },
	{ TQSL_NAME_ITEM_QSO_NOT_AFTER, "QSONotAfterDate", NULL },
	{ TQSL_NAME_ITEM_DXCC_ENTITY, "dxccEntity", NULL }
};

static char *error_strings[] = {
	"Memory allocation failure",                        /* TQSL_ALLOC_ERROR */
	"Unable to initialize random number generator",     /* TQSL_RANDOM_ERROR */
	"Invalid argument",                                 /* TQSL_ARGUMENT_ERROR */
	"Operator aborted operation",						/* TQSL_OPERATOR_ABORT */
	"No private key matches the selected certificate",	/* TQSL_NOKEY_ERROR */
	"Buffer too small",	                                /* TQSL_BUFFER_ERROR */
	"Invalid date string", 								/* TQSL_INVALID_DATE */
	"Certificate not initialized for signing",			/* TQSL_SIGNINIT_ERROR */
	"Password not correct",								/* TQSL_PASSWORD_ERROR */
};

int
tqsl_init() {
	static char semaphore = 0;
	unsigned int i;
	static char path[256];

	ERR_clear_error();
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
#ifdef __WIN32__
			strcpy(path, "C:/TQSL");
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
		if ((i = MKDIR(path, 0700)) != 0 && errno != EEXIST) {
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

int
tqsl_encodeBase64(const unsigned char *data, int datalen, char *output, int outputlen) {
	BIO *bio = NULL, *bio64 = NULL;
	int n;
	char *memp;
	int rval = 1;

	if (data == NULL || output == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return rval;
	}
	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		goto err;
	if ((bio64 = BIO_new(BIO_f_base64())) == NULL)
		goto err;
	bio = BIO_push(bio64, bio);
	if (BIO_write(bio, data, datalen) < 1)
		goto err;
	BIO_flush(bio);
	n = BIO_get_mem_data(bio, &memp);
	if (n > outputlen-1) {
		tQSL_Error = TQSL_BUFFER_ERROR;	
		goto end;
	}
	memcpy(output, memp, n);
	output[n] = 0;
	BIO_free_all(bio);
	bio = NULL;
	rval = 0;
	goto end;

err:
	tQSL_Error = TQSL_OPENSSL_ERROR;
end:
	if (bio != NULL)
		BIO_free_all(bio);
	return rval;
}

int
tqsl_decodeBase64(const char *input, unsigned char *data, int *datalen) {
	BIO *bio = NULL, *bio64 = NULL;
	int n;
	int rval = 1;

	if (input == NULL || data == NULL || datalen == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return rval;
	}
	if ((bio = BIO_new_mem_buf((void *)input, strlen(input))) == NULL)
		goto err;
	BIO_set_mem_eof_return(bio, 0);
	if ((bio64 = BIO_new(BIO_f_base64())) == NULL)
		goto err;
	bio = BIO_push(bio64, bio);
	n = BIO_read(bio, data, *datalen);
	if (n < 0)
		goto err;
	if (BIO_ctrl_pending(bio) != 0) {
		tQSL_Error = TQSL_BUFFER_ERROR;
		goto end;
	}
	*datalen = n;
	rval = 0;
	goto end;

err:
	tQSL_Error = TQSL_OPENSSL_ERROR;
end:
	if (bio != NULL)
		BIO_free_all(bio);
	return rval;
}

/* Convert a tQSL_Date field to an ISO-format date string
 */
char *
tqsl_convertDateToText(tQSL_Date *date, char *buf, int bufsiz) {
	char lbuf[10];
	int len;
	char *cp = buf;
	int bufleft = bufsiz-1;

	if (date == NULL || buf == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return NULL;
	}
	if (date->year < 1 || date->year > 9999 || date->month < 1
		|| date->month > 12 || date->day < 1 || date->day > 31)
		return NULL;
	len = sprintf(lbuf, "%04d-", date->year);
	strncpy(cp, lbuf, bufleft);
	cp += len;
	bufleft -= len;
	len = sprintf(lbuf, "%02d-", date->month);
	if (bufleft > 0)
		strncpy(cp, lbuf, bufleft);
	cp += len;
	bufleft -= len;
	len = sprintf(lbuf, "%02d", date->day);
	if (bufleft > 0)
		strncpy(cp, lbuf, bufleft);
	bufleft -= len;
	if (bufleft < 0)
		return NULL;
	buf[bufsiz-1] = '\0';
	return buf;
}

int
tqsl_isDateValid(const tQSL_Date *d) {
	static int mon_days[] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

	if (d == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 0;
	}
	if (d->year < 1 || d->year > 9999)
		return 0;
	if (d->month < 1 || d->month > 12)
		return 0;
	if (d->day < 1 || d->day > 31)
		return 0;
	mon_days[2] = ((d->year % 4) == 0 && ((d->year % 100) != 0 || (d->year % 400) == 0))
		? 29 : 28;
	if (d->day > mon_days[d->month])
		return 0;
	return 1;
}

int
tqsl_isDateNull(const tQSL_Date *d) {
	if (d == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	return (d->year == 0 && d->month == 0 && d->day == 0) ? 1 : 0;
}

/* Compare two tQSL_Date values, returning -1, 0, 1
 */
int
tqsl_compareDates(const tQSL_Date *a, const tQSL_Date *b) {
	if (a == NULL || b == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (a->year < b->year)
		return -1;
	if (a->year > b->year)
		return 1;
	if (a->month < b->month)
		return -1;
	if (a->month > b->month)
		return 1;
	if (a->day < b->day)
		return -1;
	if (a->day > b->day)
		return 1;
	return 0;
}

/* Fill a tQSL_Date struct with the date from a text string
 */
int
tqsl_initDate(tQSL_Date *date, const char *str) {
	const char *cp;

	if (date == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (str == NULL) {
		date->year = date->month = date->day = 0;
		return 0;
	}
	if ((cp = strchr(str, '-')) != NULL) {
		/* Parse YYYY-MM-DD */
		date->year = atoi(str);
		cp++;
		date->month = atoi(cp);
		cp = strchr(cp, '-');
		if (cp == NULL)
			goto err;
		cp++;
		date->day = atoi(cp);
	} else if (strlen(str) == 8) {
		/* Parse YYYYMMDD */
		char frag[10];
		strncpy(frag, str, 4);
		frag[4] = 0;
		date->year = atoi(frag);
		strncpy(frag, str+4, 2);
		frag[2] = 0;
		date->month = atoi(frag);
		date->day = atoi(str+6);
	} else	/* Invalid ISO date string */
		goto err;
	if (date->year < 1 || date->year > 9999)
		goto err;
	if (date->month < 1 || date->month > 12)
		goto err;
	if (date->day < 1 || date->day > 31)
		goto err;
	return 0;
err:
	tQSL_Error = TQSL_INVALID_DATE;
		return 1;
}
