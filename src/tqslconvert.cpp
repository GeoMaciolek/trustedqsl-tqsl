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
#include <sys/stat.h>
#include <signal.h>
#include "tqslerrno.h"
#include <cstring>
#include <string>
#include <ctype.h>
#include <set>
#include <db.h>

#include <locale.h>
//#include <iostream>

#ifndef _WIN32
    #include <unistd.h>
    #include <dirent.h>
#else
    #include <direct.h>
    #include "windirent.h"
#endif

#include "winstrdefs.h"

using std::set;
using std::string;

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
	char *dbpath;
	DB_ENV* dbenv;
	DB_TXN* txn;
	DBC* cursor;
	FILE* errfile;
	char serial[512];
	char callsign[64];
	bool allow_dupes;
	bool need_ident_rec;
	char *appName;
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
	dbpath = NULL;
	dbenv = NULL;
	txn = NULL;
	cursor = NULL;
	errfile = NULL;
	memset(&serial, 0, sizeof serial);
	memset(&callsign, 0, sizeof callsign);
	appName = NULL;
	need_ident_rec = true;
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

using tqsllib::TQSL_CONVERTER;

static char * fix_freq(const char *in) {
    static char out[128];
    const char *p = in;
    bool decimal = false;
    char *o = out;
    while (*p) {
	if (*p == '.') {
		if (decimal) {
			p++;
			continue;
		}
		decimal = true;
	}
	*o++ = *p++;
    }
    *o = '\0';
    return out;
}

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
	{ "SUBMODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_MODE_MAX, 0, 0, NULL },
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
tqsl_beginConverter(tQSL_Converter *convp) {
	tqslTrace("tqsl_beginConverter", NULL);
	if (tqsl_init())
		return 0;
	if (!convp) {
		tqslTrace("tqsl_beginConverter", "convp=NULL");
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_CONVERTER *conv = new TQSL_CONVERTER();
	*convp = conv;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_beginADIFConverter(tQSL_Converter *convp, const char *filename, tQSL_Cert *certs,
	int ncerts, tQSL_Location loc) {
	tqslTrace("tqsl_beginADIFConverter", NULL);
	if (tqsl_init())
		return 0;
	if (!convp || !filename) {
		tqslTrace("tqsl_beginADIFConverter", "arg err convp=0x%lx filename=0x%lx certs=0x%lx", convp, filename, certs);
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	tQSL_ADIF adif;
	if (tqsl_beginADIF(&adif, filename)) {
		tqslTrace("tqsl_beginADIFConverter", "tqsl_beginADIF fail %d", tQSL_Error);
		return 1;
	}
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
	tqslTrace("tqsl_beginCabrilloConverter", NULL);

	if (tqsl_init())
		return 0;
	if (!convp || !filename) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		tqslTrace("tqsl_beginCabrilloConverter", "arg error convp=0x%lx, filename=0x%lx, certs=0x%lx", convp, filename, certs);
		return 1;
	}
	tQSL_Cabrillo cab;
	if (tqsl_beginCabrillo(&cab, filename)) {
		tqslTrace("tqsl_beginCabrilloConverter", "tqsl_beginCabrillo fail %d", tQSL_Error);
		return 1;
	}
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
	tqslTrace("tqsl_endConverter", NULL);

	if (!convp || CAST_TQSL_CONVERTER(*convp) == 0)
		return 0;

	TQSL_CONVERTER* conv;

	if ((conv = check_conv(*convp))) {
		if (conv->txn) conv->txn->abort(conv->txn);
		if (conv->seendb) {
			conv->seendb->compact(conv->seendb, NULL, NULL, NULL, NULL, 0, NULL);
			conv->seendb->close(conv->seendb, 0);
		}
		if (conv->dbenv) {
			char **unused;
			conv->dbenv->txn_checkpoint(conv->dbenv, 0, 0, 0);
			conv->dbenv->log_archive(conv->dbenv, &unused, DB_ARCH_REMOVE);
			conv->dbenv->close(conv->dbenv, 0);
		}
		// close files and clean up converters, if any
		if (conv->adif) tqsl_endADIF(&conv->adif);
		if (conv->cab) tqsl_endCabrillo(&conv->cab);
		if (conv->dbpath) free(conv->dbpath);
		if (conv->errfile) fclose(conv->errfile);
	}

	if (conv->appName) free(conv->appName);
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

static const char *notypes[] = { "D", "T", "M", "N", "C", "" };

static const char *
tqsl_infer_band(const char* infreq) {
	char *oldlocale = setlocale(LC_NUMERIC, "C");
	double freq = atof(infreq);
	setlocale(LC_NUMERIC, oldlocale);
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
			// Allow for cases where loggers that don't log the
			// real frequency.
			if (low == 10100) low = 10000;
			else if (low == 18068) low = 18000;
			else if (low == 24890) low = 24000;
			if (freq_khz >= low && freq_khz <= high) {
				match = true;
			}
		} else {
			if (freq >= low && freq <= high)
				match = true;
			if (freq >= low && high == 0)
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
	tqslTrace("tqsl_setADIFConverterDateFilter", NULL);

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

// Remove the dupes db files
static void
remove_db(const char *path)  {
	tqslTrace("remove_db", "path=%s", path);
#ifdef _WIN32
	wchar_t* wpath = utf8_to_wchar(path);
	_WDIR *dir = _wopendir(wpath);
	free_wchar(wpath);
#else
	DIR *dir = opendir(path);
#endif
	if (dir != NULL) {
#ifdef _WIN32
		struct _wdirent *ent = NULL;
		while ((ent = _wreaddir(dir)) != NULL) {
			if (!wcscmp(ent->d_name, L"duplicates.db") ||
			!wcsncmp(ent->d_name, L"log.", 4) ||
			!wcsncmp(ent->d_name, L"__db.", 5)) {
#else
		struct dirent *ent = NULL;
		while ((ent = readdir(dir)) != NULL) {
			if (!strcmp(ent->d_name, "duplicates.db") ||
			!strncmp(ent->d_name, "log.", 4) ||
			!strncmp(ent->d_name, "__db.", 5)) {
#endif
				string fname = path;
				int rstat;
#ifdef _WIN32
				char dname[TQSL_MAX_PATH_LEN];
				wcstombs(dname, ent->d_name, TQSL_MAX_PATH_LEN);
				fname = fname + "/" + dname;
				wchar_t* wfname = utf8_to_wchar(fname.c_str());
				tqslTrace("remove_db", "unlinking %s", fname.c_str());
				rstat = _wunlink(wfname);
				free_wchar(wfname);
#else
				fname = fname + "/" + ent->d_name;
				tqslTrace("remove_db", "unlinking %s", fname.c_str());
				rstat = unlink(fname.c_str());
#endif
				if (rstat < 0) {
					tqslTrace("remove_db", "can't unlink %s: %s", fname.c_str(), strerror(errno));
				}
			}
		}
#ifdef _WIN32
		_wclosedir(dir);
#else
		closedir(dir);
#endif
	}
	return;
}
#ifndef _WIN32
// Callback method for the dbenv->failchk() call
// Used to determine if the given pid/tid is
// alive.
static int isalive(DB_ENV *env, pid_t pid, db_threadid_t tid, uint32_t flags) {
	int alive = 0;

	if (pid == getpid()) {
		alive = 1;
	} else if (kill(pid, 0) == 0) {
		alive = 1;
	} else if (errno == EPERM) {
		alive = 1;
	}
	return alive;
}
#endif // _WIN32

// Open the duplicates database

static bool open_db(TQSL_CONVERTER *conv, bool readonly) {
	bool dbinit_cleanup = false;
	int dbret;
	bool triedRemove = false;
	bool triedDelete = false;
	int envflags = DB_INIT_TXN|DB_INIT_LOG|DB_INIT_MPOOL|DB_RECOVER|DB_REGISTER|DB_CREATE;
	string fixedpath = tQSL_BaseDir; //must be first because of gotos
	size_t found = fixedpath.find('\\');

	tqslTrace("open_db", "path=%s", fixedpath.c_str());
	//bdb complains about \\s in path on windows...

	while (found != string::npos) {
		fixedpath.replace(found, 1, "/");
		found = fixedpath.find('\\');
	}

	conv->dbpath = strdup(fixedpath.c_str());

#ifndef _WIN32
	// Clean up junk in that directory
	DIR *dir = opendir(fixedpath.c_str());
	if (dir != NULL) {
		struct dirent *ent;
		while ((ent = readdir(dir)) != NULL) {
			if (ent->d_name[0] == '.')
				continue;
			struct stat s;
			// If it's a symlink pointing to itself, remove it.
			string fname = fixedpath + "/" + ent->d_name;
			if (stat(fname.c_str(), &s)) {
				if (errno == ELOOP) {
#ifdef _WIN32
					_wunlink(ConvertFromUtf8ToUtf16(fname.c_str()));
#else
					unlink(fname.c_str());
#endif
				}
			}
		}
		closedir(dir);
	}
#endif
	string logpath = fixedpath + "/dberr.log";
#ifdef _WIN32
	wchar_t* wlogpath = utf8_to_wchar(logpath.c_str());
	conv->errfile = _wfopen(wlogpath, L"wb");
	free_wchar(wlogpath);
#else
	conv->errfile = fopen(logpath.c_str(), "wb");
#endif

 reopen:

	// Try to open the database
	while (true) {
		if (!conv->dbenv) {
			// Create the database environment handle
			if ((dbret = db_env_create(&conv->dbenv, 0))) {
				// can't make env handle
				tqslTrace("open_db", "db_env_create error %s", db_strerror(dbret));
				dbinit_cleanup = true;
				goto dbinit_end;
			}
			tqslTrace("open_db", "dbenv=0x%lx", conv->dbenv);
			if (conv->errfile) {
				conv->dbenv->set_errfile(conv->dbenv, conv->errfile);
				conv->dbenv->set_verbose(conv->dbenv, DB_VERB_RECOVERY, 1);
			}
			// Enable stale lock removal
			conv->dbenv->set_thread_count(conv->dbenv, 8);
#ifndef _WIN32
			conv->dbenv->set_isalive(conv->dbenv, isalive);
#endif
			// Log files default to 10 Mb each. We don't need nearly that much.
			if (conv->dbenv->set_lg_max)
				conv->dbenv->set_lg_max(conv->dbenv, 256 * 1024);
			// Allocate additional locking resources - some have run out with
			// the default 1000 locks
			if (conv->dbenv->set_lk_max_locks)
				conv->dbenv->set_lk_max_locks(conv->dbenv, 20000);
			if (conv->dbenv->set_lk_max_objects)
				conv->dbenv->set_lk_max_objects(conv->dbenv, 20000);
		}
		// Now open the database
		tqslTrace("open_db", "Opening the database at %s", conv->dbpath);
		if ((dbret = conv->dbenv->open(conv->dbenv, conv->dbpath, envflags, 0600))) {
			int db_errno = errno;
			tqslTrace("open_db", "dbenv->open %s error %s", conv->dbpath, db_strerror(dbret));
			if (conv->errfile)
				fprintf(conv->errfile, "opening DB %s returns status %s\n", conv->dbpath, db_strerror(dbret));
			// Can't open the database - maybe try private?
			if ((dbret == EACCES || dbret == EROFS) || (dbret == EINVAL && errno == dbret)) {
				if (!(envflags & DB_PRIVATE)) {
					envflags |= DB_PRIVATE;
					continue;
				}
			}
			// can't open environment - try to delete it and try again.
			tqslTrace("open_db", "Environment open fail, triedRemove=%d", triedRemove);
			if (!triedRemove) {
				// Remove the dross
				tqslTrace("open_db", "Removing environment");
				conv->dbenv->remove(conv->dbenv, conv->dbpath, DB_FORCE);
				conv->dbenv = NULL;
				triedRemove = true;
				if (conv->errfile)
					fprintf(conv->errfile, "About to retry after removing the environment\n");
				tqslTrace("open_db", "About to retry after removing the environment");
				continue;
			}
			tqslTrace("open_db", "Retry attempt after removing the environment failed");
			if (conv->errfile) {
				fprintf(conv->errfile, "Retry attempt after removing the environment failed.\n");
			}
			// EINVAL means that the database is corrupted to the point
			// where it can't be opened. Remove it and try again.
			if ((dbret == EINVAL || db_errno == EINVAL) && !triedDelete) {
				tqslTrace("open_db", "EINVAL. Removing db");
				conv->dbenv->close(conv->dbenv, 0);
				conv->dbenv = NULL;
				remove_db(fixedpath.c_str());
				triedDelete = true;
				continue;
			}

			// can't open environment and cleanup efforts failed.
			conv->dbenv->close(conv->dbenv, 0);
			conv->dbenv = NULL;	// this can't be recovered
			dbinit_cleanup = true;
			tqslTrace("open_db", "can't fix. abandoning.");
			remove_db(fixedpath.c_str());
			goto dbinit_end;
		}
		break;		// Opened OK.
	}

#ifndef _WIN32		// isalive() method doesn't exist for WIN32.
	// Stale lock removal
	tqslTrace("open_db", "Removing stale locks");
	dbret = conv->dbenv->failchk(conv->dbenv, 0);
	if (dbret && conv->errfile) {
		fprintf(conv->errfile, "lock removal for DB %s returns status %s\n", conv->dbpath, db_strerror(dbret));
	}
#endif

	tqslTrace("open_db", "calling db_create");
	if ((dbret = db_create(&conv->seendb, conv->dbenv, 0))) {
		// can't create db
		dbinit_cleanup = true;
		tqslTrace("open_db", "Can't create db");
		goto dbinit_end;
	}

#ifndef DB_TXN_BULK
#define DB_TXN_BULK 0
#endif
	tqslTrace("open_db", "starting transaction, readonly=%d", readonly);
	if (!readonly && (dbret = conv->dbenv->txn_begin(conv->dbenv, NULL, &conv->txn, DB_TXN_BULK))) {
		// can't start a txn
		tqslTrace("open_db", "can't create txn %s", db_strerror(dbret));
		dbinit_cleanup = true;
		goto dbinit_end;
	}

	// Probe the database type
	tqslTrace("open_db", "opening database now");
	if ((dbret = conv->seendb->open(conv->seendb, conv->txn, "duplicates.db", NULL, DB_UNKNOWN, 0, 0600))) {
		if (dbret == ENOENT) {
			tqslTrace("open_db", "DB not found, making a new one");
			dbret = conv->seendb->open(conv->seendb, conv->txn, "duplicates.db", NULL, DB_HASH, DB_CREATE, 0600);
		}
		if (dbret) {
			// can't open the db
			tqslTrace("open_db", "create failed with %s errno %d", db_strerror(dbret), errno);
			dbinit_cleanup = true;
			goto dbinit_end;
		}
	}

	DBTYPE type;
	conv->seendb->get_type(conv->seendb, &type);
	tqslTrace("open_db", "type=%d", type);
	if (type ==  DB_BTREE) {
		tqslTrace("open_db", "BTREE type. Converting.");
		// Have to convert the database.
		string dumpfile = fixedpath + "/dupedump.txt";
#ifdef _WIN32
		wchar_t* wdumpfile = utf8_to_wchar(dumpfile.c_str());
		FILE *dmp = _wfopen(wdumpfile, L"wb+");
		free_wchar(wdumpfile);
#else
		FILE *dmp = fopen(dumpfile.c_str(), "wb+");
#endif
		if (!dmp) {
			tqslTrace("open_db", "Error opening dump file %s: %s", dumpfile.c_str(), strerror(errno));
			dbinit_cleanup = true;
			goto dbinit_end;
		}
		if (!conv->cursor) {
#ifndef DB_CURSOR_BULK
#define DB_CURSOR_BULK 0
#endif
			int err = conv->seendb->cursor(conv->seendb, conv->txn, &conv->cursor, DB_CURSOR_BULK);
			if (err) {
				strncpy(tQSL_CustomError, db_strerror(err), sizeof tQSL_CustomError);
				tQSL_Error = TQSL_DB_ERROR;
				tQSL_Errno = errno;
				tqslTrace("open_db", "Error setting cursor for old DB: %s", err);
				dbinit_cleanup = true;
				goto dbinit_end;
			}
		}

		DBT dbkey, dbdata;
		char duprec[512];
		while (1) {
			memset(&dbkey, 0, sizeof dbkey);
			memset(&dbdata, 0, sizeof dbdata);
			int status = conv->cursor->c_get(conv->cursor, &dbkey, &dbdata, DB_NEXT);
			if (DB_NOTFOUND == status) {
				break;	// No more records
			}
			if (status != 0) {
				strncpy(tQSL_CustomError, db_strerror(status), sizeof tQSL_CustomError);
				tQSL_Error = TQSL_DB_ERROR;
				tQSL_Errno = errno;
				tqslTrace("open_db", "Error reading for dump: %s", db_strerror(status));
				dbinit_cleanup = true;
				goto dbinit_end;
			}
			memcpy(duprec, dbkey.data, dbkey.size);
			duprec[dbkey.size] = '\0';
			fprintf(dmp, "%s\n", duprec);
		}
		conv->cursor->close(conv->cursor);
		if (conv->txn) conv->txn->commit(conv->txn, 0);
		conv->seendb->close(conv->seendb, 0);
		conv->dbenv->remove(conv->dbenv, conv->dbpath, DB_FORCE);
		conv->dbenv->close(conv->dbenv, 0);
		conv->cursor = NULL;
		conv->seendb = NULL;
		conv->dbenv = NULL;

		// Remove the old dupe db
		tqslTrace("open_db", "Removing old format db");
		remove_db(fixedpath.c_str());

		// Now create the new database
		if ((dbret = db_env_create(&conv->dbenv, 0))) {
			// can't make env handle
			tqslTrace("open_db", "Can't make db handle: %s", db_strerror(dbret));
			dbinit_cleanup = true;
			goto dbinit_end;
		}
		if (conv->errfile)
			conv->dbenv->set_errfile(conv->dbenv, conv->errfile);
		if (conv->dbenv->set_lg_max)
			conv->dbenv->set_lg_max(conv->dbenv, 256 * 1024);
		if (conv->dbenv->set_lk_max_locks)
			conv->dbenv->set_lk_max_locks(conv->dbenv, 20000);
		if (conv->dbenv->set_lk_max_objects)
			conv->dbenv->set_lk_max_objects(conv->dbenv, 20000);
		if ((dbret = conv->dbenv->open(conv->dbenv, conv->dbpath, envflags, 0600))) {
			tqslTrace("open_db", "Error opening db: %s", db_strerror(dbret));
			if (conv->errfile)
				fprintf(conv->errfile, "opening DB %s returns status %d\n", conv->dbpath, dbret);
			dbinit_cleanup = true;
			goto dbinit_end;
		}

		if ((dbret = db_create(&conv->seendb, conv->dbenv, 0))) {
			// can't create db
			tqslTrace("open_db", "Error creating db: %s", db_strerror(dbret));
			dbinit_cleanup = true;
			goto dbinit_end;
		}

		// Create the new database
		if ((dbret = conv->seendb->open(conv->seendb, NULL, "duplicates.db", NULL, DB_HASH, DB_CREATE, 0600))) {
			// can't open the db
			tqslTrace("open_db", "Error opening new db: %s", db_strerror(dbret));
			dbinit_cleanup = true;
			goto dbinit_end;
		}
		fseek(dmp, 0, SEEK_SET);

		char d[1]= {'D'};
		memset(&dbkey, 0, sizeof dbkey);
		memset(&dbdata, 0, sizeof dbdata);
		dbdata.data = d;
		dbdata.size = 1;

		while (fgets(duprec, sizeof duprec, dmp)) {
			dbkey.data = duprec;
			dbkey.size = strlen(duprec) - 1;
			conv->seendb->put(conv->seendb, NULL, &dbkey, &dbdata, 0);
		}
		conv->seendb->close(conv->seendb, 0);
		conv->dbenv->close(conv->dbenv, 0);
		goto reopen;
	}

 dbinit_end:
	if (dbinit_cleanup) {
		tqslTrace("open_db", "DB open failed, triedDelete=%d", triedDelete);
		tQSL_Error = TQSL_DB_ERROR;
		tQSL_Errno = errno;
		strncpy(tQSL_CustomError, db_strerror(dbret), sizeof tQSL_CustomError);
		tqslTrace("open_db", "Error opening db: %s", tQSL_CustomError);
		if (conv->txn) conv->txn->abort(conv->txn);
		if (conv->seendb) conv->seendb->close(conv->seendb, 0);
		if (conv->dbenv) {
			if (conv->dbpath) {
				conv->dbenv->remove(conv->dbenv,  conv->dbpath, DB_FORCE);
				free(conv->dbpath);
				conv->dbpath = NULL;
			}
			conv->dbenv->close(conv->dbenv, 0);
		}
		if (conv->cursor) conv->cursor->close(conv->cursor);
		if (conv->errfile) fclose(conv->errfile);
		conv->txn = NULL;
		conv->dbenv = NULL;
		conv->cursor = NULL;
		conv->seendb = NULL;
		conv->errfile = NULL;
		// Handle case where the database is just broken
		if (dbret == EINVAL && !triedDelete) {
			tqslTrace("open_db", "EINVAL. Removing db");
			remove_db(fixedpath.c_str());
			triedDelete = true;
			goto reopen;
		}
		return false;
	}
	return true;
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getConverterGABBI(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;
	char signdata[1024];

	if (!(conv = check_conv(convp)))
		return 0;
	if (conv->need_ident_rec) {
		int major = 0, minor = 0, config_major = 0, config_minor = 0;
		tqsl_getVersion(&major, &minor);
		tqsl_getConfigVersion(&config_major, &config_minor);
		char temp[512];
		static char ident[512];
		snprintf(temp, sizeof temp, "%s Lib: V%d.%d Config: V%d.%d AllowDupes: %s",
			conv->appName ? conv->appName : "Unknown",
			major, minor, config_major, config_minor,
			conv->allow_dupes ? "true" : "false");
		temp[sizeof temp - 1] = '\0';
		int len = strlen(temp);
		snprintf(ident, sizeof ident, "<TQSL_IDENT:%d>%s\n", len, temp);
		ident[sizeof ident - 1] = '\0';
		conv->need_ident_rec = false;
		return ident;
	}

	if (conv->need_station_rec) {
		int uid = conv->cert_idx + conv->base_idx;
		conv->need_station_rec = false;
		const char *tStation = tqsl_getGABBItSTATION(conv->loc, uid, uid);
		tqsl_getCertificateSerialExt(conv->certs[conv->cert_idx], conv->serial, sizeof conv->serial);
		tqsl_getCertificateCallSign(conv->certs[conv->cert_idx], conv->callsign, sizeof conv->callsign);
		return tStation;
	}
	if (!conv->allow_dupes && !conv->seendb) {
		if (!open_db(conv, false)) {	// If can't open dupes DB
			return 0;
		}
	}

	TQSL_ADIF_GET_FIELD_ERROR stat;

	if (conv->rec_done) {
//cerr << "Getting rec" << endl;
		conv->rec_done = false;
		conv->clearRec();
		int cstat = 0;
		int saveErr = 0;
		if (conv->adif) {
	 		while (1) {
				tqsl_adifFieldResults result;
				if (tqsl_getADIFField(conv->adif, &result, &stat, adif_qso_record_fields, notypes, adif_allocate))
					break;
				if (stat != TQSL_ADIF_GET_FIELD_SUCCESS && stat != TQSL_ADIF_GET_FIELD_NO_NAME_MATCH)
					break;
				if (!strcasecmp(result.name, "eor"))
					break;
				if (!strcasecmp(result.name, "CALL") && result.data) {
					conv->rec.callsign_set = true;
					strncpy(conv->rec.callsign, reinterpret_cast<char *>(result.data), sizeof conv->rec.callsign);
				} else if (!strcasecmp(result.name, "BAND") && result.data) {
					conv->rec.band_set = true;
				  	strncpy(conv->rec.band, reinterpret_cast<char *>(result.data), sizeof conv->rec.band);
				} else if (!strcasecmp(result.name, "MODE") && result.data) {
					conv->rec.mode_set = true;
					strncpy(conv->rec.mode, reinterpret_cast<char *>(result.data), sizeof conv->rec.mode);
				} else if (!strcasecmp(result.name, "SUBMODE") && result.data) {
					strncpy(conv->rec.submode, reinterpret_cast<char *>(result.data), sizeof conv->rec.submode);
				} else if (!strcasecmp(result.name, "FREQ") && result.data) {
					conv->rec.band_set = true;
					strncpy(conv->rec.freq, fix_freq(reinterpret_cast<char *>(result.data)), sizeof conv->rec.freq);
					if (atof(conv->rec.freq) == 0.0)
						conv->rec.freq[0] = '\0';
				} else if (!strcasecmp(result.name, "FREQ_RX") && result.data) {
					strncpy(conv->rec.rxfreq, fix_freq(reinterpret_cast<char *>(result.data)), sizeof conv->rec.rxfreq);
					if (atof(conv->rec.rxfreq) == 0.0)
						conv->rec.rxfreq[0] = '\0';
				} else if (!strcasecmp(result.name, "BAND_RX") && result.data) {
					strncpy(conv->rec.rxband, reinterpret_cast<char *>(result.data), sizeof conv->rec.rxband);
				} else if (!strcasecmp(result.name, "SAT_NAME") && result.data) {
					strncpy(conv->rec.satname, reinterpret_cast<char *>(result.data), sizeof conv->rec.satname);
				} else if (!strcasecmp(result.name, "PROP_MODE") && result.data) {
					strncpy(conv->rec.propmode, reinterpret_cast<char *>(result.data), sizeof conv->rec.propmode);
				} else if (!strcasecmp(result.name, "QSO_DATE") && result.data) {
					conv->rec.date_set = true;
					cstat = tqsl_initDate(&(conv->rec.date), (const char *)result.data);
					if (cstat)
						saveErr = tQSL_Error;
				} else if (!strcasecmp(result.name, "TIME_ON") && result.data) {
					conv->rec.time_set = true;
					cstat = tqsl_initTime(&(conv->rec.time), (const char *)result.data);
					if (cstat)
						saveErr = tQSL_Error;
				}
				if (stat == TQSL_ADIF_GET_FIELD_SUCCESS) {
					conv->rec_text += string(reinterpret_cast<char *>(result.name)) + ": ";
					if (result.data)
						conv->rec_text += string(reinterpret_cast<char *>(result.data));
					conv->rec_text += "\n";
				}
				if (result.data)
					delete[] result.data;
			}
			if (saveErr) {
				tQSL_Error = saveErr;
				conv->rec_done = true;
				return 0;
			}
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
						conv->rec.callsign_set = true;
						strncpy(conv->rec.callsign, field.value, sizeof conv->rec.callsign);
					} else if (!strcasecmp(field.name, "BAND")) {
						conv->rec.band_set = true;
						strncpy(conv->rec.band, field.value, sizeof conv->rec.band);
					} else if (!strcasecmp(field.name, "MODE")) {
						conv->rec.mode_set = true;
						strncpy(conv->rec.mode, field.value, sizeof conv->rec.mode);
					} else if (!strcasecmp(field.name, "FREQ")) {
						conv->rec.band_set = true;
						strncpy(conv->rec.freq, field.value, sizeof conv->rec.freq);
					} else if (!strcasecmp(field.name, "QSO_DATE")) {
						conv->rec.date_set = true;
						cstat = tqsl_initDate(&(conv->rec.date), field.value);
						if (cstat)
							saveErr = tQSL_Error;
					} else if (!strcasecmp(field.name, "TIME_ON")) {
						conv->rec.time_set = true;
						cstat = tqsl_initTime(&(conv->rec.time), field.value);
						if (cstat)
							saveErr = tQSL_Error;
					}
					if (conv->rec_text != "")
						conv->rec_text += "\n";
					conv->rec_text += string(field.name) + ": " + field.value;
				}
			} while (stat == TQSL_CABRILLO_NO_ERROR);
			if (saveErr)
				tQSL_Error = saveErr;
			if (saveErr || stat != TQSL_CABRILLO_EOR) {
				conv->rec_done = true;
				return 0;
			}
		} else {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, "Converter not initialized", sizeof tQSL_CustomError);
			tqslTrace("tqsl_getConverterGABBI", "Converter not initialized");
			return 0;
		}
	}
	// Does the QSO have the basic required elements?
	if (!conv->rec.callsign_set) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid contact - QSO does not specify a Callsign");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (!conv->rec.band_set) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid contact - QSO does not specify a band or frequency");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (!conv->rec.mode_set) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid contact - QSO does not specify a mode");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (!conv->rec.date_set) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid contact - QSO does not specify a date");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (!conv->rec.time_set) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid contact - QSO does not specify a time");
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
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
	tqsl_strtoupper(conv->rec.rxband);
	tqsl_strtoupper(conv->rec.mode);
	tqsl_strtoupper(conv->rec.submode);
	char val[256] = "";
	// Try to find the GABBI mode several ways.
	val[0] = '\0';
	if (conv->rec.submode[0] != '\0') {
		char modeSub[256];
		strncpy(modeSub, conv->rec.mode, sizeof modeSub);
		size_t left = sizeof modeSub - strlen(modeSub);
		strncat(modeSub, "%", left);
		left = sizeof modeSub - strlen(modeSub);
		strncat(modeSub, conv->rec.submode, left);
		if (tqsl_getADIFMode(modeSub, val, sizeof val)) {	// mode%submode lookup failed
			// Try just the submode.
			if (tqsl_getADIFMode(conv->rec.submode, val, sizeof val)) { // bare submode failed
				if (tqsl_getADIFMode(conv->rec.mode, val, sizeof val)) {
					val[0] = '\0';
				}
			}
		}
	} else {
		// Just a mode, no submode. Look that up.
		tqsl_getADIFMode(conv->rec.mode, val, sizeof val);
	}
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
	if (conv->rec.rxband[0] && (conv->bands.find(conv->rec.rxband) == conv->bands.end())) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Invalid RX BAND (%s)", conv->rec.rxband);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (conv->rec.freq[0] && strcmp(conv->rec.band, tqsl_infer_band(conv->rec.freq))) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "Frequency %s is out of range for band %s", conv->rec.freq, conv->rec.band);
		tQSL_Error = TQSL_CUSTOM_ERROR;
		return 0;
	}
	if (conv->rec.rxfreq[0] && strcmp(conv->rec.rxband, tqsl_infer_band(conv->rec.rxfreq))) {
		conv->rec_done = true;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "RX Frequency %s is out of range for band %s", conv->rec.rxfreq, conv->rec.band);
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
			char stnloc[128];
			char qso[128];
			if (tqsl_getLocationStationDetails(conv->loc, stnloc, sizeof stnloc)) {
				stnloc[0] = '\0';
			}
			if (tqsl_getLocationQSODetails(conv->loc, qso, sizeof qso)) {
				qso[0] = '\0';
			}
			// Old-style Lookup uses signdata and cert serial number
			DBT dbkey, dbdata;
			memset(&dbkey, 0, sizeof dbkey);
			memset(&dbdata, 0, sizeof dbdata);
			// append signing key serial
			strncat(signdata, conv->serial, sizeof(signdata) - strlen(signdata)-1);
			// Updated dupe database entry. Key is formed from
			// local callsign concatenated with the QSO details
			char dupekey[128];
			snprintf(dupekey, sizeof dupekey, "%s%s", conv->callsign, qso);
			dbkey.size = strlen(signdata);
			dbkey.data = signdata;
			int dbget_err = conv->seendb->get(conv->seendb, conv->txn, &dbkey, &dbdata, 0);
			if (0 == dbget_err) {
				//lookup was successful; thus this is a duplicate.
				tQSL_Error = TQSL_DUPLICATE_QSO;
				tQSL_CustomError[0] = '\0';
				// delete the old record
				conv->seendb->del(conv->seendb, conv->txn, &dbkey, 0);
				// Update this to the current format
				memset(&dbkey, 0, sizeof dbkey);
				dbkey.size = strlen(dupekey);
				dbkey.data = dupekey;
				memset(&dbdata, 0, sizeof dbdata);
				dbdata.data = stnloc;
				dbdata.size = strlen(stnloc);
				int dbput_err;
				dbput_err = conv->seendb->put(conv->seendb, conv->txn, &dbkey, &dbdata, 0);
				if (0 != dbput_err) {
					strncpy(tQSL_CustomError, db_strerror(dbput_err), sizeof tQSL_CustomError);
					tQSL_Error = TQSL_DB_ERROR;
					return 0;
				}
				return 0;
			} else if (dbget_err != DB_NOTFOUND) {
				//non-zero return, but not "not found" - thus error
				strncpy(tQSL_CustomError, db_strerror(dbget_err), sizeof tQSL_CustomError);
				tQSL_Error = TQSL_DB_ERROR;
				return 0;
				// could be more specific but there's very little the user can do at this point anyway
			}
			memset(&dbkey, 0, sizeof dbkey);
			memset(&dbdata, 0, sizeof dbdata);

			dbkey.size = strlen(dupekey);
			dbkey.data = dupekey;
			dbget_err = conv->seendb->get(conv->seendb, conv->txn, &dbkey, &dbdata, 0);
			if (0 == dbget_err) {
				//lookup was successful; thus this is a duplicate.
				tQSL_Error = TQSL_DUPLICATE_QSO;
				// Save the original and new station location details so those can be provided
				// with an error by the caller
				char *olddup = reinterpret_cast<char *> (malloc(dbdata.size + 2));
				memcpy(olddup, dbdata.data, dbdata.size);
				olddup[dbdata.size] = '\0';
				snprintf(tQSL_CustomError, sizeof tQSL_CustomError, "%s|%s", olddup, stnloc);
				free(olddup);
				return 0;
			} else if (dbget_err != DB_NOTFOUND) {
				//non-zero return, but not "not found" - thus error
				strncpy(tQSL_CustomError, db_strerror(dbget_err), sizeof tQSL_CustomError);
				tQSL_Error = TQSL_DB_ERROR;
				return 0;
				// could be more specific but there's very little the user can do at this point anyway
			}

			memset(&dbdata, 0, sizeof dbdata);
			dbdata.data = stnloc;
			dbdata.size = strlen(stnloc);
			int dbput_err;
			dbput_err = conv->seendb->put(conv->seendb, conv->txn, &dbkey, &dbdata, 0);
			if (0 != dbput_err) {
				strncpy(tQSL_CustomError, db_strerror(dbput_err), sizeof tQSL_CustomError);
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
tqsl_setConverterAppName(tQSL_Converter convp, const char *app) {
	TQSL_CONVERTER *conv;
	if (!(conv = check_conv(convp)))
		return 1;
	if (!app) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	conv->appName = strdup(app);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_converterRollBack(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;

	tqslTrace("tqsl_converterRollBack", NULL);
	if (!(conv = check_conv(convp)))
		return 1;
	if (!conv->seendb)
		return 0;
	if (conv->txn)
		conv->txn->abort(conv->txn);
	conv->txn = NULL;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_converterCommit(tQSL_Converter convp) {
	TQSL_CONVERTER *conv;

	tqslTrace("tqsl_converterCommit", NULL);
	if (!(conv = check_conv(convp)))
		return 1;
	if (!conv->seendb)
		return 0;
	if (conv->txn)
		conv->txn->commit(conv->txn, 0);
	conv->txn = NULL;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDuplicateRecords(tQSL_Converter convp, char *key, char *data, int keylen) {
	TQSL_CONVERTER *conv;

	if (!(conv = check_conv(convp)))
		return 1;

	if (!conv->seendb) {
		if (!open_db(conv, true)) {	// If can't open dupes DB
			return 1;
		}
	}
#ifndef DB_CURSOR_BULK
#define DB_CURSOR_BULK 0
#endif
	if (!conv->cursor) {
		int err = conv->seendb->cursor(conv->seendb, conv->txn, &conv->cursor, DB_CURSOR_BULK);
		if (err) {
			strncpy(tQSL_CustomError, db_strerror(err), sizeof tQSL_CustomError);
			tQSL_Error = TQSL_DB_ERROR;
			tQSL_Errno = errno;
			return 1;
		}
	}

	DBT dbkey, dbdata;
	memset(&dbkey, 0, sizeof dbkey);
	memset(&dbdata, 0, sizeof dbdata);
	int status = conv->cursor->c_get(conv->cursor, &dbkey, &dbdata, DB_NEXT);
	if (DB_NOTFOUND == status) {
		return -1;	// No more records
	}
	if (status != 0) {
		strncpy(tQSL_CustomError, db_strerror(status), sizeof tQSL_CustomError);
		tQSL_Error = TQSL_DB_ERROR;
		tQSL_Errno = errno;
		return 1;
	}
	memcpy(key, dbkey.data, dbkey.size);
	key[dbkey.size] = '\0';

	if (dbdata.size > 9) dbdata.size = 9;
	memcpy(data, dbdata.data, dbdata.size);
	data[dbdata.size] = '\0';
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDuplicateRecordsV2(tQSL_Converter convp, char *key, char *data, int keylen) {
	TQSL_CONVERTER *conv;

	if (!(conv = check_conv(convp)))
		return 1;

	if (!conv->seendb) {
		if (!open_db(conv, true)) {	// If can't open dupes DB
			return 1;
		}
	}
#ifndef DB_CURSOR_BULK
#define DB_CURSOR_BULK 0
#endif
	if (!conv->cursor) {
		int err = conv->seendb->cursor(conv->seendb, conv->txn, &conv->cursor, DB_CURSOR_BULK);
		if (err) {
			strncpy(tQSL_CustomError, db_strerror(err), sizeof tQSL_CustomError);
			tQSL_Error = TQSL_DB_ERROR;
			tQSL_Errno = errno;
			return 1;
		}
	}

	DBT dbkey, dbdata;
	memset(&dbkey, 0, sizeof dbkey);
	memset(&dbdata, 0, sizeof dbdata);
	int status = conv->cursor->c_get(conv->cursor, &dbkey, &dbdata, DB_NEXT);
	if (DB_NOTFOUND == status) {
		return -1;	// No more records
	}
	if (status != 0) {
		strncpy(tQSL_CustomError, db_strerror(status), sizeof tQSL_CustomError);
		tQSL_Error = TQSL_DB_ERROR;
		tQSL_Errno = errno;
		return 1;
	}
	memcpy(key, dbkey.data, dbkey.size);
	key[dbkey.size] = '\0';
	if (dbdata.size > 255) dbdata.size = 255;
	memcpy(data, dbdata.data, dbdata.size);
	data[dbdata.size] = '\0';
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_putDuplicateRecord(tQSL_Converter convp, const char *key, const char *data, int keylen) {
	TQSL_CONVERTER *conv;

	if (!(conv = check_conv(convp)))
		return 0;

	if (!conv->seendb) {
		if (!open_db(conv, false)) {	// If can't open dupes DB
			return 0;
		}
	}
	DBT dbkey, dbdata;
	memset(&dbkey, 0, sizeof dbkey);
	memset(&dbdata, 0, sizeof dbdata);
	dbkey.size = keylen;
	dbkey.data = const_cast<char *>(key);

	dbdata.size = strlen(data);
	dbdata.data = const_cast<char *>(data);

	int status = conv->seendb->put(conv->seendb, conv->txn, &dbkey, &dbdata, 0);

	if (DB_KEYEXIST == status) {
		return -1;	// OK, but already there
	}

	if (status != 0) {
		strncpy(tQSL_CustomError, db_strerror(status), sizeof tQSL_CustomError);
		tQSL_Error = TQSL_DB_ERROR;
		tQSL_Errno = errno;
		return 1;
	}
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
	// Invalid callsign patterns
	// Starting with 0, Q
	// 1x other than 1A, 1M, 1S
	string first = call.substr(0, 1);
	string second = call.substr(1, 1);
	if (first == "0" || first == "Q" ||
#ifdef MARK_C7_4Y_INVALID
	    (first == "C" && second == "7") ||
	    (first == "4" && second == "Y") ||
#endif
	    (first == "1" && second != "A" && second != "M" && second != "S"))
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
