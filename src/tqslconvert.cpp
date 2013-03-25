/***************************************************************************
                          tqslconvert.cpp  -  description
                             -------------------
    begin                : Sun Nov 17 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/


#define TQSLLIB_DEF

#include "tqsllib.h"

#include "tqslconvert.h"
#include <stdio.h>
#include <errno.h>
#include "tqslerrno.h"
#include <cstring>
#include <string>
#include <vector>
#include <ctype.h>
#include <set>
#include <db.h>

#include <locale.h>
//#include <iostream>

#include "winstrdefs.h"

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
	set <string> propmodes;
	set <string> satellites;
	string rec_text;
	tQSL_Date start, end;
	DB *seendb;
	DB_ENV* dbenv;
	DB_TXN* txn;
	char serial[512];
	bool allow_dupes;
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
	allow_dupes = true; //by default, don't change existing behavior (also helps with commit)
	memset(&rec, 0, sizeof rec);
	memset(&start, 0, sizeof start);
	memset(&end, 0, sizeof end);
	seendb = NULL;
	dbenv = NULL;
	txn = NULL;
	memset(&serial, 0, sizeof serial);
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
	// Init the propagation mode data
	n = 0;
	tqsl_getNumPropagationMode(&n);
	for (int i = 0; i < n; i++) {
		val = 0;
		tqsl_getPropagationMode(i, &val, 0);
		if (val)
			propmodes.insert(val);
	}
	// Init the satellite data
	n = 0;
	tqsl_getNumSatellite(&n);
	for (int i = 0; i < n; i++) {
		val = 0;
		tqsl_getSatellite(i, &val, 0, 0, 0);
		if (val)
			satellites.insert(val);
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
	{ "CALL", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_CALLSIGN_MAX, 0, 0, NULL },
	{ "BAND", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_BAND_MAX, 0, 0, NULL },
	{ "MODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_MODE_MAX, 0, 0, NULL },
	{ "QSO_DATE", "", TQSL_ADIF_RANGE_TYPE_NONE, 10, 0, 0, NULL },
	{ "TIME_ON", "", TQSL_ADIF_RANGE_TYPE_NONE, 10, 0, 0, NULL },
	{ "FREQ", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_FREQ_MAX, 0, 0, NULL },
	{ "FREQ_RX", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_FREQ_MAX, 0, 0, NULL },
	{ "BAND_RX", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_BAND_MAX, 0, 0, NULL },
	{ "SAT_NAME", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_SATNAME_MAX, 0, 0, NULL },
	{ "PROP_MODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_PROPMODE_MAX, 0, 0, NULL },
	{ "eor", "", TQSL_ADIF_RANGE_TYPE_NONE, 0, 0, 0, NULL },
};

DLLEXPORT int CALLCONVENTION
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

DLLEXPORT int CALLCONVENTION
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

DLLEXPORT int CALLCONVENTION
tqsl_endConverter(tQSL_Converter *convp) {
	if (!convp || CAST_TQSL_CONVERTER(*convp) == 0)
		return 0;

	TQSL_CONVERTER* conv;

	if ((conv = check_conv(*convp))) {
		if (conv->txn) conv->txn->abort(conv->txn);
		if (conv->seendb) conv->seendb->close(conv->seendb, 0);
		if (conv->dbenv) conv->dbenv->close(conv->dbenv, 0);
		// close files and clean up converters, if any
		if (conv->adif) tqsl_endADIF(&conv->adif);
		if (conv->cab) tqsl_endCabrillo(&conv->cab);
	}


	
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

static const char *
tqsl_infer_band(const char* infreq) {
	setlocale(LC_NUMERIC, "C");
	double freq = atof(infreq);
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
		if (match)
			 return name;
	}
	return "";
}

DLLEXPORT int CALLCONVENTION
tqsl_setADIFConverterDateFilter(tQSL_Converter convp, tQSL_Date *start, tQSL_Date *end) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	if (start == NULL)
		conv->start.year = conv->start.month = conv->start.day = 0;
	else
		conv->start = *start;
	if (end == NULL)
		conv->end.year = conv->end.month = conv->end.day = 0;
	else
		conv->end = *end;
	return 0;
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getConverterGABBI(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;
	char signdata[1024];

	if (!(conv = check_conv(convp)))
		return 0;
	if (conv->need_station_rec) {
		int uid = conv->cert_idx + conv->base_idx;
		conv->need_station_rec = false;
		const char *tStation = tqsl_getGABBItSTATION(conv->loc, uid, uid);
		tqsl_getCertificateSerialExt(conv->certs[conv->cert_idx], conv->serial, sizeof(conv->serial));
		return tStation;
	}
	if (!conv->allow_dupes && !conv->seendb) {
		bool dbinit_cleanup=false;
		string fixedpath=tQSL_BaseDir; //must be first because of gotos
		size_t found=fixedpath.find('\\');

		if (db_env_create(&conv->dbenv, 0)) {
			// can't make env handle
			dbinit_cleanup=true;
			goto dbinit_end;
		}

		//bdb complains about \\s in path on windows... 

		while (found!=string::npos) {
			fixedpath.replace(found, 1, "/");
			found=fixedpath.find('\\');
		}

		if (conv->dbenv->open(conv->dbenv, fixedpath.c_str(), DB_INIT_TXN|DB_INIT_LOCK|DB_INIT_LOG|DB_INIT_MPOOL|DB_CREATE|DB_RECOVER, 0600)) {
			// can't open environment
			dbinit_cleanup=true;
			goto dbinit_end;
		}

		if (db_create(&conv->seendb, conv->dbenv, 0)) {
			// can't create db
			dbinit_cleanup=true;
			goto dbinit_end;
		}

#ifndef DB_TXN_BULK
#define DB_TXN_BULK 0
#endif
		if (conv->dbenv->txn_begin(conv->dbenv, NULL, &conv->txn, DB_TXN_BULK)) {
			// can't start a txn
			dbinit_cleanup=true;
			goto dbinit_end;
		}

		if (conv->seendb->open(conv->seendb, conv->txn, "duplicates.db", NULL, DB_BTREE, DB_CREATE, 0600)) {
			// can't open the db
			dbinit_cleanup=true;
			goto dbinit_end;
		}


dbinit_end:
		if (dbinit_cleanup) {
			tQSL_Error = TQSL_DB_ERROR;
			tQSL_Errno = errno;
			if (conv->txn) conv->txn->abort(conv->txn);
			if (conv->seendb) conv->seendb->close(conv->seendb, 0);
			if (conv->dbenv) conv->dbenv->close(conv->dbenv, 0);
			return 0;
		}
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
				} else if (!strcasecmp(result.name, "FREQ") && result.data) {
					strncpy(conv->rec.freq, (char *)(result.data), sizeof conv->rec.freq);
				} else if (!strcasecmp(result.name, "FREQ_RX") && result.data) {
					strncpy(conv->rec.rxfreq, (char *)(result.data), sizeof conv->rec.rxfreq);
				} else if (!strcasecmp(result.name, "BAND_RX") && result.data) {
					strncpy(conv->rec.rxband, (char *)(result.data), sizeof conv->rec.rxband);
				} else if (!strcasecmp(result.name, "SAT_NAME") && result.data) {
					strncpy(conv->rec.satname, (char *)(result.data), sizeof conv->rec.satname);
				} else if (!strcasecmp(result.name, "PROP_MODE") && result.data) {
					strncpy(conv->rec.propmode, (char *)(result.data), sizeof conv->rec.propmode);
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
			// ADIF record is complete. See if we need to infer the BAND fields.
			if (conv->rec.band[0] == 0)
				strncpy(conv->rec.band, tqsl_infer_band(conv->rec.freq), sizeof conv->rec.band);
			if (conv->rec.rxband[0] == 0)
				strncpy(conv->rec.rxband, tqsl_infer_band(conv->rec.rxfreq), sizeof conv->rec.rxband);
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
	// Check QSO date against user-specified date range.
	if (tqsl_isDateValid(&(conv->rec.date))) {
		if (tqsl_isDateValid(&(conv->start)) && tqsl_compareDates(&(conv->rec.date), &(conv->start)) < 0) {
			conv->rec_done = true;
			tQSL_Error = TQSL_DATE_OUT_OF_RANGE;
			return 0;
		}
		if (tqsl_isDateValid(&(conv->end)) && tqsl_compareDates(&(conv->rec.date), &(conv->end)) > 0) {
			conv->rec_done = true;
			tQSL_Error = TQSL_DATE_OUT_OF_RANGE;
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
	if (conv->rec.propmode[0] != '\0'
		&& conv->propmodes.find(conv->rec.propmode) == conv->propmodes.end()) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid PROP_MODE (%s)", conv->rec.propmode);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (conv->rec.satname[0] != '\0'
		&& conv->satellites.find(conv->rec.satname) == conv->satellites.end()) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid SAT_NAME (%s)", conv->rec.satname);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (!strcmp(conv->rec.propmode, "SAT") && conv->rec.satname[0] == '\0') {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "PROP_MODE = 'SAT' but no SAT_NAME");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (strcmp(conv->rec.propmode, "SAT") && conv->rec.satname[0] != '\0') {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "SAT_NAME set but PROP_MODE is not 'SAT'");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}

	// Check cert
	if (conv->ncerts <= 0) {
		conv->rec_done = true;
		tQSL_Error = TQSL_CERT_NOT_FOUND;
		return 0;
	}

	int cidx = find_matching_cert(conv);
	if (cidx < 0) {
		conv->rec_done = true;
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
	const char *grec = tqsl_getGABBItCONTACTData(conv->certs[conv->cert_idx], conv->loc, &(conv->rec),
		conv->cert_idx + conv->base_idx, signdata, sizeof(signdata));
	if (grec) {
		conv->rec_done = true;
		if (!conv->allow_dupes) {
			// Lookup uses signdata and cert serial number
			DBT dbkey, dbdata;
			char temp[2];
			memset(&dbkey, 0, sizeof dbkey);
			memset(&dbdata, 0, sizeof dbdata);
			// append signing key serial
			strncat(signdata, conv->serial, sizeof(signdata) - strlen(signdata)-1);
			dbkey.size = strlen(signdata);
			dbkey.data = signdata;
			dbdata.size = sizeof(temp);
			dbdata.data = temp;
			int dbget_err = conv->seendb->get(conv->seendb, conv->txn, &dbkey, &dbdata, 0);
			if (0 == dbget_err) {
				//lookup was successful; thus duplicate
				tQSL_Error = TQSL_DUPLICATE_QSO;
				return 0;
			} else if (dbget_err != DB_NOTFOUND) {
				//non-zero return, but not "not found" - thus error
				tQSL_Error = TQSL_DB_ERROR;
				return 0;
				// could be more specific but there's very little the user can do at this point anyway
			}
			temp[0] = 'D';
			dbdata.size = 1;
			if (0 != conv->seendb->put(conv->seendb, conv->txn, &dbkey, &dbdata, 0)) {
				tQSL_Error = TQSL_DB_ERROR;
				return 0;
			}
			
		}
	}
	return grec;
}

DLLEXPORT int CALLCONVENTION
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

DLLEXPORT int CALLCONVENTION
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

DLLEXPORT const char* CALLCONVENTION
tqsl_getConverterRecordText(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 0;
	return conv->rec_text.c_str();
}

DLLEXPORT int CALLCONVENTION
tqsl_setConverterAllowBadCall(tQSL_Converter convp, int allow) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	conv->allow_bad_calls = (allow != 0);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setConverterAllowDuplicates(tQSL_Converter convp, int allow) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	conv->allow_dupes = (allow != 0);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_converterRollBack(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;

	if (!(conv = check_conv(convp)))
		return 1;
	if (!conv->seendb)
		return 1;
	if (conv->txn)
		conv->txn->abort(conv->txn);
	conv->txn = NULL;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_converterCommit(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;

	if (!(conv = check_conv(convp)))
		return 1;
	if (!conv->seendb)
		return 1;
	if (conv->txn)
		conv->txn->commit(conv->txn, 0);
	conv->txn=NULL;
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
