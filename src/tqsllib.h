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

typedef void * tQSL_Cert;

/** Struct that holds y-m-d */
typedef struct {
	int year;
	int month;
	int day;
} tQSL_Date;

/** Certificate request data */
typedef struct tqsl_cert_req_st {
	const char *callSign;
	const char *name;
	const char *address1;
	const char *address2;
	const char *city;
	const char *state;
	const char *postalCode;
	const char *country;
	const char *emailAddress;
	int dxccEntity;
	tQSL_Date qsoNotBefore;
	tQSL_Date qsoNotAfter;
	const char *password;
	tQSL_Cert signer;
	char renew;
} TQSL_CERT_REQ;

/// Base directory for tQSL library working files.
extern const char *tQSL_BaseDir;

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup Util Utility API
  */
/** @{ */

/** Error code from most recent tQSL library call.
  *
  * The values for the error code are defined in tqslerrno.h */
extern int tQSL_Error;
/** The ADIF error code (see adif.h) */
extern ADIF_GET_FIELD_ERROR tQSL_ADIF_Error;
/** File name of file giving error. (May be empty.) */
extern char tQSL_ErrorFile[256];
/** Custom error message string */
extern char tQSL_CustomError[256];

/** Initialize the tQSL library */
int tqsl_init();

/** Gets the error string for the current tQSL library error and resets the error status.
  * See tqsl_getErrorString_v().
  */
const char *tqsl_getErrorString();
/** Gets the error string corresponding to the given error number.
  * The error string is available only until the next call to
  * tqsl_getErrorString_v or tqsl_getErrorString.
  */
const char *tqsl_getErrorString_v(int err);

/** Encode a block of data into Base64 text.
  *
  * \li \c data = block of data to encode
  * \li \c datalen = length of \c data in bytes
  * \li \c output = pointer to output buffer
  * \li \c outputlen = size of output buffer in bytes
  *
  * Returns 0 if successful.
  */
int tqsl_encodeBase64(const unsigned char *data, int datalen, char *output, int outputlen);

/** Decode Base64 text into binary data.
  *
  * \li \c input = NUL-terminated text string of Base64-encoded data
  * \li \c data = pointer to output buffer
  * \li \c datalen = pointer to int containing the size of the output buffer in bytes
  *
  * Returns 0 if successful and places the number of resulting data bytes
  * into \c *datalen.
  */
int tqsl_decodeBase64(const char *input, unsigned char *data, int *datalen);

/** Initialize a tQSL_Date object from a date string.
  *
  * The date string must be YYYY-MM-DD or YYYYMMDD format.
  *
  * Returns 0 on success, nonzero on failure
  */
int tqsl_initDate(tQSL_Date *date, const char *str);

/** Compare two tQSL_Date objects.
  *
  * Returns:
  * - -1 if \c a < \c b
  *
  * - 0 if \c a == \c b
  *
  * - 1 if \c a > \c b
  */
int tqsl_compareDates(const tQSL_Date *a, const tQSL_Date *b);

/** Converts a tQSL_Date object to a YYYY-MM-DD string.
  *
  * Returns a pointer to \c buf or NULL on error
  */
char *tqsl_convertDateToText(tQSL_Date *date, char *buf, int bufsiz);

/** Test whether a tQSL_Date contains a valid date value
  *
  * Returns 1 if the date is valid
  */
int tqsl_isDateValid(const tQSL_Date *d);

/** Test whether a tQSL_Date is empty (contains all zeroes)
  *
  * Returns 1 if the date is null
  */
int tqsl_isDateNull(const tQSL_Date *d);

/** @} */


/** \defgroup CertStuff Certificate Handling API
  *
  * Certificates are managed by manipulating tQSL_Cert objects. A tQSL_Cert
  * contains:
  *
  * \li The identity of the organization that issued the certificate (the "issuer").
  * \li The name and call sign of the amateur radio operator (ARO).
  * \li The range of QSO dates for which this certificate can be used.
  * \li The resources needed to digitally sign and verify QSO records.
  *
  * The certificate management process consists of:
  *
  * \li Applying for a certificate. Certificate requests are produced via the
  *  tqsl_createCertRequest() function, which produces a certificate-request
  * file to send to the issuer.
  * \li Importing the certificate file received from the issuer into the local
  * "certificate store," a directory managed by the tQSL library, via
  * tqsl_importCertFile().
  * \li Selecting an appropriate certificate to use to sign a QSO record via
  * tqsl_selectCertificates().
  */

/** @{ */

/** Get a list of certificates
  *
  * Selects a set of certificates from the user's certificate store
  * based on optional selection criteria. The function produces a
  * list of tQSL_Cert objects.
  *
  * \li \c certlist - Pointer to a variable that is set by the
  * function to point to the list of tQSL_Cert objects.
  * \li \c ncerts - Pointer to an int that is set to the number
  * of objects in the \c certlist list.
  * \li \c callsign - Optional call sign to match.
  * \li \c date - Optional QSO date string in ISO format. Only certs
  * that have a QSO date range that encompasses this date will be
  * returned.
  * \li \c issuer - Optional issuer (DN) string to match.
  * \li \c validonly - Only certificates currently valid for use
  * will be returned if nonzero.
  *
  * Returns 0 on success, nonzero on failure.
  *
  * Each of the tQSL_Cert objects in the list should be freed
  * by calling tqsl_freeCertificate().
  *
  */
int tqsl_selectCertificates(tQSL_Cert **certlist, int *ncerts,
	const char *callsign, const char *date, const char *issuer, int validonly);

/** Get the issuer (DN) string from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateIssuer(tQSL_Cert cert, char *buf, int bufsiz);
/** Get the issuer's organization name from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateIssuerOrganization(tQSL_Cert cert, char *buf, int bufsiz);
/** Get the issuer's organizational unit name from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateIssuerOrganizationalUnit(tQSL_Cert cert, char *buf, int bufsiz);
/** Get the ARO call sign string from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateCallSign(tQSL_Cert cert, char *buf, int bufsiz);
/** Get the ARO name string from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateAROName(tQSL_Cert cert, char *buf, int bufsiz);

/** Get the email address from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateEmailAddress(tQSL_Cert cert, char *buf, int bufsiz);

/** Get the QSO not-before date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateQSONotBeforeDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the QSO not-after date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateQSONotAfterDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the certificate's not-before date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateNotBeforeDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the certificate's not-after date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateNotAfterDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the DXCC entity number from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c dxcc - Pointer to an int to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateDXCCEntity(tQSL_Cert cert, int *dxcc);

/** Get the first address line from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateRequestAddress1(tQSL_Cert cert, char *str, int bufsiz);

/** Get the second address line from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateRequestAddress2(tQSL_Cert cert, char *str, int bufsiz);

/** Get the city from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateRequestCity(tQSL_Cert cert, char *str, int bufsiz);

/** Get the state from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateRequestState(tQSL_Cert cert, char *str, int bufsiz);

/** Get the postal (ZIP) code from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateRequestPostalCode(tQSL_Cert cert, char *str, int bufsiz);

/** Get the country from the certificate request used in applying
  * for a tQSL_Cert certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
int tqsl_getCertificateRequestCountry(tQSL_Cert cert, char *str, int bufsiz);

#define TQSL_PK_TYPE_ERR	0
#define TQSL_PK_TYPE_NONE	1
#define TQSL_PK_TYPE_UNENC	2
#define TQSL_PK_TYPE_ENC	3

/** Determine the nature of the private key associated with a
  * certificate.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  *
  * Returns one of the following values:
  *
  * \li TQSL_PK_TYPE_ERR - An error occurred. Use tqsl_getErrorString() to examine.
  * \li TQSL_PK_TYPE_NONE - No matching private key was found.
  * \li TQSL_PK_TYPE_UNENC - The matching private key is unencrypted.
  * \li TQSL_PK_TYPE_ENC - The matching private key is encrypted
  * (password protected).
  */
int tqsl_getCertificatePrivateKeyType(tQSL_Cert cert);


/** Free the memory used by the tQSL_Cert. Once this function is called,
  * \c cert should not be used again in any way.
  */
void tqsl_freeCertificate(tQSL_Cert cert);

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

/** Create a certificate-request Gabbi file.
  *
  * The \c req parameter must be properly populated with the required fields.
  *
  * If \c req->password is NULL and \c cb is not NULL, the callback will be
  * called to acquire the password. Otherwise \c req->password will be used as
  * the password.   * If the password is NULL or an empty string the generated
  * private key will be stored unencrypted.
  *
  * If req->signer is not zero and the signing certificate requires a password,
  * the password may be in req->signer_password, else signer_pwcb is called.
  */
int tqsl_createCertRequest(const char *filename, TQSL_CERT_REQ *req,
	int(*pwcb)(char *pwbuf, int pwsize));

/** @} */

/** \defgroup Sign Signing API
  */
/** @{ */

/** Initialize the tQSL_Cert object for use in signing.
  *
  * This produces an unencrypted copy of the private key in memory.
  */
int tqsl_beginSigning(tQSL_Cert cert, char *password,  int(*pwcb)(char *pwbuf, int pwsize));

/** Get the maximum size of a signature block that will be produced
  * when the tQSL_Cert is used to sign data. (Note that the size of the
  * signature block is unaffected by the size of the data block being signed.)
  */
int tqsl_getMaxSignatureSize(tQSL_Cert cert, int *sigsize);

/** Sign a data block.
  *
  * tqsl_beginSigning() must have been called for
  * the tQSL_Cert object before calling this function.
  */
int tqsl_signDataBlock(tQSL_Cert cert, unsigned char *data, int datalen, unsigned char *sig, int *siglen);

/** Terminate signing operations for this tQSL_Cert object.
  *
  * This zero-fills the unencrypted private key in memory.
  */
int tqsl_endSigning(tQSL_Cert cert);

/** @} */

#ifdef __cplusplus
}
#endif

/* Useful defines */
#define TQSL_MAX_PW_LENGTH         32     /* Password buffer length */

#endif /* TQSLLIB_H */
