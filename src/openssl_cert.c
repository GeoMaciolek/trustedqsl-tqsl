/***************************************************************************
                          openssl_cert.c  -  description
                             -------------------
    begin                : Tue May 14 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

/* Routines to massage X.509 certs for tQSL. See openssl_cert.h for overview */

/* Portions liberally copied from OpenSSL's apps source code */

/* ====================================================================
 * Copyright (c) 1998-2001 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.openssl.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    openssl-core@openssl.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.openssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#define OPENSSL_CERT_SOURCE

#include "tqsllib.h"
#include "tqslerrno.h"
#include "openssl_cert.h"
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#define tqsl_malloc malloc
#define tqsl_calloc calloc
#define tqsl_free free

#define TQSL_OBJ_TO_API(x) ((void *)(x))
#define TQSL_API_TO_OBJ(x,type) ((type)(x))
#define TQSL_API_TO_CERT(x) TQSL_API_TO_OBJ((x), tqsl_cert *)

#ifdef __WIN32__
#define MKDIR(x,y) mkdir(x)
#else
#define MKDIR(x,y) mkdir(x,y)
#endif

#define TQSL_OPEN_READ  "r"
#define TQSL_OPEN_WRITE  "w"
#define TQSL_OPEN_APPEND "a"

static char *tqsl_trim(char *buf);
static int tqsl_check_parm(const char *p, const char *parmName);
static TQSL_CERT_REQ *tqsl_copy_cert_req(TQSL_CERT_REQ *userreq);
static TQSL_CERT_REQ *tqsl_free_cert_req(TQSL_CERT_REQ *req, int seterr);
static char *tqsl_make_key_path(const char *callsign, char *path, int size);
static int tqsl_find_matching_key(X509 *cert, EVP_PKEY **keyp, TQSL_CERT_REQ **crq, const char *password, int (*cb)(char *,int));
static char *tqsl_make_cert_path(const char *filename, char *path, int size);
static int tqsl_get_cert_ext(X509 *cert, const char *ext, unsigned char *userbuf, int *buflen, int *crit);
static int tqsl_get_asn1_date(ASN1_TIME *tm, tQSL_Date *date);
static char *tqsl_sign_base64_data(tQSL_Cert cert, char *b64data);
static int fixed_password_callback(char *buf, int bufsiz, int verify, void *userdata);
static int tqsl_check_crq_field(tQSL_Cert cert, char *buf, int bufsiz);

/* Private data structures */

typedef struct {
	long id;
	X509 *cert;
	EVP_PKEY *key;
	TQSL_CERT_REQ *crq;
} tqsl_cert;

typedef struct {
	long id;
	X509 *cert;
} tqsl_crq;

static tqsl_cert * tqsl_cert_new();
static void tqsl_cert_free(tqsl_cert *p);
static int tqsl_cert_check(tqsl_cert *p);

struct tqsl_loadcert_handler {
	int type;
	int (*func)(const char *pem, X509 *x, int(*cb)(int type, const char *));
};

static int tqsl_handle_root_cert(const char *, X509 *, int (*cb)(int, const char *));
static int tqsl_handle_ca_cert(const char *, X509 *, int (*cb)(int, const char *));
static int tqsl_handle_user_cert(const char *, X509 *, int (*cb)(int, const char *));

static struct tqsl_loadcert_handler tqsl_load_root_cert = { TQSL_CERT_CB_ROOT, &tqsl_handle_root_cert };
static struct tqsl_loadcert_handler tqsl_load_ca_cert = { TQSL_CERT_CB_CA, &tqsl_handle_ca_cert };
static struct tqsl_loadcert_handler tqsl_load_user_cert = { TQSL_CERT_CB_USER, &tqsl_handle_user_cert };

static const char *notypes[] = { "" };
static adifFieldDefinitions tqsl_cert_file_fields[] = {
	{ "TQSL_CERT", "", ADIF_RANGE_TYPE_NONE, 0, 0, 0, NULL, NULL },
	{ "TQSL_CERT_USER", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, &tqsl_load_user_cert },
	{ "TQSL_CERT_CA", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, &tqsl_load_ca_cert },
	{ "TQSL_CERT_ROOT", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, &tqsl_load_root_cert },
};

static unsigned char tqsl_static_buf[2001];

static unsigned char *
tqsl_static_alloc(size_t size) {
	if (size > sizeof tqsl_static_buf)
		return NULL;
	strcpy((char *)tqsl_static_buf, "<EMPTY>");
	return tqsl_static_buf;
}

/********** PUBLIC API FUNCTIONS ***********/

int
tqsl_importCertFile(const char *file, int(*cb)(int, const char *)) {
	FILE *in;
	adifFieldResults field;
	ADIF_GET_FIELD_ERROR status;

	memset(&field, 0, sizeof field);
	if (tqsl_init())
		return 1;
	if ((in = fopen(file, TQSL_OPEN_READ)) == NULL) {
		strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		return 1;
	}
	do {
		status = adifGetField(&field, in, tqsl_cert_file_fields, notypes, &tqsl_static_alloc);
		if (status == ADIF_GET_FIELD_NO_NAME_MATCH)
			continue;
		if (status != ADIF_GET_FIELD_SUCCESS && status != ADIF_GET_FIELD_EOF) {
			strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
			tQSL_Error = TQSL_ADIF_ERROR;
			tQSL_ADIF_Error = status;
			fclose(in);
			return 1;
		}
		if (field.userPointer != NULL) {
			BIO *bio;
			X509 *cert;
			int stat;
			struct tqsl_loadcert_handler *handler;

			/* This is a certificate, supposedly. Let's make sure */

			bio = BIO_new_mem_buf((void *)field.data, atoi(field.size));
			if (in == NULL) {
				fclose(in);
				strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
				tQSL_Error = TQSL_OPENSSL_ERROR;
				return 1;
			}
			cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);
			BIO_free(bio);	
			if (cert == NULL) {
				fclose(in);
				strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
				tQSL_Error = TQSL_OPENSSL_ERROR;
				return 1;
			}
			/* It's a certificate. Let's try to add it. Any errors will be
             * reported via the callback (if any) but will not be fatal unless
             * the callback says so.
			 */
			handler = (struct tqsl_loadcert_handler *)(field.userPointer);
			stat = (*(handler->func))((const char *)(field.data), cert, cb);
			X509_free(cert);
			if (stat) {
				if (cb != NULL) {
					stat = (*cb)(handler->type | TQSL_CERT_CB_RESULT | TQSL_CERT_CB_ERROR, tqsl_getErrorString_v(tQSL_Error));
					if (stat)
						return 1;
				} else {
					/* No callback -- any errors are fatal */
					fclose(in);
					return 1;
				}
			}
		}
	} while (status != ADIF_GET_FIELD_EOF);
	fclose(in);
	return 0;
}

int
tqsl_createCertRequest(const char *filename, TQSL_CERT_REQ *userreq,
	int (*pwcb)(char *pwbuf, int pwsize)) {
	TQSL_CERT_REQ *req = NULL;
	EVP_PKEY *key = NULL;
	X509_REQ *xr = NULL;
	X509_NAME *subj = NULL;
	int nid, len;
	int rval = 1;
	FILE *out = NULL;
	BIO *bio = NULL;
	EVP_MD *digest = NULL;
	char buf[200];
	char path[256];
	char *cp;
	EVP_CIPHER *cipher = NULL;
	RSA *rsa;
	char *password;
	const char *type;

	if (tqsl_init())
		return 1;
	if (filename == NULL || userreq == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (userreq->signer != NULL && (!tqsl_cert_check(TQSL_API_TO_CERT(userreq->signer))
		|| TQSL_API_TO_CERT(userreq->signer)->key == NULL)) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if ((req = tqsl_copy_cert_req(userreq)) == NULL)
		goto end;

	/* Check parameters for validity */

	tqsl_trim((char *)req->name);
	if (tqsl_check_parm(req->name, "Name")) goto end;
	tqsl_trim((char *)req->callSign);
	if (tqsl_check_parm(req->callSign, "Call Sign")) goto end;
	tqsl_trim((char *)req->address1);
	if (tqsl_check_parm(req->address1, "Address")) goto end;
	tqsl_trim((char *)req->address2);
	tqsl_trim((char *)req->city);
	if (tqsl_check_parm(req->city, "City")) goto end;
	tqsl_trim((char *)req->state);
	tqsl_trim((char *)req->country);
	if (tqsl_check_parm(req->country, "Country")) goto end;
	tqsl_trim((char *)req->postalCode);
	tqsl_trim((char *)req->emailAddress);
	if (tqsl_check_parm(req->emailAddress, "Email address")) goto end;
	if ((cp = strchr(req->emailAddress, '@')) == NULL || strchr(cp, '.') == NULL) {
		strncpy(tQSL_CustomError, "Invalid email address", sizeof tQSL_CustomError);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		goto end;
	}
	if (!tqsl_isDateValid(&(req->qsoNotBefore))) {
		strncpy(tQSL_CustomError, "Invalid date (qsoNotBefore)", sizeof tQSL_CustomError);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		goto end;
	}
	if (!tqsl_isDateNull(&(req->qsoNotAfter))) {
		if (!tqsl_isDateValid(&(req->qsoNotAfter))) {
			strncpy(tQSL_CustomError, "Invalid date (qsoNotAfter)", sizeof tQSL_CustomError);
			tQSL_Error = TQSL_CUSTOM_ERROR;
			goto end;
		}
		if (tqsl_compareDates(&(req->qsoNotAfter), &(req->qsoNotBefore)) < 0) {
			strncpy(tQSL_CustomError, "qsoNotAfter date is earlier than qsoNotBefore", sizeof tQSL_CustomError);
			tQSL_Error = TQSL_CUSTOM_ERROR;
			goto end;
		}
	}

	/* Try opening the output stream */

	if ((out = fopen(filename, TQSL_OPEN_WRITE)) == NULL) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		goto end;
	}
	fputs("\ntQSL certificate request\n\n", out);
	tqsl_write_adif_field(out, "eoh", 0, NULL, 0);
	type = (req->signer != NULL) ? (req->renew ? "TQSL_CRQ_RENEWAL" : "TQSL_CRQ_ADDITIONAL") : "TQSL_CRQ_NEW";
	tqsl_write_adif_field(out, type, 0, NULL, 0);
	tqsl_write_adif_field(out, "TQSL_CRQ_ADDRESS1", 0, (unsigned char *)req->address1, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_ADDRESS2", 0, (unsigned char *)req->address2, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_CITY", 0, (unsigned char *)req->city, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_STATE", 0, (unsigned char *)req->state, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_POSTAL", 0, (unsigned char *)req->postalCode, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_COUNTRY", 0, (unsigned char *)req->country, -1);
	sprintf(buf, "%d", req->dxccEntity);
	tqsl_write_adif_field(out, "TQSL_CRQ_DXCC_ENTITY", 0, (unsigned char *)buf, -1);
	tqsl_convertDateToText(&(req->qsoNotBefore), buf, sizeof buf);
	tqsl_write_adif_field(out, "TQSL_CRQ_QSO_NOT_BEFORE", 0, (unsigned char *)buf, -1);
	if (!tqsl_isDateNull(&(req->qsoNotAfter))) {
		tqsl_convertDateToText(&(req->qsoNotAfter), buf, sizeof buf);
		tqsl_write_adif_field(out, "TQSL_CRQ_QSO_NOT_AFTER", 0, (unsigned char *)buf, -1);
	}

	/* Generate a new key pair */
	
	if ((key = tqsl_new_rsa_key(1024)) == NULL)
		goto end;

	/* Make the X.509 certificate request */

	if ((xr = X509_REQ_new()) == NULL)
		goto err;
	if (!X509_REQ_set_version(xr, 0L))
		goto err;
	subj = X509_REQ_get_subject_name(xr);
	nid = OBJ_txt2nid("AROcallsign");
	if (nid != NID_undef)
		X509_NAME_add_entry_by_NID(subj, nid, MBSTRING_ASC, (unsigned char *)req->callSign, -1, -1, 0);
	nid = OBJ_txt2nid("commonName");
	if (nid != NID_undef)
		X509_NAME_add_entry_by_NID(subj, nid, MBSTRING_ASC, (unsigned char *)req->name, -1, -1, 0);
	nid = OBJ_txt2nid("emailAddress");
	if (nid != NID_undef)
		X509_NAME_add_entry_by_NID(subj, nid, MBSTRING_ASC, (unsigned char *)req->emailAddress, -1, -1, 0);
	X509_REQ_set_pubkey(xr, key);
	if ((digest = EVP_md5()) == NULL)
		goto err;
	if (!X509_REQ_sign(xr, key, digest))
		goto err;
	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		goto err;
	if (!PEM_write_bio_X509_REQ(bio, xr))
		goto err;
	len = (int)BIO_get_mem_data(bio, &cp);
	tqsl_write_adif_field(out, "TQSL_CRQ_REQUEST", 0, (unsigned char *)cp, len);

	if (req->signer != NULL) {
		char *b64;
		char ibuf[256];

		if ((b64 = tqsl_sign_base64_data(req->signer, cp)) == NULL) {
			fclose(out);
			goto end;
		}
		tqsl_write_adif_field(out, "TQSL_CRQ_SIGNATURE", 0, (unsigned char *)b64, -1);
		tqsl_getCertificateIssuer(req->signer, ibuf, sizeof ibuf);
		tqsl_write_adif_field(out, "TQSL_CRQ_SIGNATURE_CERT_ISSUER", 0, (unsigned char *)ibuf, -1);
		sprintf(ibuf, "%ld", ASN1_INTEGER_get(X509_get_serialNumber(TQSL_API_TO_CERT(req->signer)->cert)));
		tqsl_write_adif_field(out, "TQSL_CRQ_SIGNATURE_CERT_SERIAL", 0, (unsigned char *)ibuf, -1);
	}

	BIO_free(bio);
	bio = NULL;
	tqsl_write_adif_field(out, "eor", 0, NULL, 0);
	if (fclose(out) == EOF) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		goto end;
	}
	out = NULL;

	/* Write the key to the key store */

	tqsl_make_key_path(req->callSign, path, sizeof path);
	if ((out = fopen(path, TQSL_OPEN_APPEND)) == NULL) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		goto end;
	}
	tqsl_write_adif_field(out, "TQSL_CRQ_ADDRESS1", 0, (unsigned char *)req->address1, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_ADDRESS2", 0, (unsigned char *)req->address2, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_CITY", 0, (unsigned char *)req->city, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_STATE", 0, (unsigned char *)req->state, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_POSTAL", 0, (unsigned char *)req->postalCode, -1);
	tqsl_write_adif_field(out, "TQSL_CRQ_COUNTRY", 0, (unsigned char *)req->country, -1);
	tqsl_write_adif_field(out, "CALLSIGN", 0, (unsigned char *)req->callSign, -1);
	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		goto err;
	password = (char *)req->password;
	if (password == NULL && pwcb != NULL) {
		if ((*pwcb)(buf, TQSL_MAX_PW_LENGTH)) {
			tQSL_Error = TQSL_OPERATOR_ABORT;
			goto end;
		}
		password = buf;
	}
	if (password != NULL && *password != '\0') {
		if ((cipher = EVP_des_ede3_cbc()) == NULL)
			goto err;
		len = strlen(password);
	} else {
		password = NULL;
		len = 0;
	}
	if (!PEM_write_bio_PrivateKey(bio, key, cipher, (unsigned char *)password, len, NULL, NULL))
		goto err;
	len = (int)BIO_get_mem_data(bio, &cp);
	tqsl_write_adif_field(out, "PRIVATE_KEY", 0, (unsigned char *)cp, len);
	BIO_free(bio);
	if ((bio = BIO_new(BIO_s_mem())) == NULL)
		goto err;
	rsa = key->pkey.rsa;	/* Assume RSA */
	if (!PEM_write_bio_RSA_PUBKEY(bio, rsa))
		goto err;
	len = (int)BIO_get_mem_data(bio, &cp);
	tqsl_write_adif_field(out, "PUBLIC_KEY", 0, (unsigned char *)cp, len);
	BIO_free(bio);
	bio = NULL;
	tqsl_write_adif_field(out, "eor", 0, NULL, 0);
	if (fclose(out) == EOF) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		goto end;
	}
	out = NULL;
		
	rval = 0;
	goto end;
err:
	tQSL_Error = TQSL_OPENSSL_ERROR;
end:
	if (bio != NULL)
		BIO_free(bio);
	if (out != NULL)
		fclose(out);
	if (xr != NULL)
		X509_REQ_free(xr);
	if (key != NULL)
		EVP_PKEY_free(key);
	if (req != NULL)
		tqsl_free_cert_req(req, 0);
	return rval;
}

int
tqsl_selectCertificates(tQSL_Cert **certlist, int *ncerts,
	const char *callsign, const char *date, const char *issuer, int validonly) {
	TQSL_X509_STACK *xcerts = NULL, *selcerts = NULL;
	char path[256];
	int i;
	X509 *x;
	int rval = 1;
	tqsl_cert *cp;
	tQSL_Date qsodate;

	if (tqsl_init())
		return 1;
	if (certlist == NULL || ncerts == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	/* Convert the dates to tQSL_Date objects */
	tqsl_initDate(&qsodate, date);
	if (!tqsl_isDateNull(&qsodate) && !tqsl_isDateValid(&qsodate)) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}

	/* Get the certs from the cert store */
	tqsl_make_cert_path("user", path, sizeof path);
	xcerts = tqsl_ssl_load_certs_from_file(path);
	if (xcerts == NULL)
		return 1;

	selcerts = tqsl_filter_cert_list(xcerts, callsign, &qsodate, issuer, validonly);
	sk_X509_free(xcerts);
	xcerts = NULL;
	if (selcerts == NULL) {
		*certlist = NULL;
		*ncerts = 0;
		goto end;
	}

	*certlist = (tQSL_Cert *)tqsl_calloc(sk_X509_num(selcerts), sizeof (tQSL_Cert));
	for (i = 0; i < sk_X509_num(selcerts); i++) {
		x = sk_X509_value(selcerts, i);
		if ((cp = tqsl_cert_new()) == NULL)
			goto end;
		cp->cert = X509_dup(x);
		(*certlist)[i] = TQSL_OBJ_TO_API(cp);
	}
	*ncerts = i;
	rval = 0;
end:
	if (xcerts != NULL)
		sk_X509_free(xcerts);
	if (selcerts != NULL)
		sk_X509_free(selcerts);
	return rval;
}


int
tqsl_getCertificateCallSign(tQSL_Cert cert, char *buf, int bufsiz) {
	char nbuf[40];
	TQSL_X509_NAME_ITEM item;

	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	item.name_buf = nbuf;
	item.name_buf_size = sizeof nbuf;
	item.value_buf = buf;
	item.value_buf_size = bufsiz;
	return !tqsl_cert_get_subject_name_entry(TQSL_API_TO_CERT(cert)->cert, "AROcallsign", &item);
}


int
tqsl_getCertificateAROName(tQSL_Cert cert, char *buf, int bufsiz) {
	char nbuf[40];
	TQSL_X509_NAME_ITEM item;

	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	item.name_buf = nbuf;
	item.name_buf_size = sizeof nbuf;
	item.value_buf = buf;
	item.value_buf_size = bufsiz;
	return !tqsl_cert_get_subject_name_entry(TQSL_API_TO_CERT(cert)->cert, "commonName", &item);
}


int
tqsl_getCertificateEmailAddress(tQSL_Cert cert, char *buf, int bufsiz) {
	char nbuf[40];
	TQSL_X509_NAME_ITEM item;

	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	item.name_buf = nbuf;
	item.name_buf_size = sizeof nbuf;
	item.value_buf = buf;
	item.value_buf_size = bufsiz;
	return !tqsl_cert_get_subject_name_entry(TQSL_API_TO_CERT(cert)->cert, "emailAddress", &item);
}


int
tqsl_getCertificateIssuer(tQSL_Cert cert, char *buf, int bufsiz) {
	char *cp;

	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	cp = X509_NAME_oneline(X509_get_issuer_name(TQSL_API_TO_CERT(cert)->cert), buf, bufsiz);
	if (cp == NULL)
		tQSL_Error = TQSL_OPENSSL_ERROR;
	return (cp == NULL);
}


int
tqsl_getCertificateIssuerOrganization(tQSL_Cert cert, char *buf, int bufsiz) {
	char nbuf[40];
	TQSL_X509_NAME_ITEM item;
	X509_NAME *iss;

	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	item.name_buf = nbuf;
	item.name_buf_size = sizeof nbuf;
	item.value_buf = buf;
	item.value_buf_size = bufsiz;
	if ((iss = X509_get_issuer_name(TQSL_API_TO_CERT(cert)->cert)) == NULL) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	return !tqsl_get_name_entry(iss, "organizationName", &item);
}

int
tqsl_getCertificateIssuerOrganizationalUnit(tQSL_Cert cert, char *buf, int bufsiz) {
	char nbuf[40];
	TQSL_X509_NAME_ITEM item;
	X509_NAME *iss;

	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	item.name_buf = nbuf;
	item.name_buf_size = sizeof nbuf;
	item.value_buf = buf;
	item.value_buf_size = bufsiz;
	if ((iss = X509_get_issuer_name(TQSL_API_TO_CERT(cert)->cert)) == NULL) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	return !tqsl_get_name_entry(iss, "organizationalUnitName", &item);
}

int
tqsl_getCertificateQSONotBeforeDate(tQSL_Cert cert, tQSL_Date *date) {
	char datebuf[40];
	int len = (sizeof datebuf) -1;

	if (tqsl_init())
		return 1;
	if (cert == NULL || date == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (tqsl_get_cert_ext(TQSL_API_TO_CERT(cert)->cert, "QSONotBeforeDate",
		(unsigned char *)datebuf, &len, NULL))
		return 1;
	datebuf[len] = 0;
	return tqsl_initDate(date, (char *)datebuf);
}

int
tqsl_getCertificateQSONotAfterDate(tQSL_Cert cert, tQSL_Date *date) {
	char datebuf[40];
	int len = (sizeof datebuf) -1;

	if (tqsl_init())
		return 1;
	if (cert == NULL || date == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (tqsl_get_cert_ext(TQSL_API_TO_CERT(cert)->cert, "QSONotAfterDate",
		(unsigned char *)datebuf, &len, NULL))
		return 1;
	datebuf[len] = 0;
	return tqsl_initDate(date, (char *)datebuf);
}

int
tqsl_getCertificateNotBeforeDate(tQSL_Cert cert, tQSL_Date *date) {
	ASN1_TIME *tm;

	if (tqsl_init())
		return 1;
	if (cert == NULL || date == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if ((tm = X509_get_notBefore(TQSL_API_TO_CERT(cert)->cert)) == NULL) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	return tqsl_get_asn1_date(tm, date);
}

int
tqsl_getCertificateNotAfterDate(tQSL_Cert cert, tQSL_Date *date) {
	ASN1_TIME *tm;

	if (tqsl_init())
		return 1;
	if (cert == NULL || date == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if ((tm = X509_get_notAfter(TQSL_API_TO_CERT(cert)->cert)) == NULL) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	return tqsl_get_asn1_date(tm, date);
}

int
tqsl_getCertificateDXCCEntity(tQSL_Cert cert, int *dxcc) {
	char buf[40];
	int len = sizeof buf;
	if (tqsl_init())
		return 1;
	if (cert == NULL || dxcc == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (tqsl_get_cert_ext(TQSL_API_TO_CERT(cert)->cert, "dxccEntity",
		(unsigned char *)buf, &len, NULL))
		return 1;
	*dxcc = atoi(buf);
	return 0;
}

int
tqsl_getCertificatePrivateKeyType(tQSL_Cert cert) {
	EVP_PKEY *key = NULL;
	if (tqsl_init())
		return 1;
	if (!tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (!tqsl_find_matching_key(TQSL_API_TO_CERT(cert)->cert, &key, NULL, "", NULL)) {
		if (tQSL_Error == TQSL_PASSWORD_ERROR) {
			tqsl_getErrorString();	/* Clear the error */
			return TQSL_PK_TYPE_ENC;
		}
		return TQSL_PK_TYPE_ERR;
	}
	return TQSL_PK_TYPE_UNENC;
}

void
tqsl_freeCertificate(tQSL_Cert cert) {
	tqsl_cert_free(TQSL_API_TO_CERT(cert));
}

int
tqsl_beginSigning(tQSL_Cert cert, char *password,  int(*pwcb)(char *, int)) {
	if (tqsl_init())
		return 1;
	if (cert == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->key != NULL)
		return 0;
	return !tqsl_find_matching_key(TQSL_API_TO_CERT(cert)->cert, &(TQSL_API_TO_CERT(cert)->key),
		&(TQSL_API_TO_CERT(cert)->crq), password, pwcb);
}

int
tqsl_getMaxSignatureSize(tQSL_Cert cert, int *sigsize) {
	if (tqsl_init())
		return 1;
	if (cert == NULL || sigsize == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->key == NULL) {
		tQSL_Error = TQSL_SIGNINIT_ERROR;
		return 1;
	}
	*sigsize = EVP_PKEY_size(TQSL_API_TO_CERT(cert)->key);
	return 0;
}

int
tqsl_signDataBlock(tQSL_Cert cert, unsigned char *data, int datalen, unsigned char *sig, int *siglen) {
	EVP_MD_CTX ctx;
	unsigned int slen = *siglen;

	if (tqsl_init())
		return 1;
	if (cert == NULL || data == NULL || sig == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->key == NULL) {
		tQSL_Error = TQSL_SIGNINIT_ERROR;
		return 1;
	}
	EVP_SignInit(&ctx, EVP_sha1());
	EVP_SignUpdate(&ctx, data, datalen);
	if (!EVP_SignFinal(&ctx, sig, &slen, TQSL_API_TO_CERT(cert)->key)) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 1;
	}
	*siglen = slen;
	return 0;
}

int
tqsl_endSigning(tQSL_Cert cert) {
	if (tqsl_init())
		return 1;
	if (cert == NULL || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->key != NULL) {
		EVP_PKEY_free(TQSL_API_TO_CERT(cert)->key);
		TQSL_API_TO_CERT(cert)->key = NULL;
	}
	return 0;
}

int
tqsl_getCertificateRequestAddress1(tQSL_Cert cert, char *buf, int bufsiz) {
	if (tqsl_check_crq_field(cert, buf, bufsiz))
		return 1;
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->address1) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->address1),
		bufsiz);
	return 0;
}
	
int
tqsl_getCertificateRequestAddress2(tQSL_Cert cert, char *buf, int bufsiz) {
	if (tqsl_check_crq_field(cert, buf, bufsiz))
		return 1;
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->address2) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->address2),
		bufsiz);
	return 0;
}
	
int
tqsl_getCertificateRequestCity(tQSL_Cert cert, char *buf, int bufsiz) {
	if (tqsl_check_crq_field(cert, buf, bufsiz))
		return 1;
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->city) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->city),
		bufsiz);
	return 0;
}
	
int
tqsl_getCertificateRequestState(tQSL_Cert cert, char *buf, int bufsiz) {
	if (tqsl_check_crq_field(cert, buf, bufsiz))
		return 1;
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->state) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->state),
		bufsiz);
	return 0;
}
	
int
tqsl_getCertificateRequestPostalCode(tQSL_Cert cert, char *buf, int bufsiz) {
	if (tqsl_check_crq_field(cert, buf, bufsiz))
		return 1;
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->postalCode) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->postalCode),
		bufsiz);
	return 0;
}
	
int
tqsl_getCertificateRequestCountry(tQSL_Cert cert, char *buf, int bufsiz) {
	if (tqsl_check_crq_field(cert, buf, bufsiz))
		return 1;
	strncpy(buf,
		(((TQSL_API_TO_CERT(cert)->crq)->country) == NULL ? "" : (TQSL_API_TO_CERT(cert)->crq)->country),
		bufsiz);
	return 0;
}
	
/********** END OF PUBLIC API FUNCTIONS **********/

/* Utility functions to manage private data structures */

static int
tqsl_check_crq_field(tQSL_Cert cert, char *buf, int bufsiz) {
	if (tqsl_init())
		return 1;
	if (cert == NULL || buf == NULL || bufsiz < 0 || !tqsl_cert_check(TQSL_API_TO_CERT(cert))) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (TQSL_API_TO_CERT(cert)->crq == NULL) {
		if (!tqsl_find_matching_key(TQSL_API_TO_CERT(cert)->cert, NULL,
			&(TQSL_API_TO_CERT(cert)->crq), "", NULL) && tQSL_Error != TQSL_PASSWORD_ERROR)
			return 1;
	}
	return 0;
}

static int
tqsl_cert_check(tqsl_cert *p) {
	return (p != NULL && p->id == 0xCE && p->cert != NULL);
}

static tqsl_cert *
tqsl_cert_new() {
	tqsl_cert *p;

	p = (tqsl_cert *)tqsl_calloc(1, sizeof(tqsl_cert));
	if (p != NULL)
		p->id = 0xCE;
	return p;
}

static void
tqsl_cert_free(tqsl_cert *p) {
	if (p == NULL || p->id != 0xCE)
		return;
	p->id = 0;
	if (p->cert != NULL)
		X509_free(p->cert);
	if (p->key != NULL)
		EVP_PKEY_free(p->key);
	if (p->crq != NULL)
		tqsl_free_cert_req(p->crq, 0);
	tqsl_free(p);
}

static TQSL_CERT_REQ *
tqsl_free_cert_req(TQSL_CERT_REQ *req, int seterr) {
	if (req == NULL)
		return NULL;
	if (req->callSign != NULL) tqsl_free((char *)req->callSign);
	if (req->name != NULL) tqsl_free((char *)req->name);
	if (req->address1 != NULL) tqsl_free((char *)req->address1);
	if (req->address2 != NULL) tqsl_free((char *)req->address2);
	if (req->city != NULL) tqsl_free((char *)req->city);
	if (req->state != NULL) tqsl_free((char *)req->state);
	if (req->postalCode != NULL) tqsl_free((char *)req->postalCode);
	if (req->country != NULL) tqsl_free((char *)req->country);
	if (req->emailAddress != NULL) tqsl_free((char *)req->emailAddress);
	tqsl_free(req);
	if (seterr) {
		errno = seterr;
		tQSL_Error = TQSL_SYSTEM_ERROR;
	}
	return NULL;
}

static TQSL_CERT_REQ *
tqsl_copy_cert_req(TQSL_CERT_REQ *userreq) {
	TQSL_CERT_REQ *req;

	if ((req = (TQSL_CERT_REQ *)tqsl_calloc(1, sizeof (TQSL_CERT_REQ))) == NULL) {
		errno = ENOMEM;
		return NULL;
	}
	req->callSign = strdup((userreq->callSign == NULL) ? "" : userreq->callSign);
	if (req->callSign == NULL) return tqsl_free_cert_req(req, ENOMEM);
	req->name = strdup((userreq->name == NULL) ? "" : userreq->name);
	if (req->name == NULL) return tqsl_free_cert_req(req, ENOMEM);
	req->address1 = strdup((userreq->address1 == NULL) ? "" : userreq->address1);
	if (req->address1 == NULL) return tqsl_free_cert_req(req, ENOMEM);
	req->address2 = strdup((userreq->address2 == NULL) ? "" : userreq->address2);
	if (req->address2 == NULL) return tqsl_free_cert_req(req, ENOMEM);
	req->city = strdup((userreq->city == NULL) ? "" : userreq->city);
	if (req->city == NULL) return tqsl_free_cert_req(req, ENOMEM);
	req->state = strdup((userreq->state == NULL) ? "" : userreq->state);
	if (req->state == NULL) return tqsl_free_cert_req(req, ENOMEM);
	req->postalCode = strdup((userreq->postalCode == NULL) ? "" : userreq->postalCode);
	if (req->postalCode == NULL) return tqsl_free_cert_req(req, ENOMEM);
	req->country = strdup((userreq->country == NULL) ? "" : userreq->country);
	if (req->country == NULL) return tqsl_free_cert_req(req, ENOMEM);
	req->emailAddress = strdup((userreq->emailAddress == NULL) ? "" : userreq->emailAddress);
	if (req->emailAddress == NULL) return tqsl_free_cert_req(req, ENOMEM);
	req->dxccEntity = userreq->dxccEntity;
	req->qsoNotBefore = userreq->qsoNotBefore;
	req->qsoNotAfter = userreq->qsoNotAfter;
	req->password = userreq->password;	/* Don't need a local copy */
	req->signer = userreq->signer;	/* Don't need a local copy */
	req->renew = userreq->renew;
	return req;
}

static int
tqsl_check_parm(const char *p, const char *parmName) {
	if (strlen(p) == 0) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "Missing parameter: ", sizeof tQSL_CustomError);
		strncat(tQSL_CustomError, parmName, sizeof tQSL_CustomError - strlen(tQSL_CustomError));
		return 1;
	}
	return 0;
}

static char *
tqsl_trim(char *buf) {
	char lastc;
	char *cp, *op;

	/* Trim white space off end of string */
	cp = buf + strlen(buf);
	while (cp != buf) {
		cp--;
		if (!isspace(*cp))
			break;
		*cp = 0;
	}
	/* Skip past leading whitespace */
	for (cp = buf; isspace(*cp); cp++)
		;
	/* Fold runs of white space into single space */
	lastc = 0;
	op = buf;
	for (; *cp != '\0'; cp++) {
		if (isspace(*cp))
			*cp = ' ';
		if (*cp != ' ' || lastc != ' ')
			*op++ = *cp;
		lastc = *cp;
	}
	*op = '\0';
	return cp;
}
/* Filter a list (stack) of X509 certs based on call sign, QSO date and/or
 * issuer.
 *
 * Returns a new stack of matching certs without altering the original stack.
 *
 * Note that you don't have to supply any of the criteria. If you supply
 * none, you;ll just get back an exact copy of the stack.
 */
CLIENT_STATIC STACK_OF(X509) *
tqsl_filter_cert_list(STACK_OF(X509) *sk, const char *callsign,
	const tQSL_Date *date, const char *issuer, int isvalid) {

	const char *cp;
	char buf[256], name_buf[256];
	TQSL_X509_NAME_ITEM item;
	X509 *x;
	STACK_OF(X509) *newsk;
	int i, ok;
	tQSL_Date qso_date;

	if (tqsl_init())
		return NULL;
	if ((newsk = sk_X509_new_null()) == NULL)
		return NULL;
	/* Loop through the list of certs */
	for (i = 0; i < sk_X509_num(sk); i++) {
		ok = 1;	/* Certificate is selected unless some check says otherwise */
		x = sk_X509_value(sk, i);
		/* Compare issuer if asked to */
		if (issuer != NULL) {
			cp = X509_NAME_oneline(X509_get_issuer_name(x), buf, sizeof(buf));
			if (cp == NULL || strcmp(cp, issuer))
				ok = 0;
		}
		/* Check call sign if asked */
		if (ok && callsign != NULL) {
			item.name_buf = name_buf;
			item.name_buf_size = sizeof name_buf;
			item.value_buf = buf;
			item.value_buf_size = sizeof buf;
			if (!tqsl_cert_get_subject_name_entry(x, "AROcallsign", &item))
				ok = 0;
			else
				ok = !strcmp(callsign, item.value_buf);
		}
		/* Check QSO date if asked */
		if (ok && date != NULL && !tqsl_isDateNull(date)) {
			if (!tqsl_cert_get_subject_date(x, "QSONotBeforeDate", &qso_date))
				ok = 0;
			else if (tqsl_compareDates(date, &qso_date) < 0)
				ok = 0;
			else if (!tqsl_cert_get_subject_date(x, "QSONotAfterDate", &qso_date))
				ok = 0;
			else  if (tqsl_compareDates(date, &qso_date) > 0)
				ok = 0;
		}
		/* If no check failed, copy this cert onto the new stack */
		if (ok)
			sk_X509_push(newsk, X509_dup(x));
	}
	return newsk;
}


/* Set up a read-only BIO from the given file and pass to
 * tqsl_ssl_load_certs_from_BIO.
 */
CLIENT_STATIC STACK_OF(X509) *
tqsl_ssl_load_certs_from_file(const char *filename) {
	BIO *in;
	STACK_OF(X509) *sk;
	FILE *cfile;

	if ((cfile = fopen(filename, "r")) == NULL) {
		strncpy(tQSL_ErrorFile, filename, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		return NULL;
	}
	if ((in = BIO_new_fp(cfile, 0)) == NULL) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	sk = tqsl_ssl_load_certs_from_BIO(in);
	BIO_free(in);
	fclose(cfile);
	return sk;
}

/* Load a set of certs from a file into a stack. The file may contain
 * other X509 objects (e.g., CRLs), but we'll ignore those.
 *
 * Return NULL if there are no certs in the file or on error.
 */
CLIENT_STATIC STACK_OF(X509) *
tqsl_ssl_load_certs_from_BIO(BIO *in) {
	STACK_OF(X509_INFO) *sk = NULL;
	STACK_OF(X509) *stack = NULL;
	X509_INFO *xi;

	if (tqsl_init())
		return NULL;
	if (!(stack = sk_X509_new_null())) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	if (!(sk = PEM_X509_INFO_read_bio(in,NULL,NULL,NULL))) {
		sk_X509_free(stack);
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	/* Extract the certs from the X509_INFO objects and put them on a stack */
	while (sk_X509_INFO_num(sk)) {
		xi=sk_X509_INFO_shift(sk);
		if (xi->x509 != NULL) {
			sk_X509_push(stack,xi->x509);
			xi->x509 = NULL;
		}
		X509_INFO_free(xi);
	}
	if(!sk_X509_num(stack)) {
		sk_X509_free(stack);
		stack = NULL;
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strcpy(tQSL_CustomError, "No certificates found");
	}
	sk_X509_INFO_free(sk);
	return stack;
}

/* Chain-verify a cert against a set of CA and a set of trusted root certs.
 *
 * Returns NULL if cert verifies, an error message if it does not.
 */
CLIENT_STATIC const char *
tqsl_ssl_verify_cert(X509 *cert, STACK_OF(X509) *cacerts, STACK_OF(X509) *rootcerts, int purpose,
	int (*cb)(int ok, X509_STORE_CTX *ctx)) {
	X509_STORE *store;
	X509_STORE_CTX *ctx;
	int rval;
	const char *errm;

	if (cert == NULL)
		return "No certificate to verify";
	if (tqsl_init())
		return NULL;
	store = X509_STORE_new();
	if (store == NULL)
		return "Out of memory";
	if (cb != NULL)
		X509_STORE_set_verify_cb_func(store,cb);
	ctx = X509_STORE_CTX_new();
	if (ctx == NULL) {
		X509_STORE_free(store);
		return "Out of memory";
	}
	X509_STORE_CTX_init(ctx, store, cert, cacerts);
	if (rootcerts)
		X509_STORE_CTX_trusted_stack(ctx, rootcerts);
	if (purpose >= 0)
		X509_STORE_CTX_set_purpose(ctx, purpose);
	X509_STORE_CTX_set_flags(ctx, X509_V_FLAG_CB_ISSUER_CHECK);
	rval = X509_verify_cert(ctx);
	errm = X509_verify_cert_error_string(ctx->error);
	X509_STORE_CTX_free(ctx);
	if (rval)
		return NULL;
	if (errm != NULL)
		return errm;
	return "Verification failed";
}

/* [static] - Grab the data from an X509_NAME_ENTRY and put it into
 * a TQSL_X509_NAME_ITEM object, checking buffer sizes.
 *
 * Returns 0 on error, 1 if okay.
 *
 * It's okay for the name_buf or value_buf item of the object to
 * be NULL; it'll just be skipped.
 */
static int
tqsl_get_name_stuff(X509_NAME_ENTRY *entry, TQSL_X509_NAME_ITEM *name_item) {
	ASN1_OBJECT *obj;
	ASN1_STRING *value;
	const char *val;
	unsigned int len;

	if (entry == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 0;
	}
	obj = X509_NAME_ENTRY_get_object(entry);
	if (obj == NULL) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return 0;
	}
	if (name_item->name_buf != NULL) {
		len = i2t_ASN1_OBJECT(name_item->name_buf, name_item->name_buf_size, obj);
		if (len <= 0 || len > strlen(name_item->name_buf)) {
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 0;
		}
	}
	if (name_item->value_buf != NULL) {
		value = X509_NAME_ENTRY_get_data(entry);
		val = (const char *)ASN1_STRING_data(value);
		strncpy(name_item->value_buf, val, name_item->value_buf_size);
		name_item->value_buf[name_item->value_buf_size-1] = '\0';
		if (strlen(val) > strlen(name_item->value_buf)) {
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 0;
		}
	}
	return 1;
}

/* Get a name entry from an X509_NAME by its index.
 */
CLIENT_STATIC int
tqsl_get_name_index(X509_NAME *name, int index, TQSL_X509_NAME_ITEM *name_item) {
	X509_NAME_ENTRY *entry;
	int num_entries;

	if (tqsl_init())
		return 0;
	if (name == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 0;
	}
	num_entries = X509_NAME_entry_count(name);
	if (num_entries <= 0 || index >= num_entries) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 0;
	}
	entry = X509_NAME_get_entry(name, index);
	return tqsl_get_name_stuff(entry, name_item);
}

/* Get a name entry from an X509_NAME by its name.
 */
CLIENT_STATIC int
tqsl_get_name_entry(X509_NAME *name, const char *obj_name, TQSL_X509_NAME_ITEM *name_item) {
	X509_NAME_ENTRY *entry;
	int num_entries, i;

	if (tqsl_init())
		return 0;
	num_entries = X509_NAME_entry_count(name);
	if (num_entries <= 0)
		return 0;
	/* Loop through the name entries */
	for (i = 0; i < num_entries; i++) {
		entry = X509_NAME_get_entry(name, i);
		if (!tqsl_get_name_stuff(entry, name_item))
			continue;
		if (name_item->name_buf != NULL && !strcmp(name_item->name_buf, obj_name)) {
			/* Found the wanted entry */
			return 1;
		}
	}
	return 0;
}

/* Get the number of name entries in an X509_NAME
 */
CLIENT_STATIC int
tqsl_get_name_count(X509_NAME *name) {
	if (name == NULL)
		return 0;
	return X509_NAME_entry_count(name);
}

/* Get the number of name entries in the cert's subject DN
 */
CLIENT_STATIC int
tqsl_cert_get_subject_name_count(X509 *cert) {
	if (cert == NULL)
		return 0;
	return tqsl_get_name_count(X509_get_subject_name(cert));
}

/* Get a name entry from a cert's subject name by its index.
 */
CLIENT_STATIC int
tqsl_cert_get_subject_name_index(X509 *cert, int index, TQSL_X509_NAME_ITEM *name_item) {
	if (cert == NULL)
		return 0;
	if (tqsl_init())
		return 0;
	return tqsl_get_name_index(X509_get_subject_name(cert), index, name_item);
}

/* Get a name entry from a cert's subject name by its name.
 */
CLIENT_STATIC int
tqsl_cert_get_subject_name_entry(X509 *cert, const char *obj_name, TQSL_X509_NAME_ITEM *name_item) {
	X509_NAME *name;

	if (cert == NULL)
		return 0;
	if (tqsl_init())
		return 0;
	if ((name = X509_get_subject_name(cert)) == NULL)
		return 0;
	return tqsl_get_name_entry(name, obj_name, name_item);
}

/* Get a date entry from a cert's subject DN into a tQSL_Date object.
 */
CLIENT_STATIC int
tqsl_cert_get_subject_date(X509 *cert, const char *obj_name, tQSL_Date *date) {
	char buf[256], name_buf[256];
	TQSL_X509_NAME_ITEM item;

	if (tqsl_init())
		return 0;
	item.name_buf = name_buf;
	item.name_buf_size = sizeof name_buf;
	item.value_buf = buf;
	item.value_buf_size = sizeof buf;
	if (!tqsl_cert_get_subject_name_entry(cert, obj_name, &item))
		return 0;
	return !tqsl_initDate(date, buf);
}

/* Initialize the tQSL (really OpenSSL) random number generator
 * Return 0 on error.
 */
CLIENT_STATIC int
tqsl_init_random() {
	char fname[256];
	static int initialized = 0;

	if (initialized)
		return 1;
	if (RAND_file_name(fname, sizeof fname) != NULL)
		RAND_load_file(fname, -1);
	initialized = RAND_status();
	if (!initialized)
		tQSL_Error = TQSL_RANDOM_ERROR;
	return initialized;
}

/* Generate an RSA key of at least 1024 bits length
 */
CLIENT_STATIC EVP_PKEY *
tqsl_new_rsa_key(int nbits) {
	EVP_PKEY *pkey;

	if (nbits < 1024) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return NULL;
	}
	if ((pkey = EVP_PKEY_new()) == NULL) {
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	if (!tqsl_init_random())	/* Unable to init RN generator */
		return NULL;
	if (!EVP_PKEY_assign_RSA(pkey, RSA_generate_key(nbits, 0x10001, NULL, NULL))) {
		EVP_PKEY_free(pkey);
		tQSL_Error = TQSL_OPENSSL_ERROR;
		return NULL;
	}
	return pkey;
}

/* Output an ADIF field to a file descriptor.
 */
CLIENT_STATIC int
tqsl_write_adif_field(FILE *fp, const char *fieldname, char type, const unsigned char *value, int len) {
	if (fieldname == NULL)	/* Silly caller */
		return 0;
	if (fputc('<', fp) == EOF)
		return 1;
	if (fputs(fieldname, fp) == EOF)
		return 1;
	if (type && type != ' ' && type != '\0') {
		if (fputc(':', fp) == EOF)
			return 1;
		if (fputc(type, fp) == EOF)
			return 1;
	}
	if (value != NULL && len != 0) {
		if (len < 0)
			len = strlen((const char *)value);
		if (fputc(':', fp) == EOF)
			return 1;
		fprintf(fp, "%d>", len);
		if (fwrite(value, 1, len, fp) != (unsigned int) len)
			return 1;
	} else if (fputc('>', fp) == EOF)
			return 1;
	if (fputs("\n\n", fp) == EOF)
		return 1;
	return 0;
}

static int
tqsl_self_signed_is_ok(int ok, X509_STORE_CTX *ctx) {
	if (ctx->error == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT)
		return 1;
	return ok;
}

static char *
tqsl_make_cert_path(const char *filename, char *path, int size) {
	strncpy(path, tQSL_BaseDir, size);
	strncat(path, "/certs", size - strlen(path));
	MKDIR(path, 0700);
	strncat(path, "/", size - strlen(path));
	strncat(path, filename, size - strlen(path));
	return path;
}

static char *
tqsl_make_key_path(const char *callsign, char *path, int size) {
	char fixcall[256];
	int i;

	strncpy(fixcall, callsign, sizeof fixcall);
	for (i = 0; i < (int)sizeof fixcall && fixcall[i] != '\0'; i++) {
		fixcall[i] = toupper(fixcall[i]);
		if (!isdigit(fixcall[i]) && !isalpha(fixcall[i]))
			fixcall[i] = '_';
	}
	strncpy(path, tQSL_BaseDir, size);
	strncat(path, "/keys", size - strlen(path));
	MKDIR(path, 0700);
	strncat(path, "/", size - strlen(path));
	strncat(path, fixcall, size - strlen(path));
	return path;
}

static int
tqsl_handle_root_cert(const char *pem, X509 *x, int (*cb)(int, const char *)) {
	const char *cp;

	/* Verify self-signature on the root certificate */
	if ((cp = tqsl_ssl_verify_cert(x, NULL, NULL, 0, &tqsl_self_signed_is_ok)) != NULL) {
		strncpy(tQSL_CustomError, cp, sizeof tQSL_CustomError);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 1;
	}
	return tqsl_store_cert(pem, x, "root", TQSL_CERT_CB_ROOT, cb);
}

static int
tqsl_ssl_error_is_nofile() {
	unsigned long l = ERR_peek_error();
	if (tQSL_Error == TQSL_OPENSSL_ERROR &&
		ERR_GET_LIB(l) == ERR_LIB_SYS && ERR_GET_FUNC(l) == SYS_F_FOPEN)
		return 1;
	if (tQSL_Error == TQSL_SYSTEM_ERROR && errno == ENOENT)
		return 1;
	return 0;
}

static int
tqsl_handle_ca_cert(const char *pem, X509 *x, int (*cb)(int, const char *)) {
	STACK_OF(X509) *root_sk;
	char rootpath[256];
	const char *cp;

	tqsl_make_cert_path("root", rootpath, sizeof rootpath);
	if ((root_sk = tqsl_ssl_load_certs_from_file(rootpath)) == NULL) {
		if (!tqsl_ssl_error_is_nofile())
			return 1;
	}
	cp = tqsl_ssl_verify_cert(x, NULL, root_sk, 0, NULL);
	sk_X509_free(root_sk);
	if (cp != NULL) {
		strncpy(tQSL_CustomError, cp, sizeof tQSL_CustomError);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 1;
	}
	return tqsl_store_cert(pem, x, "authorities", TQSL_CERT_CB_CA, cb);
}

static int
tqsl_handle_user_cert(const char *cpem, X509 *x, int (*cb)(int, const char *)) {
	STACK_OF(X509) *root_sk, *ca_sk;
	char rootpath[256], capath[256];
	char pem[sizeof tqsl_static_buf];
	const char *cp;

	strncpy(pem, cpem, sizeof pem);
	/* Match the public key in the supplied certificate with a
     * private key in the key store.
	 */
	if (!tqsl_find_matching_key(x, NULL, NULL, "", NULL)) {
		if (tQSL_Error != TQSL_PASSWORD_ERROR)
			return 1;
		tQSL_Error = TQSL_NO_ERROR;	/* clear error */
	}

	/* Check the chain of authority back to a trusted root */
	tqsl_make_cert_path("root", rootpath, sizeof rootpath);
	if ((root_sk = tqsl_ssl_load_certs_from_file(rootpath)) == NULL) {
		if (!tqsl_ssl_error_is_nofile())
			return 1;
	}
	tqsl_make_cert_path("authorities", capath, sizeof capath);
	if ((ca_sk = tqsl_ssl_load_certs_from_file(capath)) == NULL) {
		if (!tqsl_ssl_error_is_nofile()) {
			sk_X509_free(root_sk);
			return 1;
		}
	}
	cp = tqsl_ssl_verify_cert(x, ca_sk, root_sk, 0, NULL);
	sk_X509_free(ca_sk);
	sk_X509_free(root_sk);
	if (cp != NULL) {
		strncpy(tQSL_CustomError, cp, sizeof tQSL_CustomError);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 1;
	}
	return tqsl_store_cert(pem, x, "user", TQSL_CERT_CB_USER, cb);
}

CLIENT_STATIC int
tqsl_store_cert(const char *pem, X509 *cert, const char *certfile, int type,
	int (*cb)(int, const char *)) {
	STACK_OF(X509) *sk;
	char path[256];
	char issuer[256];
	FILE *out;

	tqsl_make_cert_path(certfile, path, sizeof path);

	X509_NAME_oneline(X509_get_issuer_name(cert), issuer, sizeof issuer);
	/* Check for dupes */
	if ((sk = tqsl_ssl_load_certs_from_file(path)) == NULL) {
		if (!tqsl_ssl_error_is_nofile())
			return 1;	/* Unexpected OpenSSL error */
	}
	/* Check each certificate */
	if (sk != NULL) {
		long serial;
		int i, n;

		serial = ASN1_INTEGER_get(X509_get_serialNumber(cert));
		n = sk_X509_num(sk);
		for (i = 0; i < n; i++) {
			char buf[256];
			X509 *x;
			const char *cp;

			x = sk_X509_value(sk, i);
			cp = X509_NAME_oneline(X509_get_issuer_name(x), buf, sizeof buf);
			if (cp != NULL && !strcmp(cp, issuer)) {
				if (serial == ASN1_INTEGER_get(X509_get_serialNumber(x)))
					break;	/* We have a match */
			}
		}
		sk_X509_free(sk);
		if (i < n) {	/* Have a match -- cert is already in the file */
			if (cb != NULL) {
				char subject[256];
				char buf[256];
				int rval;

				X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof subject);
				strcpy(buf, "Duplicate certificate: ");
				strncat(buf, subject, sizeof buf);
				rval = (*cb)(type | TQSL_CERT_CB_WARNING, buf);
				if (rval) {
					tQSL_Error = TQSL_CUSTOM_ERROR;
					strcpy(tQSL_CustomError, "Duplicate certificate");
					return 1;
				}
			}
			return 0;
		}
	}
	/* Cert is not a duplicate. Append it to the certificate file */
	if (cb != NULL) {
		char subject[256];
		char buf[256];
		int rval;

		X509_NAME_oneline(X509_get_subject_name(cert), subject, sizeof subject);
		strcpy(buf, "Adding cert for: ");
		strncat(buf, subject, sizeof buf);
		rval = (*cb)(type | TQSL_CERT_CB_PROMPT, buf);
		if (rval) {
			tQSL_Error = TQSL_OPERATOR_ABORT;
			return 1;
		}
	}
	if ((out = fopen(path, "a")) == NULL) {
		strncpy(tQSL_ErrorFile, path, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		return 1;
	}
	fwrite(pem, 1, strlen(pem), out);
	if (fclose(out) == EOF) {
		strncpy(tQSL_ErrorFile, certfile, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_SYSTEM_ERROR;
		return 1;
	}
	return 0;
}

static int pw_aborted;

static int
fixed_password_callback(char *buf, int bufsiz, int verify, void *userdata) {
	pw_aborted = 0;
	if (userdata != NULL)
		strncpy(buf, (char *)userdata, bufsiz);
	else
		buf[0] = 0;
	return 0;
}

static int
prompted_password_callback(char *buf, int bufsiz, int verify, void *userdata) {
	pw_aborted = 0;
	if (userdata != NULL) {
		int (*cb)(char *, int) = (int (*)(char *, int))userdata;
		if ((*cb)(buf, bufsiz)) {
			pw_aborted = 1;
			return 0;
		}
	} else
		buf[0] = 0;
	return strlen(buf);
}

static int
tqsl_find_matching_key(X509 *cert, EVP_PKEY **keyp, TQSL_CERT_REQ **crq, const char *password, int (*cb)(char *,int)) {
	char path[256];
	char aro[256];
	TQSL_X509_NAME_ITEM item = { path, sizeof path, aro, sizeof aro };
	char privkey[2001] = "";
	EVP_PKEY *cert_key = NULL;
	RSA *rsa = NULL, *prsa = NULL;
	FILE *in = NULL;
	int rval = 0;
	int match = 0;
	int adif_status;
	long e;
	adifFieldResults field;
	BIO *bio = NULL;
	int (*ssl_cb)(char *, int, int, void *) = NULL;
	void *cb_user = NULL;
	static adifFieldDefinitions fields[] = {
		{ "PUBLIC_KEY", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		{ "PRIVATE_KEY", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		{ "CALLSIGN", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		{ "TQSL_CRQ_ADDRESS1", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		{ "TQSL_CRQ_ADDRESS2", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		{ "TQSL_CRQ_CITY", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		{ "TQSL_CRQ_STATE", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		{ "TQSL_CRQ_POSTAL", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		{ "TQSL_CRQ_COUNTRY", "", ADIF_RANGE_TYPE_NONE, 2000, 0, 0, NULL, NULL },
		{ "eor", "", ADIF_RANGE_TYPE_NONE, 0, 0, 0, NULL, NULL },
		{ "", "", ADIF_RANGE_TYPE_NONE, 0, 0, 0, NULL, NULL },
	};

	if (keyp != NULL)
		*keyp = NULL;

	if (!tqsl_cert_get_subject_name_entry(cert, "AROcallsign", &item))
		return rval;
	tqsl_make_key_path(aro, path, sizeof path);
	if ((in = fopen(path, TQSL_OPEN_READ)) == NULL) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		return rval;
	}
	if ((cert_key = X509_get_pubkey(cert)) == NULL)
		goto err;
	if (crq != NULL) {
		if (*crq != NULL)
			tqsl_free_cert_req(*crq, 0);
		*crq = (TQSL_CERT_REQ *)tqsl_calloc(1, sizeof (TQSL_CERT_REQ));
	}
	do {
		adif_status = adifGetField(&field, in, fields, notypes, &tqsl_static_alloc);
		if (adif_status == ADIF_GET_FIELD_EOF) {
			tQSL_Error = TQSL_NOKEY_ERROR;
			goto end;
		}
		/* Compare the keys */
		if (!strcmp(field.name, "PRIVATE_KEY"))
			strncpy(privkey, (char *)field.data, sizeof privkey);
		else if (!strcmp(field.name, "PUBLIC_KEY")) {
			if ((bio = BIO_new_mem_buf((void *)field.data, atoi(field.size))) == NULL)
				goto err;
			if ((rsa = PEM_read_bio_RSA_PUBKEY(bio, NULL, NULL, NULL)) == NULL)
				goto err;
			BIO_free(bio);
			bio = NULL;
			if (BN_cmp(rsa->n, cert_key->pkey.rsa->n) == 0)
				if (BN_cmp(rsa->e, cert_key->pkey.rsa->e) == 0)
					match = 1;
		} else if (!strcmp(field.name, "eor")) {
			if (match && privkey[0] != '\0') {
				/* We have a winner */
				if ((bio = BIO_new_mem_buf((void *)privkey, strlen(privkey))) == NULL)
					goto err;
				if (password != NULL) {
					ssl_cb = &fixed_password_callback;
					cb_user = (void *)password;
				} else if (cb != NULL) {
					ssl_cb = &prompted_password_callback;
					cb_user = (void *)cb;
				}
				if ((prsa = PEM_read_bio_RSAPrivateKey(bio, NULL, ssl_cb, cb_user)) == NULL)
					goto err;
				if ((*keyp = EVP_PKEY_new()) == NULL)
					goto err;
				EVP_PKEY_assign_RSA(*keyp, prsa);
				prsa = NULL;
				rval = 1;
				goto end;
			}
		} else if (crq != NULL && !strcmp(field.name, "TQSL_CRQ_ADDRESS1")) {
			(*crq)->address1 = (char *)tqsl_malloc(strlen((char *)field.data)+1);
			strcpy((char *)((*crq)->address1), (char *)field.data);
		} else if (crq != NULL && !strcmp(field.name, "TQSL_CRQ_ADDRESS2")) {
			(*crq)->address2 = (char *)tqsl_malloc(strlen((char *)field.data)+1);
			strcpy((char *)((*crq)->address2), (char *)field.data);
		} else if (crq != NULL && !strcmp(field.name, "TQSL_CRQ_CITY")) {
			(*crq)->city = (char *)tqsl_malloc(strlen((char *)field.data)+1);
			strcpy((char *)((*crq)->city), (char *)field.data);
		} else if (crq != NULL && !strcmp(field.name, "TQSL_CRQ_STATE")) {
			(*crq)->state = (char *)tqsl_malloc(strlen((char *)field.data)+1);
			strcpy((char *)((*crq)->state), (char *)field.data);
		} else if (crq != NULL && !strcmp(field.name, "TQSL_CRQ_POSTAL")) {
			(*crq)->postalCode = (char *)tqsl_malloc(strlen((char *)field.data)+1);
			strcpy((char *)((*crq)->postalCode), (char *)field.data);
		} else if (crq != NULL && !strcmp(field.name, "TQSL_CRQ_COUNTRY")) {
			(*crq)->country = (char *)tqsl_malloc(strlen((char *)field.data)+1);
			strcpy((char *)((*crq)->country), (char *)field.data);
		}
	} while (adif_status == ADIF_GET_FIELD_SUCCESS || adif_status == ADIF_GET_FIELD_NO_NAME_MATCH);
	tQSL_Error = TQSL_ADIF_ERROR;
	goto end;
err:
	e = ERR_peek_error();
	if ((ERR_GET_LIB(e) == ERR_LIB_EVP && ERR_GET_REASON(e) == EVP_R_BAD_DECRYPT)
		|| (ERR_GET_LIB(e) == ERR_LIB_PEM && ERR_GET_REASON(e) == PEM_R_BAD_PASSWORD_READ)) {
		tqsl_getErrorString();	/* clear error */
		tQSL_Error = pw_aborted ? TQSL_OPERATOR_ABORT : TQSL_PASSWORD_ERROR;
		ERR_clear_error();
	} else
		tQSL_Error = TQSL_OPENSSL_ERROR;
end:
	if (prsa != NULL)
		RSA_free(prsa);
	if (rsa != NULL)
		RSA_free(rsa);
	if (bio != NULL)
		BIO_free(bio);
	if (cert_key != NULL)
		EVP_PKEY_free(cert_key);
	if (in != NULL)
		fclose(in);
	return rval;
}

static int
tqsl_get_cert_ext(X509 *cert, const char *ext, unsigned char *userbuf, int *buflen, int *crit) {
	int i, n, datasiz;
	X509_EXTENSION *xe;
	char buf[256];
	ASN1_OBJECT *obj;
	ASN1_OCTET_STRING *data;

	if (tqsl_init())
		return 1;
	if (cert == NULL || ext == NULL || userbuf == NULL || buflen == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	n = X509_get_ext_count(cert);
	for (i = 0; i < n; i++) {
		xe = X509_get_ext(cert, i);
		if (xe == NULL) {
			tQSL_Error = TQSL_OPENSSL_ERROR;
			return 1;
		}
		buf[0] = '\0';
		obj = X509_EXTENSION_get_object(xe);
		if (obj)
			OBJ_obj2txt(buf, sizeof buf, obj, 0);
		if (strcmp(buf, ext))
			continue;
		/* This is the desired extension */
		data = X509_EXTENSION_get_data(xe);
		if (data != NULL) {
			datasiz = ASN1_STRING_length(data);
			if (datasiz > *buflen) {
				tQSL_Error = TQSL_BUFFER_ERROR;
				return 1;
			}
			*buflen = datasiz;
			if (datasiz)
				memcpy(userbuf, ASN1_STRING_data(data), datasiz);
		}
		if (crit != NULL)
			*crit = X509_EXTENSION_get_critical(xe);
		return 0;
	}
	strncpy(tQSL_CustomError, "Certificate Extension not found: ", sizeof tQSL_CustomError);
	strncat(tQSL_CustomError + strlen(tQSL_CustomError), ext, (sizeof tQSL_CustomError) -
		strlen(tQSL_CustomError));
	tQSL_Error = TQSL_CUSTOM_ERROR;
	return 1;
}

static int
tqsl_get_asn1_date(ASN1_TIME *tm, tQSL_Date *date) {
	char *v;
	int i;

	i=tm->length;
	v=(char *)tm->data;

	if (i >= 14) {
		for (i=0; i<12; i++)
			if ((v[i] > '9') || (v[i] < '0')) goto err;
		date->year =  (v[0]-'0')*1000+(v[1]-'0')*100 + (v[2]-'0')*10+(v[3]-'0');
		date->month = (v[4]-'0')*10+(v[5]-'0');
		if ((date->month > 12) || (date->month < 1)) goto err;
		date->day = (v[6]-'0')*10+(v[7]-'0');
	} else if (i < 12) {
		goto err;
	} else {
		for (i=0; i<10; i++)
			if ((v[i] > '9') || (v[i] < '0')) goto err;
		date->year = (v[0]-'0')*10+(v[1]-'0');
		if (date->year < 50) date->year+=100;
		date->year += 1900;
		date->month = (v[2]-'0')*10+(v[3]-'0');
		if ((date->month > 12) || (date->month < 1)) goto err;
		date->day = (v[4]-'0')*10+(v[5]-'0');
	}
	return 0;

err:
	tQSL_Error = TQSL_INVALID_DATE;
	return 1;
}

static char *
tqsl_sign_base64_data(tQSL_Cert cert, char *b64data) {
	int len;
	static unsigned char sig[256];
	int siglen = sizeof sig;

	if (!strncmp(b64data, "-----", 5)) {
		b64data = strchr(b64data, '\n');
		if (b64data == NULL)
			return NULL;
		b64data++;
	}
	len = sizeof tqsl_static_buf;
	if (tqsl_decodeBase64(b64data, tqsl_static_buf, &len))
		return NULL;
	if (tqsl_signDataBlock(cert, tqsl_static_buf, len, sig, &siglen))
		return NULL;
	if (tqsl_encodeBase64(sig, siglen, (char *)tqsl_static_buf, sizeof tqsl_static_buf))
		return NULL;
	return (char *)tqsl_static_buf;
}