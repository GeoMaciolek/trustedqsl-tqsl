/***************************************************************************
                          location.cpp  -  description
                             -------------------
    begin                : Wed Nov 6 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: location.cpp,v 1.14 2013/03/01 13:20:30 k1mu Exp $
 ***************************************************************************/


#define DXCC_TEST

#define TQSLLIB_DEF

#include "location.h"

#include <errno.h>
#include <stdlib.h>
#include <zlib.h>
#ifdef __APPLE__
#include <CoreFoundation/CFBundle.h>
#endif
#include <cstring>
#include <fstream>
#include <algorithm>
#include <vector>
#include <iostream>
#include <utility>
#include <map>
#include <string>
#include <cctype>
#include <functional>
#include "tqsllib.h"
#include "tqslerrno.h"
#include "xml.h"
#include "openssl_cert.h"
#ifdef _WIN32
	#include "windows.h"
#endif

#include "winstrdefs.h"

using std::string;
using std::vector;
using std::map;
using std::pair;
using std::make_pair;
using std::ofstream;
using std::ios;
using std::endl;
using std::exception;

namespace tqsllib {

class TQSL_LOCATION_ITEM {
 public:
	TQSL_LOCATION_ITEM() : ivalue(0) {}
	string text;
	string label;
	string zonemap;
	int ivalue;
};

class TQSL_LOCATION_FIELD {
 public:
	TQSL_LOCATION_FIELD() {}
	TQSL_LOCATION_FIELD(string i_gabbi_name, const char *i_label, int i_data_type, int i_data_len,
		int i_input_type, int i_flags = 0);
	string label;
	string gabbi_name;
	int data_type;
	int data_len;
	string cdata;
	vector<TQSL_LOCATION_ITEM> items;
	int idx;
	int idata;
	int input_type;
	int flags;
	bool changed;
	string dependency;
};

TQSL_LOCATION_FIELD::TQSL_LOCATION_FIELD(string i_gabbi_name, const char *i_label, int i_data_type,
	int i_data_len, int i_input_type, int i_flags) : data_type(i_data_type), data_len(i_data_len), cdata(""),
	input_type(i_input_type), flags(i_flags) {
		if (!i_gabbi_name.empty())
		gabbi_name = i_gabbi_name;
	if (i_label)
		label = i_label;
	idx = idata = 0;
}

typedef vector<TQSL_LOCATION_FIELD> TQSL_LOCATION_FIELDLIST;

class TQSL_LOCATION_PAGE {
 public:
	TQSL_LOCATION_PAGE() : complete(false), prev(0), next(0) {}
	bool complete;
	int prev, next;
	string dependentOn, dependency;
	map<string, vector<string> > hash;
	TQSL_LOCATION_FIELDLIST fieldlist;
};

typedef vector<TQSL_LOCATION_PAGE> TQSL_LOCATION_PAGELIST;

class TQSL_NAME {
 public:
	TQSL_NAME(string n = "", string c = "") : name(n), call(c) {}
	string name;
	string call;
};

class TQSL_LOCATION {
 public:
	TQSL_LOCATION() : sentinel(0x5445), page(0), cansave(false), sign_clean(false), cert_flags(TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED), newflags(false) {}

	~TQSL_LOCATION() { sentinel = 0; }
	int sentinel;
	int page;
	bool cansave;
	string name;
	TQSL_LOCATION_PAGELIST pagelist;
	vector<TQSL_NAME> names;
	string signdata;
	bool sign_clean;
	string tSTATION;
	string tCONTACT;
	string sigspec;
	char data_errors[512];
	int cert_flags;
	bool newflags;
};

class Band {
 public:
	string name, spectrum;
	int low, high;
};

class Mode {
 public:
	string mode, group;
};

class PropMode {
 public:
	string descrip, name;
};

class Satellite {
 public:
	Satellite() {
		start.year = start.month = start.day = 0;
		end.year = end.month = end.day = 0;
	}
	string descrip, name;
	tQSL_Date start, end;
};

bool
operator< (const Band& o1, const Band& o2) {
	static const char *suffixes[] = { "M", "CM", "MM"};
	static const char *prefix_chars = "0123456789.";
	// get suffixes
	string b1_suf = o1.name.substr(o1.name.find_first_not_of(prefix_chars));
	string b2_suf = o2.name.substr(o2.name.find_first_not_of(prefix_chars));
	if (b1_suf != b2_suf) {
		// Suffixes differ -- compare suffixes
		int b1_idx = (sizeof suffixes / sizeof suffixes[0]);
		int b2_idx = b1_idx;
		for (int i = 0; i < static_cast<int>(sizeof suffixes / sizeof suffixes[0]); i++) {
			if (b1_suf == suffixes[i])
				b1_idx = i;
			if (b2_suf == suffixes[i])
				b2_idx = i;
		}
		return b1_idx < b2_idx;
	}
	return atof(o1.name.c_str()) > atof(o2.name.c_str());
}

bool
operator< (const PropMode& o1, const PropMode& o2) {
	if (o1.descrip < o2.descrip)
		return true;
	if (o1.descrip == o2.descrip)
		return (o1.name < o2.name);
	return false;
}

bool
operator< (const Satellite& o1, const Satellite& o2) {
	if (o1.descrip < o2.descrip)
		return true;
	if (o1.descrip == o2.descrip)
		return (o1.name < o2.name);
	return false;
}

bool
operator< (const Mode& o1, const Mode& o2) {
	static const char *groups[] = { "CW", "PHONE", "IMAGE", "DATA" };
	// m1 < m2 if m1 is a modegroup and m2 is not
	if (o1.mode == o1.group) {
		if (o2.mode != o2.group)
			return true;
	} else if (o2.mode == o2.group) {
		return false;
	}
	// If groups are same, compare modes
	if (o1.group == o2.group)
		return o1.mode < o2.mode;
	int m1_g = (sizeof groups / sizeof groups[0]);
	int m2_g = m1_g;
	for (int i = 0; i < static_cast<int>(sizeof groups / sizeof groups[0]); i++) {
		if (o1.group == groups[i])
			m1_g = i;
		if (o2.group == groups[i])
			m2_g = i;
	}
	return m1_g < m2_g;
}

}	// namespace tqsllib

using tqsllib::XMLElement;
using tqsllib::XMLElementList;
using tqsllib::Band;
using tqsllib::Mode;
using tqsllib::PropMode;
using tqsllib::Satellite;
using tqsllib::TQSL_LOCATION;
using tqsllib::TQSL_LOCATION_PAGE;
using tqsllib::TQSL_LOCATION_PAGELIST;
using tqsllib::TQSL_LOCATION_FIELD;
using tqsllib::TQSL_LOCATION_FIELDLIST;
using tqsllib::TQSL_LOCATION_ITEM;
using tqsllib::TQSL_NAME;
using tqsllib::ROOTCERT;
using tqsllib::CACERT;
using tqsllib::USERCERT;
using tqsllib::tqsl_get_pem_serial;

#define CAST_TQSL_LOCATION(x) (reinterpret_cast<TQSL_LOCATION *>((x)))

typedef map<int, string> IntMap;

// config data

static XMLElement tqsl_xml_config;
static int tqsl_xml_config_major = -1;
static int tqsl_xml_config_minor = 0;
static IntMap DXCCMap;
static IntMap DXCCZoneMap;
static vector< pair<int, string> > DXCCList;
static vector<Band> BandList;
static vector<Mode> ModeList;
static vector<PropMode> PropModeList;
static vector<Satellite> SatelliteList;
static map<int, XMLElement> tqsl_page_map;
static map<string, XMLElement> tqsl_field_map;
static map<string, string> tqsl_adif_map;
static map<string, pair<int, int> > tqsl_cabrillo_map;
static map<string, pair<int, int> > tqsl_cabrillo_user_map;


static char
char_toupper(char c) {
	return toupper(c);
}

static string
string_toupper(const string& in) {
	string out = in;
	transform(out.begin(), out.end(), out.begin(), char_toupper);
	return out;
}
// trim from start
static inline std::string &ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
        return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
	return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
	return ltrim(rtrim(s));
}

#define TQSL_NPAGES 4

static TQSL_LOCATION *
check_loc(tQSL_Location loc, bool unclean = true) {
	if (tqsl_init())
		return 0;
	if (loc == 0)
		return 0;
	if (unclean)
		CAST_TQSL_LOCATION(loc)->sign_clean = false;
	return CAST_TQSL_LOCATION(loc);
}

static int
tqsl_load_xml_config() {
	if (tqsl_xml_config.getElementList().size() > 0)	// Already init'd
		return 0;
	XMLElement default_config;
	XMLElement user_config;
	string default_path;

#ifdef _WIN32
	HKEY hkey;
	DWORD dtype;
	char wpath[TQSL_MAX_PATH_LEN];
	DWORD bsize = sizeof wpath;
	int wval;
	if ((wval = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		"Software\\TrustedQSL", 0, KEY_READ, &hkey)) == ERROR_SUCCESS) {
		wval = RegQueryValueEx(hkey, "InstallPath", 0, &dtype, (LPBYTE)wpath, &bsize);
		RegCloseKey(hkey);
		if (wval == ERROR_SUCCESS)
			default_path = string(wpath) + "\\config.xml";
	}
#elif defined(__APPLE__)
	// Get path to config.xml resource from bundle
	CFBundleRef tqslBundle = CFBundleGetMainBundle();
	CFURLRef configXMLURL = CFBundleCopyResourceURL(tqslBundle, CFSTR("config"), CFSTR("xml"), NULL);
	if (configXMLURL) {
		CFStringRef pathString = CFURLCopyFileSystemPath(configXMLURL, kCFURLPOSIXPathStyle);
		CFRelease(configXMLURL);

		// Convert CFString path to config.xml to string object
		CFIndex maxStringLengthInBytes = CFStringGetMaximumSizeForEncoding(CFStringGetLength(pathString), kCFStringEncodingUTF8);
		char *pathCString = static_cast<char *>(malloc(maxStringLengthInBytes));
		if (pathCString) {
			CFStringGetCString(pathString, pathCString, maxStringLengthInBytes, kCFStringEncodingASCII);
			CFRelease(pathString);
			default_path = string(pathCString);
			free(pathCString);
		}
	}
#else
	default_path = CONFDIR "config.xml"; //KC2YWE: Removed temporarily. There's got to be a better way to do this
#endif

#ifdef _WIN32
	string user_path = string(tQSL_BaseDir) + "\\config.xml";
#else
	string user_path = string(tQSL_BaseDir) + "/config.xml";
#endif

	int default_status = default_config.parseFile(default_path.c_str());
	int user_status = user_config.parseFile(user_path.c_str());
	if (default_status != XML_PARSE_NO_ERROR && user_status != XML_PARSE_NO_ERROR) {
		if (user_status == XML_PARSE_SYSTEM_ERROR)
			tQSL_Error = TQSL_CONFIG_ERROR;
		else
			tQSL_Error = TQSL_CONFIG_SYNTAX_ERROR;
		return 1;
	}

	int default_major = -1;
	int default_minor = 0;
	int user_major = -1;
	int user_minor = 0;

	XMLElement top;
	if (default_config.getFirstElement("tqslconfig", top)) {
		default_major = strtol(top.getAttribute("majorversion").first.c_str(), NULL, 10);
		default_minor = strtol(top.getAttribute("minorversion").first.c_str(), NULL, 10);
	}
	if (user_config.getFirstElement("tqslconfig", top)) {
		user_major = strtol(top.getAttribute("majorversion").first.c_str(), NULL, 10);
		user_minor = strtol(top.getAttribute("minorversion").first.c_str(), NULL, 10);
	}

	if (default_major > user_major
		|| (default_major == user_major && default_minor > user_minor)) {
			tqsl_xml_config = default_config;
			tqsl_xml_config_major = default_major;
			tqsl_xml_config_minor = default_minor;
			return 0;
	}
	if (user_major < 0) {
		tQSL_Error = TQSL_CONFIG_SYNTAX_ERROR;
		return 1;
	}
	tqsl_xml_config	= user_config;
	tqsl_xml_config_major = user_major;
	tqsl_xml_config_minor = user_minor;
	return 0;
}

static int
tqsl_get_xml_config_section(const string& section, XMLElement& el) {
	if (tqsl_load_xml_config())
		return 1;
	XMLElement top;
	if (!tqsl_xml_config.getFirstElement("tqslconfig", top)) {
		tqsl_xml_config.clear();
		tQSL_Error = TQSL_CONFIG_SYNTAX_ERROR;
		return 1;
	}
	if (!top.getFirstElement(section, el)) {
		tQSL_Error = TQSL_CONFIG_SYNTAX_ERROR;
		return 1;
	}
	return 0;
}

static int
tqsl_load_provider_list(vector<TQSL_PROVIDER> &plist) {
	plist.clear();
	XMLElement providers;
	if (tqsl_get_xml_config_section("providers", providers))
		return 1;
	XMLElement provider;
	bool gotit = providers.getFirstElement("provider", provider);
	while (gotit) {
		TQSL_PROVIDER pdata;
		memset(&pdata, 0, sizeof pdata);
		pair<string, bool> rval = provider.getAttribute("organizationName");
		if (!rval.second) {
			tQSL_Error = TQSL_PROVIDER_NOT_FOUND;
			return 1;
		}
		strncpy(pdata.organizationName, rval.first.c_str(), sizeof pdata.organizationName);
		XMLElement item;
		if (provider.getFirstElement("organizationalUnitName", item))
			strncpy(pdata.organizationalUnitName, item.getText().c_str(),
				sizeof pdata.organizationalUnitName);
		if (provider.getFirstElement("emailAddress", item))
			strncpy(pdata.emailAddress, item.getText().c_str(),
				sizeof pdata.emailAddress);
		if (provider.getFirstElement("url", item))
			strncpy(pdata.url, item.getText().c_str(),
				sizeof pdata.url);
		plist.push_back(pdata);
		gotit = providers.getNextElement(provider);
		if (gotit && provider.getElementName() != "provider")
			break;
	}
	return 0;
}

static XMLElement tCONTACT_sign;

static int
make_sign_data(TQSL_LOCATION *loc) {
	map<string, string> field_data;

	// Loop through the location pages, getting field data
	//
	int old_page = loc->page;
	tqsl_setStationLocationCapturePage(loc, 1);
	do {
		TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];
		for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
			TQSL_LOCATION_FIELD& f = p.fieldlist[i];
			string s;
			if (f.input_type == TQSL_LOCATION_FIELD_DDLIST || f.input_type == TQSL_LOCATION_FIELD_LIST) {
				if (f.idx < 0 || f.idx >= static_cast<int>(f.items.size()))
					s = "";
				else
					s = f.items[f.idx].text;
			} else if (f.data_type == TQSL_LOCATION_FIELD_INT) {
				char buf[20];
				snprintf(buf, sizeof buf, "%d", f.idata);
				s = buf;
			} else {
				s = f.cdata;
			}
			field_data[f.gabbi_name] = s;
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		tqsl_nextStationLocationCapture(loc);
	} while (1);
	tqsl_setStationLocationCapturePage(loc, old_page);

	loc->signdata = "";
	loc->sign_clean = false;
	XMLElement sigspecs;
	if (tqsl_get_xml_config_section("sigspecs", sigspecs)) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - it does not have a sigspecs section",
			sizeof tQSL_CustomError);
		return 1;
	}
	XMLElement sigspec;
	if (!sigspecs.getFirstElement("sigspec", sigspec)) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - it does not have a sigspec section",
			sizeof tQSL_CustomError);
		return 1;
	}
	loc->sigspec = "SIGN_";
	loc->sigspec += sigspec.getAttribute("name").first;
	loc->sigspec += "_V";
	loc->sigspec += sigspec.getAttribute("version").first;

	tCONTACT_sign.clear();
	if (!sigspec.getFirstElement("tCONTACT", tCONTACT_sign)) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - missing sigspec.tCONTACT",
			sizeof tQSL_CustomError);
		return 1;
	}
	if (tCONTACT_sign.getElementList().size() == 0) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - empty sigspec.tCONTACT",
			sizeof tQSL_CustomError);
		return 1;
	}
	XMLElement tSTATION;
	if (!sigspec.getFirstElement("tSTATION", tSTATION)) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - missing sigspec.tSTATION",
			sizeof tQSL_CustomError);
		return 1;
	}
	XMLElement specfield;
	bool ok;
	if (!(ok = tSTATION.getFirstElement(specfield))) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - missing tSTATION.specfield",
			sizeof tQSL_CustomError);
		return 1;
	}
	do {
		string value = field_data[specfield.getElementName()];
		value = trim(value);
		if (value == "") {
			pair<string, bool> attr = specfield.getAttribute("required");
			if (attr.second && strtol(attr.first.c_str(), NULL, 10)) {
				string err = specfield.getElementName() + " field required by ";
				attr = sigspec.getAttribute("name");
				if (attr.second)
					err += attr.first + " ";
				attr = sigspec.getAttribute("version");
				if (attr.second)
					err += "V" + attr.first + " ";
				err += "signature specification not found";
				tQSL_Error = TQSL_CUSTOM_ERROR;
				strncpy(tQSL_CustomError, err.c_str(), sizeof tQSL_CustomError);
				return 1;
			}
		} else {
			loc->signdata += value;
		}
		ok = tSTATION.getNextElement(specfield);
	} while (ok);
	loc->sign_clean = true;
	return 0;
}

static int
init_dxcc() {
	if (DXCCMap.size() > 0)
		return 0;
	XMLElement dxcc;
	if (tqsl_get_xml_config_section("dxcc", dxcc))
		return 1;
	XMLElement dxcc_entity;
	bool ok = dxcc.getFirstElement("entity", dxcc_entity);
	while (ok) {
		pair<string, bool> rval = dxcc_entity.getAttribute("arrlId");
		pair<string, bool> zval = dxcc_entity.getAttribute("zonemap");
		if (rval.second) {
			int num = strtol(rval.first.c_str(), NULL, 10);
			DXCCMap[num] = dxcc_entity.getText();
			if (zval.second)
				DXCCZoneMap[num] = zval.first;
			DXCCList.push_back(make_pair(num, dxcc_entity.getText()));
		}
		ok = dxcc.getNextElement(dxcc_entity);
	}
	return 0;
}

static int
init_band() {
	if (BandList.size() > 0)
		return 0;
	XMLElement bands;
	if (tqsl_get_xml_config_section("bands", bands))
		return 1;
	XMLElement config_band;
	bool ok = bands.getFirstElement("band", config_band);
	while (ok) {
		Band b;
		b.name = config_band.getText();
		b.spectrum = config_band.getAttribute("spectrum").first;
		b.low = strtol(config_band.getAttribute("low").first.c_str(), NULL, 10);
		b.high = strtol(config_band.getAttribute("high").first.c_str(), NULL, 10);
		BandList.push_back(b);
		ok = bands.getNextElement(config_band);
	}
	sort(BandList.begin(), BandList.end());
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getConfigVersion(int *major, int *minor) {
	if (tqsl_init())
		return 1;
	if (tqsl_load_xml_config())
		return 1;
	if (major)
		*major = tqsl_xml_config_major;
	if (minor)
		*minor = tqsl_xml_config_minor;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumBand(int *number) {
	if (number == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_band())
		return 1;
	*number = BandList.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getBand(int index, const char **name, const char **spectrum, int *low, int *high) {
	if (index < 0 || name == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_band())
		return 1;
	if (index >= static_cast<int>(BandList.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*name = BandList[index].name.c_str();
	if (spectrum)
		*spectrum = BandList[index].spectrum.c_str();
	if (low)
		*low = BandList[index].low;
	if (high)
		*high = BandList[index].high;
	return 0;
}

static int
init_mode() {
	if (ModeList.size() > 0)
		return 0;
	XMLElement modes;
	if (tqsl_get_xml_config_section("modes", modes))
		return 1;
	XMLElement config_mode;
	bool ok = modes.getFirstElement("mode", config_mode);
	while (ok) {
		Mode m;
		m.mode = config_mode.getText();
		m.group = config_mode.getAttribute("group").first;
		ModeList.push_back(m);
		ok = modes.getNextElement(config_mode);
	}
	sort(ModeList.begin(), ModeList.end());
	return 0;
}

static int
init_propmode() {
	if (PropModeList.size() > 0)
		return 0;
	XMLElement propmodes;
	if (tqsl_get_xml_config_section("propmodes", propmodes))
		return 1;
	XMLElement config_mode;
	bool ok = propmodes.getFirstElement("propmode", config_mode);
	while (ok) {
		PropMode p;
		p.descrip = config_mode.getText();
		p.name = config_mode.getAttribute("name").first;
		PropModeList.push_back(p);
		ok = propmodes.getNextElement(config_mode);
	}
	sort(PropModeList.begin(), PropModeList.end());
	return 0;
}

static int
init_satellite() {
	if (SatelliteList.size() > 0)
		return 0;
	XMLElement satellites;
	if (tqsl_get_xml_config_section("satellites", satellites))
		return 1;
	XMLElement config_sat;
	bool ok = satellites.getFirstElement("satellite", config_sat);
	while (ok) {
		Satellite s;
		s.descrip = config_sat.getText();
		s.name = config_sat.getAttribute("name").first;
		tQSL_Date d;
		if (!tqsl_initDate(&d, config_sat.getAttribute("startDate").first.c_str()))
			s.start = d;
		if (!tqsl_initDate(&d, config_sat.getAttribute("endDate").first.c_str()))
			s.end = d;
		SatelliteList.push_back(s);
		ok = satellites.getNextElement(config_sat);
	}
	sort(SatelliteList.begin(), SatelliteList.end());
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumMode(int *number) {
	if (tqsl_init())
		return 1;
	if (number == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_mode())
		return 1;
	*number = ModeList.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getMode(int index, const char **mode, const char **group) {
	if (index < 0 || mode == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_mode())
		return 1;
	if (index >= static_cast<int>(ModeList.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*mode = ModeList[index].mode.c_str();
	if (group)
		*group = ModeList[index].group.c_str();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumDXCCEntity(int *number) {
	if (number == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc())
		return 1;
	*number = DXCCList.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDXCCEntity(int index, int *number, const char **name) {
	if (index < 0 || name == 0 || number == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc())
		return 1;
	if (index >= static_cast<int>(DXCCList.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*number = DXCCList[index].first;
	*name = DXCCList[index].second.c_str();
	return 0;
}


DLLEXPORT int CALLCONVENTION
tqsl_getDXCCEntityName(int number, const char **name) {
	if (name == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc())
		return 1;
	IntMap::const_iterator it;
	it = DXCCMap.find(number);
	if (it == DXCCMap.end()) {
		tQSL_Error = TQSL_NAME_NOT_FOUND;
		return 1;
	}
	*name = it->second.c_str();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getDXCCZoneMap(int number, const char **zonemap) {
	if (zonemap == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc())
		return 1;
	IntMap::const_iterator it;
	it = DXCCZoneMap.find(number);
	if (it == DXCCZoneMap.end()) {
		tQSL_Error = TQSL_NAME_NOT_FOUND;
		return 1;
	}
	const char *map = it->second.c_str();
	if (!map || map[0] == '\0')
		*zonemap = NULL;
	else
		*zonemap = map;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumPropagationMode(int *number) {
	if (tqsl_init())
		return 1;
	if (number == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_propmode())
		return 1;
	*number = PropModeList.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getPropagationMode(int index, const char **name, const char **descrip) {
	if (index < 0 || name == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_propmode())
		return 1;
	if (index >= static_cast<int>(PropModeList.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*name = PropModeList[index].name.c_str();
	if (descrip)
		*descrip = PropModeList[index].descrip.c_str();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumSatellite(int *number) {
	if (tqsl_init())
		return 1;
	if (number == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_satellite())
		return 1;
	*number = SatelliteList.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getSatellite(int index, const char **name, const char **descrip,
	tQSL_Date *start, tQSL_Date *end) {
	if (index < 0 || name == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_satellite())
		return 1;
	if (index >= static_cast<int>(SatelliteList.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*name = SatelliteList[index].name.c_str();
	if (descrip)
		*descrip = SatelliteList[index].descrip.c_str();
	if (start)
		*start = SatelliteList[index].start;
	if (end)
		*end = SatelliteList[index].end;
	return 0;
}

static int
init_cabrillo_map() {
	if (tqsl_cabrillo_map.size() > 0)
		return 0;
	XMLElement cabrillo_map;
	if (tqsl_get_xml_config_section("cabrillomap", cabrillo_map))
		return 1;
	XMLElement cabrillo_item;
	bool ok = cabrillo_map.getFirstElement("cabrillocontest", cabrillo_item);
	while (ok) {
		if (cabrillo_item.getText() != "" && strtol(cabrillo_item.getAttribute("field").first.c_str(), NULL, 10) > TQSL_MIN_CABRILLO_MAP_FIELD)
			tqsl_cabrillo_map[cabrillo_item.getText()] =
				make_pair(strtol(cabrillo_item.getAttribute("field").first.c_str(), NULL, 10)-1,
					(cabrillo_item.getAttribute("type").first == "VHF") ? TQSL_CABRILLO_VHF : TQSL_CABRILLO_HF);
		ok = cabrillo_map.getNextElement(cabrillo_item);
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_clearCabrilloMap() {
	tqsl_cabrillo_user_map.clear();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setCabrilloMapEntry(const char *contest, int field, int contest_type) {
	if (contest == 0 || field <= TQSL_MIN_CABRILLO_MAP_FIELD ||
		(contest_type != TQSL_CABRILLO_HF && contest_type != TQSL_CABRILLO_VHF)) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	tqsl_cabrillo_user_map[string_toupper(contest)] = make_pair(field-1, contest_type);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getCabrilloMapEntry(const char *contest, int *fieldnum, int *contest_type) {
	if (contest == 0 || fieldnum == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_cabrillo_map())
		return 1;
	map<string, pair<int, int> >::iterator it;
	if ((it = tqsl_cabrillo_user_map.find(string_toupper(contest))) == tqsl_cabrillo_user_map.end()) {
		if ((it = tqsl_cabrillo_map.find(string_toupper(contest))) == tqsl_cabrillo_map.end()) {
			*fieldnum = 0;
			return 0;
		}
	}
	*fieldnum = it->second.first + 1;
	if (contest_type)
		*contest_type = it->second.second;
	return 0;
}

static int
init_adif_map() {
	if (tqsl_adif_map.size() > 0)
		return 0;
	XMLElement adif_map;
	if (tqsl_get_xml_config_section("adifmap", adif_map))
		return 1;
	XMLElement adif_item;
	bool ok = adif_map.getFirstElement("adifmode", adif_item);
	while (ok) {
		if (adif_item.getText() != "" && adif_item.getAttribute("mode").first != "")
			tqsl_adif_map[adif_item.getText()] = adif_item.getAttribute("mode").first;
		ok = adif_map.getNextElement(adif_item);
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_clearADIFModes() {
	tqsl_adif_map.clear();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setADIFMode(const char *adif_item, const char *mode) {
	if (adif_item == 0 || mode == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_adif_map()) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - ADIF map invalid",
			sizeof tQSL_CustomError);
		return 1;
	}
	string umode = string_toupper(mode);
	tqsl_adif_map[string_toupper(adif_item)] = umode;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getADIFMode(const char *adif_item, char *mode, int nmode) {
	if (adif_item == 0 || mode == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_adif_map()) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - ADIF map invalid",
			sizeof tQSL_CustomError);
		return 1;
	}
	string orig = adif_item;
	orig = string_toupper(orig);
	string amode;
	if (tqsl_adif_map.find(orig) != tqsl_adif_map.end())
		amode = tqsl_adif_map[orig];
	if (nmode < static_cast<int>(amode.length())+1) {
		tQSL_Error = TQSL_BUFFER_ERROR;
		return 1;
	}
	strncpy(mode, amode.c_str(), nmode);
	return 0;
}

static int
init_loc_maps() {
	if (tqsl_field_map.size() > 0)
		return 0;
	XMLElement config_pages;
	if (tqsl_get_xml_config_section("locpages", config_pages))
		return 1;
	XMLElement config_page;
	tqsl_page_map.clear();
	bool ok;
	for (ok = config_pages.getFirstElement("page", config_page); ok; ok = config_pages.getNextElement(config_page)) {
		pair <string, bool> Id = config_page.getAttribute("Id");
		int page_num = strtol(Id.first.c_str(), NULL, 10);
		if (!Id.second || page_num < 1) {	// Must have the Id!
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, "TQSL Configuration file invalid - page missing ID",
				sizeof tQSL_CustomError);
			return 1;
		}
		tqsl_page_map[page_num] = config_page;
	}

	XMLElement config_fields;
	if (tqsl_get_xml_config_section("locfields", config_fields))
		return 1;
	XMLElement config_field;
	for (ok = config_fields.getFirstElement("field", config_field); ok; ok = config_fields.getNextElement(config_field)) {
		pair <string, bool> Id = config_field.getAttribute("Id");
		if (!Id.second) {	// Must have the Id!
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, "TQSL Configuration file invalid - field missing ID",
				sizeof tQSL_CustomError);
			return 1;
		}
		tqsl_field_map[Id.first] = config_field;
	}

	return 0;
}

static bool inMap(int cqvalue, int ituvalue, bool cqz, bool ituz, const char *map) {
/*
 * Parse the zone map and return true if the value is a valid zone number
 * The maps are colon-separated number pairs, with a list of pairs comma separated.
 */
	int cq, itu;
	bool result = false;

	// No map or empty string -> all match
	if (!map || map[0] == '\0') {
		return true;
	}

	char *mapcopy = strdup(map);
	char *mapPart = strtok(mapcopy, ",");
	while (mapPart) {
		sscanf(mapPart, "%d:%d", &itu, &cq);
		if (cqz && ituz) {
			if ((cq == cqvalue || cqvalue == 0) && (itu == ituvalue || ituvalue == 0)) {
				result = true;
				break;
			}
		} else if (cqz && (cq == cqvalue || cqvalue == 0)) {
			result = true;
			break;
		} else if (ituz && (itu == ituvalue || ituvalue == 0)) {
			result = true;
			break;
		}
		mapPart = strtok(NULL, ",");
	}
	free(mapcopy);
	return result;
}

static TQSL_LOCATION_FIELD *
get_location_field(int page, const string& gabbi, TQSL_LOCATION *loc) {
	if (page == 0)
		page = loc->page;
	for (; page > 0; page = loc->pagelist[page-1].prev) {
		TQSL_LOCATION_FIELDLIST& fl = loc->pagelist[page-1].fieldlist;
		for (int j = 0; j < static_cast<int>(fl.size()); j++) {
			if (fl[j].gabbi_name == gabbi)
				return &(fl[j]);
		}
	}
	return 0;
}


static int
update_page(int page, TQSL_LOCATION *loc) {
	TQSL_LOCATION_PAGE& p = loc->pagelist[page-1];
	int dxcc;
	int current_entity = -1;
	int loaded_cqz = -1;
	int loaded_ituz = -1;
	for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
		TQSL_LOCATION_FIELD& field = p.fieldlist[i];
		field.changed = false;
		if (field.gabbi_name == "CALL") {
			if (field.items.size() == 0 || loc->newflags) {
				// Build list of call signs from available certs
				field.changed = true;
				field.items.clear();
				loc->newflags = false;
				p.hash.clear();
				tQSL_Cert *certlist;
				int ncerts;
				tqsl_selectCertificates(&certlist, &ncerts, 0, 0, 0, 0, loc->cert_flags);
				for (int i = 0; i < ncerts; i++) {
					char callsign[40];
					tqsl_getCertificateCallSign(certlist[i], callsign, sizeof callsign);
					tqsl_getCertificateDXCCEntity(certlist[i], &dxcc);
					char ibuf[10];
					snprintf(ibuf, sizeof ibuf, "%d", dxcc);
					bool found = false;
					// Only add a given DXCC entity to a call once.
					map<string, vector<string> >::iterator call_p;
					for (call_p = p.hash.begin(); call_p != p.hash.end(); call_p++) {
						if (call_p->first == callsign && call_p->second[0] == ibuf) {
							found = true;
							break;
						}
					}
					if (!found)
						p.hash[callsign].push_back(ibuf);
					tqsl_freeCertificate(certlist[i]);
				}
				// Fill the call sign list
				map<string, vector<string> >::iterator call_p;
				field.idx = -1;
				for (call_p = p.hash.begin(); call_p != p.hash.end(); call_p++) {
					TQSL_LOCATION_ITEM item;
					item.text = call_p->first;
					if (item.text == field.cdata)
						field.idx = static_cast<int>(field.items.size());
					field.items.push_back(item);
				}
				if (field.idx >= 0) {
					field.cdata = field.items[field.idx].text;
				}
			}
		} else if (field.gabbi_name == "DXCC") {
			// Note: Expects CALL to be field 0 of this page.
			string call = p.fieldlist[0].cdata;
			if (field.items.size() == 0 || call != field.dependency) {
				// rebuild list
				field.changed = true;
				init_dxcc();
				int olddxcc = strtol(field.cdata.c_str(), NULL, 10);
				field.items.clear();
				field.idx = 0;
#ifdef DXCC_TEST
				const char *dxcc_test = getenv("TQSL_DXCC");
				if (dxcc_test) {
					vector<string> &entlist = p.hash[call];
					char *parse_dxcc = strdup(dxcc_test);
					char *cp = strtok(parse_dxcc, ",");
					while (cp) {
						if (find(entlist.begin(), entlist.end(), string(cp)) == entlist.end())
							entlist.push_back(cp);
						cp = strtok(0, ",");
					}
					free(parse_dxcc);
				}
#endif
				vector<string>::iterator ip;
				for (ip = p.hash[call].begin(); ip != p.hash[call].end(); ip++) {
					TQSL_LOCATION_ITEM item;
					item.text = *ip;
					item.ivalue = strtol(ip->c_str(), NULL, 10);
					IntMap::iterator dxcc_it = DXCCMap.find(item.ivalue);
					if (dxcc_it != DXCCMap.end()) {
						item.label = dxcc_it->second;
						item.zonemap = DXCCZoneMap[item.ivalue];
					}
					if (item.ivalue == olddxcc)
						field.idx = field.items.size();
					field.items.push_back(item);
				}
				if (field.items.size() > 0)
					field.cdata = field.items[field.idx].text;
				field.dependency = call;
			} // rebuild list
		} else {
			if (tqsl_field_map.find(field.gabbi_name) == tqsl_field_map.end()) {
				// Shouldn't happen!
				tQSL_Error = TQSL_CUSTOM_ERROR;
				strncpy(tQSL_CustomError, "TQSL Configuration file invalid - field map mismatch.",
					sizeof tQSL_CustomError);
				return 1;
			}
			XMLElement config_field = tqsl_field_map.find(field.gabbi_name)->second;
			pair<string, bool> attr = config_field.getAttribute("dependsOn");
			if (attr.first != "") {
				// Items list depends on other field
				TQSL_LOCATION_FIELD *fp = get_location_field(page, attr.first, loc);
				if (fp) {
					// Found the dependency field. Now find the enums to use
					string val = fp->cdata;
					if (fp->items.size() > 0)
						val = fp->items[fp->idx].text;
					if (val == field.dependency)
						continue;
					field.dependency = val;
					field.changed = true;
					field.items.clear();
					XMLElement enumlist;
					bool ok = config_field.getFirstElement("enums", enumlist);
					while (ok) {
						pair<string, bool> dependency = enumlist.getAttribute("dependency");
						if (dependency.second && dependency.first == val) {
							if (!(field.flags & TQSL_LOCATION_FIELD_MUSTSEL)) {
								TQSL_LOCATION_ITEM item;
								item.label = "[None]";
								field.items.push_back(item);
							}
							XMLElement enumitem;
							bool iok = enumlist.getFirstElement("enum", enumitem);
							while (iok) {
								TQSL_LOCATION_ITEM item;
								item.text = enumitem.getAttribute("value").first;
								item.label = enumitem.getText();
								item.zonemap = enumitem.getAttribute("zonemap").first;
								field.items.push_back(item);
								iok = enumlist.getNextElement(enumitem);
							}
						}
						ok = config_field.getNextElement(enumlist);
					} // enum loop
				} else {
					tQSL_Error = TQSL_CUSTOM_ERROR;
					strncpy(tQSL_CustomError, "TQSL Configuration file invalid - dependent field not found.",
						sizeof tQSL_CustomError);
					return 1;
				}
			} else {
				// No dependencies
				TQSL_LOCATION_FIELD *ent = get_location_field(page, "DXCC", loc);
				current_entity = strtol(ent->cdata.c_str(), NULL, 10);
				bool cqz = field.gabbi_name == "CQZ";
				bool ituz = field.gabbi_name == "ITUZ";
				if (field.items.size() == 0 || (cqz && current_entity != loaded_cqz) || (ituz && current_entity != loaded_ituz)) {
					XMLElement enumlist;
					if (config_field.getFirstElement("enums", enumlist)) {
						field.items.clear();
						field.changed = true;
						if (!(field.flags & TQSL_LOCATION_FIELD_MUSTSEL)) {
							TQSL_LOCATION_ITEM item;
							item.label = "[None]";
							field.items.push_back(item);
						}
						XMLElement enumitem;
						bool iok = enumlist.getFirstElement("enum", enumitem);
						while (iok) {
							TQSL_LOCATION_ITEM item;
							item.text = enumitem.getAttribute("value").first;
							item.label = enumitem.getText();
							item.zonemap = enumitem.getAttribute("zonemap").first;
							field.items.push_back(item);
							iok = enumlist.getNextElement(enumitem);
						}
					} else {
						// No enums supplied
						int ftype = strtol(config_field.getAttribute("intype").first.c_str(), NULL, 10);
						if (ftype == TQSL_LOCATION_FIELD_LIST || ftype == TQSL_LOCATION_FIELD_DDLIST) {
							// This a list field
							int lower = strtol(config_field.getAttribute("lower").first.c_str(), NULL, 10);
							int upper = strtol(config_field.getAttribute("upper").first.c_str(), NULL, 10);
							const char *zoneMap;
							/* Get the map */
							if (tqsl_getDXCCZoneMap(current_entity, &zoneMap)) {
								zoneMap = NULL;
							}
							if (upper < lower) {
								tQSL_Error = TQSL_CUSTOM_ERROR;
								strncpy(tQSL_CustomError, "TQSL Configuration file invalid - field range order incorrect.",
									sizeof tQSL_CustomError);
								return 1;
							}
							field.items.clear();
							field.changed = true;
							if (cqz)
								loaded_cqz = current_entity;
							if (ituz)
								loaded_ituz = current_entity;
							if (!(field.flags & TQSL_LOCATION_FIELD_MUSTSEL)) {
								TQSL_LOCATION_ITEM item;
								item.label = "[None]";
								field.items.push_back(item);
							}
							char buf[40];
							for (int j = lower; j <= upper; j++) {
								if (!zoneMap || inMap(j, j, cqz, ituz, zoneMap)) {
									snprintf(buf, sizeof buf, "%d", j);
									TQSL_LOCATION_ITEM item;
									item.text = buf;
									item.ivalue = j;
									field.items.push_back(item);
								}
							}
						} // intype != TEXT
					} // enums supplied
				} // itemlist not empty and current entity
			} // no dependencies
		} // field name not CALL|DXCC
	} // field loop

	/* Sanity check zones */
	bool zonesok = true;
	// Try for subdivision info first
	string zone_error = "";
	TQSL_LOCATION_FIELD *state = get_location_field(page, "US_STATE", loc);
	if (state) {
		zone_error = "Invalid zone selections for state";
	} else {
		state = get_location_field(page, "CA_PROVINCE", loc);
		if (state) {
			zone_error = "Invalid zone selections for province";
		} else {
			state = get_location_field(page, "RU_OBLAST", loc);
			if (state) {
				zone_error = "Invalid zone selections for oblast";
			} else {
				// If no subdivision, use entity.
				state = get_location_field(page, "DXCC", loc);
				zone_error = "Invalid zone selections for DXCC entity";
			}
		}
	}

	if (state && state->items.size() > 0) {
		TQSL_LOCATION_FIELD *cqz = get_location_field(page, "CQZ", loc);
		TQSL_LOCATION_FIELD *ituz = get_location_field(page, "ITUZ", loc);
		string szm = state->items[state->idx].zonemap;
		const char* stateZoneMap = szm.c_str();
		int currentCQ = cqz->idata;
		int currentITU = ituz->idata;

		if (!inMap(currentCQ, currentITU, true, true, stateZoneMap)) {
			zonesok = false;
		}
		TQSL_LOCATION_FIELD *zerr = get_location_field(page, "ZERR", loc);
		if (zerr) {
			if(!zonesok) {
				zerr->cdata = zone_error;
			} else {
				zerr->cdata = "";
			}
		}
	}
	p.complete = true;
	return 0;
}

static int
make_page(TQSL_LOCATION_PAGELIST& pagelist, int page_num) {
	if (init_loc_maps())
		return 1;

	if (tqsl_page_map.find(page_num) == tqsl_page_map.end()) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, "TQSL Configuration file invalid - page reference could not be found.",
			sizeof tQSL_CustomError);
		return 1;
	}

	TQSL_LOCATION_PAGE p;
	pagelist.push_back(p);

	XMLElement& config_page = tqsl_page_map[page_num];

	pagelist.back().prev = strtol(config_page.getAttribute("follows").first.c_str(), NULL, 10);
	XMLElement config_pageField;
	bool field_ok = config_page.getFirstElement("pageField", config_pageField);
	while (field_ok) {
		string field_name = config_pageField.getText();
		if (field_name == "" || tqsl_field_map.find(field_name) == tqsl_field_map.end()) {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, "TQSL Configuration file invalid - page references undefined field.",
				sizeof tQSL_CustomError);
			return 1;
		}
		XMLElement& config_field = tqsl_field_map[field_name];
		TQSL_LOCATION_FIELD loc_field(
			field_name,
			config_field.getAttribute("label").first.c_str(),
			(config_field.getAttribute("type").first == "C") ? TQSL_LOCATION_FIELD_CHAR : TQSL_LOCATION_FIELD_INT,
			strtol(config_field.getAttribute("len").first.c_str(), NULL, 10),
			strtol(config_field.getAttribute("intype").first.c_str(), NULL, 10),
			strtol(config_field.getAttribute("flags").first.c_str(), NULL, 10)
		);	// NOLINT(whitespace/parens)
		pagelist.back().fieldlist.push_back(loc_field);
		field_ok = config_page.getNextElement(config_pageField);
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_initStationLocationCapture(tQSL_Location *locp) {
	if (tqsl_init())
		return 1;
	if (locp == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION *loc = new TQSL_LOCATION;
	*locp = loc;
	if (init_loc_maps())
		return 1;
	map<int, XMLElement>::iterator pit;
	for (pit = tqsl_page_map.begin(); pit != tqsl_page_map.end(); pit++) {
		if (make_page(loc->pagelist, pit->first))
			return 1;
	}

	loc->page = 1;
	if (update_page(1, loc))
		return 1;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_endStationLocationCapture(tQSL_Location *locp) {
	if (tqsl_init())
		return 1;
	if (locp == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (*locp == 0)
		return 0;
	if (CAST_TQSL_LOCATION(*locp)->sentinel == 0x5445)
		delete CAST_TQSL_LOCATION(*locp);
	*locp = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_updateStationLocationCapture(tQSL_Location locp) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
//	TQSL_LOCATION_PAGE &p = loc->pagelist[loc->page-1];
	return update_page(loc->page, loc);
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumStationLocationCapturePages(tQSL_Location locp, int *npages) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (npages == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*npages = loc->pagelist.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationCapturePage(tQSL_Location locp, int *page) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (page == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*page = loc->page;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setStationLocationCapturePage(tQSL_Location locp, int page) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (page < 1 || page > static_cast<int>(loc->pagelist.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	loc->page = page;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setStationLocationCertFlags(tQSL_Location locp, int flags) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (loc->cert_flags != flags) {
		loc->cert_flags = flags;
		loc->newflags = true;
		loc->page = 1;
		if (update_page(1, loc))
			return 1;
	}
	return 0;
}


static int
find_next_page(TQSL_LOCATION *loc) {
	// Set next page based on page dependencies
	TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];
	map<int, XMLElement>::iterator pit;
	p.next = 0;
	for (pit = tqsl_page_map.begin(); pit != tqsl_page_map.end(); pit++) {
		if (strtol(pit->second.getAttribute("follows").first.c_str(), NULL, 10) == loc->page) {
			string dependsOn = pit->second.getAttribute("dependsOn").first;
			string dependency = pit->second.getAttribute("dependency").first;
			if (dependsOn == "") {
				p.next = pit->first;
				break;
			}
			TQSL_LOCATION_FIELD *fp = get_location_field(0, dependsOn, loc);
			//if (fp->idx>=fp->items.size()) { cerr<<"!! " __FILE__ "(" << __LINE__ << "): Was going to index out of fp->items"<<endl; }
			//else {
			if (static_cast<int>(fp->items.size()) > fp->idx && fp->items[fp->idx].text == dependency) {
				p.next = pit->first;
				break;	// Found next page
			//}
			}
		}
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_nextStationLocationCapture(tQSL_Location locp) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (find_next_page(loc))
		return 0;
	TQSL_LOCATION_PAGE &p = loc->pagelist[loc->page-1];
	if (p.next > 0)
		loc->page = p.next;
	update_page(loc->page, loc);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_prevStationLocationCapture(tQSL_Location locp) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_PAGE &p = loc->pagelist[loc->page-1];
//cerr << "prev: " << p.prev << endl;
	if (p.prev > 0)
		loc->page = p.prev;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_hasNextStationLocationCapture(tQSL_Location locp, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (rval == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (find_next_page(loc))
		return 1;
	*rval = (loc->pagelist[loc->page-1].next > 0);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_hasPrevStationLocationCapture(tQSL_Location locp, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (rval == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = (loc->pagelist[loc->page-1].prev > 0);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumLocationField(tQSL_Location locp, int *numf) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (numf == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	*numf = fl.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataLabelSize(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;

	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (rval == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = fl[field_num].label.size()+1;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataLabel(tQSL_Location locp, int field_num, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, fl[field_num].label.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataGABBISize(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;

	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (rval == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = fl[field_num].gabbi_name.size()+1;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataGABBI(tQSL_Location locp, int field_num, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, fl[field_num].gabbi_name.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldInputType(tQSL_Location locp, int field_num, int *type) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (type == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*type = fl[field_num].input_type;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldChanged(tQSL_Location locp, int field_num, int *changed) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (changed == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*changed = fl[field_num].changed;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataType(tQSL_Location locp, int field_num, int *type) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (type == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*type = fl[field_num].data_type;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldFlags(tQSL_Location locp, int field_num, int *flags) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (flags == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*flags = fl[field_num].flags;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldDataLength(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (rval == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = fl[field_num].data_len;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldCharData(tQSL_Location locp, int field_num, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (fl[field_num].flags & TQSL_LOCATION_FIELD_UPPER)
		strncpy(buf, string_toupper(fl[field_num].cdata).c_str(), bufsiz);
	else
		strncpy(buf, fl[field_num].cdata.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldIntData(tQSL_Location locp, int field_num, int *dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (dat == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*dat = fl[field_num].idata;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldIndex(tQSL_Location locp, int field_num, int *dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (dat == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (fl[field_num].input_type != TQSL_LOCATION_FIELD_DDLIST
		&& fl[field_num].input_type != TQSL_LOCATION_FIELD_LIST) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*dat = fl[field_num].idx;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setLocationFieldCharData(tQSL_Location locp, int field_num, const char *buf) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	fl[field_num].cdata = string(buf).substr(0, fl[field_num].data_len);
	if (fl[field_num].flags & TQSL_LOCATION_FIELD_UPPER)
		fl[field_num].cdata = string_toupper(fl[field_num].cdata);
	return 0;
}

/* Set the field's index. For pick lists, this is the index into
 * 'items'. In that case, also set the field's data to the picked value.
 */
DLLEXPORT int CALLCONVENTION
tqsl_setLocationFieldIndex(tQSL_Location locp, int field_num, int dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	fl[field_num].idx = dat;
	if (fl[field_num].input_type == TQSL_LOCATION_FIELD_DDLIST
		|| fl[field_num].input_type == TQSL_LOCATION_FIELD_LIST) {
		if (dat >= 0 && dat < static_cast<int>(fl[field_num].items.size())) {
			fl[field_num].idx = dat;
			fl[field_num].cdata = fl[field_num].items[dat].text;
			fl[field_num].idata = fl[field_num].items[dat].ivalue;
		} else {
			tQSL_Error = TQSL_ARGUMENT_ERROR;
			return 1;
		}
	}
	return 0;
}

/* Set the field's integer data.
 */
DLLEXPORT int CALLCONVENTION
tqsl_setLocationFieldIntData(tQSL_Location locp, int field_num, int dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (field_num < 0 || field_num >= static_cast<int>(fl.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	fl[field_num].idata = dat;
	return 0;
}

/* For pick lists, this is the index into
 * 'items'. In that case, also set the field's char data to the picked value.
 */
DLLEXPORT int CALLCONVENTION
tqsl_getNumLocationFieldListItems(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (rval == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	*rval = fl[field_num].items.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationFieldListItem(tQSL_Location locp, int field_num, int item_idx, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == 0 || field_num < 0 || field_num >= static_cast<int>(fl.size())
		|| (fl[field_num].input_type != TQSL_LOCATION_FIELD_LIST
		&& fl[field_num].input_type != TQSL_LOCATION_FIELD_DDLIST)) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (item_idx < 0 || item_idx >= static_cast<int>(fl[field_num].items.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	string& str = (fl[field_num].items[item_idx].label == "")
		? fl[field_num].items[item_idx].text
		: fl[field_num].items[item_idx].label;
	strncpy(buf, str.c_str(), bufsiz);
	return 0;
}

static string
tqsl_station_data_filename(const char *f = "station_data") {
	string s = tQSL_BaseDir;
#ifdef _WIN32
	s += "\\";
#else
	s += "/";
#endif
	s += f;
	return s;
}

static int
tqsl_load_station_data(XMLElement &xel) {
	int status = xel.parseFile(tqsl_station_data_filename().c_str());
	if (status) {
		if (errno == ENOENT)		// If there's no file, no error.
			return 0;
		strncpy(tQSL_ErrorFile, tqsl_station_data_filename().c_str(), sizeof tQSL_ErrorFile);
		if (status == XML_PARSE_SYSTEM_ERROR) {
			tQSL_Error = TQSL_FILE_SYSTEM_ERROR;
			tQSL_Errno = errno;
		} else {
			tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
		}
		return 1;
	}
	return status;
}

static int
tqsl_dump_station_data(XMLElement &xel) {
	ofstream out;
	string fn = tqsl_station_data_filename();

	out.exceptions(ios::failbit | ios::eofbit | ios::badbit);
	try {
		out.open(fn.c_str());
		out << xel << endl;
		out.close();
	}
	catch(exception& x) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
				"Unable to save new station location file (%s): %s/%s",
				fn.c_str(), x.what(), strerror(errno));
		return 1;
	}
	return 0;
}

static int
tqsl_load_loc(TQSL_LOCATION *loc, XMLElementList::iterator ep, bool ignoreZones) {
	bool exists;
	loc->page = 1;
	loc->data_errors[0] = '\0';
	int bad_ituz = 0;
	int bad_cqz = 0;
	while(1) {
		TQSL_LOCATION_PAGE& page = loc->pagelist[loc->page-1];
		for (int fidx = 0; fidx < static_cast<int>(page.fieldlist.size()); fidx++) {
			TQSL_LOCATION_FIELD& field = page.fieldlist[fidx];
			if (field.gabbi_name != "") {
				// A field that may exist
				XMLElement el;
				if (ep->second.getFirstElement(field.gabbi_name, el)) {
					field.cdata = el.getText();
					switch (field.input_type) {
                                                case TQSL_LOCATION_FIELD_DDLIST:
                                                case TQSL_LOCATION_FIELD_LIST:
							exists = false;
							for (int i = 0; i < static_cast<int>(field.items.size()); i++) {
								string cp = field.items[i].text;
								int q = strcasecmp(field.cdata.c_str(), cp.c_str());
								if (q == 0) {
									field.idx = i;
									field.cdata = cp;
									field.idata = field.items[i].ivalue;
									exists = true;
									break;
								}
							}
							if (!exists) {
								if (field.gabbi_name == "CQZ")
									bad_cqz = strtol(field.cdata.c_str(), NULL, 10);
								else if (field.gabbi_name == "ITUZ")
									bad_ituz = strtol(field.cdata.c_str(), NULL, 10);
								else if (field.gabbi_name == "CALL" || field.gabbi_name == "DXCC")
									field.idx = -1;
							}
							break;
                                                case TQSL_LOCATION_FIELD_TEXT:
							field.cdata = trim(field.cdata);
							if (field.data_type == TQSL_LOCATION_FIELD_INT)
								field.idata = strtol(field.cdata.c_str(), NULL, 10);
							break;
					}
				}
			}
			if (update_page(loc->page, loc))
				return 1;
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		tqsl_nextStationLocationCapture(loc);
	}
	if (ignoreZones)
		return 0;
	if (bad_cqz && bad_ituz) {
		snprintf(loc->data_errors, sizeof(loc->data_errors),
			"This station location is configured with invalid CQ zone %d and invalid ITU zone %d.", bad_cqz, bad_ituz);
	} else if (bad_cqz) {
		snprintf(loc->data_errors, sizeof(loc->data_errors), "This station location is configured with invalid CQ zone %d.", bad_cqz);
	} else if (bad_ituz) {
		snprintf(loc->data_errors, sizeof(loc->data_errors), "This station location is configured with invalid ITU zone %d.", bad_ituz);
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
	tqsl_getStationDataEnc(tQSL_StationDataEnc *sdata) {
	char *dbuf = NULL;
	size_t dlen = 0;
	gzFile in = gzopen(tqsl_station_data_filename().c_str(), "rb");

	if (!in) {
		if (errno == ENOENT) {
			*sdata = NULL;
			return 0;
		}
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		strncpy(tQSL_ErrorFile, tqsl_station_data_filename().c_str(), sizeof tQSL_ErrorFile);
		return 1;
	}
	char buf[2048];
	int rcount;
	while ((rcount = gzread(in, buf, sizeof buf)) > 0) {
		dlen += rcount;
	}
	dbuf = reinterpret_cast<char *>(malloc(dlen + 2));
	if (!dbuf)
		return 1;
	*sdata = dbuf;

	gzrewind(in);
	while ((rcount = gzread(in, dbuf, sizeof buf)) > 0) {
		dbuf += rcount;
	}
	*dbuf = '\0';
	gzclose(in);
	return 0;
}

DLLEXPORT int CALLCONVENTION
	tqsl_freeStationDataEnc(tQSL_StationDataEnc sdata) {
	if (sdata)
		free(sdata);
	return 0; //can never fail
}

DLLEXPORT int CALLCONVENTION
tqsl_mergeStationLocations(const char *locdata) {
	XMLElement sfile;
	XMLElement new_top_el;
	XMLElement top_el;
	vector<string> calls;

	if (tqsl_load_station_data(top_el))
		return 1;
	new_top_el.parseString(locdata);

	if (!new_top_el.getFirstElement(sfile))
		sfile.setElementName("StationDataFile");

	// Build  alist of valid calls
	tQSL_Cert *certlist;
	int ncerts;
	tqsl_selectCertificates(&certlist, &ncerts, 0, 0, 0, 0, TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED | TQSL_SELECT_CERT_SUPERCEDED);
	calls.clear();
	for (int i = 0; i < ncerts; i++) {
		char callsign[40];
		tqsl_getCertificateCallSign(certlist[i], callsign, sizeof(callsign));
		calls.push_back(callsign);
		tqsl_freeCertificate(certlist[i]);
	}

	XMLElementList& ellist = sfile.getElementList();
	XMLElementList::iterator ep;
	XMLElement call;
	for (ep = ellist.find("StationData"); ep != ellist.end(); ep++) {
		if (ep->first != "StationData")
			break;
		pair<string, bool> rval = ep->second.getAttribute("name");
		if (rval.second) {
			TQSL_LOCATION *oldloc;
			TQSL_LOCATION *newloc;
			ep->second.getFirstElement("CALL", call);
			for (size_t j = 0; j < calls.size(); j++) {
				if (calls[j] == call.getText()) {
					if (tqsl_getStationLocation(reinterpret_cast<tQSL_Location *>(&oldloc), rval.first.c_str())) { // Location doesn't exist
						if (tqsl_initStationLocationCapture(reinterpret_cast<tQSL_Location *>(&newloc)) == 0) {
							if (tqsl_load_loc(newloc, ep, true) == 0) {	// new loads OK
								tqsl_setStationLocationCaptureName(newloc, rval.first.c_str());
								tqsl_saveStationLocationCapture(newloc, 0);
								tqsl_endStationLocationCapture(reinterpret_cast<tQSL_Location *>(&newloc));
							}
						}
					} else {
						tqsl_endStationLocationCapture(reinterpret_cast<tQSL_Location *>(&oldloc));
					}
				}
			}
		}
	}
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_deleteStationLocation(const char *name) {
	XMLElement top_el;
	if (tqsl_load_station_data(top_el))
		return 1;
	XMLElement sfile;
	if (!top_el.getFirstElement(sfile))
		sfile.setElementName("StationDataFile");

	XMLElementList& ellist = sfile.getElementList();
	XMLElementList::iterator ep;
	for (ep = ellist.find("StationData"); ep != ellist.end(); ep++) {
		if (ep->first != "StationData")
			break;
		pair<string, bool> rval = ep->second.getAttribute("name");
		if (rval.second && !strcasecmp(rval.first.c_str(), name)) {
			ellist.erase(ep);
			return tqsl_dump_station_data(sfile);
		}
	}
	tQSL_Error = TQSL_LOCATION_NOT_FOUND;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocation(tQSL_Location *locp, const char *name) {
	if (tqsl_initStationLocationCapture(locp))
		return 1;
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(*locp)))
		return 1;

	loc->name = name;
	XMLElement top_el;
	if (tqsl_load_station_data(top_el))
		return 1;
	XMLElement sfile;
	if (!top_el.getFirstElement(sfile))
		sfile.setElementName("StationDataFile");

	XMLElementList& ellist = sfile.getElementList();

	bool exists = false;
	XMLElementList::iterator ep;
	for (ep = ellist.find("StationData"); ep != ellist.end(); ep++) {
		if (ep->first != "StationData")
			break;
		pair<string, bool> rval = ep->second.getAttribute("name");
		if (rval.second && !strcasecmp(trim(rval.first).c_str(), trim(loc->name).c_str())) {
			exists = true;
			break;
		}
	}
	if (!exists) {
		tQSL_Error = TQSL_LOCATION_NOT_FOUND;
		return 1;
	}
	return tqsl_load_loc(loc, ep, false);
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationErrors(tQSL_Location locp, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	strncpy(buf, loc->data_errors, bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumStationLocations(tQSL_Location locp, int *nloc) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (nloc == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	loc->names.clear();
	XMLElement top_el;
	if (tqsl_load_station_data(top_el))
		return 1;
	XMLElement sfile;
	if (top_el.getFirstElement(sfile)) {
		XMLElement sd;
		bool ok = sfile.getFirstElement("StationData", sd);
		while (ok && sd.getElementName() == "StationData") {
			pair<string, bool> name = sd.getAttribute("name");
			if (name.second) {
				XMLElement xc;
				string call;
				if (sd.getFirstElement("CALL", xc))
					call = xc.getText();
				loc->names.push_back(TQSL_NAME(name.first, call));
			}
			ok = sfile.getNextElement(sd);
		}
	}
	*nloc = loc->names.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationName(tQSL_Location locp, int idx, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (buf == 0 || idx < 0 || idx >= static_cast<int>(loc->names.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, loc->names[idx].name.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationCallSign(tQSL_Location locp, int idx, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (buf == 0 || idx < 0 || idx >= static_cast<int>(loc->names.size())) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, loc->names[idx].call.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationField(tQSL_Location locp, const char *name, char *namebuf, int bufsize) {
	int old_page;
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (name == NULL || namebuf == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (tqsl_getStationLocationCapturePage(loc, &old_page))
		return 1;
	string find = name;

	tqsl_setStationLocationCapturePage(loc, 1);
	do {
		int numf;
		if (tqsl_getNumLocationField(loc, &numf))
			return 1;
		for (int i = 0; i < numf; i++) {
			TQSL_LOCATION_FIELD& field = loc->pagelist[loc->page-1].fieldlist[i];
			if (find == field.gabbi_name) {	// Found it
				switch (field.input_type) {
                                        case TQSL_LOCATION_FIELD_DDLIST:
                                        case TQSL_LOCATION_FIELD_LIST:
						if (field.data_type == TQSL_LOCATION_FIELD_INT) {
							char numbuf[20];
							if (static_cast<int>(field.items.size()) <= field.idx) {
								strncpy(namebuf, field.cdata.c_str(), bufsize);
							} else if (field.idx == 0 && field.items[field.idx].label == "[None]") {
								strncpy(namebuf, "", bufsize);
							} else {
								snprintf(numbuf, sizeof numbuf, "%d", field.items[field.idx].ivalue);
								strncpy(namebuf, numbuf, bufsize);
							}
						} else if (field.idx < 0 || field.idx >= static_cast<int>(field.items.size())) {
							// Allow CALL to not be in the items list
							if (field.idx == -1 && i == 0)
								strncpy(namebuf, field.cdata.c_str(), bufsize);
							else
								strncpy(namebuf, "", bufsize);
						} else {
							strncpy(namebuf, field.items[field.idx].text.c_str(), bufsize);
						}
						break;
                                        case TQSL_LOCATION_FIELD_TEXT:
						field.cdata = trim(field.cdata);
						if (field.flags & TQSL_LOCATION_FIELD_UPPER)
							field.cdata = string_toupper(field.cdata);
						strncpy(namebuf, field.cdata.c_str(), bufsize);
						break;
				}
				goto done;
			}
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		if (tqsl_nextStationLocationCapture(loc))
			return 1;
	} while (1);
	strncpy(namebuf, "", bufsize);		// Did not find it
 done:
	tqsl_setStationLocationCapturePage(loc, old_page);
	return 0;
}

static int
tqsl_location_to_xml(TQSL_LOCATION *loc, XMLElement& sd) {
	int old_page;
	if (tqsl_getStationLocationCapturePage(loc, &old_page))
		return 1;
	tqsl_setStationLocationCapturePage(loc, 1);
	do {
		int numf;
		if (tqsl_getNumLocationField(loc, &numf))
			return 1;
		for (int i = 0; i < numf; i++) {
			TQSL_LOCATION_FIELD& field = loc->pagelist[loc->page-1].fieldlist[i];
			XMLElement fd;
			fd.setPretext(sd.getPretext() + "  ");
			fd.setElementName(field.gabbi_name);
			switch (field.input_type) {
                                case TQSL_LOCATION_FIELD_DDLIST:
                                case TQSL_LOCATION_FIELD_LIST:
					if (field.idx < 0 || field.idx >= static_cast<int>(field.items.size())) {
						fd.setText("");
					} else if (field.data_type == TQSL_LOCATION_FIELD_INT) {
						char numbuf[20];
						snprintf(numbuf, sizeof numbuf, "%d", field.items[field.idx].ivalue);
						fd.setText(numbuf);
					} else {
						fd.setText(field.items[field.idx].text);
					}
					break;
                                case TQSL_LOCATION_FIELD_TEXT:
					field.cdata = trim(field.cdata);
					if (field.flags & TQSL_LOCATION_FIELD_UPPER)
						field.cdata = string_toupper(field.cdata);
					fd.setText(field.cdata);
					break;
			}
			if (strcmp(fd.getText().c_str(), ""))
				sd.addElement(fd);
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		if (tqsl_nextStationLocationCapture(loc))
			return 1;
	} while (1);
	tqsl_setStationLocationCapturePage(loc, old_page);
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_setStationLocationCaptureName(tQSL_Location locp, const char *name) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (name == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	loc->name = name;
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getStationLocationCaptureName(tQSL_Location locp, char *namebuf, int bufsize) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (namebuf == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(namebuf, loc->name.c_str(), bufsize);
	namebuf[bufsize-1] = 0;
	return 0;
}


DLLEXPORT int CALLCONVENTION
tqsl_saveStationLocationCapture(tQSL_Location locp, int overwrite) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (loc->name == "") {
		tQSL_Error = TQSL_EXPECTED_NAME;
		return 1;
	}
	XMLElement top_el;
	if (tqsl_load_station_data(top_el))
		return 1;
	XMLElement sfile;
	if (!top_el.getFirstElement(sfile))
		sfile.setElementName("StationDataFile");

	XMLElementList& ellist = sfile.getElementList();
	bool exists = false;
	XMLElementList::iterator ep;
	for (ep = ellist.find("StationData"); ep != ellist.end(); ep++) {
		if (ep->first != "StationData")
			break;
		pair<string, bool> rval = ep->second.getAttribute("name");
		if (rval.second && !strcasecmp(rval.first.c_str(), loc->name.c_str())) {
			exists = true;
			break;
		}
	}
	if (exists && !overwrite) {
		tQSL_Error = TQSL_NAME_EXISTS;
		return 1;
	}
	XMLElement sd("StationData");
	sd.setPretext("\n  ");
	if (tqsl_location_to_xml(loc, sd))
		return 1;
	sd.setAttribute("name", loc->name);
	sd.setText("\n  ");

	// If 'exists', ep points to the existing station record
	if (exists)
		ellist.erase(ep);

	sfile.addElement(sd);
	sfile.setText("\n");
	return tqsl_dump_station_data(sfile);
}


DLLEXPORT int CALLCONVENTION
tqsl_signQSORecord(tQSL_Cert cert, tQSL_Location locp, TQSL_QSO_RECORD *rec, unsigned char *sig, int *siglen) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false)))
		return 1;
	if (make_sign_data(loc))
		return 1;
	XMLElement specfield;
	bool ok = tCONTACT_sign.getFirstElement(specfield);
	string rec_sign_data = loc->signdata;
	while (ok) {
		string eln = specfield.getElementName();
		const char *elname = eln.c_str();
		const char *value = 0;
		char buf[100];
		if (!strcmp(elname, "CALL"))
			value = rec->callsign;
		else if (!strcmp(elname, "BAND"))
			value = rec->band;
		else if (!strcmp(elname, "BAND_RX"))
			value = rec->rxband;
		else if (!strcmp(elname, "MODE"))
			value = rec->mode;
		else if (!strcmp(elname, "FREQ"))
			value = rec->freq;
		else if (!strcmp(elname, "FREQ_RX"))
			value = rec->rxfreq;
		else if (!strcmp(elname, "PROP_MODE"))
			value = rec->propmode;
		else if (!strcmp(elname, "SAT_NAME"))
			value = rec->satname;
		else if (!strcmp(elname, "QSO_DATE")) {
			if (tqsl_isDateValid(&(rec->date)))
				value = tqsl_convertDateToText(&(rec->date), buf, sizeof buf);
		} else if (!strcmp(elname, "QSO_TIME")) {
			if (tqsl_isTimeValid(&(rec->time)))
				value = tqsl_convertTimeToText(&(rec->time), buf, sizeof buf);
		} else {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
				"Unknown field in signing specification: %s", elname);
			return 1;
		}
		if (value == 0 || value[0] == 0) {
			pair<string, bool> attr = specfield.getAttribute("required");
			if (attr.second && strtol(attr.first.c_str(), NULL, 10)) {
				string err = specfield.getElementName() + " field required by signature specification not found";
				tQSL_Error = TQSL_CUSTOM_ERROR;
				strncpy(tQSL_CustomError, err.c_str(), sizeof tQSL_CustomError);
				return 1;
			}
		} else {
			string v(value);
			rec_sign_data += trim(v);
		}
		ok = tCONTACT_sign.getNextElement(specfield);
	}
	return tqsl_signDataBlock(cert, (const unsigned char *)rec_sign_data.c_str(), rec_sign_data.size(), sig, siglen);
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getGABBItCERT(tQSL_Cert cert, int uid) {
	static string s;

	s = "";
	char buf[3000];
	if (tqsl_getCertificateEncoded(cert, buf, sizeof buf))
		return 0;
	char *cp = strstr(buf, "-----END CERTIFICATE-----");
	if (cp)
		*cp = 0;
	if ((cp = strstr(buf, "\n")))
		cp++;
	else
		cp = buf;
	s = "<Rec_Type:5>tCERT\n";
	char sbuf[10], lbuf[40];
	snprintf(sbuf, sizeof sbuf, "%d", uid);
	snprintf(lbuf, sizeof lbuf, "<CERT_UID:%d>%s\n", static_cast<int>(strlen(sbuf)), sbuf);
	s += lbuf;
	snprintf(lbuf, sizeof lbuf, "<CERTIFICATE:%d>", static_cast<int>(strlen(cp)));
	s += lbuf;
	s += cp;
	s += "<eor>\n";
	return s.c_str(); //KC2YWE 1/26 - dangerous but might work since s is static
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getGABBItSTATION(tQSL_Location locp, int uid, int certuid) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false)))
		return 0;
	unsigned char *buf = 0;
	int bufsiz = 0;
	loc->tSTATION = "<Rec_Type:8>tSTATION\n";
	char sbuf[10], lbuf[40];
	snprintf(sbuf, sizeof sbuf, "%d", uid);
	snprintf(lbuf, sizeof lbuf, "<STATION_UID:%d>%s\n", static_cast<int>(strlen(sbuf)), sbuf);
	loc->tSTATION += lbuf;
	snprintf(sbuf, sizeof sbuf, "%d", certuid);
	snprintf(lbuf, sizeof lbuf, "<CERT_UID:%d>%s\n", static_cast<int>(strlen(sbuf)), sbuf);
	loc->tSTATION += lbuf;
	int old_page = loc->page;
	tqsl_setStationLocationCapturePage(loc, 1);
	do {
		TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];
		for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
			TQSL_LOCATION_FIELD& f = p.fieldlist[i];
			string s;
			if (f.input_type == TQSL_LOCATION_FIELD_BADZONE)	// Don't output these to tSTATION
				continue;

			if (f.input_type == TQSL_LOCATION_FIELD_DDLIST || f.input_type == TQSL_LOCATION_FIELD_LIST) {
				if (f.idx < 0 || f.idx >= static_cast<int>(f.items.size())) {
					s = "";
				} else {
					/* Alaska counties are stored as 'pseudo-county|real-county'
					 * so output just the real county name
					 */
					s = f.items[f.idx].text;
					size_t pos = s.find("|");
					if (pos != string::npos)
						s = s.substr(++pos, string::npos);
				}
			} else if (f.data_type == TQSL_LOCATION_FIELD_INT) {
				char buf[20];
				snprintf(buf, sizeof buf, "%d", f.idata);
				s = buf;
			} else {
				s = f.cdata;
			}
			if (s.size() == 0)
				continue;
			int wantsize = s.size() + f.gabbi_name.size() + 20;
			if (buf == 0 || bufsiz < wantsize) {
				if (buf != 0)
					delete[] buf;
				buf = new unsigned char[wantsize];
				bufsiz = wantsize;
			}
			if (tqsl_adifMakeField(f.gabbi_name.c_str(), 0, (unsigned char *)s.c_str(), s.size(), buf, bufsiz)) {
				delete[] buf;
				return 0;
			}
			loc->tSTATION += (const char *)buf;
			loc->tSTATION += "\n";
		}
		int rval;
		if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
			break;
		tqsl_nextStationLocationCapture(loc);
	} while (1);
	tqsl_setStationLocationCapturePage(loc, old_page);
	if (buf != 0)
		delete[] buf;
	loc->tSTATION += "<eor>\n";
	return loc->tSTATION.c_str();
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getGABBItCONTACTData(tQSL_Cert cert, tQSL_Location locp, TQSL_QSO_RECORD *qso, int stationuid, char* signdata, int sdlen) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false)))
		return 0;
	if (make_sign_data(loc))
		return 0;
	XMLElement specfield;
	bool ok = tCONTACT_sign.getFirstElement(specfield);
	string rec_sign_data = loc->signdata;
	while(ok) {
		string en = specfield.getElementName();
		const char *elname = en.c_str();
		const char *value = 0;
		char buf[100];
		if (!strcmp(elname, "CALL"))
			value = qso->callsign;
		else if (!strcmp(elname, "BAND"))
			value = qso->band;
		else if (!strcmp(elname, "BAND_RX"))
			value = qso->rxband;
		else if (!strcmp(elname, "MODE"))
			value = qso->mode;
		else if (!strcmp(elname, "FREQ"))
			value = qso->freq;
		else if (!strcmp(elname, "FREQ_RX"))
			value = qso->rxfreq;
		else if (!strcmp(elname, "PROP_MODE"))
			value = qso->propmode;
		else if (!strcmp(elname, "SAT_NAME"))
			value = qso->satname;
		else if (!strcmp(elname, "QSO_DATE")) {
			if (tqsl_isDateValid(&(qso->date)))
				value = tqsl_convertDateToText(&(qso->date), buf, sizeof buf);
		} else if (!strcmp(elname, "QSO_TIME")) {
			if (tqsl_isTimeValid(&(qso->time)))
				value = tqsl_convertTimeToText(&(qso->time), buf, sizeof buf);
		} else {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
				"Unknown field in signing specification: %s", elname);
			return 0;
		}
		if (value == 0 || value[0] == 0) {
			pair<string, bool> attr = specfield.getAttribute("required");
			if (attr.second && strtol(attr.first.c_str(), NULL, 10)) {
				string err = specfield.getElementName() + " field required by signature specification not found";
				tQSL_Error = TQSL_CUSTOM_ERROR;
				strncpy(tQSL_CustomError, err.c_str(), sizeof tQSL_CustomError);
				return 0;
			}
		} else {
			string v(value);
			rec_sign_data += trim(v);
		}
		ok = tCONTACT_sign.getNextElement(specfield);
	}
	unsigned char sig[129];
	int siglen = sizeof sig;
	rec_sign_data = string_toupper(rec_sign_data);
	if (tqsl_signDataBlock(cert, (const unsigned char *)rec_sign_data.c_str(), rec_sign_data.size(), sig, &siglen))
		return 0;
	char b64[512];
	if (tqsl_encodeBase64(sig, siglen, b64, sizeof b64))
		return 0;
	loc->tCONTACT = "<Rec_Type:8>tCONTACT\n";
	char sbuf[10], lbuf[40];
	snprintf(sbuf, sizeof sbuf, "%d", stationuid);
	snprintf(lbuf, sizeof lbuf, "<STATION_UID:%d>%s\n", static_cast<int>(strlen(sbuf)), sbuf);
	loc->tCONTACT += lbuf;
	char buf[256];
	tqsl_adifMakeField("CALL", 0, (const unsigned char *)qso->callsign, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	tqsl_adifMakeField("BAND", 0, (const unsigned char *)qso->band, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	tqsl_adifMakeField("MODE", 0, (const unsigned char *)qso->mode, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	// Optional fields
	if (qso->freq[0] != 0) {
		tqsl_adifMakeField("FREQ", 0, (const unsigned char *)qso->freq, -1, (unsigned char *)buf, sizeof buf);
		loc->tCONTACT += buf;
		loc->tCONTACT += "\n";
	}
	if (qso->rxfreq[0] != 0) {
		tqsl_adifMakeField("FREQ_RX", 0, (const unsigned char *)qso->rxfreq, -1, (unsigned char *)buf, sizeof buf);
		loc->tCONTACT += buf;
		loc->tCONTACT += "\n";
	}
	if (qso->propmode[0] != 0) {
		tqsl_adifMakeField("PROP_MODE", 0, (const unsigned char *)qso->propmode, -1, (unsigned char *)buf, sizeof buf);
		loc->tCONTACT += buf;
		loc->tCONTACT += "\n";
	}
	if (qso->satname[0] != 0) {
		tqsl_adifMakeField("SAT_NAME", 0, (const unsigned char *)qso->satname, -1, (unsigned char *)buf, sizeof buf);
		loc->tCONTACT += buf;
		loc->tCONTACT += "\n";
	}
	if (qso->rxband[0] != 0) {
		tqsl_adifMakeField("BAND_RX", 0, (const unsigned char *)qso->rxband, -1, (unsigned char *)buf, sizeof buf);
		loc->tCONTACT += buf;
		loc->tCONTACT += "\n";
	}
	// Date and Time
	char date_buf[40] = "";
	tqsl_convertDateToText(&(qso->date), date_buf, sizeof date_buf);
	tqsl_adifMakeField("QSO_DATE", 0, (const unsigned char *)date_buf, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	date_buf[0] = 0;
	tqsl_convertTimeToText(&(qso->time), date_buf, sizeof date_buf);
	tqsl_adifMakeField("QSO_TIME", 0, (const unsigned char *)date_buf, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	tqsl_adifMakeField(loc->sigspec.c_str(), '6', (const unsigned char *)b64, -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	// Signature
	tqsl_adifMakeField("SIGNDATA", 0, (const unsigned char *)rec_sign_data.c_str(), -1, (unsigned char *)buf, sizeof buf);
	loc->tCONTACT += buf;
	loc->tCONTACT += "\n";
	loc->tCONTACT += "<eor>\n";
	if (signdata)
		strncpy(signdata, rec_sign_data.c_str(), sdlen);
	return loc->tCONTACT.c_str();
}

DLLEXPORT const char* CALLCONVENTION
tqsl_getGABBItCONTACT(tQSL_Cert cert, tQSL_Location locp, TQSL_QSO_RECORD *qso, int stationuid) {
	return tqsl_getGABBItCONTACTData(cert, locp, qso, stationuid, NULL, 0);
}


DLLEXPORT int CALLCONVENTION
tqsl_getLocationCallSign(tQSL_Location locp, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false)))
		return 1;
	if (buf == 0 || bufsiz <= 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_PAGE& p = loc->pagelist[0];
	for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
		TQSL_LOCATION_FIELD f = p.fieldlist[i];
		if (f.gabbi_name == "CALL") {
			strncpy(buf, f.cdata.c_str(), bufsiz);
			buf[bufsiz-1] = 0;
			if (static_cast<int>(f.cdata.size()) >= bufsiz) {
				tQSL_Error = TQSL_BUFFER_ERROR;
				return 1;
			}
			return 0;
		}
	}
	tQSL_Error = TQSL_CALL_NOT_FOUND;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getLocationDXCCEntity(tQSL_Location locp, int *dxcc) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false)))
		return 1;
	if (dxcc == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_PAGE& p = loc->pagelist[0];
	for (int i = 0; i < static_cast<int>(p.fieldlist.size()); i++) {
		TQSL_LOCATION_FIELD f = p.fieldlist[i];
		if (f.gabbi_name == "DXCC") {
			if (f.idx < 0 || f.idx >= static_cast<int>(f.items.size()))
				break;	// No matching DXCC entity
			*dxcc = f.items[f.idx].ivalue;
			return 0;
		}
	}
	tQSL_Error = TQSL_NAME_NOT_FOUND;
	return 1;
}

DLLEXPORT int CALLCONVENTION
tqsl_getNumProviders(int *n) {
	if (n == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	vector<TQSL_PROVIDER> plist;
	if (tqsl_load_provider_list(plist))
		return 1;
	if (plist.size() == 0) {
		tQSL_Error = TQSL_PROVIDER_NOT_FOUND;
		return 1;
	}
	*n = plist.size();
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_getProvider(int idx, TQSL_PROVIDER *provider) {
	if (provider == NULL || idx < 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	vector<TQSL_PROVIDER> plist;
	if (tqsl_load_provider_list(plist))
		return 1;
	if (idx >= static_cast<int>(plist.size())) {
		tQSL_Error = TQSL_PROVIDER_NOT_FOUND;
		return 1;
	}
	*provider = plist[idx];
	return 0;
}

DLLEXPORT int CALLCONVENTION
tqsl_importTQSLFile(const char *file, int(*cb)(int type, const char *, void *), void *userdata) {
	XMLElement topel;
	int status = topel.parseFile(file);
	if (status) {
		strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
		if (status == XML_PARSE_SYSTEM_ERROR) {
			tQSL_Error = TQSL_FILE_SYSTEM_ERROR;
			tQSL_Errno = errno;
		} else {
			tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
		}
		return 1;
	}
	XMLElement tqsldata;
	if (!topel.getFirstElement("tqsldata", tqsldata)) {
		strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
		return 1;
	}
	XMLElement section;
	bool stat = tqsldata.getFirstElement("tqslcerts", section);
	if (stat) {
		XMLElement cert;
		bool cstat = section.getFirstElement("rootcert", cert);
		while (cstat) {
			tqsl_import_cert(cert.getText().c_str(), ROOTCERT, cb, userdata);
			cstat = section.getNextElement(cert);
		}
		cstat = section.getFirstElement("cacert", cert);
		while (cstat) {
			tqsl_import_cert(cert.getText().c_str(), CACERT, cb, userdata);
			cstat = section.getNextElement(cert);
		}
		cstat = section.getFirstElement("usercert", cert);
		while (cstat) {
			tqsl_import_cert(cert.getText().c_str(), USERCERT, cb, userdata);
			cstat = section.getNextElement(cert);
		}
	}
	stat = tqsldata.getFirstElement("tqslconfig", section);
	if (stat) {
		// Check to make sure we aren't overwriting newer version
		int major = strtol(section.getAttribute("majorversion").first.c_str(), NULL, 10);
		int minor = strtol(section.getAttribute("minorversion").first.c_str(), NULL, 10);
		int curmajor, curminor;
		if (tqsl_getConfigVersion(&curmajor, &curminor))
			return 1;
		if (major < curmajor)
			return 0;
		if (major == curmajor)
			if (minor <= curminor)
				return 0;

		// Save the configuration file
#ifdef _WIN32
		string fn = string(tQSL_BaseDir) + "\\config.xml";
#else
		string fn = string(tQSL_BaseDir) + "/config.xml";
#endif
		ofstream out;
		out.exceptions(ios::failbit | ios::eofbit | ios::badbit);
		try {
			out.open(fn.c_str());
			out << section << endl;
			out.close();
		}
		catch(exception& x) {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			snprintf(tQSL_CustomError, sizeof tQSL_CustomError,
				"Error writing new configuration file (%s): %s/%s",
				fn.c_str(), x.what(), strerror(errno));
			if (cb)
				return (*cb)(TQSL_CERT_CB_RESULT | TQSL_CERT_CB_ERROR | TQSL_CERT_CB_CONFIG,
					fn.c_str(), userdata);
			return 1;
		}
		// Clear stored config data to force re-reading new config
		tqsl_xml_config.clear();
		DXCCMap.clear();
		DXCCList.clear();
		BandList.clear();
		ModeList.clear();
		tqsl_page_map.clear();
		tqsl_field_map.clear();
		tqsl_adif_map.clear();
		tqsl_cabrillo_map.clear();
		string version = "Configuration V" + section.getAttribute("majorversion").first + "."
			+ section.getAttribute("minorversion").first + "\n" + fn;
		if (cb)
			return (*cb)(TQSL_CERT_CB_RESULT | TQSL_CERT_CB_LOADED | TQSL_CERT_CB_CONFIG,
				version.c_str(), userdata);
	}
	return 0;
}

/*
 * Get the first user certificate from a .tq6 file
*/
DLLEXPORT int CALLCONVENTION
tqsl_getSerialFromTQSLFile(const char *file, long *serial) {
	XMLElement topel;
	int status =  topel.parseFile(file);
	if (status) {
		strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
		if (status == XML_PARSE_SYSTEM_ERROR) {
			tQSL_Error = TQSL_FILE_SYSTEM_ERROR;
			tQSL_Errno = errno;
		} else {
			tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
		}
		return 1;
	}
	XMLElement tqsldata;
	if (!topel.getFirstElement("tqsldata", tqsldata)) {
		strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
		return 1;
	}
	XMLElement section;
	bool stat = tqsldata.getFirstElement("tqslcerts", section);
	if (stat) {
		XMLElement cert;
		bool cstat = section.getFirstElement("usercert", cert);
		if (cstat) {
			if (tqsl_get_pem_serial(cert.getText().c_str(), serial)) {
				strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
				tQSL_Error = TQSL_FILE_SYNTAX_ERROR;
				return 1;
			}
			return 0;
		}
	}
	return 1;
}
