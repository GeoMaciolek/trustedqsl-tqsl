/***************************************************************************
                          gen_crq.cpp  -  description
                             -------------------
    begin                : Sat Dec 14 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$

 Generates a set of certificate-request files.

 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <exception>
#ifdef HAVE_GETOPT_H
	#include <getopt.h>
#endif
#include <string.h>
#include "tqsllib.h"
#include "tqslexc.h"

using namespace std;

int
usage() {
	cerr << "Usage: -e email -d dxcc [-c sign_call] [-x sign_dxcc] call1 [call2 ...]" << endl;
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[]) {
	string sign_call, email_addr;
	int dxcc = 0, sign_dxcc = 0;
	tQSL_Cert sign_cert = 0;

	try {
		if (tqsl_init())
			throw tqslexc();
		int c;
		while ((c = getopt(argc, argv, "c:x:e:d:")) != -1) {
			switch (c) {
				case 'c':
					sign_call = optarg;
					break;
				case 'x':
					sign_dxcc = atoi(optarg);
					break;
				case 'd':
					dxcc = atoi(optarg);
					break;
				case 'e':
					email_addr = optarg;
					break;
				default:
					usage();
			}
		}
		if (optind >= argc || email_addr == "" || dxcc == 0)
			usage();
		if (sign_call != "") {
//			if (sign_dxcc == 0)
//				usage();
			tQSL_Cert *list;
			int ncerts;
			if (tqsl_selectCertificates(&list, &ncerts, sign_call.c_str(), sign_dxcc, 0, 0, 1))
				throw tqslexc();
			if (ncerts < 1) {
				string erm = "No signing certificate found for " + sign_call;
				if (sign_dxcc) {
					char buf[30];
					sprintf(buf, " with DXCC=%d", sign_dxcc);
					erm += buf;
				}
				throw myexc(erm);
			}
			sign_cert = *list;
		} else if (sign_dxcc != 0)
			usage();
		if (sign_cert) {
			char buf[512];
			long serial;
			int cdxcc;
			if (tqsl_getCertificateIssuer(sign_cert, buf, sizeof buf))
				throw tqslexc();
			if (tqsl_getCertificateSerial(sign_cert, &serial))
				throw tqslexc();
			if (tqsl_getCertificateDXCCEntity(sign_cert, &cdxcc))
				throw tqslexc();
			cout << "Signing certificate issuer: " << buf << endl;
			cout << "Signing certificate serial: " << serial << endl;
			cout << "  Signing certificate DXCC: " << cdxcc << endl;
			if (tqsl_beginSigning(sign_cert, (char *)"", 0, 0))
				throw tqslexc();
		}
		TQSL_CERT_REQ crq;
		memset(&crq, 0, sizeof crq);
		strcpy(crq.name, "Ish Kabibble");
		strcpy(crq.address1, "1 No Place");
		strcpy(crq.city, "City");
		strcpy(crq.state, "ST");
		strcpy(crq.country, "USA");
		strcpy(crq.emailAddress, email_addr.c_str());
		crq.dxccEntity = dxcc;
		tqsl_initDate(&crq.qsoNotBefore, "1945-11-15");
		crq.signer = sign_cert;
		for (; optind < argc; optind++) {
			string call = argv[optind];
			strcpy(crq.callSign, call.c_str());
			for (char *cp = argv[optind]; *cp; cp++) {
				if (*cp == '/')
					*cp = '_';
			}
			string filename = string(argv[optind]) + ".tq5";
			cout << "Creating CRQ for " << crq.callSign << " DXCC=" << crq.dxccEntity << endl;
			if (tqsl_createCertRequest(filename.c_str(), &crq, 0, 0))
				throw tqslexc();
		}
		return EXIT_SUCCESS;
	} catch (exception& x) {
		cerr << "Aborting: " << x.what() << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
