/***************************************************************************
                          test_openssl.cpp  -  description
                             -------------------
    begin                : Tue May 14 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#define TEST_NOPRIVATE	1

#ifndef LOTW_SERVER
#undef TEST_NOPRIVATE
#define TEST_NOPRIVATE	1
#endif

/* Test the LOTW OpenSSL interface routines */

#include <iostream>
#include <string>
#ifdef LOTW_SERVER
#include "testharness.h"
#include "certificate.h"
#endif
#include "tqsllib.h"
#include "openssl_cert.h"
#include <openssl/err.h>
#include <openssl/evp.h>

static bool query = false;
static bool printem = false;

#ifndef LOTW_SERVER
#undef logError
#define stdlog ""
#define errlog "ERROR: "
void
logError(const char *x, const char *y) {
	cout << x << y << endl;
}
void
logError(const char *x, string y) {
	logError(x, y.c_str());
}
int harness_main(int argc, char *argv[]);
#endif

#define test_issuer_bad "This is just a test"
#define test_issuer "/C=US/ST=CT/L=Newington/O=American Radio Relay League/OU=Logbook of the World/CN=Logbook of the World Production CA/DC=arrl.org/Email=lotw@arrl.org"
// static int cb(int ok, X509_STORE_CTX *ctx);
static int tqsl_callback(int type, const char *msg);

int
main(int argc, char *argv[]) {
#ifdef LOTW_SERVER
	harness_init();
	return harness_run(argc, argv);
#else
	return harness_main(argc, argv);
#endif
}

#ifndef TEST_NOPRIVATE
X509 *
get_cert(const char *fname) {
	STACK_OF(X509) *certs = tqsl_ssl_load_certs_from_file(fname);
	if (certs == NULL)
		logError(errlog, string(fname) + string(": ") + tqsl_getErrorString());
	return sk_X509_value(certs, 0);
}
#endif

int
harness_main(int argc, char *argv[]) {
	int i;

	tqsl_init();

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v"))
			printem = true;
		if (!strcmp(argv[i], "-p"))
			query = true;
	}
#ifdef LOTW_SERVER
	stdlog.setPreamble("test_openssl:");
	errlog.setPreamble("test_openssl ERROR:");
#endif

#ifdef LOTW_SERVER
	chdir("/home/jbloom/projects/lotwdb/lotwdb");
#endif

	logError(stdlog, "Tests Started");

#ifndef TEST_NOPRIVATE
	logError(stdlog, "Testing tqsl_ssl_load_certs");
	string fn = "test_certs.pem";	// Stack of certs, just to test loading more than one
	string userfn = "AROcert.pem";
	string cafn = "ARRLproductionCAcert.pem";
	string rootfn = "ARRLrootCAcert.pem";
	string badcacert = "badissuer.pem";
	string selfsignedcert = "selfsigned.pem";
	STACK_OF(X509) *stack = tqsl_ssl_load_certs_from_file(fn.c_str());
	if (!stack)
		logError(errlog, fn + string(": ") + tqsl_getErrorString());
	else {
		int n = sk_X509_num(stack);
		char buf[80];
		sprintf(buf, "Loaded %d certificates from %s", n, fn.c_str());
		logError(stdlog, buf);
		if (printem) {
			X509 *x;
			while ((x = sk_X509_shift(stack))) {
				X509_print_fp(stderr, x);
				X509_free(x);
			}
		}
		sk_X509_free(stack);
	}
	// Now let's try verifying the test cert
	STACK_OF(X509) *cacerts = tqsl_ssl_load_certs_from_file(cafn.c_str());
	if (cacerts == NULL)
		logError(errlog, cafn + string(": ") + tqsl_getErrorString());
	STACK_OF(X509) *rootcerts = tqsl_ssl_load_certs_from_file(rootfn.c_str());
	if (rootcerts == NULL)
		logError(errlog, rootfn + string(": ") + tqsl_getErrorString());
	X509 *cert = get_cert(userfn.c_str());
	const char *errm;
	logError(stdlog, "Testing a \"normal\" user cert signed by an intermediate CA");
	if ((errm = tqsl_ssl_verify_cert(cert, cacerts, rootcerts, -1, printem ? &cb : NULL)) == 0) {
		logError(stdlog, userfn + ": verified");
		char buf[256], namebuf[256];
		logError(stdlog, "Looking up callsign in cert");
		TQSL_X509_NAME_ITEM item;
		item.name_buf = 0;
		item.name_buf = namebuf;
		item.name_buf_size = sizeof namebuf;
		item.value_buf = buf;
		item.value_buf_size = sizeof buf;
		if (tqsl_cert_get_subject_name_entry(cert, "AROcallsign", &item))
			logError(stdlog, string("Callsign in cert: ") + item.value_buf);
		else
			logError(errlog, "Unable to extract callsign from certificate");
		TQSL_DATE date;
		if (tqsl_cert_get_subject_date(cert, "QSONotBeforeDate", &date))
			logError(stdlog, string("QSO not-before date: ") + tqsl_date_to_text(&date, buf, sizeof buf));
		else
			logError(errlog, "Unable to retrieve QSO not-before date");
		if (tqsl_cert_get_subject_date(cert, "QSONotAfterDate", &date))
			logError(stdlog, string("QSO not-after date: ") + tqsl_date_to_text(&date, buf, sizeof buf));
		else
			logError(errlog, "Unable to retrieve QSO not-before date");
		// Dump the subject name entries
		int n = tqsl_cert_get_subject_name_count(cert);
		sprintf(buf, "Dumping %d subject name entries:", n);
		logError(stdlog, buf);
		for (int i = 0; i < n; i++) {
			if (tqsl_cert_get_subject_name_index(cert, i, &item))
				logError(stdlog, string("   ") + string(item.name_buf) + " = " + string(item.value_buf));
			else {
				sprintf(buf, "   Failed to retrieve item %d", i);
				logError(errlog, buf);
			}
		}
		// We can try a filter
		logError(stdlog, "Test filtering the certificate list -- real issuer, callsign, good QSO date");
		tqsl_date_init(&date, "1998-06-30");
		STACK_OF(X509) *s = sk_X509_new_null();
		if (!s) {
			logError(errlog, "Unable to initialize X509 stack (out of memory?)");
			exit(1);
		}
		sk_X509_push(s, X509_dup(cert));
		STACK_OF(X509) *newsk = tqsl_filter_cert_list(s, "WA1VVB", &date, test_issuer, 1);
		sprintf(buf, "%d certs match the filter parameters", sk_X509_num(newsk));
		if (1 == sk_X509_num(newsk))
			logError(stdlog, buf);
		else
			logError(errlog, buf);
		sk_X509_free(newsk);
		logError(stdlog, "Test filtering the certificate list -- fake issuer");
		newsk = tqsl_filter_cert_list(s, 0, 0, test_issuer_bad, 1);
		sprintf(buf, "%d certs match the filter parameters", sk_X509_num(newsk));
		if (0 == sk_X509_num(newsk))
			logError(stdlog, buf);
		else
			logError(errlog, buf);
		sk_X509_free(newsk);
		logError(stdlog, "Test filtering the certificate list -- bad callsign");
		newsk = tqsl_filter_cert_list(s, "KE3Z", 0, 0, 1);
		sprintf(buf, "%d certs match the filter parameters", sk_X509_num(newsk));
		if (0 == sk_X509_num(newsk))
			logError(stdlog, buf);
		else
			logError(errlog, buf);
		sk_X509_free(newsk);
		logError(stdlog, "Test filtering the certificate list -- bad QSO date (early)");
		tqsl_date_init(&date, "1997-12-31");
		newsk = tqsl_filter_cert_list(s, 0, &date, 0, 1);
		sprintf(buf, "%d certs match the filter parameters", sk_X509_num(newsk));
		if (0 == sk_X509_num(newsk))
			logError(stdlog, buf);
		else
			logError(errlog, buf);
		sk_X509_free(newsk);
		logError(stdlog, "Test filtering the certificate list -- bad QSO date (late)");
		tqsl_date_init(&date, "2000-01-01");
		newsk = tqsl_filter_cert_list(s, 0, &date, 0, 1);
		sprintf(buf, "%d certs match the filter parameters", sk_X509_num(newsk));
		if (0 == sk_X509_num(newsk))
			logError(stdlog, buf);
		else
			logError(errlog, buf);
		sk_X509_free(newsk);
		sk_X509_free(s);
	} else {
		logError(errlog, string(userfn + ": ") + errm);
		X509_free(cert);
	}
	logError(stdlog, "Testing a cert signed directly by the trusted root");
	cert = get_cert(rootfn.c_str());
	if ((errm = tqsl_ssl_verify_cert(cert, NULL, rootcerts, -1, printem ? &cb : NULL)) == 0)
		logError(stdlog, rootfn + ": verified");
	else
		logError(errlog, string(rootfn + ": ") + errm);
	logError(stdlog, "Testing a cert signed by a CA whose cert isn't available");
	cert = get_cert(badcacert.c_str());
	if ((errm = tqsl_ssl_verify_cert(cert, cacerts, rootcerts, -1, printem ? &cb : NULL)) == 0)
		logError(errlog, badcacert + ": verified but shouldn't have");
	else
		logError(stdlog, string(badcacert + " failed to verify, as expected: ") + errm);
	logError(stdlog, "Testing a self-signed cert");
	cert = get_cert(selfsignedcert.c_str());
	if ((errm = tqsl_ssl_verify_cert(cert, cacerts, rootcerts, -1, printem ? &cb : NULL)) == 0)
		logError(errlog, selfsignedcert + ": verified but shouldn't have");
	else
		logError(stdlog, string(selfsignedcert + " failed to verify, as expected: ") + errm);
#ifndef TEST_NORSA
	logError(stdlog, "Testing RSA stuff");
	logError(stdlog, "Generating RSA key < 1024 bits");
	EVP_PKEY *pk = tqsl_new_rsa_key(1000);
	if (pk != NULL)
		logError(errlog, "Key generation should have failed but didn't");
	else
		logError(stdlog, string("Failed, as expected: ") + tqsl_getErrorString());
	logError(stdlog, "Generating RSA key = 1024 bits");
	pk = tqsl_new_rsa_key(1024);
	if (pk == NULL)
		logError(errlog, string("Key generation FAILED: ") + tqsl_getErrorString());
	else {
		logError(stdlog, "Succeeded");
		EVP_PKEY_free(pk);
	}
#endif	// TEST_NORSA
#endif	// TEST_NOPRIVATE
	logError(stdlog, "Now testing the public API");
	string certs_dir = string(tQSL_BaseDir) + "/certs";
	logError(stdlog, "Cleaning out old certs from " + certs_dir);
	system((string("rm -rf ") + certs_dir).c_str());
	certs_dir = string(tQSL_BaseDir) + "/keys";
	logError(stdlog, "Cleaning out old keys from " + certs_dir);
	system((string("rm -rf ") + certs_dir).c_str());
	logError(stdlog, "Loading certificates from file");
	if (tqsl_importCertFile("adif_CERT_test", &tqsl_callback))
		logError(errlog, tqsl_getErrorString());
	else
		logError(stdlog, "Load succeeded");
	logError(stdlog, "Loading certificates from file with bad user cert (signed by unknown CA)");
	if (tqsl_importCertFile("adif_CERT_test_bad_user", NULL))
		logError(stdlog, string("Failed as expected: ") + tqsl_getErrorString());
	else
		logError(errlog, "Load succeeded -- SHOULDN'T HAVE!");
	logError(stdlog, "Creating certificate request");
	TQSL_CERT_REQ apireq = {
		"W1AW", "Hiram P. Maxim", "225 Main Street", "", "Newington", "CT",
		"06111", "USA", "jbloom@arrl.org", 104 };
	if (tqsl_createCertRequest("reqtmp", &apireq, 0))
		logError(errlog, string("Failed: ") + tqsl_getErrorString());
	else
		logError(stdlog, "Succeeded");
	logError(stdlog, "Creating certificate request w/missing call sign");
	TQSL_CERT_REQ badapireq1 =  {
		"", "Hiram P. Maxim", "225 Main Street", "", "Newington", "CT",
		"06111", "USA", "jbloom@arrl.org", 104 };
	if (tqsl_createCertRequest("reqtmp", &badapireq1, 0))
		logError(stdlog, string("Failed as expected: ") + tqsl_getErrorString());
	else
		logError(errlog, "Succeeded, SHOULDN'T HAVE");
	logError(stdlog, "Creating certificate request w/bad email address");
	TQSL_CERT_REQ badapireq2 = {
		"W1AW", "Hiram P. Maxim", "225 Main Street", "", "Newington", "CT",
		"06111", "USA", "jbloom!arrl.org", 104 };
	if (tqsl_createCertRequest("reqtmp", &badapireq2, 0))
		logError(stdlog, string("Failed as expected: ") + tqsl_getErrorString());
	else
		logError(errlog, "Succeeded, SHOULDN'T HAVE");
	logError(stdlog, "Finished testing the public API");
	logError(stdlog, "Tests Completed");
	return 0;
}

static int tqsl_callback(int type, const char *msg) {
	char buf[80];

	if (TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_ERROR) {
		logError(errlog, string("Error loading certificate: ") + msg);
		return 0;
	}
	if (TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_WARNING) {
		logError(stdlog, string("WARNING: ") + msg);
		return 0;
	}
	/* Must be a TQSL_CERT_CB_PROMPT */
//	if (query || printem) {
		if (type == TQSL_CERT_CB_ROOT)
			logError(stdlog, "Warning! ROOT CERTIFICATE BEING ADDED!");
		logError(stdlog, msg);
//	}
	if (query) {
		printf("Add cert? ");
		fgets(buf, sizeof buf, stdin);
		if (buf[0] != 'y' && buf[0] != 'Y')
			return 1;
	}
	return 0;
}

/*
static int cb(int ok, X509_STORE_CTX *ctx)
	{
	char buf[512];

	if (!ok)
		{
		X509_NAME_oneline(
				X509_get_subject_name(ctx->current_cert),buf,256);
		sprintf(buf, "%s; ",buf);
		sprintf(buf+strlen(buf), "error %d at %d depth lookup:%s",ctx->error,
			ctx->error_depth,
			X509_verify_cert_error_string(ctx->error));
 		ERR_clear_error();
		logError(stdlog, buf);
	}
	return(ok);
	}

*/
