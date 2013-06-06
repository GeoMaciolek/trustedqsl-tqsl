/***************************************************************************
                          tqsllib.c  -  description
                             -------------------
    begin                : Mon May 20 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#define TQSLLIB_DEF

//#include "sysconfig.h" //KC2YWE: Removed provisionally

#include "tqsllib.h"
#include "tqslerrno.h"
#include "adif.h"
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#ifdef _WIN32
	#include <io.h>
	#include <windows.h>
    #include <direct.h>
#endif
#include <openssl/err.h>
#include <openssl/objects.h>
#include <openssl/evp.h>

#ifdef _WIN32
#define MKDIR(x,y) _mkdir(x)
#else
#define MKDIR(x,y) mkdir(x,y)
#endif

DLLEXPORTDATA int tQSL_Error = 0;
DLLEXPORTDATA int tQSL_Errno = 0;
DLLEXPORTDATA TQSL_ADIF_GET_FIELD_ERROR tQSL_ADIF_Error;
const char *tQSL_BaseDir = 0;
DLLEXPORTDATA char tQSL_ErrorFile[256];
DLLEXPORTDATA char tQSL_CustomError[256];

#define TQSL_OID_BASE "1.3.6.1.4.1.12348.1."
#define TQSL_OID_CALLSIGN TQSL_OID_BASE "1"
#define TQSL_OID_QSO_NOT_BEFORE TQSL_OID_BASE "2"
#define TQSL_OID_QSO_NOT_AFTER TQSL_OID_BASE "3"
#define TQSL_OID_DXCC_ENTITY TQSL_OID_BASE "4"
#define TQSL_OID_SUPERCEDED_CERT TQSL_OID_BASE "5"
#define TQSL_OID_CRQ_ISSUER_ORGANIZATION TQSL_OID_BASE "6"
#define TQSL_OID_CRQ_ISSUER_ORGANIZATIONAL_UNIT TQSL_OID_BASE "7"

static const char *custom_objects[][3] = {
	{ TQSL_OID_CALLSIGN, "AROcallsign", NULL },
	{ TQSL_OID_QSO_NOT_BEFORE, "QSONotBeforeDate", NULL },
	{ TQSL_OID_QSO_NOT_AFTER, "QSONotAfterDate", NULL },
	{ TQSL_OID_DXCC_ENTITY, "dxccEntity", NULL },
	{ TQSL_OID_SUPERCEDED_CERT, "supercededCertificate", NULL },
	{ TQSL_OID_CRQ_ISSUER_ORGANIZATION, "tqslCRQIssuerOrganization", NULL },
	{ TQSL_OID_CRQ_ISSUER_ORGANIZATIONAL_UNIT, "tqslCRQIssuerOrganizationalUnit", NULL },
};

static const char *error_strings[] = {
	"Memory allocation failure",				/* TQSL_ALLOC_ERROR */
	"Unable to initialize random number generator",		/* TQSL_RANDOM_ERROR */
	"Invalid argument",					/* TQSL_ARGUMENT_ERROR */
	"Operator aborted operation",				/* TQSL_OPERATOR_ABORT */
	"No private key matches the selected certificate",	/* TQSL_NOKEY_ERROR */
	"Buffer too small",					/* TQSL_BUFFER_ERROR */
	"Invalid date format",					/* TQSL_INVALID_DATE */
	"Certificate not initialized for signing",		/* TQSL_SIGNINIT_ERROR */
	"Password not correct",					/* TQSL_PASSWORD_ERROR */
	"Expected name",					/* TQSL_EXPECTED_NAME */
	"Name exists",						/* TQSL_NAME_EXISTS */
	"Data for this DXCC entity could not be found",		/* TQSL_NAME_NOT_FOUND */
	"Invalid time format",					/* TQSL_INVALID_TIME */
	"QSO date is not within the date range specified on your Callsign Certificate",	/* TQSL_CERT_DATE_MISMATCH */
	"Certificate provider not found",			/* TQSL_PROVIDER_NOT_FOUND */
	"No certificate for key",				/* TQSL_CERT_KEY_ONLY */
	"Configuration file cannot be opened",			/* TQSL_CONFIG_ERROR */
	"Certificate or private key not found",			/* TQSL_CERT_NOT_FOUND */
	"PKCS#12 file not TQSL compatible",			/* TQSL_PKCS12_ERROR */
	"Certificate not TQSL compatible",			/* TQSL_CERT_TYPE_ERROR */
	"Date out of range",					/* TQSL_DATE_OUT_OF_RANGE */
	"Duplicate QSO suppressed",				/* TQSL_DUPLICATE_QSO */
	"Database error",					/* TQSL_DB_ERROR */
	"The selected location could not be found",		/* TQSL_LOCATION_NOT_FOUND */
	"The selected callsign could not be found",		/* TQSL_CALL_NOT_FOUND */
	"The TQSL configuration file cannot be parsed",		/* TQSL_CONFIG_SYNTAX_ERROR */
	"This file can not be processed due to a system error",	/* TQSL_FILE_SYSTEM_ERROR */
	"The format of this file is incorrect.",		/* TQSL_FILE_SYNTAX_ERROR */
};

static int pmkdir(const char *path, int perm) {
	char dpath[TQSL_MAX_PATH_LEN];
	char npath[TQSL_MAX_PATH_LEN];
	char *cp;

	strncpy(dpath, path, sizeof dpath);
	cp = strtok(dpath, "/\\");
	npath[0] = 0;
	while (cp) {
		if (strlen(cp) > 0 && cp[strlen(cp)-1] != ':') {
			strcat(npath, "/");
			strcat(npath, cp);
			if (MKDIR(npath, perm) != 0 && errno != EEXIST)
				return 1;
		} else
			strcat(npath, cp);
		cp = strtok(NULL, "/\\");
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_init() {
	static char semaphore = 0;
	unsigned int i;
	static char path[TQSL_MAX_PATH_LEN];
#ifdef _WIN32
	//lets cin/out/err work in windows
	//AllocConsole();
	//freopen("CONIN$", "r", stdin); 
//freopen("CONOUT$", "w", stdout);
//freopen("CONOUT$", "w", stderr); 
	static char shortPath[TQSL_MAX_PATH_LEN];
	HKEY hkey;
	DWORD dtype;
	DWORD bsize = sizeof path;
	int wval;
#endif

	/* OpenSSL API tends to change between minor version numbers, so make sure
	 * we're using the right version */
	long SSLver = SSLeay();
	int SSLmajor = (SSLver >> 28) & 0xff;
	int SSLminor = (SSLver >> 20) & 0xff;
	int TQSLmajor = (OPENSSL_VERSION_NUMBER >> 28) & 0xff;
	int TQSLminor =  (OPENSSL_VERSION_NUMBER >> 20) & 0xff;

	if (SSLmajor != TQSLmajor || 
		(SSLminor != TQSLminor && 
		(SSLmajor != 9 && SSLminor != 7 && TQSLminor == 6))) {
		tQSL_Error = TQSL_OPENSSL_VERSION_ERROR;
		return 1;
	}
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
#if defined(_WIN32)
			if ((wval = RegOpenKeyEx(HKEY_CURRENT_USER,
				"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders",
				0, KEY_READ, &hkey)) == ERROR_SUCCESS) {
				wval = RegQueryValueEx(hkey, "AppData", 0, &dtype, (LPBYTE)path, &bsize);
				RegCloseKey(hkey);
			}
			if (wval != ERROR_SUCCESS)
				strcpy(path, "C:");
			wval = GetShortPathName(path, shortPath, TQSL_MAX_PATH_LEN);
			if (wval != 0)
				strncpy(path, shortPath, TQSL_MAX_PATH_LEN);
			strcat(path, "/TrustedQSL");
#elif defined(LOTW_SERVER)
			strcpy(path, "/var/lotw/tqsl");
#else //some unix flavor
			if (getenv("HOME") != NULL) {
				strncpy(path, getenv("HOME"), sizeof path);
				strncat(path, "/", sizeof path - strlen(path)-1);
				strncat(path, ".tqsl", sizeof path - strlen(path)-1);
			} else
				strcpy(path, ".tqsl");
#endif
		}
		if (pmkdir(path, 0700)) {
			strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
			tQSL_Error = TQSL_SYSTEM_ERROR;
			tQSL_Errno = errno;
			return 1;
		}
		tQSL_BaseDir = path;
	}
	semaphore = 1;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setDirectory(const char *dir) {
	static char path[TQSL_MAX_PATH_LEN];
	if (strlen(dir) >= TQSL_MAX_PATH_LEN) {
		tQSL_Error = TQSL_BUFFER_ERROR;
		return 1;
	}
	strcpy(path, dir);
	tQSL_BaseDir = path;
	return 0;
}

DLLEXPORT const char* CALLCONVENTION
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
			strncat(buf, tQSL_ErrorFile, sizeof buf - strlen(buf)-1);
			strncat(buf, ": ", sizeof buf - strlen(buf)-1);
		}
		strncat(buf, strerror(errno), sizeof buf - strlen(buf)-1);
		return buf;
	}
	if (err == TQSL_OPENSSL_ERROR) {
		openssl_err = ERR_get_error();
		strcpy(buf, "OpenSSL error: ");
		if (openssl_err)
			ERR_error_string_n(openssl_err, buf + strlen(buf), sizeof buf - strlen(buf)-1);
		else
			strncat(buf, "[error code not available]", sizeof buf - strlen(buf)-1);
		return buf;
	}
	if (err == TQSL_ADIF_ERROR) {
		buf[0] = 0;
		if (strlen(tQSL_ErrorFile) > 0) {
			strncpy(buf, tQSL_ErrorFile, sizeof buf);
			strncat(buf, ": ", sizeof buf - strlen(buf)-1);
		}
		strncat(buf, tqsl_adifGetError(tQSL_ADIF_Error), sizeof buf - strlen(buf)-1);
		return buf;
	}
	if (err == TQSL_CABRILLO_ERROR) {
		buf[0] = 0;
		if (strlen(tQSL_ErrorFile) > 0) {
			strncpy(buf, tQSL_ErrorFile, sizeof buf);
			strncat(buf, ": ", sizeof buf - strlen(buf)-1);
		}
		strncat(buf, tqsl_cabrilloGetError(tQSL_Cabrillo_Error), sizeof buf - strlen(buf)-1);
		return buf;
	}
	if (err == TQSL_OPENSSL_VERSION_ERROR) {
		sprintf(buf, "Incompatible OpenSSL Library version %d.%d.%d; expected %d.%d.%d",
			int(SSLeay() >> 28) & 0xff, int(SSLeay() >> 20) & 0xff, int(SSLeay() >> 12) & 0xff,
			int(OPENSSL_VERSION_NUMBER >> 28) & 0xff, int(OPENSSL_VERSION_NUMBER >> 20) & 0xff,
			int(OPENSSL_VERSION_NUMBER >> 12) & 0xff);
		return buf;
	}
	adjusted_err = err - TQSL_ERROR_ENUM_BASE;
	if (adjusted_err < 0 || adjusted_err >= (int)(sizeof error_strings / sizeof error_strings[0])) {
		sprintf(buf, "Invalid error code: %d", err);
		return buf;
	}
	return error_strings[adjusted_err];
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getErrorString() {
	const char *cp;
	cp = tqsl_getErrorString_v(tQSL_Error);
	tQSL_Error = TQSL_NO_ERROR;
	tQSL_ErrorFile[0] = 0;
	tQSL_CustomError[0] = 0;
	return cp;
}

DLLEXPORT int CALLCONVENTION
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
	if (BIO_flush(bio) != 1)
		goto err;;
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

DLLEXPORT int CALLCONVENTION
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
DLLEXPORT char* CALLCONVENTION
tqsl_convertDateToText(const tQSL_Date *date, char *buf, int bufsiz) {
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

DLLEXPORT int CALLCONVENTION
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

DLLEXPORT int CALLCONVENTION
tqsl_isDateNull(const tQSL_Date *d) {
	if (d == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	return (d->year == 0 && d->month == 0 && d->day == 0) ? 1 : 0;
}

/* Convert a tQSL_Time field to an ISO-format date string
 */
DLLEXPORT char* CALLCONVENTION
tqsl_convertTimeToText(const tQSL_Time *time, char *buf, int bufsiz) {
	char lbuf[10];
	int len;
	char *cp = buf;
	int bufleft = bufsiz-1;

	if (time == NULL || time == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return NULL;
	}
	if (!tqsl_isTimeValid(time))
		return NULL;
	len = sprintf(lbuf, "%02d:", time->hour);
	strncpy(cp, lbuf, bufleft);
	cp += len;
	bufleft -= len;
	len = sprintf(lbuf, "%02d:", time->minute);
	if (bufleft > 0)
		strncpy(cp, lbuf, bufleft);
	cp += len;
	bufleft -= len;
	len = sprintf(lbuf, "%02d", time->second);
	if (bufleft > 0)
		strncpy(cp, lbuf, bufleft);
	cp += len;
	bufleft -= len;
	if (bufleft > 0)
		strcpy(cp, "Z");
	bufleft -= 1;
	if (bufleft < 0)
		return NULL;
	buf[bufsiz-1] = '\0';
	return buf;
}

DLLEXPORT int CALLCONVENTION
tqsl_isTimeValid(const tQSL_Time *t) {
	if (t == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 0;
	}
	if (t->hour < 0 || t->hour > 23)
		return 0;
	if (t->minute < 0 || t->minute > 59)
		return 0;
	if (t->second < 0 || t->second > 59)
		return 0;
	return 1;
}

/* Compare two tQSL_Date values, returning -1, 0, 1
 */
DLLEXPORT int CALLCONVENTION
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
DLLEXPORT int CALLCONVENTION
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

/* Fill a tQSL_Time struct with the time from a text string
 */
DLLEXPORT int CALLCONVENTION
tqsl_initTime(tQSL_Time *time, const char *str) {
	const char *cp;
	int parts[3];
	int i;

	if (time == NULL || str == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	time->hour = time->minute = time->second = 0;
	if (strlen(str) < 3) {
		tQSL_Error = TQSL_INVALID_TIME;
		return 1;
	}
	parts[0] = parts[1] = parts[2] = 0;
	for (i = 0, cp = str; i < int(sizeof parts / sizeof parts[0]); i++) {
		if (strlen(cp) < 2)
			break;
		if (!isdigit(*cp) || !isdigit(*(cp+1)))
			goto err;
		if (i == 0 && strlen(str) == 3) {
			// Special case: HMM -- no colons, one-digit hour
			parts[i] = *cp - '0';
			++cp;
		} else {
			parts[i] = (*cp - '0') * 10 + *(cp+1) - '0';
			cp += 2;
		}
		if (*cp == ':')
			cp++;
	}
	
	if (parts[0] < 0 || parts[0] > 23)
		goto err;
	if (parts[1] < 0 || parts[1] > 59)
		goto err;
	if (parts[2] < 0 || parts[2] > 59)
		goto err;
	time->hour = parts[0];
	time->minute = parts[1];
	time->second = parts[2];
	return 0;
err:
	tQSL_Error = TQSL_INVALID_TIME;
		return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getVersion(int *major, int *minor) {
	if (major)
		*major = TQSLLIB_VERSION_MAJOR;
	if (minor)
		*minor = TQSLLIB_VERSION_MINOR;
	return 0;
}
