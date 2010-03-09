/***************************************************************************
                          location.cpp  -  description
                             -------------------
    begin                : Wed Nov 6 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#ifdef MAC
#include "Carbon.h"
#endif

#define DXCC_TEST

#define TQSLLIB_DEF

#include "location.h"

#include <cstring>
#include <fstream>
#include <algorithm>
#include "tqsllib.h"
#include "tqslerrno.h"
#include "xml.h"
#include "openssl_cert.h"
#ifdef __WIN32__
	#include "windows.h"
#endif

#include <vector>

using namespace std;

namespace tqsllib {

class TQSL_LOCATION_ITEM {
public:
	TQSL_LOCATION_ITEM() : ivalue(0) {}
	std::string text;
	std::string label;
	std::string zonemap;
	int ivalue;
};

class TQSL_LOCATION_FIELD {
public:
	TQSL_LOCATION_FIELD() {}
	TQSL_LOCATION_FIELD(const char *i_gabbi_name, const char *i_label, int i_data_type, int i_data_len,
		int i_input_type, int i_flags = 0);
	std::string label;
	std::string gabbi_name;
	int data_type;
	int data_len;
	std::string cdata;
	std::vector<TQSL_LOCATION_ITEM> items;
	int idx;
	int idata;
	int input_type;
	int flags;
	bool changed;
	std::string dependency;
};

TQSL_LOCATION_FIELD::TQSL_LOCATION_FIELD(const char *i_gabbi_name, const char *i_label, int i_data_type,
	int i_data_len, int i_input_type, int i_flags) : data_type(i_data_type), data_len(i_data_len), cdata(""),
	input_type(i_input_type), flags(i_flags) {
	if (i_gabbi_name)
		gabbi_name = i_gabbi_name;
	if (i_label)
		label = i_label;
	idx = idata = 0;
}

typedef std::vector<TQSL_LOCATION_FIELD> TQSL_LOCATION_FIELDLIST;

class TQSL_LOCATION_PAGE {
public:
	TQSL_LOCATION_PAGE() : complete(false), prev(0), next(0) {}
	bool complete;
	int prev, next;
	string dependentOn, dependency;
	std::map<string, std::vector<string> > hash;
	TQSL_LOCATION_FIELDLIST fieldlist;
};

typedef std::vector<TQSL_LOCATION_PAGE> TQSL_LOCATION_PAGELIST;

class TQSL_NAME {
public:
	TQSL_NAME(string n = "", string c = "") : name(n), call(c) {}
	std::string name;
	std::string call;
};

class TQSL_LOCATION {
public:
	TQSL_LOCATION() : sentinel(0x5445), page(0), cansave(false), sign_clean(false) {}
	~TQSL_LOCATION() { sentinel = 0; }
	int sentinel;
	int page;
	bool cansave;
	std::string name;
	TQSL_LOCATION_PAGELIST pagelist;
	std::vector<TQSL_NAME> names;
	std::string signdata;
	bool sign_clean;
	std::string tSTATION;
	std::string tCONTACT;
	std::string sigspec;
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
		for (int i = 0; i < int(sizeof suffixes / sizeof suffixes[0]); i++) {
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
	} else if (o2.mode == o2.group)
		return false;
	// If groups are same, compare modes
	if (o1.group == o2.group)
		return o1.mode < o2.mode;
	int m1_g = (sizeof groups / sizeof groups[0]);
	int m2_g = m1_g;
	for (int i = 0; i < int(sizeof groups / sizeof groups[0]); i++) {
		if (o1.group == groups[i])
			m1_g = i;
		if (o2.group == groups[i])
			m2_g = i;
	}
	return m1_g < m2_g;
}

}	// namespace

using namespace tqsllib;

#define CAST_TQSL_LOCATION(x) ((tqsllib::TQSL_LOCATION *)(x))

typedef map<int, string> IntMap;

// config data

static XMLElement tqsl_xml_config;
static int tqsl_xml_config_major = -1;
static int tqsl_xml_config_minor = 0;
static IntMap DXCCMap;
static IntMap DXCCZoneMap;
static vector< pair<int,string> > DXCCList;
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

#ifdef __WIN32__
	HKEY hkey;
	DWORD dtype;
	char wpath[TQSL_MAX_PATH_LEN];
	DWORD bsize = sizeof wpath;
	int wval;
	if ((wval = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		"Software\\TrustedQSL\\TQSLCert", 0, KEY_READ, &hkey)) == ERROR_SUCCESS) {
		wval = RegQueryValueEx(hkey, "InstallPath", 0, &dtype, (LPBYTE)wpath, &bsize);
		RegCloseKey(hkey);
		if (wval == ERROR_SUCCESS)
			default_path = string(wpath) + "/config.xml";
	}
#elif defined(MAC)
	CFURLRef mainBundleURL;
	FSRef bundleFSRef;
	char npath[1024];

	mainBundleURL = CFBundleCopyBundleURL(CFBundleGetMainBundle());
	CFURLGetFSRef(mainBundleURL, &bundleFSRef);
	FSRefMakePath(&bundleFSRef, (unsigned char*)npath, sizeof(npath) - 1);
	// if last char is not a /, append one
	if ((strlen(npath) > 0) && (npath[strlen(npath)-1] != '/'))
		strcat(npath,"/");
	CFRelease(mainBundleURL);

	default_path = string(npath) + "Contents/Resources/config.xml";
#else
	default_path = CONFDIR "/config.xml";
#endif

	string user_path = string(tQSL_BaseDir) + "/config.xml";

	default_config.parseFile(default_path.c_str());
	user_config.parseFile(user_path.c_str());

	int default_major = -1;
	int default_minor = 0;
	int user_major = -1;
	int user_minor = 0;

	XMLElement top;
	if (default_config.getFirstElement("tqslconfig", top)) {
		default_major = atoi(top.getAttribute("majorversion").first.c_str());
		default_minor = atoi(top.getAttribute("minorversion").first.c_str());
	}
	if (user_config.getFirstElement("tqslconfig", top)) {
		user_major = atoi(top.getAttribute("majorversion").first.c_str());
		user_minor = atoi(top.getAttribute("minorversion").first.c_str());
	}

	if (default_major > user_major
		|| (default_major == user_major && default_minor > user_minor)) {
			tqsl_xml_config = default_config;
			tqsl_xml_config_major = default_major;
			tqsl_xml_config_minor = default_minor;
			return 0;
	}
	if (user_major < 0) {
		tQSL_Error = TQSL_CONFIG_ERROR;
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
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}
	if (!top.getFirstElement(section, el)) {
		tQSL_Error = TQSL_CONFIG_ERROR;
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
		for (int i = 0; i < (int)p.fieldlist.size(); i++) {
			TQSL_LOCATION_FIELD& f = p.fieldlist[i];
			string s;
			if (f.input_type == TQSL_LOCATION_FIELD_DDLIST || f.input_type == TQSL_LOCATION_FIELD_LIST) {
				if (f.idx < 0 || f.idx >= (int)f.items.size())
					s = "";
				else
					s = f.items[f.idx].text;
			} else if (f.data_type == TQSL_LOCATION_FIELD_INT) {
				char buf[20];
				sprintf(buf, "%d", f.idata);
				s = buf;
			} else
				s = f.cdata;
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
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}
	XMLElement sigspec;
	if (!sigspecs.getFirstElement("sigspec", sigspec)) {
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}
	loc->sigspec = "SIGN_";
	loc->sigspec += sigspec.getAttribute("name").first;
	loc->sigspec += "_V";
	loc->sigspec += sigspec.getAttribute("version").first;
	
	tCONTACT_sign.clear();
	if (!sigspec.getFirstElement("tCONTACT", tCONTACT_sign)) {
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}
	if (tCONTACT_sign.getElementList().size() == 0) {
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}
	XMLElement tSTATION;
	if (!sigspec.getFirstElement("tSTATION", tSTATION)) {
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}
	XMLElement specfield;
	bool ok;
	if (!(ok = tSTATION.getFirstElement(specfield))) {
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}
	do {
		string value = field_data[specfield.getElementName()];
		if (value == "") {
			pair<string, bool> attr = specfield.getAttribute("required");
			if (attr.second && atoi(attr.first.c_str())){
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
			string v(value);
			string::size_type idx = v.find_first_not_of(" \t");
			if (idx != string::npos)
				v = v.substr(idx);
			idx = v.find_last_not_of(" \t");
			if (idx != string::npos)
				v = v.substr(0, idx+1);
			loc->signdata += v;
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
		pair<string,bool> rval = dxcc_entity.getAttribute("arrlId");
		pair<string,bool> zval = dxcc_entity.getAttribute("zonemap");
		if (rval.second) {
			int num = atoi(rval.first.c_str());
			DXCCMap[num] = dxcc_entity.getText();
			if (zval.second) 
				DXCCZoneMap[num] = zval.first.c_str();
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
		b.low = atoi(config_band.getAttribute("low").first.c_str());
		b.high = atoi(config_band.getAttribute("high").first.c_str());
		BandList.push_back(b);
		ok = bands.getNextElement(config_band);
	}
	sort(BandList.begin(), BandList.end());
	return 0;
}

DLLEXPORT int
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
	
DLLEXPORT int
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

DLLEXPORT int
tqsl_getBand(int index, const char **name, const char **spectrum, int *low, int *high) {
	if (index < 0 || name == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_band())
		return 1;
	if (index >= (int)BandList.size()) {
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
	
DLLEXPORT int
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

DLLEXPORT int
tqsl_getMode(int index, const char **mode, const char **group) {
	if (index < 0 || mode == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_mode())
		return 1;
	if (index >= (int)ModeList.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*mode = ModeList[index].mode.c_str();
	if (group)
		*group = ModeList[index].group.c_str();
	return 0;
}

DLLEXPORT int
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

DLLEXPORT int
tqsl_getDXCCEntity(int index, int *number, const char **name) {
	if (index < 0 || name == 0 || number == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc())
		return 1;
	if (index >= (int)DXCCList.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*number = DXCCList[index].first;
	*name = DXCCList[index].second.c_str();
	return 0;
}


DLLEXPORT int
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

DLLEXPORT int
tqsl_getDXCCZoneMap(int number, const char **zonemap) {
	if (zonemap == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_dxcc())
		return 1;
	IntMap::const_iterator it;
	it = DXCCZoneMap.find(number);
	if (it == DXCCMap.end()) {
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

DLLEXPORT int
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

DLLEXPORT int
tqsl_getPropagationMode(int index, const char **name, const char **descrip) {
	if (index < 0 || name == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_propmode())
		return 1;
	if (index >= (int)PropModeList.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*name = PropModeList[index].name.c_str();
	if (descrip)
		*descrip = PropModeList[index].descrip.c_str();
	return 0;
}

DLLEXPORT int
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

DLLEXPORT int
tqsl_getSatellite(int index, const char **name, const char **descrip,
	tQSL_Date *start, tQSL_Date *end) {
	if (index < 0 || name == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_satellite())
		return 1;
	if (index >= (int)SatelliteList.size()) {
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
		if (cabrillo_item.getText() != "" && atoi(cabrillo_item.getAttribute("field").first.c_str()) > TQSL_MIN_CABRILLO_MAP_FIELD)
			tqsl_cabrillo_map[cabrillo_item.getText()] =
				make_pair(atoi(cabrillo_item.getAttribute("field").first.c_str())-1,
					(cabrillo_item.getAttribute("type").first == "VHF") ? TQSL_CABRILLO_VHF : TQSL_CABRILLO_HF);
		ok = cabrillo_map.getNextElement(cabrillo_item);
	}
	return 0;
}

DLLEXPORT int
tqsl_clearCabrilloMap() {
	tqsl_cabrillo_user_map.clear();
	return 0;
}

DLLEXPORT int
tqsl_setCabrilloMapEntry(const char *contest, int field, int contest_type) {
	if (contest == 0 || field <= TQSL_MIN_CABRILLO_MAP_FIELD ||
		(contest_type != TQSL_CABRILLO_HF && contest_type != TQSL_CABRILLO_VHF)) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	tqsl_cabrillo_user_map[string_toupper(contest)] = make_pair(field-1, contest_type);
	return 0;
}

DLLEXPORT int
tqsl_getCabrilloMapEntry(const char *contest, int *fieldnum, int *contest_type) {
	if (contest == 0 || fieldnum == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_cabrillo_map())
		return 1;
	map<string, pair<int,int> >::iterator it;
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

DLLEXPORT int
tqsl_clearADIFModes() {
	tqsl_adif_map.clear();
	return 0;
}

DLLEXPORT int
tqsl_setADIFMode(const char *adif_item, const char *mode) {
	if (adif_item == 0 || mode == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_adif_map()) {
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}
	string umode = string_toupper(mode);
	tqsl_adif_map[string_toupper(adif_item)] = umode;
	return 0;
}

DLLEXPORT int
tqsl_getADIFMode(const char *adif_item, char *mode, int nmode) {
	if (adif_item == 0 || mode == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (init_adif_map()) {
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}
	string orig = adif_item;
	orig = string_toupper(orig);
	string amode;
	if (tqsl_adif_map.find(orig) != tqsl_adif_map.end())
		amode = tqsl_adif_map[orig];
	if (nmode < (int)amode.length()+1) {
		tQSL_Error = TQSL_BUFFER_ERROR;
		return 1;
	}
	strcpy(mode, amode.c_str());
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
		int page_num = atoi(Id.first.c_str());
		if (!Id.second || page_num < 1) {	// Must have the Id!
			tQSL_Error = TQSL_CONFIG_ERROR;
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
			tQSL_Error = TQSL_CONFIG_ERROR;
			return 1;
		}
		tqsl_field_map[Id.first] = config_field;
	}

	return 0;
}

static bool inMap(int value, bool cqz, bool ituz, const char *map) {
/*
 * Parse the zone map and return true if the value is a valid zone number
 * The maps are colon-separated number pairs, with a list of pairs comma separated.
 */
	int cq, itu;
	bool result = false;
	char *mapcopy = strdup(map);
	char *mapPart = strtok(mapcopy, ",");

	// No map or empty string -> all match
	if (!map || map[0] == '\0')
		return true;
	while (mapPart) {
		sscanf(mapPart, "%d:%d", &itu, &cq);
		if (cqz && (cq == value)) {
			result = true;
			goto out;
		}
		else if (ituz && (itu == value)) {
			result = true;
			goto out;
		}
		mapPart = strtok(NULL, ",");
	} 
out:
	free (mapcopy);
	return result;
}

static TQSL_LOCATION_FIELD *
get_location_field(int page, const string& gabbi, TQSL_LOCATION *loc) {
	if (page == 0)
		page = loc->page;
	for (; page > 0; page = loc->pagelist[page-1].prev) {
		TQSL_LOCATION_FIELDLIST& fl = loc->pagelist[page-1].fieldlist;
		for (int j = 0; j < (int)fl.size(); j++) {
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
	int current_entity = 0;
	int loaded_cqz = 0;
	int loaded_ituz = 0;
	for (int i = 0; i < (int)p.fieldlist.size(); i++) {
		TQSL_LOCATION_FIELD& field = p.fieldlist[i];
		field.changed = false;
		if (field.gabbi_name == "CALL") {
			if (field.items.size() == 0) {
				// Build list of call signs from available certs
				field.changed = true;
				p.hash.clear();
				tQSL_Cert *certlist;
				int ncerts;
				tqsl_selectCertificates(&certlist, &ncerts, 0, 0, 0, 0, 1);
				for (int i = 0; i < ncerts; i++) {
					char callsign[40];
					tqsl_getCertificateCallSign(certlist[i], callsign, sizeof callsign);
					tqsl_getCertificateDXCCEntity(certlist[i], &dxcc);
					char ibuf[10];
					sprintf(ibuf, "%d", dxcc);
					p.hash[callsign].push_back(ibuf);
					tqsl_freeCertificate(certlist[i]);
				}
				// Fill the call sign list
				map<string, vector<string> >::iterator call_p;
				field.idx = 0;
				for (call_p = p.hash.begin(); call_p != p.hash.end(); call_p++) {
					TQSL_LOCATION_ITEM item;
					item.text = call_p->first;
					if (item.text == field.cdata)
						field.idx = field.items.size();
					field.items.push_back(item);
				}
				if (field.items.size() > 0)
					field.cdata = field.items[field.idx].text;
				else
					field.cdata = "";
			}
		} else if (field.gabbi_name == "DXCC") {
			// Note: Expects CALL to be field 0 of this page.
			string call = p.fieldlist[0].cdata;
			if (field.items.size() == 0 || call != field.dependency) {
				// rebuild list
				field.changed = true;
				init_dxcc();
				int olddxcc = atoi(field.cdata.c_str());
				field.items.clear();
				field.idx = 0;
#ifdef DXCC_TEST
				const char *dxcc_test = getenv("TQSL_DXCC");
				if (dxcc_test) {
					vector<string> &entlist = p.hash[call];
					char *parse_dxcc = new char[strlen(dxcc_test) + 1];
					strcpy(parse_dxcc, dxcc_test);
					char *cp = strtok(parse_dxcc, ",");
					while (cp) {
						if (find(entlist.begin(), entlist.end(), string(cp)) == entlist.end())
							entlist.push_back(cp);
						cp = strtok(0, ",");
					}
					delete[] parse_dxcc;
				}
#endif
				vector<string>::iterator ip;
				for (ip = p.hash[call].begin(); ip != p.hash[call].end(); ip++) {
					TQSL_LOCATION_ITEM item;
					item.text = *ip;
					item.ivalue = atoi(ip->c_str());
					IntMap::iterator dxcc_it = DXCCMap.find(item.ivalue);
					if (dxcc_it != DXCCMap.end())
						item.label = dxcc_it->second;
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
				tQSL_Error = TQSL_CONFIG_ERROR;
				return 1;
			}
			XMLElement config_field = tqsl_field_map.find(field.gabbi_name)->second;
			pair<string,bool> attr = config_field.getAttribute("dependsOn");
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
							// Put selectable "none" entry at the end to permit bypass
							if (field.flags & TQSL_LOCATION_FIELD_SELNXT) {
								TQSL_LOCATION_ITEM item;
								item.label = "[None]";
								field.items.push_back(item);
							}
							break;
						}
						ok = config_field.getNextElement(enumlist);
					} // enum loop
				} else {
					tQSL_Error = TQSL_CONFIG_ERROR;
					return 1;
				}
			} else {
				// No dependencies
				TQSL_LOCATION_FIELD *ent = get_location_field(page, "DXCC", loc);
				current_entity = atoi(ent->cdata.c_str());
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
						// Put selectable "none" entry at the end to permit bypass
						if (field.flags & TQSL_LOCATION_FIELD_SELNXT) {
							TQSL_LOCATION_ITEM item;
							item.label = "[None]";
							field.items.push_back(item);
						}
					} else {
						// No enums supplied
						if (atoi(config_field.getAttribute("intype").first.c_str()) != TQSL_LOCATION_FIELD_TEXT) {
							// This a list field
							int lower = atoi(config_field.getAttribute("lower").first.c_str());
							int upper = atoi(config_field.getAttribute("upper").first.c_str());
							const char *zoneMap;
							/* Get the map */
							if (tqsl_getDXCCZoneMap(current_entity, &zoneMap)) {
								zoneMap = NULL;
							}
							if (upper < lower) {
								tQSL_Error = TQSL_CONFIG_ERROR;
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
								if (!zoneMap || inMap(j, cqz, ituz, zoneMap)) {
									sprintf(buf, "%d", j);
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
	TQSL_LOCATION_FIELD *state = get_location_field(page, "US_STATE", loc);
	if (!state)
		state = get_location_field(page, "CA_PROVINCE", loc);
	if (state && state->idx > 0) {
		TQSL_LOCATION_FIELD *cqz = get_location_field(page, "CQZ", loc);
		TQSL_LOCATION_FIELD *ituz = get_location_field(page, "ITUZ", loc);
		const char *stateZoneMap = state->items[state->idx].zonemap.c_str();

		int currentCQ = cqz->idata;
		int currentITU = ituz->idata;

		if (!inMap(currentITU, false, true, stateZoneMap)) {
			int wantedITUZ = atoi(stateZoneMap);
			for (int i = 0; i < (int)ituz->items.size(); i++) {
				if (atoi(ituz->items[i].text.c_str()) == wantedITUZ) {
					ituz->idx = i;
					ituz->cdata = ituz->items[i].text;
					ituz->idata = ituz->items[i].ivalue;
					zonesok = false;
					break;
				}
			}
		}
		if (!inMap(currentCQ, true, false, stateZoneMap)) {
			while (*stateZoneMap != ':')
				stateZoneMap++;
			int wantedCQZ = atoi(++stateZoneMap);
			for (int i = 0; i < (int)cqz->items.size(); i++) {
				if (atoi(cqz->items[i].text.c_str()) == wantedCQZ) {
					cqz->idx = i;
					cqz->cdata = cqz->items[i].text;
					cqz->idata = cqz->items[i].ivalue;
					zonesok = false;
					break;
				}
			}
		}
		if (!zonesok)
			update_page(page-1, loc);
	}
	p.complete = true;
	return 0;
}

static int
make_page(TQSL_LOCATION_PAGELIST& pagelist, int page_num) {
	if (init_loc_maps())
		return 1;

	if (tqsl_page_map.find(page_num) == tqsl_page_map.end()) {
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}

	TQSL_LOCATION_PAGE p;
	pagelist.push_back(p);

	XMLElement& config_page = tqsl_page_map[page_num];

	pagelist.back().prev = atoi(config_page.getAttribute("follows").first.c_str());
	XMLElement config_pageField;
	bool field_ok = config_page.getFirstElement("pageField", config_pageField);
	while (field_ok) {
		string field_name = config_pageField.getText();
		if (field_name == "" || tqsl_field_map.find(field_name) == tqsl_field_map.end()) {
			tQSL_Error = TQSL_CONFIG_ERROR;	// Page references undefined field
			return 1;
		}
		XMLElement& config_field = tqsl_field_map[field_name];
		TQSL_LOCATION_FIELD loc_field(
			field_name.c_str(),
			config_field.getAttribute("label").first.c_str(),
			(config_field.getAttribute("type").first == "C") ? TQSL_LOCATION_FIELD_CHAR : TQSL_LOCATION_FIELD_INT,
			atoi(config_field.getAttribute("len").first.c_str()),
			atoi(config_field.getAttribute("intype").first.c_str()),
			atoi(config_field.getAttribute("flags").first.c_str())
		);
		pagelist.back().fieldlist.push_back(loc_field);
		field_ok = config_page.getNextElement(config_pageField);
	}
	return 0;
}

DLLEXPORT int
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

DLLEXPORT int
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

DLLEXPORT int
tqsl_updateStationLocationCapture(tQSL_Location locp) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
//	TQSL_LOCATION_PAGE &p = loc->pagelist[loc->page-1];
	return update_page(loc->page, loc);
}

DLLEXPORT int
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

DLLEXPORT int
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

DLLEXPORT int
tqsl_setStationLocationCapturePage(tQSL_Location locp, int page) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (page < 1 || page > (int)loc->pagelist.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	loc->page = page;
	return 0;
}

static int
find_next_page(TQSL_LOCATION *loc) {
	// Set next page based on page dependencies
	TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];
	map<int, XMLElement>::iterator pit;
	p.next = 0;
	for (pit = tqsl_page_map.begin(); pit != tqsl_page_map.end(); pit++) {
		if (atoi(pit->second.getAttribute("follows").first.c_str()) == loc->page) {
			string dependsOn = pit->second.getAttribute("dependsOn").first;
			string dependency = pit->second.getAttribute("dependency").first;
			if (dependsOn == "") {
				p.next = pit->first;
				break;
			}
			TQSL_LOCATION_FIELD *fp = get_location_field(0, dependsOn, loc);
			if (fp->items[fp->idx].text == dependency) {
				p.next = pit->first;
				break;	// Found next page
			}
		}
	}
	return 0;
}

DLLEXPORT int
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

DLLEXPORT int
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

DLLEXPORT int
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

DLLEXPORT int
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

DLLEXPORT int
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

DLLEXPORT int
tqsl_getLocationFieldDataLabelSize(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;

	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (rval == 0 || field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = fl[field_num].label.size()+1;
	return 0;
}

DLLEXPORT int
tqsl_getLocationFieldDataLabel(tQSL_Location locp, int field_num, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == 0 || field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, fl[field_num].label.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int
tqsl_getLocationFieldDataGABBISize(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;

	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (rval == 0 || field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = fl[field_num].gabbi_name.size()+1;
	return 0;
}

DLLEXPORT int
tqsl_getLocationFieldDataGABBI(tQSL_Location locp, int field_num, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == 0 || field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, fl[field_num].gabbi_name.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int
tqsl_getLocationFieldInputType(tQSL_Location locp, int field_num, int *type) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (type == 0 || field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*type = fl[field_num].input_type;
	return 0;
}

DLLEXPORT int
tqsl_getLocationFieldChanged(tQSL_Location locp, int field_num, int *changed) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (changed == 0 || field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*changed = fl[field_num].changed;
	return 0;
}

DLLEXPORT int
tqsl_getLocationFieldDataType(tQSL_Location locp, int field_num, int *type) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (type == 0 || field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*type = fl[field_num].data_type;
	return 0;
}

DLLEXPORT int
tqsl_getLocationFieldFlags(tQSL_Location locp, int field_num, int *flags) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (flags == 0 || field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*flags = fl[field_num].flags;
	return 0;
}

DLLEXPORT int
tqsl_getLocationFieldDataLength(tQSL_Location locp, int field_num, int *rval) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (rval == 0 || field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*rval = fl[field_num].data_len;
	return 0;
}

DLLEXPORT int
tqsl_getLocationFieldCharData(tQSL_Location locp, int field_num, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == 0 || field_num < 0 || field_num >= (int)fl.size()) {
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

DLLEXPORT int
tqsl_getLocationFieldIntData(tQSL_Location locp, int field_num, int *dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (dat == 0 || field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*dat = fl[field_num].idata;
	return 0;
}

DLLEXPORT int
tqsl_getLocationFieldIndex(tQSL_Location locp, int field_num, int *dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (dat == 0 || field_num < 0 || field_num >= (int)fl.size()) {
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

DLLEXPORT int
tqsl_setLocationFieldCharData(tQSL_Location locp, int field_num, const char *buf) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == 0 || field_num < 0 || field_num >= (int)fl.size()) {
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
DLLEXPORT int
tqsl_setLocationFieldIndex(tQSL_Location locp, int field_num, int dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	fl[field_num].idx = dat;
	if (fl[field_num].input_type == TQSL_LOCATION_FIELD_DDLIST
		|| fl[field_num].input_type == TQSL_LOCATION_FIELD_LIST) {
		if (dat >= 0 && dat < (int)fl[field_num].items.size()) {
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
DLLEXPORT int
tqsl_setLocationFieldIntData(tQSL_Location locp, int field_num, int dat) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (field_num < 0 || field_num >= (int)fl.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	fl[field_num].idata = dat;
	return 0;
}

/* For pick lists, this is the index into
 * 'items'. In that case, also set the field's char data to the picked value.
 */
DLLEXPORT int
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

DLLEXPORT int
tqsl_getLocationFieldListItem(tQSL_Location locp, int field_num, int item_idx, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	TQSL_LOCATION_FIELDLIST &fl = loc->pagelist[loc->page-1].fieldlist;
	if (buf == 0 || field_num < 0 || field_num >= (int)fl.size()
		|| (fl[field_num].input_type != TQSL_LOCATION_FIELD_LIST
		&& fl[field_num].input_type != TQSL_LOCATION_FIELD_DDLIST)) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	if (item_idx < 0 || item_idx >= (int)fl[field_num].items.size()) {
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
tqsl_station_data_filename(const char *f = "/station_data") {
	string s = tQSL_BaseDir;
	s += f;
	return s;
}

static int
tqsl_load_station_data(XMLElement &xel) {
	xel.parseFile(tqsl_station_data_filename().c_str());
	return 0;
}

static int
tqsl_dump_station_data(XMLElement &xel) {
	ofstream out;
	string fn = tqsl_station_data_filename();

	out.exceptions(std::ios::failbit | std::ios::eofbit | std::ios::badbit);
	try {
		out.open(fn.c_str());
		out << xel << endl;
		out.close();
	}
	catch (exception& x) {
		tQSL_Error = TQSL_CUSTOM_ERROR;
		strncpy(tQSL_CustomError, x.what(), sizeof tQSL_CustomError);
		return 1;
	}
	return 0;

}

DLLEXPORT int
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
		pair<string,bool> rval = ep->second.getAttribute("name");
		if (rval.second && !strcasecmp(rval.first.c_str(), name)) {
			ellist.erase(ep);
			return tqsl_dump_station_data(sfile);
		}
	}
	return TQSL_NAME_NOT_FOUND;
}

DLLEXPORT int
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
		pair<string,bool> rval = ep->second.getAttribute("name");
		if (rval.second && !strcasecmp(rval.first.c_str(), loc->name.c_str())) {
			exists = true;
			break;
		}
	}
	if (!exists) {
		tQSL_Error = TQSL_NAME_NOT_FOUND;
		return 1;
	}
	loc->page = 1;
	while(1) {
		TQSL_LOCATION_PAGE& page = loc->pagelist[loc->page-1];
		for (int fidx = 0; fidx < (int)page.fieldlist.size(); fidx++) {
			TQSL_LOCATION_FIELD& field = page.fieldlist[fidx];
			if (field.gabbi_name != "") {
				// A field that may exist
				XMLElement el;
				if (ep->second.getFirstElement(field.gabbi_name, el)) {
					const char *value = el.getText().c_str();
					field.cdata = value;
					switch (field.input_type) {
						case TQSL_LOCATION_FIELD_DDLIST:
						case TQSL_LOCATION_FIELD_LIST:
							for (int i = 0; i < (int)field.items.size(); i++) {
								const char *cp = field.items[i].text.c_str();
								int q = strcasecmp(value, cp);
								if (q == 0) {
									field.idx = i;
									field.cdata = cp;
									field.idata = field.items[i].ivalue;
									break;
								}
							}
							break;
						case TQSL_LOCATION_FIELD_TEXT:
							if (field.data_type == TQSL_LOCATION_FIELD_INT)
								field.idata = atoi(field.cdata.c_str());
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
	return 0;
}

DLLEXPORT int
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

DLLEXPORT int
tqsl_getStationLocationName(tQSL_Location locp, int idx, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (buf == 0 || idx < 0 || idx >= (int)loc->names.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, loc->names[idx].name.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

DLLEXPORT int
tqsl_getStationLocationCallSign(tQSL_Location locp, int idx, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp)))
		return 1;
	if (buf == 0 || idx < 0 || idx >= (int)loc->names.size()) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	strncpy(buf, loc->names[idx].call.c_str(), bufsiz);
	buf[bufsiz-1] = 0;
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
			string text;
			switch (field.input_type) {
				case TQSL_LOCATION_FIELD_DDLIST:
				case TQSL_LOCATION_FIELD_LIST:
					if (field.data_type == TQSL_LOCATION_FIELD_INT) {
						char numbuf[20];
						sprintf(numbuf, "%d", field.items[field.idx].ivalue);
						fd.setText(numbuf);
					} else if (field.idx < 0 || field.idx >= (int)field.items.size())
						fd.setText("");
					else
						fd.setText(field.items[field.idx].text);
					break;
				case TQSL_LOCATION_FIELD_TEXT:
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
	return 0;
}

DLLEXPORT int
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

DLLEXPORT int
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


DLLEXPORT int
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
		pair<string,bool> rval = ep->second.getAttribute("name");
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
	tqsl_dump_station_data(sfile);
	return 0;
}


DLLEXPORT int
tqsl_signQSORecord(tQSL_Cert cert, tQSL_Location locp, TQSL_QSO_RECORD *rec, unsigned char *sig, int *siglen) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false)))
		return 1;
	if (make_sign_data(loc))
		return 1;
	XMLElement specfield;
	bool ok = tCONTACT_sign.getFirstElement(specfield);
	string rec_sign_data = loc->signdata;
	do {
		const char *elname = specfield.getElementName().c_str();
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
			strcpy(tQSL_CustomError, "Unknown field in signing specification: ");
			strncat(tQSL_CustomError, elname, sizeof tQSL_CustomError - strlen(tQSL_CustomError));
			return 1;
		}
		if (value == 0 || value[0] == 0) {
			pair<string, bool> attr = specfield.getAttribute("required");
			if (attr.second && atoi(attr.first.c_str())){
				string err = specfield.getElementName() + " field required by signature specification not found";
				tQSL_Error = TQSL_CUSTOM_ERROR;
				strncpy(tQSL_CustomError, err.c_str(), sizeof tQSL_CustomError);
				return 1;
			}
		} else {
			string v(value);
			string::size_type idx = v.find_first_not_of(" \t");
			if (idx != string::npos)
				v = v.substr(idx);
			idx = v.find_last_not_of(" \t");
			if (idx != string::npos)
				v = v.substr(0, idx+1);
			rec_sign_data += v;
		}
		ok = tCONTACT_sign.getNextElement(specfield);
	} while (ok);
	return tqsl_signDataBlock(cert, (const unsigned char *)rec_sign_data.c_str(), rec_sign_data.size(), sig, siglen);
}

DLLEXPORT const char *
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
	sprintf(sbuf, "%d", uid);
	sprintf(lbuf, "<CERT_UID:%d>%s\n", (int)strlen(sbuf), sbuf);
	s += lbuf;
	sprintf(lbuf, "<CERTIFICATE:%d>", (int)strlen(cp));
	s += lbuf;
	s += cp;
	s += "<eor>\n";
	return s.c_str();
}

DLLEXPORT const char *
tqsl_getGABBItSTATION(tQSL_Location locp, int uid, int certuid) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false)))
		return 0;
	unsigned char *buf = 0;
	int bufsiz = 0;
	loc->tSTATION = "<Rec_Type:8>tSTATION\n";
	char sbuf[10], lbuf[40];
	sprintf(sbuf, "%d", uid);
	sprintf(lbuf, "<STATION_UID:%d>%s\n", (int)strlen(sbuf), sbuf);
	loc->tSTATION += lbuf;
	sprintf(sbuf, "%d", certuid);
	sprintf(lbuf, "<CERT_UID:%d>%s\n", (int)strlen(sbuf), sbuf);
	loc->tSTATION += lbuf;
	int old_page = loc->page;
	tqsl_setStationLocationCapturePage(loc, 1);
	do {
		TQSL_LOCATION_PAGE& p = loc->pagelist[loc->page-1];
		for (int i = 0; i < (int)p.fieldlist.size(); i++) {
			TQSL_LOCATION_FIELD& f = p.fieldlist[i];
			string s;
			if (f.input_type == TQSL_LOCATION_FIELD_DDLIST || f.input_type == TQSL_LOCATION_FIELD_LIST) {
				if (f.idx < 0 || f.idx >= (int)f.items.size())
					s = "";
				else
					s = f.items[f.idx].text;
			} else if (f.data_type == TQSL_LOCATION_FIELD_INT) {
				char buf[20];
				sprintf(buf, "%d", f.idata);
				s = buf;
			} else
				s = f.cdata;
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

DLLEXPORT const char *
tqsl_getGABBItCONTACT(tQSL_Cert cert, tQSL_Location locp, TQSL_QSO_RECORD *qso, int stationuid) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false)))
		return 0;
	if (make_sign_data(loc))
		return 0;
	XMLElement specfield;
	bool ok = tCONTACT_sign.getFirstElement(specfield);
	string rec_sign_data = loc->signdata;
	do {
		const char *elname = specfield.getElementName().c_str();
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
			strcpy(tQSL_CustomError, "Unknown field in signing specification: ");
			strncat(tQSL_CustomError, elname, sizeof tQSL_CustomError - strlen(tQSL_CustomError));
			return 0;
		}
		if (value == 0 || value[0] == 0) {
			pair<string, bool> attr = specfield.getAttribute("required");
			if (attr.second && atoi(attr.first.c_str())){
				string err = specfield.getElementName() + " field required by signature specification not found";
				tQSL_Error = TQSL_CUSTOM_ERROR;
				strncpy(tQSL_CustomError, err.c_str(), sizeof tQSL_CustomError);
				return 0;
			}
		} else {
			string v(value);
			string::size_type idx = v.find_first_not_of(" \t");
			if (idx != string::npos)
				v = v.substr(idx);
			idx = v.find_last_not_of(" \t");
			if (idx != string::npos)
				v = v.substr(0, idx+1);
			rec_sign_data += v;
		}
		ok = tCONTACT_sign.getNextElement(specfield);
	} while (ok);
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
	sprintf(sbuf, "%d", stationuid);
	sprintf(lbuf, "<STATION_UID:%d>%s\n", (int)strlen(sbuf), sbuf);
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
	return loc->tCONTACT.c_str();
}

DLLEXPORT int
tqsl_getLocationCallSign(tQSL_Location locp, char *buf, int bufsiz) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false)))
		return 1;
	if (buf == 0 || bufsiz <= 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_PAGE& p = loc->pagelist[0];
	for (int i = 0; i < (int)p.fieldlist.size(); i++) {
		TQSL_LOCATION_FIELD f = p.fieldlist[i];
		if (f.gabbi_name == "CALL") {
			strncpy(buf, f.cdata.c_str(), bufsiz);
			buf[bufsiz-1] = 0;
			if ((int)f.cdata.size() >= bufsiz) {
				tQSL_Error = TQSL_BUFFER_ERROR;
				return 1;
			}
			return 0;
		}
	}
	tQSL_Error = TQSL_NAME_NOT_FOUND;
	return 1;
}

DLLEXPORT int
tqsl_getLocationDXCCEntity(tQSL_Location locp, int *dxcc) {
	TQSL_LOCATION *loc;
	if (!(loc = check_loc(locp, false)))
		return 1;
	if (dxcc == 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	TQSL_LOCATION_PAGE& p = loc->pagelist[0];
	for (int i = 0; i < (int)p.fieldlist.size(); i++) {
		TQSL_LOCATION_FIELD f = p.fieldlist[i];
		if (f.gabbi_name == "DXCC") {
			if (f.idx < 0 || f.idx >= (int)f.items.size())
				break;	// No matching DXCC entity
			*dxcc = f.items[f.idx].ivalue;
			return 0;
		}
	}
	tQSL_Error = TQSL_NAME_NOT_FOUND;
	return 1;
}

DLLEXPORT int
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

DLLEXPORT int
tqsl_getProvider(int idx, TQSL_PROVIDER *provider) {
	if (provider == NULL || idx < 0) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	vector<TQSL_PROVIDER> plist;
	if (tqsl_load_provider_list(plist))
		return 1;
	if (idx >= (int)plist.size()) {
		tQSL_Error = TQSL_PROVIDER_NOT_FOUND;
		return 1;
	}
	*provider = plist[idx];
	return 0;
}

DLLEXPORT int
tqsl_importTQSLFile(const char *file, int(*cb)(int type, const char *, void *), void *userdata) {
	XMLElement topel;
	if (!topel.parseFile(file)) {
		strncpy(tQSL_ErrorFile, file, sizeof tQSL_ErrorFile);
		tQSL_Error = TQSL_CONFIG_ERROR;
		return 1;
	}
	XMLElement tqsldata;
	if (!topel.getFirstElement("tqsldata", tqsldata)) {
		tQSL_Error = TQSL_CONFIG_ERROR;
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
		int major = atoi(section.getAttribute("majorversion").first.c_str());
		int minor = atoi(section.getAttribute("minorversion").first.c_str());
		int curmajor, curminor;
		if (tqsl_getConfigVersion(&curmajor, &curminor))
			return 1;
		const char *msg =  "Newer config already present";
		if (major == curmajor) {
			if (minor == curminor)
				msg = "Same config version already present";
			else if (minor > curminor)
				msg = 0;
		} else if (major > curmajor)
			msg = 0;
		if (msg) {
			if (cb)
				return (*cb)(TQSL_CERT_CB_RESULT | TQSL_CERT_CB_DUPLICATE | TQSL_CERT_CB_CONFIG,
					msg, userdata);
			return 0;
		}

		// Save the configuration file
		string fn = string(tQSL_BaseDir) + "/config.xml";
		ofstream out;
		out.exceptions(std::ios::failbit | std::ios::eofbit | std::ios::badbit);
		try {
			out.open(fn.c_str());
			out << section << endl;
			out.close();
		}
		catch (exception& x) {
			tQSL_Error = TQSL_CUSTOM_ERROR;
			strncpy(tQSL_CustomError, x.what(), sizeof tQSL_CustomError);
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
