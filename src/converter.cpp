/***************************************************************************
                          converter.cpp  -  description
                             -------------------
    begin                : Sun Dec 15 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
	#include <getopt.h>
#endif
#include "tqsllib.h"
#include "tqslerrno.h"
#include "tqslconvert.h"
#include "tqslexc.h"

using namespace std;

int usage() {
	cerr << "Usage: converter [-ac] station-location infile [outfile]\n";
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[]) {
	enum { UNKNOWN, CABRILLO, ADIF } type = UNKNOWN;
	int opt;

	while ((opt = getopt(argc, argv, "ca")) != -1) {
		switch (opt) {
			case 'c':
				type = CABRILLO;
				break;
			case 'a':
				type = ADIF;
				break;
			default:
				usage();
		}
	}
	if (argc - optind < 2)
		usage();
	tQSL_Converter conv = 0;
	try {
		if (tqsl_init())
			throw tqslexc();
		// Get the specified station location data
		tQSL_Location loc;
		if (tqsl_getStationLocation(&loc, argv[optind++]))
			throw tqslexc();
		// Get the callsign and DXCC entity to use
		char call[256];
		int dxcc;
		if (tqsl_getLocationCallSign(loc, call, sizeof call))
			throw tqslexc();
		if (tqsl_getLocationDXCCEntity(loc, &dxcc))
			throw tqslexc();
		// Get a list of available signing certificates
		tQSL_Cert *certs;
		int ncerts;
		if (tqsl_selectCertificates(&certs, &ncerts, call, dxcc, 0, 0, 1))
			throw tqslexc();
		if (ncerts < 1)
			throw myexc(string("No certificates available for ") + call);
		int stat = 1;
		if (type == UNKNOWN || type == CABRILLO) {
			if ((stat = tqsl_beginCabrilloConverter(&conv, argv[optind], certs, ncerts, loc)) != 0
				&& type == CABRILLO)
				throw tqslexc();
		}
		if (stat) {
			if (tqsl_beginADIFConverter(&conv, argv[optind], certs, ncerts, loc))
				throw tqslexc();
		}
		optind++;
		const char *ofile = (optind < argc) ? argv[optind] : "converted.tq7";
		ofstream out;
		out.open(ofile, ios::out|ios::trunc|ios::binary);
		if (!out.is_open())
			throw myexc(string("Unable to open ") + ofile);
		bool haveout = false;
		do {
			const char *gabbi = tqsl_getConverterGABBI(conv);
			if (gabbi) {
				haveout = true;
				out << gabbi;
				continue;
			}
   			if (tQSL_Error == TQSL_SIGNINIT_ERROR) {
   				tQSL_Cert cert;
   				if (tqsl_getConverterCert(conv, &cert))
					throw tqslexc();
   				if (tqsl_beginSigning(cert, 0, 0, 0))
					throw tqslexc();
   				continue;
   			}
   			break;
		} while (1);
		out.close();
		if (tQSL_Error != TQSL_NO_ERROR)
			throw tqslexc();
		else if (!haveout)
			cerr << "Empty log file" << endl;
	} catch (exception& x) {
		char buf[40] = "";
		int lineno;
		if (conv && !tqsl_getConverterLine(conv, &lineno)) // && lineno > 0)
			sprintf(buf, " on line %d", lineno);
		cerr << "Aborted: " << x.what() << buf << endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
