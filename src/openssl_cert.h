/***************************************************************************
                          openssl_cert.h  -  description
                             -------------------
    begin                : Tue May 14 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef OPENSSL_CERT_H
#define OPENSSL_CERT_H

/** \file
  * OpenSSL X509 certificate interface functions.
  */

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include <openssl/x509.h>
#include <openssl/e_os.h>

#undef CLIENT_STATIC
#ifndef LOTW_SERVER
#define CLIENT_STATIC static
#else
#define CLIENT_STATIC
#endif

typedef STACK_OF(X509) TQSL_X509_STACK;

typedef struct {
	char *name_buf;
	int name_buf_size;
	char *value_buf;
	int value_buf_size;
} TQSL_X509_NAME_ITEM;

typedef struct {
	int year;
	int month;
	int day;
} TQSL_DATE;

#if defined(LOTW_SERVER) || defined(OPENSSL_CERT_SOURCE)

#ifdef __cplusplus
extern "C" {
#endif

/// Loads a stack of certificates from the caller-supplied BIO
/** See the OpenSSL documentation for background on BIO operations.
  *
  * Returns a pointer to an OpenSSL X509 stack, as used by
  * tqsl_ssl_verify_cert()
  */
CLIENT_STATIC TQSL_X509_STACK *tqsl_ssl_load_certs_from_BIO(BIO *in);
/// Loads a stack of certificates from a file
/** See tqsl_ssl_load_certs_from_BIO()
  */
CLIENT_STATIC TQSL_X509_STACK *tqsl_ssl_load_certs_from_file(const char *filename);

/// Verifies a certificate using stacks of certificates
/** The user supplies the X509 certificate to verify (the test certificate)
  * along with two stacks of certificates. The \c cacerts stack is a list
  * of certificates, one of which was used to sign the test certificate.
  * The \c rootcerts are considered "trusted." One of them must have been used
  * to sign either the test certificate itself or the CA cert that signed
  * the test certificate.
  *
  * Returns NULL if the test certificate is valid, othewise returns an error message.
  */
CLIENT_STATIC const char *tqsl_ssl_verify_cert(X509 *cert, TQSL_X509_STACK *cacerts, TQSL_X509_STACK *rootcerts, int purpose,
	int (*cb)(int ok, X509_STORE_CTX *ctx));

/// Get the number of name entries in an X509 name object
CLIENT_STATIC int tqsl_get_name_count(X509_NAME *name);

/// Retrieve a name entry from an X509 name object by index
CLIENT_STATIC int tqsl_get_name_index(X509_NAME *name, int index, TQSL_X509_NAME_ITEM *name_item);

/// Retrieve a name entry from an X509 name object by name
CLIENT_STATIC int tqsl_get_name_entry(X509_NAME *name, const char *obj_name, TQSL_X509_NAME_ITEM *name_item);

/// Get the number of name entries in an X509 cert's subject name
CLIENT_STATIC int tqsl_cert_get_subject_name_count(X509 *cert);

/// Retrieve a name entry from an X509 cert's subject name by index
CLIENT_STATIC int tqsl_cert_get_subject_name_index(X509 *cert, int index, TQSL_X509_NAME_ITEM *name_item);

/// Retrieve a name entry from an X509 cert's subject name by name
CLIENT_STATIC int tqsl_cert_get_subject_name_entry(X509 *cert, const char *obj_name, TQSL_X509_NAME_ITEM *name_item);

/// Retrieve a name entry date from an X509 cert's subject name by name
CLIENT_STATIC int tqsl_cert_get_subject_date(X509 *cert, const char *obj_name, TQSL_DATE *date);

/// Filter a list (stack) of certs based on (optional) call sign, qso date and issuer criteria
/** Returns a (possibly empty) stack of certificates that match the criteria. Returns NULL
  * on error.
  *
  * The returned stack contains \em copies of the certs from the input stack. The input
  * stack is not altered.
  */
CLIENT_STATIC TQSL_X509_STACK *tqsl_filter_cert_list(TQSL_X509_STACK *sk, const char *callsign,
	const TQSL_DATE *date, const char *issuer, int isvalid);

/// Initialize a TQSL_DATE object from a date string
/** The date string must be YYYY-MM-DD or YYYYMMDD format. */
CLIENT_STATIC int tqsl_date_init(TQSL_DATE *date, const char *str);

/// Compare two TQSL_DATE objects
/** Returns:
  * - -1 if \c a < \c b
  *
  * - 0 if \c a == \c b
  *
  * - 1 if \c a > \c b
  */
CLIENT_STATIC int tqsl_date_compare(const TQSL_DATE *a, const TQSL_DATE *b);

/// Converts a TQSL_DATE object to a YYYY-MM-DD string
/** Returns a pointer to \c buf or NULL on error */
CLIENT_STATIC char *tqsl_date_to_text(TQSL_DATE *date, char *buf, int bufsiz);

CLIENT_STATIC EVP_PKEY *tqsl_new_rsa_key(int nbits);

CLIENT_STATIC int tqsl_store_cert(const char *pem, X509 *cert, const char *certfile,
	int type, int (*cb)(int, const char *));

CLIENT_STATIC int tqsl_write_adif_field(FILE *fp, const char *fieldname, char type, const unsigned char *value, int len);

#ifdef __cplusplus
}
#endif

#endif /* defined(LOTW_SERVER) || defined(OPENSSL_CERT_SOURCE) */

#define TQSL_OBJ_TO_API(x) ((void *)(x))
#define TQSL_API_TO_OBJ(x,type) ((type)(x))
#define TQSL_API_TO_CERT(x) TQSL_API_TO_OBJ((x), tQSL_Cert)

#endif /* OPENSSL_CERT_H */
