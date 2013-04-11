/***************************************************************************
                          cabrillo.cpp  -  description
                             -------------------
    begin                : Thu Dec 5 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/


#define TQSLLIB_DEF

#include "tqsllib.h"
#include "tqslerrno.h"
#include <cstdio>
#include <cstring>
#include <ctype.h>
#include <errno.h>

#include "winstrdefs.h"

#define TQSL_CABRILLO_MAX_RECORD_LENGTH 120

DLLEXPORTDATA TQSL_CABRILLO_ERROR_TYPE tQSL_Cabrillo_Error;

static char errmsgbuf[256];
static char errmsgdata[40];

struct TQSL_CABRILLO;

static int freq_to_band(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp);
static int mode_xlat(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp);
static int time_fixer(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp);

struct cabrillo_field_def {
	const char *name;
	int loc;
	int (*process)(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp);
};

static cabrillo_field_def cabrillo_dummy[] = {
	{ "CALL", 6, 0 },
	{ "BAND", 0, freq_to_band },
	{ "MODE", 1, mode_xlat },
	{ "QSO_DATE", 2, 0 },
	{ "TIME_ON", 3, time_fixer },
	{ "FREQ", 0, 0 },
	{ "MYCALL", 4, 0 },
};

/*

// Cabrillo QSO template specs

// Call in field 6
static cabrillo_field_def cabrillo_c6[] = {
	{ "BAND", 0, freq_to_band },
	{ "MODE", 1, mode_xlat },
	{ "QSO_DATE", 2, 0 },
	{ "TIME_ON", 3, time_fixer },
	{ "CALL", 6, 0 },
	{ "FREQ", 0, 0 },
	{ "MYCALL", 4, 0 },
};

// Call in field 7
static cabrillo_field_def cabrillo_c7[] = {
	{ "BAND", 0, freq_to_band },
	{ "MODE", 1, mode_xlat },
	{ "QSO_DATE", 2, 0 },
	{ "TIME_ON", 3, time_fixer },
	{ "CALL", 7, 0 },
	{ "FREQ", 0, 0 },
	{ "MYCALL", 4, 0 },
};

// Call in field 8
static cabrillo_field_def cabrillo_c8[] = {
	{ "BAND", 0, freq_to_band },
	{ "MODE", 1, mode_xlat },
	{ "QSO_DATE", 2, 0 },
	{ "TIME_ON", 3, time_fixer },
	{ "CALL", 8, 0 },
	{ "FREQ", 0, 0 },
	{ "MYCALL", 4, 0 },
};

// Call in field 9
static cabrillo_field_def cabrillo_c9[] = {
	{ "BAND", 0, freq_to_band },
	{ "MODE", 1, mode_xlat },
	{ "QSO_DATE", 2, 0 },
	{ "TIME_ON", 3, time_fixer },
	{ "CALL", 9, 0 },
	{ "FREQ", 0, 0 },
	{ "MYCALL", 4, 0 },
};

*/

struct cabrillo_contest {
	char *contest_name;
	TQSL_CABRILLO_FREQ_TYPE type;
	cabrillo_field_def *fields;
	int nfields;
};

/*
static cabrillo_contest cabrillo_contests[] = {
	// ARRL SS
	{ "ARRL-SS-CW", TQSL_CABRILLO_HF, cabrillo_c9, sizeof cabrillo_c9 / sizeof cabrillo_c9[0] },
	{ "ARRL-SS-SSB", TQSL_CABRILLO_HF, cabrillo_c9, sizeof cabrillo_c9 / sizeof cabrillo_c9[0] },
	// Other ARRL HF
	{ "ARRL-DX-CW", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "ARRL-DX-SSB", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "ARRL-10", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "ARRL-160", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "ARRL-RTTY", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "IARU-HF", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	// CQ HF
	{ "CQ-WW-CW", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "CQ-WW-SSB", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "CQ-160-CW", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "CQ-160-SSB", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "CQ-WPX-CW", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "CQ-WPX-RTTY", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "CQ-WPX-SSB", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	// WAE
	{ "WAEDC", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "DARC-WAEDC-CW", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "DARC-WAEDC-RTTY", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "DARC-WAEDC-SSB", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	// NA/NAQP
	{ "NAQP-CW", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "NAQP-RTTY", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "NAQP-SSB", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "NA-SPRINT-CW", TQSL_CABRILLO_HF, cabrillo_c8, sizeof cabrillo_c8 / sizeof cabrillo_c8[0] },
	{ "NA-SPRINT-SSB", TQSL_CABRILLO_HF, cabrillo_c8, sizeof cabrillo_c8 / sizeof cabrillo_c8[0] },
	// Other HF
	{ "STEW-PERRY", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "OCEANIA-DX-CW", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "OCEANIA-DX-SSB", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "AP-SPRINT", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "NEQP", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "TARA-RTTY", TQSL_CABRILLO_HF, cabrillo_c8, sizeof cabrillo_c8 / sizeof cabrillo_c8[0] },
	{ "SAC-CW", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	{ "SAC-SSB", TQSL_CABRILLO_HF, cabrillo_c7, sizeof cabrillo_c7 / sizeof cabrillo_c7[0] },
	// Misc HF
	{ "BARTG-RTTY", TQSL_CABRILLO_HF, cabrillo_c8, sizeof cabrillo_c8 / sizeof cabrillo_c8[0] },
	{ "RSGB-IOTA", TQSL_CABRILLO_HF, cabrillo_c8, sizeof cabrillo_c8 / sizeof cabrillo_c8[0] },
	// ARRL/CQ VHF
	{ "ARRL-UHF-AUG", TQSL_CABRILLO_VHF, cabrillo_c6, sizeof cabrillo_c6 / sizeof cabrillo_c6[0] },
	{ "ARRL-VHF-JAN", TQSL_CABRILLO_VHF, cabrillo_c6, sizeof cabrillo_c6 / sizeof cabrillo_c6[0] },
	{ "ARRL-VHF-JUN", TQSL_CABRILLO_VHF, cabrillo_c6, sizeof cabrillo_c6 / sizeof cabrillo_c6[0] },
	{ "ARRL-VHF-SEP", TQSL_CABRILLO_VHF, cabrillo_c6, sizeof cabrillo_c6 / sizeof cabrillo_c6[0] },
	{ "CQ-VHF", TQSL_CABRILLO_VHF, cabrillo_c6, sizeof cabrillo_c6 / sizeof cabrillo_c6[0] },
};
*/

struct TQSL_CABRILLO {
	int sentinel;
	FILE *fp;
	char *filename;
	cabrillo_contest *contest;
	int field_idx;
	char rec[TQSL_CABRILLO_MAX_RECORD_LENGTH+1];
	char *datap;
	int line_no;
	char *fields[12];
};

#define CAST_TQSL_CABRILLO(p) ((struct TQSL_CABRILLO *)p)

static TQSL_CABRILLO *
check_cabrillo(tQSL_Cabrillo cabp) {
	if (tqsl_init())
		return 0;
	if (cabp == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 0;
	}
	if (CAST_TQSL_CABRILLO(cabp)->sentinel != 0x2449)
		return 0;
	return CAST_TQSL_CABRILLO(cabp);
}

static char *
tqsl_parse_cabrillo_record(char *rec) {
	char *cp = strchr(rec, ':');
	if (!cp)
		return 0;
	*cp++ = 0;
	if (strlen(rec) > TQSL_CABRILLO_FIELD_NAME_LENGTH_MAX)
		return 0;
	while (isspace(*cp))
		cp++;
	char *sp;
	if ((sp = strchr(cp, '\r')) != 0)
		*sp = '\0';
	if ((sp = strchr(cp, '\n')) != 0)
		*sp = '\0';
	for (sp = cp + strlen(cp); sp != cp; ) {
		sp--;
		if (isspace(*sp))
			*sp = '\0';
		else
			break;
	}
	for (sp = rec; *sp; sp++)
		*sp = toupper(*sp);
	return cp;
}

static int
freq_to_band(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp) {
	if (!strcasecmp(fp->value, "light")) {
		strcpy(fp->value, "SUBMM");
		return 0;
	}
	int freq = atoi(fp->value);
	const char *band = 0;
	if (cab->contest->type == TQSL_CABRILLO_HF) {
		if (freq < 30) {
			// Handle known CT misbehavior
			if (freq == 7)
				band = "40M";
			if (freq == 14)
				band = "20M";
			if (freq == 21)
				band = "15M";
			if (freq == 28)
				band = "10M";
		} else if (freq >= 1800 && freq <= 2000)
			band = "160M";
		else if (freq >= 3500 && freq <= 4000)
			band = "80M";
		else if (freq >= 7000 && freq <= 7300)
			band = "40M";
		else if (freq >= 10100 && freq <= 10150)
			band = "30M";
		else if (freq >= 14000 && freq <= 14350)
			band = "20M";
		else if (freq >= 18068 && freq <= 18168)
			band = "17M";
		else if (freq >= 21000 && freq <= 21450)
			band = "15M";
		else if (freq >= 24890 && freq <= 24990)
			band = "12M";
		else if (freq >= 28000 && freq <= 29700)
			band = "10M";
	} else {
		// VHF+
		if (freq == 50)
			band = "6M";
		else if (freq == 144)
			band = "2M";
		else if (freq == 222)
			band = "1.25M";
		else if (freq == 432)
			band = "70CM";
		else if (freq == 903)
			band = "33CM";
		else if (!strncmp(fp->value, "1.2", 3))
			band = "23CM";
		else if (!strncmp(fp->value, "2.3", 3))
			band = "13CM";
		else if (!strncmp(fp->value, "3.4", 3))
			band = "9CM";
		else if (!strncmp(fp->value, "5.7", 3))
			band = "6CM";
		else if (freq == 10)
			band = "3CM";
		else if (freq == 24)
			band = "1.25CM";
		else if (freq == 47)
			band = "6MM";
		else if (freq == 76)
			band = "4MM";
		else if (freq == 119)
			band = "2.5MM";
		else if (freq == 142)
			band = "2MM";
		else if (freq == 242)
			band = "1MM";
		else if (freq == 300)
			band = "SUBMM";
	}
	if (band == 0)
		return 1;
	strcpy(fp->value, band);
	return 0;
}

static int
mode_xlat(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp) {
	static struct {
		const char *cmode;
		const char *gmode;
	} modes[] = {
		{"CW", "CW"}, {"PH", "Phone"}, {"FM", "Phone"}, {"RY", "RTTY"}
	};
	for (int i = 0; i < int(sizeof modes / sizeof modes[0]); i++) {
		if (!strcasecmp(fp->value, modes[i].cmode)) {
			strcpy(fp->value, modes[i].gmode);
			return 0;
		}
	}
	return 1;
}

static int
time_fixer(TQSL_CABRILLO *cab, tqsl_cabrilloField *fp) {
	if (strlen(fp->value) == 0)
		return 0;
	char *cp;
	for (cp = fp->value; *cp; cp++)
		if (!isdigit(*cp))
			break;
	if (*cp)
		return 1;
	sprintf(fp->value, "%04d", atoi(fp->value));
	return 0;
}

static void
tqsl_free_cabrillo_contest(struct cabrillo_contest *c) {
		if (c->contest_name)
			free(c->contest_name);
		if (c->fields)
			free(c->fields);
		free(c);
}

static struct cabrillo_contest *
tqsl_new_cabrillo_contest(const char *contest_name, int call_field, int contest_type) {
	cabrillo_contest *c = (cabrillo_contest *)calloc(1, sizeof (struct cabrillo_contest));
	if (c == NULL)
		return NULL;
	if ((c->contest_name = (char *)malloc(strlen(contest_name)+1)) == NULL) {
		tqsl_free_cabrillo_contest(c);
		return NULL;
	}
	strcpy(c->contest_name, contest_name);
	c->type = (TQSL_CABRILLO_FREQ_TYPE)contest_type;
	if ((c->fields = (struct cabrillo_field_def *)calloc(1, sizeof cabrillo_dummy)) == NULL) {
		tqsl_free_cabrillo_contest(c);
		return NULL;
	}
	memcpy(c->fields, cabrillo_dummy, sizeof cabrillo_dummy);
	c->fields[0].loc = call_field-1;
	c->nfields = sizeof cabrillo_dummy / sizeof cabrillo_dummy[0];
	return c;
}

static void
tqsl_free_cab(struct TQSL_CABRILLO *cab) {
	if (!cab || cab->sentinel != 0x2449)
		return;
	cab->sentinel = 0;
	if (cab->filename)
		free(cab->filename);
	if (cab->fp)
		fclose(cab->fp);
	if (cab->contest)
		tqsl_free_cabrillo_contest(cab->contest);
	free(cab);
}

DLLEXPORT int CALLCONVENTION
tqsl_beginCabrillo(tQSL_Cabrillo *cabp, const char *filename) {
	TQSL_CABRILLO_ERROR_TYPE terrno;
	if (filename == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	struct TQSL_CABRILLO *cab;
	cab = (struct TQSL_CABRILLO *)calloc(1, sizeof (struct TQSL_CABRILLO));
	if (cab == NULL) {
		tQSL_Error = TQSL_ALLOC_ERROR;
		goto err;
	}
	cab->sentinel = 0x2449;
	cab->field_idx = -1;
	if ((cab->fp = fopen(filename, "r")) == NULL) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		goto err;
	}
	char *cp;
	terrno = TQSL_CABRILLO_NO_START_RECORD;
	while ((cp = fgets(cab->rec, sizeof cab->rec, cab->fp)) != 0) {
		cab->line_no++;
		if (tqsl_parse_cabrillo_record(cab->rec) != 0
			&& !strcmp(cab->rec, "START-OF-LOG"))
			break;
	}
	if (cp != 0) {
		terrno = TQSL_CABRILLO_NO_CONTEST_RECORD;
		while ((cp = fgets(cab->rec, sizeof cab->rec, cab->fp)) != 0) {
			cab->line_no++;
			char *vp;
			if ((vp = tqsl_parse_cabrillo_record(cab->rec)) != 0
				&& !strcmp(cab->rec, "CONTEST")
				&& strtok(vp, " \t\r\n") != 0) {
				terrno = TQSL_CABRILLO_UNKNOWN_CONTEST;
				int callfield, contest_type;
				if (tqsl_getCabrilloMapEntry(vp, &callfield, &contest_type)) {
					tqsl_free_cab(cab);
					return 1;
				}
				if (callfield != 0)
					cab->contest = tqsl_new_cabrillo_contest(vp, callfield, contest_type);
/*				for (int i = 0; i < int(sizeof cabrillo_contests / sizeof cabrillo_contests[0]); i++) {
					if (!strcasecmp(vp, cabrillo_contests[i].contest_name)) {
						cab->contest = cabrillo_contests + i;
						break;
					}
				}
*/
				if (cab->contest == 0) {
					strncpy(errmsgdata, vp, sizeof errmsgdata);
					cp = 0;
				}
				break;
			}
		}
	}
	if (cp == 0) {
		if (ferror(cab->fp)) {
			tQSL_Error = TQSL_SYSTEM_ERROR;
			tQSL_Errno = errno;
			goto err;
		}
		tQSL_Cabrillo_Error = terrno;
		tQSL_Error = TQSL_CABRILLO_ERROR;
		goto err;
	}
	if ((cab->filename = (char *)malloc(strlen(filename)+1)) == NULL) {
		tQSL_Error = TQSL_ALLOC_ERROR;
		goto err;
	}
	strcpy(cab->filename, filename);
	*((struct TQSL_CABRILLO **)cabp) = cab;
	return 0;
err:
	strncpy(tQSL_ErrorFile, filename, sizeof tQSL_ErrorFile);
	tQSL_ErrorFile[sizeof tQSL_ErrorFile-1] = 0;
	tqsl_free_cab(cab);
	return 1;

}

DLLEXPORT int CALLCONVENTION
tqsl_endCabrillo(tQSL_Cabrillo *cabp) {
	TQSL_CABRILLO *cab;
	if (cabp == 0)
		return 0;
	cab = CAST_TQSL_CABRILLO(*cabp);
	if (!cab || cab->sentinel != 0x2449)
		return 0;
	tqsl_free_cab(cab);
	*cabp = 0;
	return 0;
}

DLLEXPORT const char* CALLCONVENTION
tqsl_cabrilloGetError(TQSL_CABRILLO_ERROR_TYPE err) {
	const char *msg = 0;
	switch (err) {
		case TQSL_CABRILLO_NO_ERROR:
			msg = "Cabrillo success";
			break;
		case TQSL_CABRILLO_EOF:
			msg = "Cabrillo end-of-file";
			break;
		case TQSL_CABRILLO_EOR:
			msg = "Cabrillo end-of-record";
			break;
		case TQSL_CABRILLO_NO_START_RECORD:
			msg = "Cabrillo missing START-OF-LOG record";
			break;
		case TQSL_CABRILLO_NO_CONTEST_RECORD:
			msg = "Cabrillo missing CONTEST record";
			break;
		case TQSL_CABRILLO_UNKNOWN_CONTEST:
			snprintf(errmsgbuf, sizeof errmsgbuf, "Cabrillo unknown CONTEST: %s", errmsgdata);
			msg = errmsgbuf;
			break;
		case TQSL_CABRILLO_BAD_FIELD_DATA:
			snprintf(errmsgbuf, sizeof errmsgbuf, "Cabrillo field data error in %s field", errmsgdata);
			msg = errmsgbuf;
			break;
	}
	if (!msg) {
		snprintf(errmsgbuf, sizeof errmsgbuf, "Cabrillo unknown error: %d", err);
		if (errmsgdata[0] != '\0')
			snprintf(errmsgbuf + strlen(errmsgbuf), sizeof errmsgbuf - strlen(errmsgbuf),
				" (%s)", errmsgdata);
		msg = errmsgbuf;
	}
	errmsgdata[0] = '\0';
	return msg;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCabrilloField(tQSL_Cabrillo cabp, tqsl_cabrilloField *field, TQSL_CABRILLO_ERROR_TYPE *error) {
	TQSL_CABRILLO *cab;
	cabrillo_field_def *fp;
	const char *fielddat;

	if ((cab = check_cabrillo(cabp)) == 0)
		return 1;
	if (field == 0 || error == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (cab->datap == 0 || cab->field_idx < 0 || cab->field_idx >= cab->contest->nfields) {
		char *cp;
		while ((cp = fgets(cab->rec, sizeof cab->rec, cab->fp)) != 0) {
			cab->line_no++;
			cab->datap = tqsl_parse_cabrillo_record(cab->rec);
			if (cab->datap != 0) {
				if (!strcasecmp(cab->rec, "QSO")) {
					cab->field_idx = 0;
					char *fieldp = strtok(cab->datap, " \t\r\n");
					memset(cab->fields, 0, sizeof cab->fields);
					for (int i = 0; fieldp && i < int(sizeof cab->fields / sizeof cab->fields[0]); i++) {
						cab->fields[i] = fieldp;
						fieldp = strtok(0, " \t\r\n");
					}
					break;
				} else if (!strcasecmp(cab->rec, "END-OF-LOG")) {
					*error = TQSL_CABRILLO_EOF;
					return 0;
				}
			}
		}
		if (cp == 0) {
			if (ferror(cab->fp)) {
				tQSL_Error = TQSL_SYSTEM_ERROR;
				tQSL_Errno = errno;
				goto err;
			} else {
				*error = TQSL_CABRILLO_EOF;
				return 0;
			}
		}
	}
	// Record data is okay and field index is valid.
		
   	fp = cab->contest->fields + cab->field_idx;
   	strncpy(field->name, fp->name, sizeof field->name);
	fielddat = cab->fields[fp->loc];
	if (fielddat == 0) {
		tQSL_Cabrillo_Error = TQSL_CABRILLO_BAD_FIELD_DATA;
		tQSL_Error = TQSL_CABRILLO_ERROR;
		strncpy(errmsgdata, field->name, sizeof errmsgdata);
		goto err;
	}
   	strncpy(field->value, fielddat, sizeof field->value);

/*
	int idx;
	for (idx = fp->start-1; idx < fp->end; idx++)
		if (!isspace(cab->rec[idx]))
			break;
	if (idx < fp->end)
	   	strncpy(field->value, cab->rec + idx, fp->end - idx);
	field->value[fp->end - idx] = '\0';
	if (strlen(field->value) != 0) {
		char *sp = field->value + strlen(field->value);
		do {
			--sp;
			if (!isspace(*sp))
				break;
			*sp = 0;
		} while (sp != field->value);
	}
*/

	if (fp->process && fp->process(cab, field)) {
		tQSL_Cabrillo_Error = TQSL_CABRILLO_BAD_FIELD_DATA;
		tQSL_Error = TQSL_CABRILLO_ERROR;
		strncpy(errmsgdata, field->name, sizeof errmsgdata);
		goto err;
	}
   	cab->field_idx++;
   	if (cab->field_idx >= cab->contest->nfields)
   		*error = TQSL_CABRILLO_EOR;
	else
		*error = TQSL_CABRILLO_NO_ERROR;
	return 0;
err:
	strncpy(tQSL_ErrorFile, cab->filename, sizeof tQSL_ErrorFile);
	tQSL_ErrorFile[sizeof tQSL_ErrorFile-1] = 0;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCabrilloContest(tQSL_Cabrillo cabp, char *buf, int bufsiz) {
	TQSL_CABRILLO *cab;
	if ((cab = check_cabrillo(cabp)) == 0)
		return 1;
	if (buf == 0 || bufsiz <= 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (bufsiz < (int)strlen(cab->contest->contest_name)+1) {
		tQSL_Error = TQSL_BUFFER_ERROR;
		return 1;
	}
	strcpy(buf, cab->contest->contest_name);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCabrilloFreqType(tQSL_Cabrillo cabp, TQSL_CABRILLO_FREQ_TYPE *type) {
	TQSL_CABRILLO *cab;
	if ((cab = check_cabrillo(cabp)) == 0)
		return 1;
	if (type == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*type = cab->contest->type;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCabrilloLine(tQSL_Cabrillo cabp, int *lineno) {
	TQSL_CABRILLO *cab;
	if ((cab = check_cabrillo(cabp)) == 0)
		return 1;
	if (lineno == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*lineno = cab->line_no;
	return 0;
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getCabrilloRecordText(tQSL_Cabrillo cabp) {
	TQSL_CABRILLO *cab;
	if ((cab = check_cabrillo(cabp)) == 0)
		return 0;
	return cab->rec;
}
