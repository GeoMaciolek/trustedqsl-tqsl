/***************************************************************************
                          tqslconvert.cpp  -  description
                             -------------------
    begin                : Sun Nov 17 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#define TQSLLIB_DEF

#include "tqsllib.h"

#include "tqslconvert.h"
#include <stdio.h>
#include "tqslerrno.h"
#include <cstring>
#include <string>
#include <ctype.h>
#include <set>

//#include <iostream>

using namespace std;

static bool checkCallSign(const string& call);

namespace tqsllib {

class TQSL_CONVERTER {
public:
	TQSL_CONVERTER();
	~TQSL_CONVERTER();
	void clearRec();
	int sentinel;
//	FILE *file;
	tQSL_ADIF adif;
	tQSL_Cabrillo cab;
	tQSL_Cert *certs;
	int ncerts;
	tQSL_Location loc;
	TQSL_QSO_RECORD rec;
	bool rec_done;
	int cert_idx;
	int base_idx;
	bool need_station_rec;
	bool *certs_used;
	bool allow_bad_calls;
	set <string> modes;
	set <string> bands;
	string rec_text;
};

inline TQSL_CONVERTER::TQSL_CONVERTER()  : sentinel(0x4445) {
//	file = 0;
	adif = 0;
	cab = 0;
	cert_idx = -1;
	base_idx = 1;
	certs_used = 0;
	need_station_rec = false;
	rec_done = true;
	allow_bad_calls = false;
	memset(&rec, 0, sizeof rec);
	// Init the band data
	const char *val;
	int n = 0;
	tqsl_getNumBand(&n);
	for (int i = 0; i < n; i++) {
		val = 0;
		tqsl_getBand(i, &val, 0, 0, 0);
		if (val)
			bands.insert(val);
	}
	// Init the mode data
	n = 0;
	tqsl_getNumMode(&n);
	for (int i = 0; i < n; i++) {
		val = 0;
		tqsl_getMode(i, &val, 0);
		if (val)
			modes.insert(val);
	}
}

inline TQSL_CONVERTER::~TQSL_CONVERTER() {
	clearRec();
//	if (file)
//		fclose(file);
	tqsl_endADIF(&adif);
	if (certs_used)
		delete[] certs_used;
	sentinel = 0;
}

inline void TQSL_CONVERTER::clearRec() {
	memset(&rec, 0, sizeof rec);
	rec_text = "";
}

#define CAST_TQSL_CONVERTER(x) ((tqsllib::TQSL_CONVERTER *)(x))

}	// namespace tqsllib

using namespace tqsllib;

#ifndef HAVE_SNPRINTF
#include <stdarg.h>
static int
snprintf(char *str, int, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int rval = vsprintf(str, fmt, ap);
	va_end(ap);
	return rval;
}
#endif	// HAVE_SNPRINTF

static char *
tqsl_strtoupper(char *str) {
	for (char *cp = str; *cp != '\0'; cp++)
		*cp = toupper(*cp);
	return str;
}

static TQSL_CONVERTER *
check_conv(tQSL_Converter conv) {
	if (tqsl_init())
		return 0;
	if (conv == 0 || CAST_TQSL_CONVERTER(conv)->sentinel != 0x4445)
		return 0;
	return CAST_TQSL_CONVERTER(conv);
}

static tqsl_adifFieldDefinitions adif_qso_record_fields[] = {
	{ "CALL", "", TQSL_ADIF_RANGE_TYPE_NONE, 13, 0, 0, NULL },
	{ "BAND", "", TQSL_ADIF_RANGE_TYPE_NONE, 5, 0, 0, NULL },
	{ "MODE", "", TQSL_ADIF_RANGE_TYPE_NONE, 16, 0, 0, NULL },
	{ "QSO_DATE", "", TQSL_ADIF_RANGE_TYPE_NONE, 10, 0, 0, NULL },
	{ "TIME_ON", "", TQSL_ADIF_RANGE_TYPE_NONE, 10, 0, 0, NULL },
	{ "FREQ", "", TQSL_ADIF_RANGE_TYPE_NONE, 15, 0, 0, NULL },
	{ "eor", "", TQSL_ADIF_RANGE_TYPE_NONE, 0, 0, 0, NULL },
};

DLLEXPORT int
tqsl_beginADIFConverter(tQSL_Converter *convp, const char *filename, tQSL_Cert *certs,
	int ncerts, tQSL_Location loc) {
	if (tqsl_init())
		return 0;
	if (!convp || !filename) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	tQSL_ADIF adif;
	if (tqsl_beginADIF(&adif, filename))
		return 1;
	TQSL_CONVERTER *conv = new TQSL_CONVERTER();
	conv->adif = adif;
	conv->certs = certs;
	conv->ncerts = ncerts;
	if (ncerts > 0) {
		conv->certs_used = new bool[ncerts];
		for (int i = 0; i < ncerts; i++)
			conv->certs_used[i] = false;
	}
	conv->loc = loc;
	*convp = conv;
	return 0;
}

DLLEXPORT int
tqsl_beginCabrilloConverter(tQSL_Converter *convp, const char *filename, tQSL_Cert *certs,
	int ncerts, tQSL_Location loc) {
	if (tqsl_init())
		return 0;
	if (!convp || !filename) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	tQSL_Cabrillo cab;
	if (tqsl_beginCabrillo(&cab, filename))
		return 1;
	TQSL_CONVERTER *conv = new TQSL_CONVERTER();
	conv->cab = cab;
	conv->certs = certs;
	conv->ncerts = ncerts;
	if (ncerts > 0) {
		conv->certs_used = new bool[ncerts];
		for (int i = 0; i < ncerts; i++)
			conv->certs_used[i] = false;
	}
	conv->loc = loc;
	*convp = conv;
	return 0;
}

DLLEXPORT int
tqsl_endConverter(tQSL_Converter *convp) {
	if (!convp || CAST_TQSL_CONVERTER(*convp) == 0)
		return 0;
	if (CAST_TQSL_CONVERTER(*convp)->sentinel == 0x4445)
		delete CAST_TQSL_CONVERTER(*convp);
	*convp = 0;
	return 0;
}

static unsigned char *
adif_allocate(size_t size) {
	return new unsigned char[size];
}

static int
find_matching_cert(TQSL_CONVERTER *conv) {
	int i;
	for (i = 0; i < conv->ncerts; i++) {
		tQSL_Date cdate;

		if (tqsl_getCertificateQSONotBeforeDate(conv->certs[i], &cdate))
			return -1;
		if (tqsl_compareDates(&(conv->rec.date), &cdate) < 0)
			continue;
		if (tqsl_getCertificateQSONotAfterDate(conv->certs[i], &cdate))
			return -1;
		if (tqsl_compareDates(&(conv->rec.date), &cdate) > 0)
			continue;
		return i;
	}
	return -1;
}

static const char *notypes[] = { "D","T","M","N","C","" };

/*
static char *
tqsl_strdup(const char *str) {
	char *cp = new char[strlen(str)+1];
	strcpy(cp, str);
	return cp;
}
*/

DLLEXPORT const char *
tqsl_getConverterGABBI(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 0;
	if (conv->need_station_rec) {
		int uid = conv->cert_idx + conv->base_idx;
		conv->need_station_rec = false;
		return tqsl_getGABBItSTATION(conv->loc, uid, uid);
	}
	TQSL_ADIF_GET_FIELD_ERROR stat;
	if (conv->rec_done) {
//cerr << "Getting rec" << endl;
		conv->rec_done = false;
		conv->clearRec();
		int cstat = 0;
		if (conv->adif) {
	 		do {
 				tqsl_adifFieldResults result;
				if (tqsl_getADIFField(conv->adif, &result, &stat, adif_qso_record_fields, notypes, adif_allocate))
					break;
				if (stat != TQSL_ADIF_GET_FIELD_SUCCESS && stat != TQSL_ADIF_GET_FIELD_NO_NAME_MATCH)
					break;
				if (!strcasecmp(result.name, "eor"))
					break;
				if (!strcasecmp(result.name, "CALL") && result.data) {
					strncpy(conv->rec.callsign, (char *)(result.data), sizeof conv->rec.callsign);
				} else if (!strcasecmp(result.name, "BAND") && result.data) {
					strncpy(conv->rec.band, (char *)(result.data), sizeof conv->rec.band);
				} else if (!strcasecmp(result.name, "MODE") && result.data) {
					strncpy(conv->rec.mode, (char *)(result.data), sizeof conv->rec.mode);
				} else if (!strcasecmp(result.name, "FREQ") && result.data) {
					strncpy(conv->rec.freq, (char *)(result.data), sizeof conv->rec.freq);
				} else if (!strcasecmp(result.name, "QSO_DATE") && result.data) {
					cstat = tqsl_initDate(&(conv->rec.date), (const char *)result.data);
				} else if (!strcasecmp(result.name, "TIME_ON") && result.data) {
					cstat = tqsl_initTime(&(conv->rec.time), (const char *)result.data);
				}
				if (stat == TQSL_ADIF_GET_FIELD_SUCCESS) {
					conv->rec_text += string((char *)result.name) + ": ";
					if (result.data)
						conv->rec_text += string((char *)result.data);
					conv->rec_text += "\n";
				}
				if (result.data)
						delete[] result.data;
			} while (cstat == 0);
			if (cstat)
				return 0;
			if (stat == TQSL_ADIF_GET_FIELD_EOF)
				return 0;
			if (stat != TQSL_ADIF_GET_FIELD_SUCCESS) {
				tQSL_ADIF_Error = stat;
				tQSL_Error = TQSL_ADIF_ERROR;
				return 0;
			}
			// ADIF record is complete. See if we need to infer the BAND field.
			if (conv->rec.band[0] == 0) {
				double freq = atof(conv->rec.freq);
				double freq_khz = freq * 1000.0;
				int nband = 0;
				tqsl_getNumBand(&nband);
				for (int i = 0; i < nband; i++) {
					const char *name;
					const char *spectrum;
					int low, high;
					if (tqsl_getBand(i, &name, &spectrum, &low, &high))
						break;
					bool match = false;
					if (!strcmp(spectrum, "HF")) {
						if (freq_khz >= low && freq_khz <= high)
							match = true;
					} else {
						if (freq >= low && freq <= high)
							match = true;
					}
					if (match) {
						strncpy(conv->rec.band, name, sizeof conv->rec.band);
						break;
					}
				}
			}
		} else if (conv->cab) {
			TQSL_CABRILLO_ERROR_TYPE stat;
			do {
				tqsl_cabrilloField field;
				if (tqsl_getCabrilloField(conv->cab, &field, &stat))
					return 0;
				if (stat == TQSL_CABRILLO_NO_ERROR || stat == TQSL_CABRILLO_EOR) {
					// Field found
					if (!strcasecmp(field.name, "CALL")) {
						strncpy(conv->rec.callsign, field.value, sizeof conv->rec.callsign);
					} else if (!strcasecmp(field.name, "BAND")) {
						strncpy(conv->rec.band, field.value, sizeof conv->rec.band);
					} else if (!strcasecmp(field.name, "MODE")) {
						strncpy(conv->rec.mode, field.value, sizeof conv->rec.mode);
					} else if (!strcasecmp(field.name, "FREQ")) {
						strncpy(conv->rec.freq, field.value, sizeof conv->rec.freq);
					} else if (!strcasecmp(field.name, "QSO_DATE")) {
						cstat = tqsl_initDate(&(conv->rec.date), field.value);
					} else if (!strcasecmp(field.name, "TIME_ON")) {
						cstat = tqsl_initTime(&(conv->rec.time), field.value);
					}
					if (conv->rec_text != "")
						conv->rec_text += "\n";
					conv->rec_text += string(field.name) + ": " + field.value;
				}
			} while (stat == TQSL_CABRILLO_NO_ERROR && cstat == 0);
			if (cstat || stat != TQSL_CABRILLO_EOR)
				return 0;
		} else {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strcpy(tQSL_CustomError, "Converter not initialized");
			return 0;
		}
	}
	// Do field value mapping
	tqsl_strtoupper(conv->rec.callsign);
	if (!conv->allow_bad_calls) {
		if (!checkCallSign(conv->rec.callsign)) {
			conv->rec_done = true;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid amateur CALL (%s)", conv->rec.callsign);
			tQSL_Error = TQSL_CUSTOM_ERROR;
			return 0;
		}
	}
	tqsl_strtoupper(conv->rec.band);
	tqsl_strtoupper(conv->rec.mode);
	char val[256] = "";
	tqsl_getADIFMode(conv->rec.mode, val, sizeof val);
	if (val[0] != '\0')
		strncpy(conv->rec.mode, val, sizeof conv->rec.mode);
	// Check field validities
	if (conv->modes.find(conv->rec.mode) == conv->modes.end()) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid MODE (%s)", conv->rec.mode);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (conv->bands.find(conv->rec.band) == conv->bands.end()) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid BAND (%s)", conv->rec.band);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}

	// Check cert
	if (conv->ncerts <= 0) {
		tQSL_Error = TQSL_CERT_NOT_FOUND;
		return 0;
	}

	int cidx = find_matching_cert(conv);
	if (cidx < 0) {
		tQSL_Error = TQSL_CERT_DATE_MISMATCH;
		return 0;
	}
	if (cidx != conv->cert_idx) {
		// Switching certs
		conv->cert_idx = cidx;
		if (!conv->certs_used[conv->cert_idx]) {
			// Need to output tCERT, tSTATION
			conv->need_station_rec = true;
			conv->certs_used[conv->cert_idx] = true;
			return tqsl_getGABBItCERT(conv->certs[conv->cert_idx], conv->cert_idx + conv->base_idx);
		}
	}
	const char *grec = tqsl_getGABBItCONTACT(conv->certs[conv->cert_idx], conv->loc, &(conv->rec),
		conv->cert_idx + conv->base_idx);
	if (grec)
		conv->rec_done = true;
	return grec;
}

DLLEXPORT int
tqsl_getConverterCert(tQSL_Converter convp, tQSL_Cert *certp) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	if (certp == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*certp = conv->certs[conv->cert_idx];
	return 0;
}

DLLEXPORT int
tqsl_getConverterLine(tQSL_Converter convp, int *lineno) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	if (lineno == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (conv->cab)
		return tqsl_getCabrilloLine(conv->cab, lineno);
	else if (conv->adif)
		return tqsl_getADIFLine(conv->adif, lineno);
	*lineno = 0;
	return 0;
}

DLLEXPORT const char *
tqsl_getConverterRecordText(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 0;
	return conv->rec_text.c_str();
}

DLLEXPORT int
tqsl_setConverterAllowBadCall(tQSL_Converter convp, int allow) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	conv->allow_bad_calls = (allow != 0);
	return 0;
}

static bool
hasValidCallSignChars(const string& call) {
	// Check for invalid characters
	if (call.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/") != string::npos)
		return false;
	// Need at least one letter
	if (call.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == string::npos)
		return false;
	// Need at least one number
	if (call.find_first_of("0123456789") == string::npos)
		return false;
	return true;
}

static bool
checkCallSign(const string& call) {
	if (!hasValidCallSignChars(call))
		return false;
	if (call.length() < 3)
		return false;
	string::size_type idx, newidx;
	for (idx = 0; idx != string::npos; idx = newidx+1) {
		string s;
		newidx = call.find('/', idx);
		if (newidx == string::npos)
			s = call.substr(idx);
		else
			s = call.substr(idx, newidx - idx);
		if (s.length() == 0)
			return false;	// Leading or trailing '/' is bad, bad!
		if (newidx == string::npos)
			break;
	}
	return true;
}
