/***************************************************************************
                          dumptqsldata.c  -  description
                             -------------------
    begin                : Mon Mar 3 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

/* Dumps the config data from the TQSL library */

#include <stdio.h>
#include <stdlib.h>
#include "tqsllib.h"

void
errchk(int stat) {
	if (stat) {
		printf("ERROR: %s\n", tqsl_getErrorString());
		exit(1);
	}
}

int
main() {
	int count, i;
	const char *cp1, *cp2;
	int low, high;

	errchk(tqsl_init());
	puts("===== MODES =====\n   Mode       Group");
	errchk(tqsl_getNumMode(&count));
	for (i = 0; i < count; i++) {
		errchk(tqsl_getMode(i, &cp1, &cp2));
		printf("   %-10.10s %s\n", cp1, cp2);
	}
	puts("\n===== BANDS =====\n   Band       Spectrum  Low      High");
	errchk(tqsl_getNumBand(&count));
	for (i = 0; i < count; i++) {
		errchk(tqsl_getBand(i, &cp1, &cp2, &low, &high));
		printf("   %-10.10s %-8.8s  %-8d %d\n", cp1, cp2, low, high);
	}
	puts("\n===== DXCC =====\n   Entity  Name");
	errchk(tqsl_getNumDXCCEntity(&count));
	for (i = 0; i < count; i++) {
		errchk(tqsl_getDXCCEntity(i, &low, &cp1));
		printf("   %-6d  %s\n", low, cp1);
	}
	return 0;
}
