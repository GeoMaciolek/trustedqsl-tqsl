/***************************************************************************
                          tqsl.cpp  -  description
                             -------------------
    begin                : Mon Nov 4 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

using namespace std;

#include <wx/wxprec.h>
#include <wx/object.h>
#include <wx/config.h>

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include <wx/resource.h>
#include <wx/wxhtml.h>
#include <wx/wfstream.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <zlib.h>
#include "tqslwiz.h"
#include "qsodatadialog.h"
#include "tqslerrno.h"
#include "tqslexcept.h"
#include "tqslpaths.h"
#include "stationdial.h"
#include "tqslexcept.h"
#include "tqslconvert.h"
#include "adif.h"
#include "dxcc.h"
#include "tqsl_prefs.h"

#include "version.h"

#undef ALLOW_UNCOMPRESSED

#include "tqslbuild.h"

static DocPaths docpaths("tqslapp");

enum {
	tm_f_import = 7000,
	tm_f_import_compress,
	tm_f_compress,
	tm_f_uncompress,
	tm_f_preferences,
	tm_f_new,
	tm_f_edit,
	tm_f_exit,
	tm_s_add,
	tm_s_edit,
	tm_h_contents,
	tm_h_about,
};

#define TQSL_CD_MSG TQSL_ID_LOW
#define TQSL_CD_CANBUT TQSL_ID_LOW+1

static char *ErrorTitle = "TQSL Error";

/////////// Application //////////////

class QSLApp : public wxApp {
public:
	QSLApp();
	virtual ~QSLApp();
	bool OnInit();
	virtual wxLog *CreateLogTarget();
};

IMPLEMENT_APP(QSLApp)


static int
getPassword(char *buf, int bufsiz, tQSL_Cert cert) {
	char call[TQSL_CALLSIGN_MAX+1] = "";
	int dxcc = 0;
	const char *dxccname = "Unknown";
	tqsl_getCertificateCallSign(cert, call, sizeof call);
	tqsl_getCertificateDXCCEntity(cert, &dxcc);
	tqsl_getDXCCEntityName(dxcc, &dxccname);
	
	wxString message = wxString::Format("Enter the password to unlock the private key for\n"
		"%s -- %s\n(This is the password you made up when you\nrequested the certificate.)",
		call, dxccname);
	
	wxString pw = wxGetPasswordFromUser(message, "Enter password", "",
		wxGetApp().GetTopWindow());
	if (pw == "")
		return 1;
	strncpy(buf, pw.c_str(), bufsiz);
	return 0;
}

class ConvertingDialog : public wxDialog {
public:
	ConvertingDialog(wxWindow *parent, const char *filename = "");
	void OnCancel(wxCommandEvent&);
	bool running;
	wxStaticText *msg;
private:
	wxButton *canbut;

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ConvertingDialog, wxDialog)
	EVT_BUTTON(TQSL_CD_CANBUT, ConvertingDialog::OnCancel)
END_EVENT_TABLE()

void
ConvertingDialog::OnCancel(wxCommandEvent&) {
	running = false;
	canbut->Enable(FALSE);
}

ConvertingDialog::ConvertingDialog(wxWindow *parent, const char *filename)
	: wxDialog(parent, -1, wxString("Signing QSO Data")),
	running(true) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	wxString label = wxString("Converting ") + filename + " to TQSL format";
	sizer->Add(new wxStaticText(this, -1, label), 0, wxALL|wxALIGN_CENTER, 10);
	msg = new wxStaticText(this, TQSL_CD_MSG, "");
	sizer->Add(msg, 0, wxALL|wxALIGN_CENTER, 10);
	canbut = new wxButton(this, TQSL_CD_CANBUT, "Cancel");
	sizer->Add(canbut, 0, wxALL|wxEXPAND, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

#define TQSL_DR_START TQSL_ID_LOW
#define TQSL_DR_END TQSL_ID_LOW+1
#define TQSL_DR_OK TQSL_ID_LOW+2
#define TQSL_DR_CAN TQSL_ID_LOW+3
#define TQSL_DR_MSG TQSL_ID_LOW+4

class DateRangeDialog : public wxDialog {
public:
	DateRangeDialog(wxWindow *parent = 0);
	tQSL_Date start, end;
private:
	void OnOk(wxCommandEvent&);	
	void OnCancel(wxCommandEvent&);
	virtual bool TransferDataFromWindow();
	wxTextCtrl *start_tc, *end_tc;
	wxStaticText *msg;
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(DateRangeDialog, wxDialog)
	EVT_BUTTON(TQSL_DR_OK, DateRangeDialog::OnOk)
	EVT_BUTTON(TQSL_DR_CAN, DateRangeDialog::OnCancel)
END_EVENT_TABLE()

DateRangeDialog::DateRangeDialog(wxWindow *parent) : wxDialog(parent, -1, wxString("QSO Date Range")) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1,
		"You may set the starting and/or ending QSO dates\n"
		"in order to select QSOs from the input file.\n\n"
		"QSOs prior to the starting date or after the ending\n"
		"date will not be signed or included in the output file.\n\n"
		"You may leave either date (or both dates) blank."
	), 0, wxALL|wxALIGN_CENTER, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, "Start Date (YYYY-MM-DD)"), 0, wxRIGHT, 5);
	start_tc = new wxTextCtrl(this, TQSL_DR_START);
	hsizer->Add(start_tc, 0, 0, 0);
	sizer->Add(hsizer, 0, wxALL|wxALIGN_CENTER, 10);
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, "End Date (YYYY-MM-DD)"), 0, wxRIGHT, 5);
	end_tc = new wxTextCtrl(this, TQSL_DR_END);
	hsizer->Add(end_tc, 0, 0, 0);
	sizer->Add(hsizer, 0, wxALL|wxALIGN_CENTER, 10);
	msg = new wxStaticText(this, TQSL_DR_MSG, "");
	sizer->Add(msg, 0, wxALL, 5);
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxButton(this, TQSL_DR_OK, "Ok"), 0, wxRIGHT, 5);
	hsizer->Add(new wxButton(this, TQSL_DR_CAN, "Cancel"), 0, wxLEFT, 10);
	sizer->Add(hsizer, 0, wxALIGN_CENTER|wxALL, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

bool
DateRangeDialog::TransferDataFromWindow() {
	wxString text = start_tc->GetValue();
	if (text.Trim() == "")
		start.year = start.month = start.day = 0;
	else if (tqsl_initDate(&start, text.c_str()) || !tqsl_isDateValid(&start)) {
		msg->SetLabel("Start date is invalid");
		return false;
	}
	text = end_tc->GetValue();
	if (text.Trim() == "")
		end.year = end.month = end.day = 0;
	else if (tqsl_initDate(&end, text.c_str()) || !tqsl_isDateValid(&end)) {
		msg->SetLabel("End date is invalid");
		return false;
	}
	return true;
}

void
DateRangeDialog::OnOk(wxCommandEvent&) {
	if (TransferDataFromWindow())
		EndModal(wxOK);
}

void
DateRangeDialog::OnCancel(wxCommandEvent&) {
	EndModal(wxCANCEL);
}

static void
init_modes() {
	tqsl_clearADIFModes();
	wxConfig *config = (wxConfig *)wxConfig::Get();
	long cookie;
	wxString key, value;
	config->SetPath("/modeMap");
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, "");
		tqsl_setADIFMode(key.c_str(), value.c_str());
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath("/");
}

static void
init_contests() {
	tqsl_clearCabrilloMap();
	wxConfig *config = (wxConfig *)wxConfig::Get();
	long cookie;
	wxString key, value;
	config->SetPath("/cabrilloMap");
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, "");
		int contest_type = atoi(value.c_str());
		int callsign_field = atoi(value.AfterFirst(';').c_str());
		tqsl_setCabrilloMapEntry(key.c_str(), callsign_field, contest_type);
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath("/");
}

static void
check_tqsl_error(int rval) {
	if (rval == 0)
		return;
	const char *msg = tqsl_getErrorString();
	throw TQSLException(msg);
}

static tQSL_Cert *certlist = 0;
static int ncerts;

static void
free_certlist() {
	if (certlist) {
		for (int i = 0; i < ncerts; i++)
			tqsl_freeCertificate(certlist[i]);
		certlist = 0;
	}
}

static void
get_certlist(string callsign, int dxcc) {
	free_certlist();
	tqsl_selectCertificates(&certlist, &ncerts,
		(callsign == "") ? 0 : callsign.c_str(), dxcc, 0, 0, 0);
}

class MyFrame : public wxFrame {
public:
	MyFrame(const wxString& title, int x, int y, int w, int h);

	void AddStationLocation(wxCommandEvent& event);
	void EditStationLocation(wxCommandEvent& event);
	void EnterQSOData(wxCommandEvent& event);
	void EditQSOData(wxCommandEvent& event);
	void ImportQSODataFile(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
	void OnHelpAbout(wxCommandEvent& event);
	void OnHelpContents(wxCommandEvent& event);
#ifdef ALLOW_UNCOMPRESSED
	void OnFileCompress(wxCommandEvent& event);
#endif
	void OnPreferences(wxCommandEvent& event);
	bool ConvertLogFile(tQSL_Location loc, wxString& infile, wxString& outfile, bool compress = false);
	tQSL_Location SelectStationLocation(const wxString& title = "", bool editonly = false);
	void WriteQSOFile(QSORecordList& recs, const char *fname = 0, bool force = false);

	wxTextCtrl *logwin;
	wxHtmlHelpController help;

	DECLARE_EVENT_TABLE()
};

class LogList : public wxLog {
public:
	LogList(MyFrame *frame) : wxLog(), _frame(frame) {}
	virtual void DoLogString(const wxChar *szString, time_t t);
private:
	MyFrame *_frame;
};

void LogList::DoLogString(const wxChar *szString, time_t) {
	wxTextCtrl *_logwin = 0;
	if (!strncmp(szString, "Debug:", 6))
		return;
	if (_frame != 0)
		_logwin = _frame->logwin;
	if (_logwin == 0) {
		cerr << szString << endl;
		return;
	}
	_logwin->AppendText(szString);
	_logwin->AppendText("\n");
}

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_MENU(tm_s_add, MyFrame::AddStationLocation)
	EVT_MENU(tm_s_edit, MyFrame::EditStationLocation)
	EVT_MENU(tm_f_new, MyFrame::EnterQSOData)
	EVT_MENU(tm_f_edit, MyFrame::EditQSOData)
#ifdef ALLOW_UNCOMPRESSED
	EVT_MENU(tm_f_import, MyFrame::ImportQSODataFile)
#endif
	EVT_MENU(tm_f_import_compress, MyFrame::ImportQSODataFile)
#ifdef ALLOW_UNCOMPRESSED
	EVT_MENU(tm_f_compress, MyFrame::OnFileCompress)
	EVT_MENU(tm_f_uncompress, MyFrame::OnFileCompress)
#endif
	EVT_MENU(tm_f_exit, MyFrame::OnExit)
	EVT_MENU(tm_f_preferences, MyFrame::OnPreferences)
	EVT_MENU(tm_h_contents, MyFrame::OnHelpContents)
	EVT_MENU(tm_h_about, MyFrame::OnHelpAbout)
	EVT_CLOSE(MyFrame::OnExit)
END_EVENT_TABLE()

void
MyFrame::OnExit(wxCommandEvent& WXUNUSED(event)) {
	Destroy();
}

MyFrame::MyFrame(const wxString& title, int x, int y, int w, int h)
	: wxFrame(0, -1, title, wxPoint(x, y), wxSize(w, h)) {

	// File menu
	wxMenu *file_menu = new wxMenu;
	file_menu->Append(tm_f_import_compress, "&Sign existing ADIF or Cabrillo file...");
#ifdef ALLOW_UNCOMPRESSED
	file_menu->Append(tm_f_import, "Sign &Uncompressed...");
#endif
	file_menu->AppendSeparator();
	file_menu->Append(tm_f_new, "Create &New ADIF file...");
	file_menu->Append(tm_f_edit, "&Edit existing ADIF file...");
	file_menu->AppendSeparator();
#ifdef ALLOW_UNCOMPRESSED
	file_menu->Append(tm_f_compress, "Co&mpress...");
	file_menu->Append(tm_f_uncompress, "&Uncompress...");
	file_menu->AppendSeparator();
#endif
	file_menu->Append(tm_f_preferences, "&Preferences...");
	file_menu->AppendSeparator();
	file_menu->Append(tm_f_exit, "E&xit");

	// Station menu
	wxMenu *stn_menu = new wxMenu;
	stn_menu->Append(tm_s_add, "&Add location");
	stn_menu->Append(tm_s_edit, "&Edit locations");

	// Help menu
	wxMenu *help_menu = new wxMenu;
	help.UseConfig(wxConfig::Get());
	wxString hhp = docpaths.FindAbsoluteValidPath("tqslapp.hhp");
	if (wxFileNameFromPath(hhp) != "") {
		if (help.AddBook(hhp))
		help_menu->Append(tm_h_contents, "&Contents");
		help_menu->AppendSeparator();
	}
	help_menu->Append(tm_h_about, "&About");

	// Main menu
	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, "&File");
	menu_bar->Append(stn_menu, "&Station");
	menu_bar->Append(help_menu, "&Help");

	SetMenuBar(menu_bar);

	logwin = new wxTextCtrl(this, -1, "", wxDefaultPosition, wxSize(400, 200),
		wxTE_MULTILINE|wxTE_READONLY);
}

static wxString
run_station_wizard(wxWindow *parent, tQSL_Location loc, wxHtmlHelpController *help = 0,
	wxString title = "Add Station Location", wxString dataname = "") {
	wxString rval("");
	get_certlist("", 0);
	if (ncerts == 0)
		throw TQSLException("No certificates available");
	TQSLWizard *wiz = new TQSLWizard(loc, parent, help, title);
	wiz->GetPage(true);
	TQSLWizPage *page = wiz->GetPage();
	if (page == 0)
		throw TQSLException("Error getting first wizard page");
	wiz->AdjustSize();
	// Note: If dynamically created pages are larger than the pages already
	// created (the initial page and the final page), the wizard will need to
	// be resized, but we don't presently have that happening. (The final
	// page is larger than all expected dynamic pages.)
	bool okay = wiz->RunWizard(page);
	rval = wiz->GetLocationName();
	wiz->Destroy();
	if (!okay)
		return rval;
	check_tqsl_error(tqsl_setStationLocationCaptureName(loc, rval.c_str()));
	check_tqsl_error(tqsl_saveStationLocationCapture(loc, 1));
	return rval;
}

void
MyFrame::OnHelpContents(wxCommandEvent& WXUNUSED(event)) {
	help.Display("main.htm");
}

void
MyFrame::OnHelpAbout(wxCommandEvent& WXUNUSED(event)) {
	wxString msg = "TQSL V" VERSION "." BUILD "\n(c) 2001-2003\nAmerican Radio Relay League\n\n";
	int major, minor;
	if (tqsl_getVersion(&major, &minor))
		wxLogError(tqsl_getErrorString());
	else
		msg += wxString::Format("TrustedQSL library V%d.%d\n", major, minor);
	if (tqsl_getConfigVersion(&major, &minor))
		wxLogError(tqsl_getErrorString());
	else
		msg += wxString::Format("Configuration data V%d.%d", major, minor);
	wxMessageBox(msg, "About", wxOK|wxCENTRE|wxICON_INFORMATION, this);
}

void
MyFrame::AddStationLocation(wxCommandEvent& WXUNUSED(event)) {
	tQSL_Location loc;
	try {
		check_tqsl_error(tqsl_initStationLocationCapture(&loc));
		run_station_wizard(this, loc, &help);
		check_tqsl_error(tqsl_endStationLocationCapture(&loc));
	}
	catch (TQSLException& x) {
		wxLogError(x.what());
//		wxMessageBox(x.what(), error_title);
	}
}

void
MyFrame::EditStationLocation(wxCommandEvent& WXUNUSED(event)) {
	SelectStationLocation("Edit Station Locations", true);
}

void
MyFrame::WriteQSOFile(QSORecordList& recs, const char *fname, bool force) {
	if (recs.empty()) {
		wxLogWarning("No QSO records");
		return;
	}
	wxString s_fname;
	if (fname)
		s_fname = fname;
	if (s_fname == "" || !force) {
		wxString path, basename, type;
		wxSplitPath(s_fname.c_str(), &path, &basename, &type);
		if (type != "")
			basename += "." + type;
		if (path == "")
			path = wxConfig::Get()->Read("QSODataPath", "");
		s_fname = wxFileSelector("Save File", path, basename, "adi",
			"ADIF files (*.adi)|*.adi|All files (*.*)|*.*",
			wxSAVE|wxOVERWRITE_PROMPT, this);
		if (s_fname == "")
			return;
		wxConfig::Get()->Write("QSODataPath", wxPathOnly(s_fname));
	}
	ofstream out(s_fname.c_str(), ios::out|ios::trunc|ios::binary);
	if (!out.is_open())
		return;
	unsigned char buf[256];
	QSORecordList::iterator it;
	for (it = recs.begin(); it != recs.end(); it++) {
		wxString dtstr;
		tqsl_adifMakeField("CALL", 0, (const unsigned char*)it->_call.c_str(), -1, buf, sizeof buf);
		out << buf << endl;
		tqsl_adifMakeField("BAND", 0, (const unsigned char*)it->_band.c_str(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		tqsl_adifMakeField("MODE", 0, (const unsigned char*)it->_mode.c_str(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		dtstr.Printf("%04d%02d%02d", it->_date.year, it->_date.month, it->_date.day);
		tqsl_adifMakeField("QSO_DATE", 0, (const unsigned char*)dtstr.c_str(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		dtstr.Printf("%02d%02d", it->_time.hour, it->_time.minute);
		tqsl_adifMakeField("TIME_ON", 0, (const unsigned char*)dtstr.c_str(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		if (it->_freq != "") {
			tqsl_adifMakeField("FREQ", 0, (const unsigned char*)it->_freq.c_str(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (it->_rxband != "") {
			tqsl_adifMakeField("BAND_RX", 0, (const unsigned char*)it->_rxband.c_str(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (it->_rxfreq != "") {
			tqsl_adifMakeField("FREQ_RX", 0, (const unsigned char*)it->_rxfreq.c_str(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (it->_propmode != "") {
			tqsl_adifMakeField("PROP_MODE", 0, (const unsigned char*)it->_propmode.c_str(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (it->_satellite != "") {
			tqsl_adifMakeField("SAT_NAME", 0, (const unsigned char*)it->_satellite.c_str(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		out << "<EOR>" << endl;
	}
	out.close();
	wxLogMessage("Wrote %d QSO records to %s", recs.size(), s_fname.c_str());
}

static tqsl_adifFieldDefinitions fielddefs[] = {
	{ "CALL", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_CALLSIGN_MAX, 0, 0, 0, 0 },
	{ "BAND", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_BAND_MAX, 0, 0, 0, 0 },
	{ "BAND_RX", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_BAND_MAX, 0, 0, 0, 0 },
	{ "MODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_MODE_MAX, 0, 0, 0, 0 },
	{ "FREQ", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_FREQ_MAX, 0, 0, 0, 0 },
	{ "FREQ_RX", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_FREQ_MAX, 0, 0, 0, 0 },
	{ "QSO_DATE", "", TQSL_ADIF_RANGE_TYPE_NONE, 8, 0, 0, 0, 0 },
	{ "TIME_ON", "", TQSL_ADIF_RANGE_TYPE_NONE, 6, 0, 0, 0, 0 },
	{ "SAT_NAME", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_SATNAME_MAX, 0, 0, 0, 0 },
	{ "PROP_MODE", "", TQSL_ADIF_RANGE_TYPE_NONE, TQSL_PROPMODE_MAX, 0, 0, 0, 0 },
	{ "EOR", "", TQSL_ADIF_RANGE_TYPE_NONE, 0, 0, 0, 0, 0 },
	{ "", "", TQSL_ADIF_RANGE_TYPE_NONE, 0, 0, 0, 0, 0 },
};

static char *defined_types[] = { "T", "D", "M", "C", "N", "6" };

static unsigned char *
adif_alloc(size_t n) {
	return new unsigned char[n];
}

void
MyFrame::EditQSOData(wxCommandEvent& WXUNUSED(event)) {
	QSORecordList recs;
	wxString file = wxFileSelector("Open File", wxConfig::Get()->Read("QSODataPath", ""), "", "adi",
			"ADIF files (*.adi)|*.adi|All files (*.*)|*.*",
			wxOPEN|wxFILE_MUST_EXIST, this);
	if (file == "")
		return;
	init_modes();
	tqsl_adifFieldResults field;
	TQSL_ADIF_GET_FIELD_ERROR stat;
	tQSL_ADIF adif;
	check_tqsl_error(tqsl_beginADIF(&adif, file.c_str()));
	QSORecord rec;
	do {
		check_tqsl_error(tqsl_getADIFField(adif, &field, &stat, fielddefs, defined_types, adif_alloc));
		if (stat == TQSL_ADIF_GET_FIELD_SUCCESS) {
			if (!strcasecmp(field.name, "CALL"))
				rec._call = (const char *)field.data;
			else if (!strcasecmp(field.name, "BAND"))
				rec._band = (const char *)field.data;
			else if (!strcasecmp(field.name, "BAND_RX"))
				rec._rxband = (const char *)field.data;
			else if (!strcasecmp(field.name, "MODE")) {
				rec._mode = (const char *)field.data;
				char amode[40];
				if (tqsl_getADIFMode(rec._mode.c_str(), amode, sizeof amode) == 0 && amode[0] != '\0')
					rec._mode = amode;
			} else if (!strcasecmp(field.name, "FREQ"))
				rec._freq = (const char *)field.data;
			else if (!strcasecmp(field.name, "FREQ_RX"))
				rec._rxfreq = (const char *)field.data;
			else if (!strcasecmp(field.name, "PROP_MODE"))
				rec._propmode = (const char *)field.data;
			else if (!strcasecmp(field.name, "SAT_NAME"))
				rec._satellite = (const char *)field.data;
			else if (!strcasecmp(field.name, "QSO_DATE")) {
				char *cp = (char *)field.data;
				if (strlen(cp) == 8) {
					rec._date.day = atoi(cp+6);
					*(cp+6) = '\0';
					rec._date.month = atoi(cp+4);
					*(cp+4) = '\0';
					rec._date.year = atoi(cp);
				}
			} else if (!strcasecmp(field.name, "TIME_ON")) {
				char *cp = (char *)field.data;
				if (strlen(cp) >= 4) {
					rec._time.second = 0;
					*(cp+4) = 0;
					rec._time.minute = atoi(cp+2);
					*(cp+2) = '\0';
					rec._time.hour = atoi(cp);
				}
			}
			else if (!strcasecmp(field.name, "EOR")) {
				recs.push_back(rec);
				rec = QSORecord();
			}
			delete[] field.data;
		}
	} while (stat == TQSL_ADIF_GET_FIELD_SUCCESS || stat == TQSL_ADIF_GET_FIELD_NO_NAME_MATCH);
	tqsl_endADIF(&adif);
	try {
		QSODataDialog dial(this, &help, &recs);
		if (dial.ShowModal() == wxID_OK)
			WriteQSOFile(recs, file);
	} catch (TQSLException& x) {
		wxLogError(x.what());
	}
}

void
MyFrame::EnterQSOData(wxCommandEvent& WXUNUSED(event)) {
	QSORecordList recs;
	try {
		QSODataDialog dial(this, &help, &recs);
		if (dial.ShowModal() == wxID_OK)
			WriteQSOFile(recs);
	} catch (TQSLException& x) {
		wxLogError(x.what());
	}
}

bool
MyFrame::ConvertLogFile(tQSL_Location loc, wxString& infile, wxString& outfile, bool compressed) {
    static char *iam = "TQSL V" VERSION;
   	const char *cp;
	tQSL_Converter conv = 0;
	char callsign[40];
	int dxcc;
	wxString name, ext;
	gzFile gout = 0;
	ofstream out;
	wxConfig *config = (wxConfig *)wxConfig::Get();

	check_tqsl_error(tqsl_getLocationCallSign(loc, callsign, sizeof callsign));
	check_tqsl_error(tqsl_getLocationDXCCEntity(loc, &dxcc));

	get_certlist(callsign, dxcc);
	if (ncerts == 0) {
		wxMessageBox("No matching certs for QSO", ErrorTitle, wxOK|wxCENTRE, this);
		return false;
	}

	wxLogMessage("Signing using CALL=%s, DXCC=%d", callsign, dxcc);

	init_modes();
	init_contests();

	if (compressed)
		gout = gzopen(outfile, "wb9");
	else
		out.open(outfile, ios::out|ios::trunc|ios::binary);

	if ((compressed && !gout) || (!compressed && !out)) {
		wxMessageBox(wxString("Unable to open ") + outfile, ErrorTitle, wxOK|wxCENTRE, this);
		return false;
	}

	ConvertingDialog *conv_dial = new ConvertingDialog(this, name.c_str());
	int n = 0;
	bool cancelled = false;
	int lineno = 0;
	int out_of_range = 0;
   	try {
   		if (tqsl_beginCabrilloConverter(&conv, infile.c_str(), certlist, ncerts, loc)) {
			if (tQSL_Error != TQSL_CABRILLO_ERROR || tQSL_Cabrillo_Error != TQSL_CABRILLO_NO_START_RECORD)
				check_tqsl_error(1);	// A bad error
			lineno = 0;
	   		check_tqsl_error(tqsl_beginADIFConverter(&conv, infile.c_str(), certlist, ncerts, loc));
		}
		bool range = true;
		config->Read("DateRange", &range);
		if (range) {
			DateRangeDialog dial(this);
			if (dial.ShowModal() != wxOK) {
				wxLogMessage("Cancelled");
				return false;
			}
			tqsl_setADIFConverterDateFilter(conv, &dial.start, &dial.end);
		}
		bool allow = false;
		config->Read("BadCalls", &allow);
		tqsl_setConverterAllowBadCall(conv, allow);
		wxSplitPath(infile.c_str(), 0, &name, &ext);
		if (ext != "")
			name += "." + ext;
		conv_dial->Show(TRUE);
		this->Enable(FALSE);
		bool ignore_err = false;
		int major = 0, minor = 0, config_major = 0, config_minor = 0;
		tqsl_getVersion(&major, &minor);
		tqsl_getConfigVersion(&config_major, &config_minor);
		wxString ident = wxString::Format("%s Lib: V%d.%d Config: V%d.%d", iam,
			major, minor, config_major, config_minor);
		wxString gabbi_ident = wxString::Format("<TQSL_IDENT:%d>%s", ident.length(), ident.c_str());
		gabbi_ident += "\n";
		if (compressed)
			gzwrite(gout, const_cast<char *>(gabbi_ident.c_str()), gabbi_ident.length());
		else
			out << gabbi_ident << endl;
   		do {
   	   		while ((cp = tqsl_getConverterGABBI(conv)) != 0) {
  					wxSafeYield(conv_dial);
  					if (!conv_dial->running)
  						break;
					++n;
					if ((n % 10) == 0)
		   	   			conv_dial->msg->SetLabel(wxString::Format("%d", n));
					if (compressed) {
	  					gzwrite(gout, const_cast<char *>(cp), strlen(cp));
						gzputs(gout, "\n");
					} else {
						out << cp << endl;
					}
   			}
			if (cp == 0) {
				wxSafeYield(conv_dial);
				if (!conv_dial->running)
					break;
			}
   			if (tQSL_Error == TQSL_SIGNINIT_ERROR) {
   				tQSL_Cert cert;
				int rval;
   				check_tqsl_error(tqsl_getConverterCert(conv, &cert));
				do {
	   				if ((rval = tqsl_beginSigning(cert, 0, getPassword, cert)) == 0)
						break;
					if (tQSL_Error == TQSL_PASSWORD_ERROR)
						wxLogMessage("Password error");
				} while (tQSL_Error == TQSL_PASSWORD_ERROR);
   				check_tqsl_error(rval);
   				continue;
   			}
			if (tQSL_Error == TQSL_DATE_OUT_OF_RANGE) {
				out_of_range++;
				continue;
			}
			bool has_error = (tQSL_Error != TQSL_NO_ERROR);
			if (has_error) {
				try {
					check_tqsl_error(1);
				} catch (TQSLException& x) {
					tqsl_getConverterLine(conv, &lineno);
					wxString msg = x.what();
					if (lineno)
						msg += wxString::Format(" on line %d", lineno);
					const char *bad_text = tqsl_getConverterRecordText(conv);
					if (bad_text)
						msg += wxString("\n") + bad_text;
					wxLogError(msg);
					if (!ignore_err) {
						if (wxMessageBox(wxString("Error: ") + msg + "\n\nIgnore errors?", "Error", wxYES_NO, this) == wxNO)
							throw x;
					}
					ignore_err = true;
				}
			}
			tqsl_getErrorString();	// Clear error			
			if (has_error && ignore_err)
				continue;
   			break;
		} while (1);
   		cancelled = !conv_dial->running;
   		this->Enable(TRUE);
		if (compressed)
			gzclose(gout);
		else
			out.close();
   		if (cancelled) {
   			wxLogWarning("Signing cancelled");
   		} else if (tQSL_Error != TQSL_NO_ERROR) {
   			check_tqsl_error(1);
		}
   		tqsl_endConverter(&conv);
   		delete conv_dial;
   	} catch (TQSLException& x) {
		if (compressed)
			gzclose(gout);
		else
			out.close();
   		this->Enable(TRUE);
   		delete conv_dial;
		string msg = x.what();
		tqsl_getConverterLine(conv, &lineno);
   		tqsl_endConverter(&conv);
		if (lineno)
			msg += wxString::Format(" on line %d", lineno);
		unlink(outfile.c_str());
		wxLogError("Signing aborted due to errors");
   		throw TQSLException(msg.c_str());
   	}
	if (out_of_range > 0)
		wxLogMessage(wxString::Format("%s: %d QSO records were outside the selected date range",
			infile.c_str(), out_of_range));
	if (cancelled || n == 0)
		wxLogMessage("No records output");
	else
	   	wxLogMessage(wxString::Format("%s: wrote %d records to %s", infile.c_str(), n,
			outfile.c_str()));
	if (!cancelled && n > 0)
		wxLogMessage(outfile + " is ready to be emailed or uploaded");
	else
		unlink(outfile.c_str());
	return cancelled;
}

tQSL_Location
MyFrame::SelectStationLocation(const wxString& title, bool editonly) {
   	int rval;
   	tQSL_Location loc;
   	wxString selname;
   	do {
   		TQSLGetStationNameDialog station_dial(this, &help, wxDefaultPosition, false, title, editonly);
   		if (selname != "")
   			station_dial.SelectName(selname);
   		rval = station_dial.ShowModal();
   		switch (rval) {
   			case wxID_CANCEL:
   				return 0;
   			case wxID_APPLY:
   				check_tqsl_error(tqsl_initStationLocationCapture(&loc));
   				selname = run_station_wizard(this, loc, &help);
   				check_tqsl_error(tqsl_endStationLocationCapture(&loc));
   				break;
   			case wxID_MORE:
   		   		check_tqsl_error(tqsl_getStationLocation(&loc, station_dial.Selected().c_str()));
   				selname = run_station_wizard(this, loc, &help, "Edit Station Location", station_dial.Selected());
   				check_tqsl_error(tqsl_endStationLocationCapture(&loc));
   				break;
   			case wxID_OK:
   		   		check_tqsl_error(tqsl_getStationLocation(&loc, station_dial.Selected().c_str()));
   				break;
   		}
   	} while (rval != wxID_OK);
	return loc;
}

static void
wx_tokens(const wxString& str, vector<wxString> &toks) {
	size_t idx = 0;
	size_t newidx;
	wxString tok;
	do {
		newidx = str.find(" ", idx);
		if (newidx != wxString::npos) {
			toks.push_back(str.Mid(idx, newidx - idx));
			idx = newidx + 1;
		}
	} while (newidx != wxString::npos);
	if (str.Mid(idx) != "")
		toks.push_back(str.Mid(idx));
}

void
MyFrame::ImportQSODataFile(wxCommandEvent& event) {
	wxString infile;
	try {
		bool compressed = (event.m_id == tm_f_import_compress);
		tQSL_Location loc = SelectStationLocation("Select Station Location for Signing");
		if (loc == 0)
			return;
   		// Get input file
		wxString path = wxConfig::Get()->Read("ImportPath", "");
		// Construct filter string for file-open dialog
		wxString filter = "All files (*.*)|*.*";
		vector<wxString> exts;
		wxString file_exts = wxConfig::Get()->Read("ADIFFiles", DEFAULT_ADIF_FILES);
		wx_tokens(file_exts, exts);
		for (int i = 0; i < (int)exts.size(); i++) {
			filter += "|ADIF files (*." + exts[i] + ")|*." + exts[i];
		}
		exts.clear();
		file_exts = wxConfig::Get()->Read("CabrilloFiles", DEFAULT_CABRILLO_FILES);
		wx_tokens(file_exts, exts);
		for (int i = 0; i < (int)exts.size(); i++) {
			filter += "|Cabrillo files (*." + exts[i] + ")|*." + exts[i];
		}
		infile = wxFileSelector("Select file to Sign", path, "", "adi", filter,
			wxOPEN|wxFILE_MUST_EXIST, this);
   		if (infile == "")
   			return;
		wxConfig::Get()->Write("ImportPath", wxPathOnly(infile));
		// Get output file
		wxString basename;
		wxSplitPath(infile.c_str(), 0, &basename, 0);
		path = wxConfig::Get()->Read("ExportPath", "");
		wxString deftype = compressed ? "tq8" : "tq7";
		filter = compressed ? "TQSL compressed data files (*.tq8)|*.tq8"
			: "TQSL data files (*.tq7)|*.tq7";
   		wxString outfile = wxFileSelector("Select file to write to",
   			path, basename, "tq7", filter + "|All files (*.*)|*.*",
   			wxSAVE|wxOVERWRITE_PROMPT, this);
   		if (outfile == "")
   			return;
		wxConfig::Get()->Write("ExportPath", wxPathOnly(outfile));
		char callsign[40];
		int dxccnum;
		check_tqsl_error(tqsl_getLocationCallSign(loc, callsign, sizeof callsign));
		check_tqsl_error(tqsl_getLocationDXCCEntity(loc, &dxccnum));
		DXCC dxcc;
		dxcc.getByEntity(dxccnum);
		if (wxMessageBox(wxString::Format("The file (%s) will be signed using:\n"
			"Call sign: %s\nDXCC: %s\nIs this correct?", infile.c_str(),
			callsign, dxcc.name()), "TQSL - Confirm signing", wxYES_NO, this) == wxYES)
			ConvertLogFile(loc, infile, outfile, compressed);
		else
			wxLogMessage("Signing abandoned");
	}
	catch (TQSLException& x) {
		wxString s;
		if (infile != "")
			s = infile + ": ";
		s += x.what();
		wxLogError(s);
	}
	free_certlist();
}

#ifdef ALLOW_UNCOMPRESSED

void
MyFrame::OnFileCompress(wxCommandEvent& event) {
	bool uncompress = (event.m_id == tm_f_uncompress);
	wxString defin = uncompress ? "tq8" : "tq7";
	wxString defout = uncompress ? "tq7" : "tq8";
	wxString filt_uncompressed = "TQSL data files (*.tq7)|*.tq7|All files (*.*)|*.*";
	wxString filt_compressed = "TQSL compressed data files (*.tq8)|*.tq8|All files (*.*)|*.*";
	wxString filtin = uncompress ? filt_compressed : filt_uncompressed;
	wxString filtout = uncompress ? filt_uncompressed : filt_compressed;

	wxString infile;
   	// Get input file
	wxString path = wxConfig::Get()->Read("ExportPath", "");
	infile = wxFileSelector(wxString("Select file to ") + (uncompress ? "Uncompress" : "Compress"),
   		path, "", defin, filtin,
   		wxOPEN|wxFILE_MUST_EXIST, this);
   	if (infile == "")
   		return;
	ifstream in;
	gzFile gin = 0;
	if (uncompress)
		gin = gzopen(infile.c_str(), "rb");
	else
		in.open(infile, ios::in | ios::binary);
	if ((uncompress && !gin) || (!uncompress && !in)) {
		wxMessageBox(wxString("Unable to open ") + infile, ErrorTitle, wxOK|wxCENTRE, this);
		return;
	}
	wxConfig::Get()->Write("ExportPath", wxPathOnly(infile));
	// Get output file
	wxString basename;
	wxSplitPath(infile.c_str(), 0, &basename, 0);
	path = wxConfig::Get()->Read("ExportPath", "");
	wxString deftype = uncompress ? "tq8" : "tq7";
	wxString filter = uncompress ? "TQSL compressed data files (*.tq8)|*.tq8"
		: "TQSL data files (*.tq7)|*.tq7";
   	wxString outfile = wxFileSelector("Select file to write to",
   		path, basename, defout, filtout,
   		wxSAVE|wxOVERWRITE_PROMPT, this);
   	if (outfile == "")
   		return;
	wxConfig::Get()->Write("ExportPath", wxPathOnly(outfile));

	gzFile gout = 0;
	ofstream out;

	if (uncompress)
		out.open(outfile, ios::out | ios::trunc | ios::binary);
	else
		gout = gzopen(outfile.c_str(), "wb9");
	if ((uncompress && !out) || (!uncompress && !gout)) {
		wxMessageBox(wxString("Unable to open ") + outfile, ErrorTitle, wxOK|wxCENTRE, this);
		if (uncompress)
			gzclose(gin);
		return;
	}
	char buf[512];
	int n;

	// Need to add error checks

	if (uncompress) {
		do {
			n = gzread(gin, buf, sizeof buf);
			if (n > 0)
				out.write(buf, n);
		} while (n == sizeof buf);
		gzclose(gin);
	} else {
		do {
			in.read(buf, sizeof buf);
			if (in.gcount() > 0)
				gzwrite(gout, buf, in.gcount());
		} while (in.gcount() == sizeof buf);
		gzclose(gout);
	}
	wxLogMessage(wxString::Format("%s written to %s file %s", infile.c_str(),
		(uncompress ? "uncompressed" : "compressed"), outfile.c_str()));
}

#endif // ALLOW_UNCOMPRESSED

void MyFrame::OnPreferences(wxCommandEvent& WXUNUSED(event)) {
	Preferences dial(this, &help);
	dial.ShowModal();
}

QSLApp::QSLApp() : wxApp() {
	wxConfigBase::Set(new wxConfig("tqslapp"));
}

QSLApp::~QSLApp() {
	wxConfigBase *c = wxConfigBase::Set(0);
	if (c)
		delete c;
}

wxLog *
QSLApp::CreateLogTarget() {
	MyFrame *mf = (MyFrame *)GetTopWindow();
	if (mf)
		return new LogList(mf);
	return 0;
}

bool
QSLApp::OnInit() {
	MyFrame *frame = new MyFrame("TQSL", 50, 50, 550, 400);
	frame->Show(true);
	SetTopWindow(frame);

	tQSL_Location loc = 0;
	bool locsw = false;
	for (int i = 1; i < argc; i++) {
		if (locsw) {
			if (loc)
				tqsl_endStationLocationCapture(&loc);
			if (tqsl_getStationLocation(&loc, argv[i])) {
				wxMessageBox(tqsl_getErrorString(), ErrorTitle, wxOK|wxCENTRE, frame);
				return FALSE;
			}
			locsw = false;
		} else if (!strcasecmp(argv[i], "-l")) {
			locsw = true;
		} else if (loc == 0) {
			wxMessageBox("-l option must precede filename", ErrorTitle, wxOK|wxCENTRE, frame);
			return FALSE;
		} else {
			wxString path, name, ext;
			wxSplitPath(argv[i], &path, &name, &ext);
			path += "/";
			path += name + ".tq7";
			wxString infile(argv[i]);
			try {
				if (frame->ConvertLogFile(loc, infile, path))
					break;
			} catch (TQSLException& x) {
				wxString s;
				if (infile)
					s = infile + ": ";
				s += x.what();
				wxLogError(s);
			}
		}
	}

	return true;
}
