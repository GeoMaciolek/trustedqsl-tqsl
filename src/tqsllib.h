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

#if defined(__WIN32__) && !defined(TQSL_NODLL)
	#ifdef TQSLLIB_DEF
		#define DLLEXPORT __stdcall __declspec(dllexport)
		#define DLLEXPORTDATA __declspec(dllexport)
	#else
		#define DLLEXPORT __stdcall __declspec(dllimport)
		#define DLLEXPORTDATA __declspec(dllimport)
	#endif
#else
	#define DLLEXPORT
	#define DLLEXPORTDATA
#endif

#include "adif.h"
#include "cabrillo.h"

/** \file
  * tQSL library functions.
  */

/* Sizes */
#define TQSL_MAX_PATH_LEN            256
#define TQSL_PASSWORD_MAX            80
#define TQSL_NAME_ELEMENT_MAX        256
#define TQSL_CALLSIGN_MAX            13
#define TQSL_CRQ_NAME_MAX            60
#define TQSL_CRQ_ADDR_MAX            80
#define TQSL_CRQ_CITY_MAX            80
#define TQSL_CRQ_STATE_MAX           80
#define TQSL_CRQ_POSTAL_MAX          20
#define TQSL_CRQ_COUNTRY_MAX         80
#define TQSL_CRQ_EMAIL_MAX           180
#define TQSL_BAND_MAX                6
#define TQSL_MODE_MAX                16
#define TQSL_FREQ_MAX                20
#define TQSL_SATNAME_MAX             6
#define TQSL_PROPMODE_MAX            6

#define TQSL_CERT_CB_USER            0
#define TQSL_CERT_CB_CA              1
#define TQSL_CERT_CB_ROOT            2
#define TQSL_CERT_CB_PKEY            3
#define TQSL_CERT_CB_CONFIG          4
#define TQSL_CERT_CB_CERT_TYPE(x)    ((x) & 0xf)
#define TQSL_CERT_CB_MILESTONE       0
#define TQSL_CERT_CB_RESULT          0x10
#define TQSL_CERT_CB_CALL_TYPE(x)    ((x) & TQSL_CERT_CB_RESULT)
#define TQSL_CERT_CB_PROMPT          0
#define TQSL_CERT_CB_DUPLICATE       0x100
#define TQSL_CERT_CB_ERROR           0x200
#define TQSL_CERT_CB_LOADED          0x300
#define TQSL_CERT_CB_RESULT_TYPE(x)  ((x) & 0x0f00)

typedef void * tQSL_Cert;
typedef void * tQSL_Location;

/** Struct that holds y-m-d */
typedef struct {
	int year;
	int month;
	int day;
} tQSL_Date;

/** Struct that holds h-m-s */
typedef struct {
	int hour;
	int minute;
	int second;
} tQSL_Time;

/** Certificate provider data */
typedef struct tqsl_provider_st {
	char organizationName[TQSL_NAME_ELEMENT_MAX+1];
	char organizationalUnitName[TQSL_NAME_ELEMENT_MAX+1];
	char emailAddress[TQSL_NAME_ELEMENT_MAX+1];
	char url[TQSL_NAME_ELEMENT_MAX+1];
} TQSL_PROVIDER;

/** Certificate request data */
typedef struct tqsl_cert_req_st {
	char providerName[TQSL_NAME_ELEMENT_MAX+1];
	char providerUnit[TQSL_NAME_ELEMENT_MAX+1];
	char callSign[TQSL_CALLSIGN_MAX+1];
	char name[TQSL_CRQ_NAME_MAX+1];
	char address1[TQSL_CRQ_ADDR_MAX+1];
	char address2[TQSL_CRQ_ADDR_MAX+1];
	char city[TQSL_CRQ_CITY_MAX+1];
	char state[TQSL_CRQ_STATE_MAX+1];
	char postalCode[TQSL_CRQ_POSTAL_MAX+1];
	char country[TQSL_CRQ_COUNTRY_MAX+1];
	char emailAddress[TQSL_CRQ_EMAIL_MAX+1];
	int dxccEntity;
	tQSL_Date qsoNotBefore;
	tQSL_Date qsoNotAfter;
	char password[TQSL_PASSWORD_MAX+1];
	tQSL_Cert signer;
	char renew;
} TQSL_CERT_REQ;

/** QSO data */
typedef struct {
	char callsign[TQSL_CALLSIGN_MAX+1];
	char band[TQSL_BAND_MAX+1];
	char mode[TQSL_MODE_MAX+1];
	tQSL_Date date;
	tQSL_Time time;
	char freq[TQSL_FREQ_MAX+1];
	char rxfreq[TQSL_FREQ_MAX+1];
	char rxband[TQSL_BAND_MAX+1];
	char propmode[TQSL_PROPMODE_MAX+1];
	char satname[TQSL_SATNAME_MAX+1];
} TQSL_QSO_RECORD;

/// Base directory for tQSL library working files.
extern const char *tQSL_BaseDir;

#ifdef __cplusplus
extern "C" {
#endif

/** \defgroup Util Utility API
  */
/** @{ */

/// Error code from most recent tQSL library call.
/**
  * The values for the error code are defined in tqslerrno.h */
DLLEXPORTDATA extern int tQSL_Error;
/// The ADIF error code
DLLEXPORTDATA extern TQSL_ADIF_GET_FIELD_ERROR tQSL_ADIF_Error;
/// The ADIF error code
DLLEXPORTDATA extern TQSL_CABRILLO_ERROR_TYPE tQSL_Cabrillo_Error;
/// File name of file giving error. (May be empty.)
DLLEXPORTDATA extern char tQSL_ErrorFile[256];
/// Custom error message string
DLLEXPORTDATA extern char tQSL_CustomError[256];

/** Initialize the tQSL library
  *
  * This function should be called prior to calling any other library functions.
  */
DLLEXPORT int tqsl_init();

/** Set the directory where the TQSL files are kept.
  * May be called either before of after tqsl_init(), but should be called
  * before calling any other functions in the library.
  *
  * Note that this is purely optional. The library will figure out an
  * approriate directory if tqsl_setDirectory isn't called. Unless there is
  * some particular need to set the directory explicitly, programs should
  * refrain from doing so.
  */
DLLEXPORT int tqsl_setDirectory(const char *dir);

/** Gets the error string for the current tQSL library error and resets the error status.
  * See tqsl_getErrorString_v().
  */
DLLEXPORT const char *tqsl_getErrorString();

/** Gets the error string corresponding to the given error number.
  * The error string is available only until the next call to
  * tqsl_getErrorString_v or tqsl_getErrorString.
  */
DLLEXPORT const char *tqsl_getErrorString_v(int err);

/** Encode a block of data into Base64 text.
  *
  * \li \c data = block of data to encode
  * \li \c datalen = length of \c data in bytes
  * \li \c output = pointer to output buffer
  * \li \c outputlen = size of output buffer in bytes
  */
DLLEXPORT int tqsl_encodeBase64(const unsigned char *data, int datalen, char *output, int outputlen);

/** Decode Base64 text into binary data.
  *
  * \li \c input = NUL-terminated text string of Base64-encoded data
  * \li \c data = pointer to output buffer
  * \li \c datalen = pointer to int containing the size of the output buffer in bytes
  *
  * Places the number of resulting data bytes into \c *datalen.
  */
DLLEXPORT int tqsl_decodeBase64(const char *input, unsigned char *data, int *datalen);

/** Initialize a tQSL_Date object from a date string.
  *
  * The date string must be YYYY-MM-DD or YYYYMMDD format.
  *
  * Returns 0 on success, nonzero on failure
  */
DLLEXPORT int tqsl_initDate(tQSL_Date *date, const char *str);

/** Initialize a tQSL_Time object from a time string.
  *
  * The time string must be HH[:]MM[[:]SS] format.
  *
  * Returns 0 on success, nonzero on failure
  */
DLLEXPORT int tqsl_initTime(tQSL_Time *time, const char *str);

/** Compare two tQSL_Date objects.
  *
  * Returns:
  * - -1 if \c a < \c b
  *
  * - 0 if \c a == \c b
  *
  * - 1 if \c a > \c b
  */
DLLEXPORT int tqsl_compareDates(const tQSL_Date *a, const tQSL_Date *b);

/** Converts a tQSL_Date object to a YYYY-MM-DD string.
  *
  * Returns a pointer to \c buf or NULL on error
  */
DLLEXPORT char *tqsl_convertDateToText(const tQSL_Date *date, char *buf, int bufsiz);

/** Test whether a tQSL_Date contains a valid date value
  *
  * Returns 1 if the date is valid
  */
DLLEXPORT int tqsl_isDateValid(const tQSL_Date *d);

/** Test whether a tQSL_Date is empty (contains all zeroes)
  *
  * Returns 1 if the date is null
  */
DLLEXPORT int tqsl_isDateNull(const tQSL_Date *d);

/** Test whether a tQSL_Time contains a valid time value
  *
  * Returns 1 if the time is valid
  */
DLLEXPORT int tqsl_isTimeValid(const tQSL_Time *t);

/** Converts a tQSL_Time object to a HH:MM:SSZ string.
  *
  * Returns a pointer to \c buf or NULL on error
  */
DLLEXPORT char *tqsl_convertTimeToText(const tQSL_Time *time, char *buf, int bufsiz);

/** Returns the library version. \c major and/or \c minor may be NULL.
  */
DLLEXPORT int tqsl_getVersion(int *major, int *minor);

/** Returns the configuration-file version. \c major and/or \c minor may be NULL.
  */
DLLEXPORT int tqsl_getConfigVersion(int *major, int *minor);

/** @} */


/** \defgroup CertStuff Certificate Handling API
  *
  * Certificates are managed by manipulating \c tQSL_Cert objects. A \c tQSL_Cert
  * contains:
  *
  * \li The identity of the organization that issued the certificate (the "issuer").
  * \li The name and call sign of the amateur radio operator (ARO).
  * \li The DXCC entity number for which this certificate is valid.
  * \li The range of QSO dates for which this certificate can be used.
  * \li The resources needed to digitally sign and verify QSO records.
  *
  * The certificate management process consists of:
  *
  * \li <B>Applying for a certificate.</b> Certificate requests are produced via the
  *  tqsl_createCertRequest() function, which produces a certificate-request
  * file to send to the issuer.
  * \li <B>Importing the certificate</B> file received from the issuer into the local
  * "certificate store," a directory managed by the tQSL library, via
  * tqsl_importTQSLFile().
  * \li <B>Selecting an appropriate certificate</B> to use to sign a QSO record via
  * tqsl_selectCertificates().
  */

/** @{ */

#define TQSL_SELECT_CERT_WITHKEYS 1
#define TQSL_SELECT_CERT_EXPIRED 2
#define TQSL_SELECT_CERT_SUPERCEDED 4

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
  * \li \c flag - OR of \c TQSL_SELECT_CERT_EXPIRED (include expired
  * certs), \c TQSL_SELECT_CERT_SUPERCEDED and \c TQSL_SELECT_CERT_WITHKEYS
  * (keys that don't have associated certs will be returned).
  *
  * Returns 0 on success, nonzero on failure.
  *
  * Each of the tQSL_Cert objects in the list should be freed
  * by calling tqsl_freeCertificate().
  *
  */
DLLEXPORT int tqsl_selectCertificates(tQSL_Cert **certlist, int *ncerts,
	const char *callsign, int dxcc, const tQSL_Date *date, const TQSL_PROVIDER *issuer, int flag);

/** Get a particulat certificate from the list returnded by
  * tqsl_selectCertificates. This function exists principally
  * to make it easier for VB programs to access the list of
  * certificates.
  *
  * It is the caller's responsibility to ensure that 0 <= idx < ncerts
  * (where ncerts is the value returned by tqsl_selectCertificates)
  */
DLLEXPORT int tqsl_getSelectedCertificate(tQSL_Cert *cert, const tQSL_Cert **certlist,
	int idx);

/** Find out if the "certificate" is just a key pair.
  */
DLLEXPORT int tqsl_getCertificateKeyOnly(tQSL_Cert cert, int *keyonly);

/** Get the encoded certificate for inclusion in a GABBI file.
  */
DLLEXPORT int tqsl_getCertificateEncoded(tQSL_Cert cert, char *buf, int bufsiz);

/** Get the issuer's serial number of the certificate.
  */
DLLEXPORT int tqsl_getCertificateSerial(tQSL_Cert cert, long *serial);

/** Get the issuer's serial number of the certificate as a hexadecimal string.
  * Needed for certs with long serial numbers (typically root certs).
  */
DLLEXPORT int tqsl_getCertificateSerialExt(tQSL_Cert cert, char *serial, int serialsiz);

/** Get the length of the issuer's serial number of the certificate as it will be
  * returned by tqsl_getCertificateSerialExt.
  */
DLLEXPORT int tqsl_getCertificateSerialLength(tQSL_Cert cert);

/** Get the issuer (DN) string from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateIssuer(tQSL_Cert cert, char *buf, int bufsiz);

/** Get the issuer's organization name from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateIssuerOrganization(tQSL_Cert cert, char *buf, int bufsiz);
/** Get the issuer's organizational unit name from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateIssuerOrganizationalUnit(tQSL_Cert cert, char *buf, int bufsiz);
/** Get the ARO call sign string from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateCallSign(tQSL_Cert cert, char *buf, int bufsiz);
/** Get the ARO name string from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateAROName(tQSL_Cert cert, char *buf, int bufsiz);

/** Get the email address from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c buf - Buffer to hold the returned string.
  * \li \c bufsiz - Size of \c buf.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateEmailAddress(tQSL_Cert cert, char *buf, int bufsiz);

/** Get the QSO not-before date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateQSONotBeforeDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the QSO not-after date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateQSONotAfterDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the certificate's not-before date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateNotBeforeDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the certificate's not-after date from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c date - Pointer to a tQSL_Date struct to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateNotAfterDate(tQSL_Cert cert, tQSL_Date *date);

/** Get the DXCC entity number from a tQSL_Cert.
  *
  * \li \c cert - a tQSL_Cert object, normally one returned from
  * tqsl_selectCertificates()
  * \li \c dxcc - Pointer to an int to hold the returned date.
  *
  * Returns 0 on success, nonzero on failure.
  */
DLLEXPORT int tqsl_getCertificateDXCCEntity(tQSL_Cert cert, int *dxcc);

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
DLLEXPORT int tqsl_getCertificateRequestAddress1(tQSL_Cert cert, char *str, int bufsiz);

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
DLLEXPORT int tqsl_getCertificateRequestAddress2(tQSL_Cert cert, char *str, int bufsiz);

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
DLLEXPORT int tqsl_getCertificateRequestCity(tQSL_Cert cert, char *str, int bufsiz);

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
DLLEXPORT int tqsl_getCertificateRequestState(tQSL_Cert cert, char *str, int bufsiz);

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
DLLEXPORT int tqsl_getCertificateRequestPostalCode(tQSL_Cert cert, char *str, int bufsiz);

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
DLLEXPORT int tqsl_getCertificateRequestCountry(tQSL_Cert cert, char *str, int bufsiz);

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
  * \li \c TQSL_PK_TYPE_ERR - An error occurred. Use tqsl_getErrorString() to examine.
  * \li \c TQSL_PK_TYPE_NONE - No matching private key was found.
  * \li \c TQSL_PK_TYPE_UNENC - The matching private key is unencrypted.
  * \li \c TQSL_PK_TYPE_ENC - The matching private key is encrypted
  * (password protected).
  */
DLLEXPORT int tqsl_getCertificatePrivateKeyType(tQSL_Cert cert);


/** Free the memory used by the tQSL_Cert. Once this function is called,
  * \c cert should not be used again in any way.
  */
DLLEXPORT void tqsl_freeCertificate(tQSL_Cert cert);

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
  *  \c TQSL_CERT_CB_CALL_TYPE(type) := \c TQSL_CERT_CB_MILESTONE | \c TQSL_CERT_CB_RESULT
  *
  *  \c TQSL_CERT_CB_CERT_TYPE(type) := \c TQSL_CERT_CB_ROOT | \c TQSL_CERT_CB_CA | \c TQSL_CERT_CB_USER
  *
  *  \c TQSL_CERT_CB_RESULT_TYPE(type) := \c TQSL_CERT_CB_PROMPT | \c TQSL_CERT_CB_WARNING | \c TQSL_CERT_CB_ERROR
  *
  *  \c TQSL_CERT_CB_RESULT_TYPE() is meaningful only if \c TQSL_CERT_CB_CALL_TYPE() == \c TQSL_CERT_CB_RESULT
  */
DLLEXPORT int tqsl_importTQSLFile(const char *file, int(*cb)(int type, const char *message, void *userdata), void *user);

/** Get the serial for the first user cert from a .tq6 file
  * used to support asking the user to save their cert after import
  * \li \c file is the path to the file
  * \li \c serial is where the serial number is returned
  *
  * Returns 0 on success, nonzero on failure.
  *
  */
DLLEXPORT int tqsl_getSerialFromTQSLFile(const char *file, long *serial);

/** Get the number of certificate providers known to tqsllib.
  */
DLLEXPORT int tqsl_getNumProviders(int *n);

/** Get the information for a certificate provider.
  *
  * \li \c idx is the index, 0 <= idx < tqsl_getNumProviders()
  */
DLLEXPORT int tqsl_getProvider(int idx, TQSL_PROVIDER *provider);

/** Create a certificate-request Gabbi file.
  *
  * The \c req parameter must be properly populated with the required fields.
  *
  * If \c req->password is NULL and \c cb is not NULL, the callback will be
  * called to acquire the password. Otherwise \c req->password will be used as
  * the password.  If the password is NULL or an empty string the generated
  * private key will be stored unencrypted.
  *
  * If req->signer is not zero and the signing certificate requires a password,
  * the password may be in req->signer_password, else signer_pwcb is called.
  */
DLLEXPORT int tqsl_createCertRequest(const char *filename, TQSL_CERT_REQ *req,
	int(*pwcb)(char *pwbuf, int pwsize, void *userdata), void *user);

/** Save a key pair and certificates to a file in PKCS12 format.
  *
  * The tQSL_Cert must be initialized for signing (see tqsl_beginSigning())
  * if the user certificate is being exported.
  *
  * The supplied \c p12password is used to encrypt the PKCS12 data.
  */
DLLEXPORT int tqsl_exportPKCS12File(tQSL_Cert cert, const char *filename, const char *p12password);

/** Load certificates and a private key from a PKCS12 file.
  */
DLLEXPORT int tqsl_importPKCS12File(const char *filename, const char *p12password, const char *password,
	int (*pwcb)(char *buf, int bufsiz, void *userdata), int(*cb)(int type , const char *message, void *userdata), void *user);

/** Delete a certificate and private key
  */
DLLEXPORT int tqsl_deleteCertificate(tQSL_Cert cert);

/** @} */

/** \defgroup Sign Signing API
  *
  * The Signing API uses a tQSL_Cert (see \ref CertStuff) to digitally
  * sign a block of data.
  */
/** @{ */

/** Initialize the tQSL_Cert object for use in signing.
  *
  * This produces an unencrypted copy of the private key in memory.
  *
  * if \c password is not NULL, it must point to the password to use to decrypt
  * the private key. If \c password is NULL and \c pwcb is not NULL, \c pwcb
  * is called to get the password. If the private key is encrypted and both
  * \c password and \c pwcb are NULL, or if the supplied password fails to
  * decrypt the key, a TQSL_PASSWORD_ERROR error is returned.
  *
  * \c pwcb parameters: \c pwbuf is a pointer to a buffer of \c pwsize chars.
  * The buffer should be NUL-terminated.
  */
DLLEXPORT int tqsl_beginSigning(tQSL_Cert cert, char *password,  int(*pwcb)(char *pwbuf, int pwsize, void *userdata), void *user);

/** Test whether the tQSL_Cert object is initialized for signing.
  *
  * Returns 0 if initialized. Sets tQSL_Error to TQSL_SIGNINIT_ERROR if not.
  */
DLLEXPORT int tqsl_checkSigningStatus(tQSL_Cert cert);

/** Get the maximum size of a signature block that will be produced
  * when the tQSL_Cert is used to sign data. (Note that the size of the
  * signature block is unaffected by the size of the data block being signed.)
  */
DLLEXPORT int tqsl_getMaxSignatureSize(tQSL_Cert cert, int *sigsize);

/** Sign a data block.
  *
  * tqsl_beginSigning() must have been called for
  * the tQSL_Cert object before calling this function.
  */
DLLEXPORT int tqsl_signDataBlock(tQSL_Cert cert, const unsigned char *data, int datalen, unsigned char *sig, int *siglen);

/** Verify a signed data block.
  *
  * tqsl_beginSigning() need \em not have been called.
  */
DLLEXPORT int tqsl_verifyDataBlock(tQSL_Cert cert, const unsigned char *data, int datalen, unsigned char *sig, int siglen);

/** Sign a single QSO record
  *
  * tqsl_beginSigning() must have been called for
  * the tQSL_Cert object before calling this function.
  *
  * \c loc must be a valid tQSL_Location object. See \ref Data.
  */
DLLEXPORT int tqsl_signQSORecord(tQSL_Cert cert, tQSL_Location loc, TQSL_QSO_RECORD *rec, unsigned char *sig, int *siglen);

/** Terminate signing operations for this tQSL_Cert object.
  *
  * This zero-fills the unencrypted private key in memory.
  */
DLLEXPORT int tqsl_endSigning(tQSL_Cert cert);

/** @} */

/** \defgroup Data Data API
  *
  * The Data API is used to form data into TrustedQSL records. A TrustedQSL record
  * consists of a station record and a QSO record. Together, the two records
  * fully describe one station's end of the QSO -- just as a paper QSL card does.
  *
  * The station record contains the callsign and geographic location of the
  * station submitting the QSO record. The library manages the station records.
  * The tqsl_xxxStationLocationCapture functions are used to generate and save
  * a station record. The intent is to provide an interface that makes a step-by-step
  * system (such as a GUI "wizard") easily implemented.
  *
  * The tqsl_getStationLocation() function is used to retrieve station records.
  *
  * With the necessary station location available, a signed GABBI output file can
  * be generated using the tqsl_getGABBIxxxxx functions:
  *
  * \li tqsl_getGABBItCERT() - Returns a GABBI tCERT record for the given tQSL_Cert
  * \li tqsl_getGABBItSTATION() - Returns a GABBI tSTATION record for the given
  *    tQSL_Location
  * \li tqsl_getGABBItCONTACT() - Returns a GABBI tCONTACT record for the given
  *    TQSL_QSO_RECORD, using the given tQSL_Cert and tQSL_Location.
  *
  * The GABBI format requires that the tCERT record contain an integer identifier
  * that is unique within the GABBI file. Similarly, each tSTATION record must
  * contain a unique identifier. Aditionally, the tSTATION record must reference
  * the identifier of a preceding tCERT record. Finally, each tCONTACT record must
  * reference a preceding tSTATION record. (A GABBI processor uses these identifiers
  * and references to tie the station and contact records together and to verify
  * their signature via the certificate.) It is the responsibility of the caller
  * to supply these identifiers and to ensure that the supplied references match
  * the tQSL_Cert and tQSL_Location used to create the referenced GABBI records.
  *
  * Station Location Generation
  *
  * The station-location generation process involves determining the values
  * for a number of station-location parameters. Normally this
  * will be done by prompting the user for the values. The responses given
  * by the user may determine which later fields are required. For example,
  * if the user indicates that the DXCC entity is UNITED STATES, a later
  * field would ask for the US state. This field would not be required if the
  * DXCC entity were not in the US.
  *
  * To accommodate the dynamic nature of the field requirements, the fields
  * are ordered such that dependent fields are queried after the field(s)
  * on which they depend. To make this process acceptable in a GUI
  * system, the fields are grouped into pages, where multiple fields may
  * be displayed on the same page. The grouping is such that which fields
  * are within the page is not dependent on any of the values of the
  * fields within the page. That is, a page of fields contains the same
  * fields no matter what value any of the fields contains. (However,
  * the \em values of fields within the page can depend on the values
  * of fields that precede them in the page.)
  *
  * Here is a brief overview of the sequence of events involved in
  * generating a station location interactively, one field at a time:
  *
  * 1) Call tqsl_initStationLocationCapture() (new location) or tqsl_getStationLocation()
  * (existing location).
  *
  * 2) For \c field from 0 to tqsl_getNumLocationField():
  * \li Display the field label [tqsl_getLocationFieldDataLabel()]
  * \li Get the field content from the user. This can be a selection
  * from a list, an entered integer or an entered character string,
  * depending on the value returned by tqsl_getLocationFieldInputType().
  *
  * 3) If tqsl_hasNextStationLocationCapture() returns 1, call
  * tqsl_nextStationLocationCapture() and go back to step 2.
  *
  * In the case of a GUI system, you'll probably want to display the
  * fields in pages. The sequence of events is a bit different:
  *
  * 1) Call tqsl_initStationLocationCapture() (new location) or tqsl_getStationLocation()
  * (existing location).
  *
  * 2) For \c field from 0 to tqsl_getNumLocationField(),
  * \li Display the field label [tqsl_getLocationFieldDataLabel()]
  * \li Display the field-input control This can be a list-selection
  * or an entered character string or integer, depending on the value
  * returned by tqsl_getLocationFieldInputType().
  *
  * 3) Each time the user changes a field, call tqsl_updateStationLocationCapture().
  * This may change the allowable selection for fields that follow the field
  * the user changed, so the control for each of those fields should be updated
  * as in step 2.
  *
  * 4) Once the user has completed entries for the page, if
  * tqsl_hasNextStationLocationCapture() returns 1, call
  * tqsl_nextStationLocationCapture() and go back to step 2.
  *
  * N.B. The first two fields in the station-location capture process are
  * always call sign and DXCC entity, in that order. As a practical matter, these
  * two fields must match the corresponding fields in the available certificates.
  * The library will therefore constrain the values of these fields to match
  * what's available in the certificate store. See \ref CertStuff.
  */

/** @{ */

/* Location field input types */

#define TQSL_LOCATION_FIELD_TEXT	1
#define TQSL_LOCATION_FIELD_DDLIST	2
#define TQSL_LOCATION_FIELD_LIST	3

/* Location field data types */
#define TQSL_LOCATION_FIELD_CHAR 1
#define TQSL_LOCATION_FIELD_INT 2

/** Begin the process of generating a station record */
DLLEXPORT int tqsl_initStationLocationCapture(tQSL_Location *locp);

/** Release the station-location resources. This should be called for
  * any tQSL_Location that was initialized via tqsl_initStationLocationCapture()
  * or tqsl_getStationLocation()
  */
DLLEXPORT int tqsl_endStationLocationCapture(tQSL_Location *locp);

/** Update the pages based on the currently selected settings. */
DLLEXPORT int tqsl_updateStationLocationCapture(tQSL_Location loc);

//int tqsl_getNumStationLocationCapturePages(tQSL_Location loc, int *npages);

/** Get the current page number */
DLLEXPORT int tqsl_getStationLocationCapturePage(tQSL_Location loc, int *page);

/** Set the current page number.
  * Typically, the page number will be 1 (the starting page) or a value
  * obtained from tqsl_getStationLocationCapturePage().
  */
DLLEXPORT int tqsl_setStationLocationCapturePage(tQSL_Location loc, int page);

/** Advance the page to the next one in the page sequence */
DLLEXPORT int tqsl_nextStationLocationCapture(tQSL_Location loc);

/** Return the page to the previous one in the page sequence. */
DLLEXPORT int tqsl_prevStationLocationCapture(tQSL_Location loc);

/** Returns 1 (in rval) if there is a next page */
DLLEXPORT int tqsl_hasNextStationLocationCapture(tQSL_Location loc, int *rval);

/** Returns 1 (in rval) if there is a previous page */
DLLEXPORT int tqsl_hasPrevStationLocationCapture(tQSL_Location loc, int *rval);

/** Save the station location data. Note that the name must have been
  * set via tqsl_setStationLocationCaptureName if this is a new
  * station location. If the \c overwrite parameter is zero and a
  * station location of that name is already in existance, an error
  * occurs with tQSL_Error set to TQSL_NAME_EXISTS.
  */
DLLEXPORT int tqsl_saveStationLocationCapture(tQSL_Location loc, int overwrite);

/** Get the name of the station location */
DLLEXPORT int tqsl_getStationLocationCaptureName(tQSL_Location loc, char *namebuf, int bufsiz);

/** Set the name of the station location */
DLLEXPORT int tqsl_setStationLocationCaptureName(tQSL_Location loc, const char *name);

/** Get the number of saved station locations */
DLLEXPORT int tqsl_getNumStationLocations(tQSL_Location loc, int *nloc);

/** Get the name of the specified (by \c idx) saved station location */
DLLEXPORT int tqsl_getStationLocationName(tQSL_Location loc, int idx, char *buf, int bufsiz);

/** Get the call sign from the station location */
DLLEXPORT int tqsl_getStationLocationCallSign(tQSL_Location loc, int idx, char *buf, int bufsiz);

/** Retrieve a saved station location.
  * Once finished wih the station location, tqsl_endStationLocationCapture()
  * should be called to release resources.
  */
DLLEXPORT int tqsl_getStationLocation(tQSL_Location *loc, const char *name);

/** Remove the stored station location by name. */
DLLEXPORT int tqsl_deleteStationLocation(const char *name);

/** Get the number of fields on the current station location page */
DLLEXPORT int tqsl_getNumLocationField(tQSL_Location loc, int *numf);

/** Get the number of characters in the label for the specified field */
DLLEXPORT int tqsl_getLocationFieldDataLabelSize(tQSL_Location loc, int field_num, int *rval);

/** Get the label for the specified field */
DLLEXPORT int tqsl_getLocationFieldDataLabel(tQSL_Location loc, int field_num, char *buf, int bufsiz);

/** Get the size of the GABBI name of the specified field */
DLLEXPORT int tqsl_getLocationFieldDataGABBISize(tQSL_Location loc, int field_num, int *rval);

/** Get the GABBI name of the specified field */
DLLEXPORT int tqsl_getLocationFieldDataGABBI(tQSL_Location loc, int field_num, char *buf, int bufsiz);

/** Get the input type of the input field.
  *
  * \c type will be one of TQSL_LOCATION_FIELD_TEXT, TQSL_LOCATION_FIELD_DDLIST
  * or TQSL_LOCATION_FIELD_LIST
  */
DLLEXPORT int tqsl_getLocationFieldInputType(tQSL_Location loc, int field_num, int *type);

/** Get the data type of the input field.
  *
  * \c type will be either TQSL_LOCATION_FIELD_CHAR or TQSL_LOCATION_FIELD_INT
  */
DLLEXPORT int tqsl_getLocationFieldDataType(tQSL_Location loc, int field_num, int *type);

/** Get the flags for the input field.
  *
  * \c flags  will be either
  * TQSL_LOCATION_FIELD_UPPER		Field is to be uppercased on input
  * TQSL_LOCATION_FIELD_MUSTSEL		Value must be selected
  * TQSL_LOCATION_FIELD_SELNXT		Value must be selected to allow Next/Finish
  *
  */
DLLEXPORT int tqsl_getLocationFieldFlags(tQSL_Location loc, int field_num, int *flags);

/** Get the length of the input field data. */
DLLEXPORT int tqsl_getLocationFieldDataLength(tQSL_Location loc, int field_num, int *rval);

/** Get the character data from the specified field.
  *
  * If the field input type (see tqsl_getLocationFieldInputType()) is
  * TQSL_LOCATION_FIELD_DDLIST or TQSL_LOCATION_FIELD_LIST, this will
  * return the text of the selected item.
  */
DLLEXPORT int tqsl_getLocationFieldCharData(tQSL_Location loc, int field_num, char *buf, int bufsiz);

/** Get the integer data from the specified field.
  *
  * This is only meaningful if the field data type (see tqsl_getLocationFieldDataType())
  * is TQSL_LOCATION_FIELD_INT.
  */
DLLEXPORT int tqsl_getLocationFieldIntData(tQSL_Location loc, int field_num, int *dat);

/** If the field input type (see tqsl_getLocationFieldInputType()) is
  * TQSL_LOCATION_FIELD_DDLIST or TQSL_LOCATION_FIELD_LIST, gets the
  * index of the selected list item.
  */
DLLEXPORT int tqsl_getLocationFieldIndex(tQSL_Location loc, int field_num, int *dat);

/** Get the number of items in the specified field's pick list. */
DLLEXPORT int tqsl_getNumLocationFieldListItems(tQSL_Location loc, int field_num, int *rval);

/** Get the text of a specified item of a specified field */
DLLEXPORT int tqsl_getLocationFieldListItem(tQSL_Location loc, int field_num, int item_idx, char *buf, int bufsiz);

/** Set the text data of a specified field. */
DLLEXPORT int tqsl_setLocationFieldCharData(tQSL_Location loc, int field_num, const char *buf);

/** Set the integer data of a specified field.
  */
DLLEXPORT int tqsl_setLocationFieldIntData(tQSL_Location loc, int field_num, int dat);

/** If the field input type (see tqsl_getLocationFieldInputType()) is
  * TQSL_LOCATION_FIELD_DDLIST or TQSL_LOCATION_FIELD_LIST, sets the
  * index of the selected list item.
  */
DLLEXPORT int tqsl_setLocationFieldIndex(tQSL_Location loc, int field_num, int dat);

/** Get the \e changed status of a field. The changed flag is set to 1 if the
  * field's pick list was changed during the last call to tqsl_updateStationLocationCapture
  * or zero if the list was not changed.
  */
DLLEXPORT int tqsl_getLocationFieldChanged(tQSL_Location loc, int field_num, int *changed);

/** Get the call sign from the station location. */
DLLEXPORT int tqsl_getLocationCallSign(tQSL_Location loc, char *buf, int bufsiz);

/** Get the DXCC entity from the station location. */
DLLEXPORT int tqsl_getLocationDXCCEntity(tQSL_Location loc, int *dxcc);

/** Get the number of DXCC entities in the master DXCC list.
  */
DLLEXPORT int tqsl_getNumDXCCEntity(int *number);

/** Get a DXCC entity from the list of DXCC entities by its index.
  */
DLLEXPORT int tqsl_getDXCCEntity(int index, int *number, const char **name);

/** Get the name of a DXCC Entity by its DXCC number.
  */
DLLEXPORT int tqsl_getDXCCEntityName(int number, const char **name);

/** Get the zonemap  of a DXCC Entity by its DXCC number.
  */
DLLEXPORT int tqsl_getDXCCZoneMap(int number, const char **zonemap);

/** Get the number of Band entries in the Band list */
DLLEXPORT int tqsl_getNumBand(int *number);

/** Get a band by its index.
  *
  * \c name - The GAABI name of the band.
  * \c spectrum - HF | VHF | UHF
  * \c low - The low end of the band in kHz (HF) or MHz (VHF/UHF)
  * \c high - The low high of the band in kHz (HF) or MHz (VHF/UHF)
  *
  * Note: \c spectrum, \c low and/or \c high may be NULL.
  */
DLLEXPORT int tqsl_getBand(int index, const char **name, const char **spectrum, int *low, int *high);

/** Get the number of Mode entries in the Mode list */
DLLEXPORT int tqsl_getNumMode(int *number);

/** Get a mode by its index.
  *
  * \c mode - The GAABI mode name
  * \c group - CW | PHONE | IMAGE | DATA
  *
  * Note: \c group may be NULL.
  */
DLLEXPORT int tqsl_getMode(int index, const char **mode, const char **group);

/** Get the number of Propagation Mode entries in the Propagation Mode list */
DLLEXPORT int tqsl_getNumPropagationMode(int *number);

/** Get a propagation mode by its index.
  *
  * \c name - The GAABI propagation mode name
  * \c descrip - Text description of the propagation mode
  *
  * Note: \c descrip may be NULL.
  */
DLLEXPORT int tqsl_getPropagationMode(int index, const char **name, const char **descrip);

/** Get the number of Satellite entries in the Satellite list */
DLLEXPORT int tqsl_getNumSatellite(int *number);

/** Get a satellite by its index.
  *
  * \c name - The GAABI satellite name
  * \c descrip - Text description of the satellite
  * \c start - The date the satellite entered service
  * \c end - The last date the satellite was in service
  *
  * Note: \c descrip, start and/or end may be NULL.
  */
DLLEXPORT int tqsl_getSatellite(int index, const char **name, const char **descrip,
	tQSL_Date *start, tQSL_Date *end);

/** Clear the map of Cabrillo contests.
  */
DLLEXPORT int tqsl_clearCabrilloMap();

/** Set the mapping of a Cabrillo contest name (as found in the
  * CONTEST line of a Cabrillo file) to the QSO line call-worked field number
  * and the contest type.
  *
  * \c field can have a value of TQSL_MIN_CABRILLO_MAP_FIELD (cabrillo.h)
  * or greater. Field number starts at 1.
  *
  * \c contest_type must be TQSL_CABRILLO_HF or TQSL_CABRILLO_VHF,
  * defined in cabrillo.h
  */
DLLEXPORT int tqsl_setCabrilloMapEntry(const char *contest, int field, int contest_type);

/** Get the mapping of a Cabrillo contest name (as found in the
  * CONTEST line of a Cabrillo file) to a call-worked field number
  * and the contest type.
  *
  * \c fieldnum will be set to 0 if the contest name isn't in the Cabrillo
  * map. Otherwise it is set to the QSO line field number of the call-worked
  * field, with field counting starting at 1.
  *
  * \c contest_type may be NULL. If not, it is set to the Cabrillo contest
  * type (TQSL_CABRILLO_HF or TQSL_CABRILLO_VHF), defined in cabrillo.h.
  */
DLLEXPORT int tqsl_getCabrilloMapEntry(const char *contest, int *fieldnum, int *contest_type);

/** Clear the map of ADIF modes
  */
DLLEXPORT int tqsl_clearADIFModes();

/** Set the mapping of an ADIF mode to a TQSL mode.
*/
DLLEXPORT int tqsl_setADIFMode(const char *adif_item, const char *mode);

/** Map an ADIF mode to its TQSL equivalent.
  */
DLLEXPORT int tqsl_getADIFMode(const char *adif_item, char *mode, int nmode);

/** Get a GABBI record that contains the certificate.
  *
  * \c uid is the value for the CERT_UID field
  *
  * Returns the NULL pointer on error.
  *
  * N.B. On systems that distinguish text-mode files from binary-mode files,
  * notably Windows, the GABBI records should be written in binary mode.
  */
DLLEXPORT const char *tqsl_getGABBItCERT(tQSL_Cert cert, int uid);

/** Get a GABBI record that contains the Staion Location data.
  *
  * \li \c uid is the value for the STATION_UID field.
  * \li \c certuid is the value of the asociated CERT_UID field.
  *
  * Returns the NULL pointer on error.
  *
  * N.B. On systems that distinguish text-mode files from binary-mode files,
  * notably Windows, the GABBI records should be written in binary mode.
  */
DLLEXPORT const char *tqsl_getGABBItSTATION(tQSL_Location loc, int uid, int certuid);

/** Get a GABBI record that contains the QSO data.
  *
  * \li \c uid is the value of the associated STATION_UID field.
  *
  * N.B.: If \c cert is not initialized for signing (see tqsl_beginSigning())
  * the function will return with a TQSL_SIGNINIT_ERROR error.
  *
  * Returns the NULL pointer on error.
  *
  * N.B. On systems that distinguish text-mode files from binary-mode files,
  * notably Windows, the GABBI records should be written in binary mode.
  */
DLLEXPORT const char *tqsl_getGABBItCONTACT(tQSL_Cert cert, tQSL_Location loc, TQSL_QSO_RECORD *qso,
	int stationuid);

/** @} */

#ifdef __cplusplus
}
#endif

/* Useful defines */
#define TQSL_MAX_PW_LENGTH         32     /* Password buffer length */

#endif /* TQSLLIB_H */
