/***************************************************************************
                          tqsllib.h  -  description
                             -------------------
    begin                : Mon May 20 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef TQSLLIB_H
#define TQSLLIB_H

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "adif.h"

/** \file
  * tQSL library functions.
  */

#define TQSL_CERT_CB_USER            0
#define TQSL_CERT_CB_CA              1
#define TQSL_CERT_CB_ROOT            2
#define TQSL_CERT_CB_CERT_TYPE(x)    ((x) & 0xf)
#define TQSL_CERT_CB_MILESTONE       0
#define TQSL_CERT_CB_RESULT          0x10
#define TQSL_CERT_CB_CALL_TYPE(x)    ((x) & TQSL_CERT_CB_RESULT)
#define TQSL_CERT_CB_PROMPT          0
#define TQSL_CERT_CB_WARNING         0x100
#define TQSL_CERT_CB_ERROR           0x200
#define TQSL_CERT_CB_RESULT_TYPE(x)  ((x) & 0x0f00)

struct tqsl_cert_req_st {
	char *callSign;
	char *name;
	char *address1;
	char *address2;
	char *city;
	char *state;
	char *postalCode;
	char *country;
	char *emailAddress;
	int dxccEntity;
};
typedef struct tqsl_cert_req_st TQSL_CERT_REQ;

/** Error code from most recent tQSL library call.
  *
  * The values for the error code are defined in tqslerrno.h */
extern int tQSL_Error;
extern ADIF_GET_FIELD_ERROR tQSL_ADIF_Error;
/// File name of file giving error. (May be empty.)
extern char tQSL_ErrorFile[256];
/// Base directory for tQSL library working files.
extern const char *tQSL_BaseDir;
extern char tQSL_CustomError[256];

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the tQSL library */
int tqsl_init();
/** Gets the error string corresponding to the given error number.
  * The error string is available only until the next call to
  * tqsl_getErrorString_v or tqsl_getErrorString.
  */
const char *tqsl_getErrorString_v(int err);
/** Gets the error string for the current tQSL library error and resets the error status.
  * See tqsl_getErrorString_v().
  */
const char *tqsl_getErrorString();

typedef void * tQSL_Cert;

/* tQSL_Cert *tqsl_selectCertificates(); */
/* int tqsl_checkCertificate(tQSL_Cert); */
/** Import a Gabbi cert file received from a CA
  *
  * The callback, \c cb, will be called whenever a certificate is ready
  * to be imported:
  *
  *    cb(type, message);
  *
  * \c type has several fields that can be accessed via macros:
  *
  *  TQSL_CERT_CB_CALL_TYPE(type) := TQSL_CERT_CB_MILESTONE | TQSL_CERT_CB_RESULT
  *
  *  TQSL_CERT_CB_CERT_TYPE(type) := TQSL_CERT_CB_CA | TQSL_CERT_CB_CA | TQSL_CERT_CB_USER
  *
  *  TQSL_CERT_CB_RESULT_TYPE(type) := TQSL_CERT_CB_PROMPT | TQSL_CERT_CB_WARNING | TQSL_CERT_CB_ERROR
  *
  *  TQSL_CERT_CB_CERT_TYPE() is meaningful only if TQSL_CERT_CB_CALL_TYPE()==TQSL_CERT_CB_MILESTONE
  *
  *  TQSL_CERT_CB_RESULT_TYPE() is meaningful only if TQSL_CERT_CB_CALL_TYPE()==TQSL_CERT_CB_RESULT
  */
int tqsl_importCertFile(const char *file, int(*cb)(int type, const char *));

/// Create a certificate-request Gabbi file.
/** The \c req parameter must be properly populated with the required fields.
  *
  * If no password callback is supplied or if the password callback returns
  * an empty string, the generated private key will be stored unencrypted.
  */
int tqsl_createCertRequest(const char *filename, TQSL_CERT_REQ *req, int(*pwcb)(char *pwbuf, int pwsize));

/*
tqsl_beginSignVerify();
tqsl_endSignVerify();
*/


#ifdef __cplusplus
}
#endif

#endif /* TQSLLIB_H */
