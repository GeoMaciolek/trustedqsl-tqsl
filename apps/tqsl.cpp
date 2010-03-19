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
#include <wx/wxchar.h>
#include <wx/config.h>

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include <wx/wxhtml.h>
#include <wx/wfstream.h>
#include "wxutil.h"

#include <iostream>
#include <fstream>
#include <memory>
#ifdef __WINDOWS__
	#include <io.h>
#endif
#include <zlib.h>
#include "tqslwiz.h"
#include "qsodatadialog.h"
#include "tqslerrno.h"
#include "tqslexcept.h"
#include "tqslpaths.h"
#include "stationdial.h"
#include "tqslconvert.h"
#include "dxcc.h"
#include "tqsl_prefs.h"

#undef ALLOW_UNCOMPRESSED

#include "tqslbuild.h"


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

static wxString ErrorTitle(wxT("TQSL Error"));

/////////// Application //////////////

class QSLApp : public wxApp {
public:
	QSLApp();
	virtual ~QSLApp();
	class MyFrame *GUIinit();
	bool OnInit();
//	virtual wxLog *CreateLogTarget();
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
	
	wxString message = wxString::Format(wxT("Enter the password to unlock the private key for\n"
		"%s -- %s\n(This is the password you made up when you\nrequested the certificate.)"),
		wxString(call, wxConvLocal).c_str(), wxString(dxccname, wxConvLocal).c_str());

	wxString pw = wxGetPasswordFromUser(message, wxT("Enter password"), wxT(""),
		wxGetApp().GetTopWindow());
	if (pw == wxT(""))
		return 1;
	strncpy(buf, pw.mb_str(), bufsiz);
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
	: wxDialog(parent, -1, wxString(wxT("Signing QSO Data"))),
	running(true) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	wxString label = wxString(wxT("Converting ")) + wxString(filename, wxConvLocal) + wxT(" to TQSL format");
	sizer->Add(new wxStaticText(this, -1, label), 0, wxALL|wxALIGN_CENTER, 10);
	msg = new wxStaticText(this, TQSL_CD_MSG, wxT(""));
	sizer->Add(msg, 0, wxALL|wxALIGN_CENTER, 10);
	canbut = new wxButton(this, TQSL_CD_CANBUT, wxT("Cancel"));
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

DateRangeDialog::DateRangeDialog(wxWindow *parent) : wxDialog(parent, -1, wxString(wxT("QSO Date Range"))) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1,
		wxT("You may set the starting and/or ending QSO dates\n"
		"in order to select QSOs from the input file.\n\n"
		"QSOs prior to the starting date or after the ending\n"
		"date will not be signed or included in the output file.\n\n"
		"You may leave either date (or both dates) blank.")
	), 0, wxALL|wxALIGN_CENTER, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, wxT("Start Date (YYYY-MM-DD)")), 0, wxRIGHT, 5);
	start_tc = new wxTextCtrl(this, TQSL_DR_START);
	hsizer->Add(start_tc, 0, 0, 0);
	sizer->Add(hsizer, 0, wxALL|wxALIGN_CENTER, 10);
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, wxT("End Date (YYYY-MM-DD)")), 0, wxRIGHT, 5);
	end_tc = new wxTextCtrl(this, TQSL_DR_END);
	hsizer->Add(end_tc, 0, 0, 0);
	sizer->Add(hsizer, 0, wxALL|wxALIGN_CENTER, 10);
	msg = new wxStaticText(this, TQSL_DR_MSG, wxT(""));
	sizer->Add(msg, 0, wxALL, 5);
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxButton(this, TQSL_DR_OK, wxT("Ok")), 0, wxRIGHT, 5);
	hsizer->Add(new wxButton(this, TQSL_DR_CAN, wxT("Cancel")), 0, wxLEFT, 10);
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
	if (text.Trim() == wxT(""))
		start.year = start.month = start.day = 0;
	else if (tqsl_initDate(&start, text.mb_str()) || !tqsl_isDateValid(&start)) {
		msg->SetLabel(wxT("Start date is invalid"));
		return false;
	}
	text = end_tc->GetValue();
	if (text.Trim() == wxT(""))
		end.year = end.month = end.day = 0;
	else if (tqsl_initDate(&end, text.mb_str()) || !tqsl_isDateValid(&end)) {
		msg->SetLabel(wxT("End date is invalid"));
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
	config->SetPath(wxT("/modeMap"));
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, wxT(""));
		tqsl_setADIFMode(key.mb_str(), value.mb_str());
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath(wxT("/"));
}

static void
init_contests() {
	tqsl_clearCabrilloMap();
	wxConfig *config = (wxConfig *)wxConfig::Get();
	long cookie;
	wxString key, value;
	config->SetPath(wxT("/cabrilloMap"));
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, wxT(""));
		int contest_type = atoi(value.mb_str());
		int callsign_field = atoi(value.AfterFirst(';').mb_str());
		tqsl_setCabrilloMapEntry(key.mb_str(), callsign_field, contest_type);
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath(wxT("/"));
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
	void OnExit(TQ_WXCLOSEEVENT& event);
	void DoExit(wxCommandEvent& event);
	void OnHelpAbout(wxCommandEvent& event);
	void OnHelpContents(wxCommandEvent& event);
#ifdef ALLOW_UNCOMPRESSED
	void OnFileCompress(wxCommandEvent& event);
#endif
	void OnPreferences(wxCommandEvent& event);
	bool ConvertLogFile(tQSL_Location loc, wxString& infile, wxString& outfile, bool compress = false, bool suppressdate = false, const char *password = NULL);
	tQSL_Location SelectStationLocation(const wxString& title = wxT(""), const wxString& okLabel = wxT("Ok"), bool editonly = false);
	void WriteQSOFile(QSORecordList& recs, const char *fname = 0, bool force = false);

	wxTextCtrl *logwin;
	wxHtmlHelpController help;

	DECLARE_EVENT_TABLE()
};

class MyFrame;

class LogList : public wxLog {
public:
	LogList(MyFrame *frame) : wxLog(), _frame(frame) {}
	virtual void DoLogString(const wxChar *szString, time_t t);
private:
	MyFrame *_frame;
};

void LogList::DoLogString(const wxChar *szString, time_t) {
	wxTextCtrl *_logwin = 0;
	if (!strncmp(wxString(szString).mb_str(), "Debug:", 6))
		return;
	if (_frame != 0)
		_logwin = _frame->logwin;
	if (_logwin == 0) {
		cerr << szString << endl;
		return;
	}
	_logwin->AppendText(szString);
	_logwin->AppendText(wxT("\n"));
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
	EVT_MENU(tm_f_exit, MyFrame::DoExit)
	EVT_MENU(tm_f_preferences, MyFrame::OnPreferences)
	EVT_MENU(tm_h_contents, MyFrame::OnHelpContents)
	EVT_MENU(tm_h_about, MyFrame::OnHelpAbout)
	EVT_CLOSE(MyFrame::OnExit)
END_EVENT_TABLE()

void
MyFrame::OnExit(TQ_WXCLOSEEVENT& WXUNUSED(event)) {
	Destroy();
}

void
MyFrame::DoExit(wxCommandEvent& WXUNUSED(event)) {
	Destroy();
}

MyFrame::MyFrame(const wxString& title, int x, int y, int w, int h)
	: wxFrame(0, -1, title, wxPoint(x, y), wxSize(w, h)) {

	DocPaths docpaths(wxT("tqslapp"));
	// File menu
	wxMenu *file_menu = new wxMenu;
	file_menu->Append(tm_f_import_compress, wxT("&Sign existing ADIF or Cabrillo file..."));
#ifdef ALLOW_UNCOMPRESSED
	file_menu->Append(tm_f_import, wxT("Sign &Uncompressed..."));
#endif
	file_menu->AppendSeparator();
	file_menu->Append(tm_f_new, wxT("Create &New ADIF file..."));
	file_menu->Append(tm_f_edit, wxT("&Edit existing ADIF file..."));
#ifdef ALLOW_UNCOMPRESSED
	file_menu->AppendSeparator();
	file_menu->Append(tm_f_compress, wxT("Co&mpress..."));
	file_menu->Append(tm_f_uncompress, wxT("&Uncompress..."));
#endif
#ifndef __WXMAC__	// On Mac, Preferences not on File menu
	file_menu->AppendSeparator();
#endif
	file_menu->Append(tm_f_preferences, wxT("&Preferences..."));
#ifndef __WXMAC__	// On Mac, Exit not on File menu
	file_menu->AppendSeparator();
#endif
	file_menu->Append(tm_f_exit, wxT("E&xit"));

	// Station menu
	wxMenu *stn_menu = new wxMenu;
	stn_menu->Append(tm_s_add, wxT("&Add location"));
	stn_menu->Append(tm_s_edit, wxT("&Edit locations"));

	// Help menu
	wxMenu *help_menu = new wxMenu;
	help.UseConfig(wxConfig::Get());
	wxString hhp = docpaths.FindAbsoluteValidPath(wxT("tqslapp.hhp"));
	if (wxFileNameFromPath(hhp) != wxT("")) {
		if (help.AddBook(hhp))
		help_menu->Append(tm_h_contents, wxT("&Contents"));
#ifndef __WXMAC__	// On Mac, About not on Help menu
		help_menu->AppendSeparator();
#endif
	}
	help_menu->Append(tm_h_about, wxT("&About"));

	// Main menu
	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, wxT("&File"));
	menu_bar->Append(stn_menu, wxT("&Station"));
	menu_bar->Append(help_menu, wxT("&Help"));

	SetMenuBar(menu_bar);

	logwin = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(400, 200),
		wxTE_MULTILINE|wxTE_READONLY);
}

static wxString
run_station_wizard(wxWindow *parent, tQSL_Location loc, wxHtmlHelpController *help = 0,
	wxString title = wxT("Add Station Location"), wxString dataname = wxT("")) {
	wxString rval(wxT(""));
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
	check_tqsl_error(tqsl_setStationLocationCaptureName(loc, rval.mb_str()));
	check_tqsl_error(tqsl_saveStationLocationCapture(loc, 1));
	return rval;
}

void
MyFrame::OnHelpContents(wxCommandEvent& WXUNUSED(event)) {
	help.Display(wxT("main.htm"));
}

void
MyFrame::OnHelpAbout(wxCommandEvent& WXUNUSED(event)) {
	wxString msg = wxT("TQSL V" VERSION "." BUILD "\n(c) 2001-2010\nAmerican Radio Relay League\n\n");
	int major, minor;
	if (tqsl_getVersion(&major, &minor))
		wxLogError(wxString(tqsl_getErrorString(), wxConvLocal));
	else
		msg += wxString::Format(wxT("TrustedQSL library V%d.%d\n"), major, minor);
	if (tqsl_getConfigVersion(&major, &minor))
		wxLogError(wxString(tqsl_getErrorString(), wxConvLocal));
	else
		msg += wxString::Format(wxT("Configuration data V%d.%d\n"), major, minor);
	msg += wxVERSION_STRING;
#ifdef wxUSE_UNICODE
	if (wxUSE_UNICODE)
		msg += wxT(" (Unicode)");
#endif
	wxMessageBox(msg, wxT("About"), wxOK|wxCENTRE|wxICON_INFORMATION, this);
}

static void
AddEditStationLocation(tQSL_Location loc, const wxString& title = wxT("Add Station Location")) {
	try {
		MyFrame *frame = (MyFrame *)wxGetApp().GetTopWindow();
		run_station_wizard(frame, loc, &(frame->help), title);
	}
	catch (TQSLException& x) {
		wxLogError(wxString(x.what(), wxConvLocal));
	}
}

void
MyFrame::AddStationLocation(wxCommandEvent& WXUNUSED(event)) {
	tQSL_Location loc;
	check_tqsl_error(tqsl_initStationLocationCapture(&loc));
	AddEditStationLocation(loc);
	check_tqsl_error(tqsl_endStationLocationCapture(&loc));
}

void
MyFrame::EditStationLocation(wxCommandEvent& WXUNUSED(event)) {
	SelectStationLocation(wxT("Edit Station Locations"), wxT("Close"), true);
}

void
MyFrame::WriteQSOFile(QSORecordList& recs, const char *fname, bool force) {
	if (recs.empty()) {
		wxLogWarning(wxT("No QSO records"));
		return;
	}
	wxString s_fname;
	if (fname)
		s_fname = wxString(fname, wxConvLocal);
	if (s_fname == wxT("") || !force) {
		wxString path, basename, type;
		wxSplitPath(s_fname, &path, &basename, &type);
		if (type != wxT(""))
			basename += wxT(".") + type;
		if (path == wxT(""))
			path = wxConfig::Get()->Read(wxT("QSODataPath"), wxT(""));
		s_fname = wxFileSelector(wxT("Save File"), path, basename, wxT("adi"),
			wxT("ADIF files (*.adi)|*.adi|All files (*.*)|*.*"),
			wxSAVE|wxOVERWRITE_PROMPT, this);
		if (s_fname == wxT(""))
			return;
		wxConfig::Get()->Write(wxT("QSODataPath"), wxPathOnly(s_fname));
	}
	ofstream out(s_fname.mb_str(), ios::out|ios::trunc|ios::binary);
	if (!out.is_open())
		return;
	unsigned char buf[256];
	QSORecordList::iterator it;
	for (it = recs.begin(); it != recs.end(); it++) {
		wxString dtstr;
		tqsl_adifMakeField("CALL", 0, (const unsigned char*)(const char *)it->_call.mb_str(), -1, buf, sizeof buf);
		out << buf << endl;
		tqsl_adifMakeField("BAND", 0, (const unsigned char*)(const char *)it->_band.mb_str(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		tqsl_adifMakeField("MODE", 0, (const unsigned char*)(const char *)it->_mode.mb_str(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		dtstr.Printf(wxT("%04d%02d%02d"), it->_date.year, it->_date.month, it->_date.day);
		tqsl_adifMakeField("QSO_DATE", 0, (const unsigned char*)(const char *)dtstr.mb_str(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		dtstr.Printf(wxT("%02d%02d%02d"), it->_time.hour, it->_time.minute, it->_time.second);
		tqsl_adifMakeField("TIME_ON", 0, (const unsigned char*)(const char *)dtstr.mb_str(), -1, buf, sizeof buf);
		out << "   " << buf << endl;
		if (it->_freq != wxT("")) {
			tqsl_adifMakeField("FREQ", 0, (const unsigned char*)(const char *)it->_freq.mb_str(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (it->_rxband != wxT("")) {
			tqsl_adifMakeField("BAND_RX", 0, (const unsigned char*)(const char *)it->_rxband.mb_str(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (it->_rxfreq != wxT("")) {
			tqsl_adifMakeField("FREQ_RX", 0, (const unsigned char*)(const char *)it->_rxfreq.mb_str(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (it->_propmode != wxT("")) {
			tqsl_adifMakeField("PROP_MODE", 0, (const unsigned char*)(const char *)it->_propmode.mb_str(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		if (it->_satellite != wxT("")) {
			tqsl_adifMakeField("SAT_NAME", 0, (const unsigned char*)(const char *)it->_satellite.mb_str(), -1, buf, sizeof buf);
			out << "   " << buf << endl;
		}
		out << "<EOR>" << endl;
	}
	out.close();
	wxLogMessage(wxT("Wrote %d QSO records to %s"), (int)recs.size(), s_fname.c_str());
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

static const char *defined_types[] = { "T", "D", "M", "C", "N", "6" };

static unsigned char *
adif_alloc(size_t n) {
	return new unsigned char[n];
}

void
MyFrame::EditQSOData(wxCommandEvent& WXUNUSED(event)) {
	QSORecordList recs;
	wxString file = wxFileSelector(wxT("Open File"), wxConfig::Get()->Read(wxT("QSODataPath"), wxT("")), wxT(""), wxT("adi"),
			wxT("ADIF files (*.adi)|*.adi|All files (*.*)|*.*"),
			wxOPEN|wxFILE_MUST_EXIST, this);
	if (file == wxT(""))
		return;
	init_modes();
	tqsl_adifFieldResults field;
	TQSL_ADIF_GET_FIELD_ERROR stat;
	tQSL_ADIF adif;
	check_tqsl_error(tqsl_beginADIF(&adif, file.mb_str()));
	QSORecord rec;
	do {
		check_tqsl_error(tqsl_getADIFField(adif, &field, &stat, fielddefs, defined_types, adif_alloc));
		if (stat == TQSL_ADIF_GET_FIELD_SUCCESS) {
			if (!strcasecmp(field.name, "CALL"))
				rec._call = wxString((const char *)field.data, wxConvLocal);
			else if (!strcasecmp(field.name, "BAND"))
				rec._band = wxString((const char *)field.data, wxConvLocal);
			else if (!strcasecmp(field.name, "BAND_RX"))
				rec._rxband = wxString((const char *)field.data, wxConvLocal);
			else if (!strcasecmp(field.name, "MODE")) {
				rec._mode = wxString((const char *)field.data, wxConvLocal);
				char amode[40];
				if (tqsl_getADIFMode(rec._mode.mb_str(), amode, sizeof amode) == 0 && amode[0] != '\0')
					rec._mode = wxString(amode, wxConvLocal);
			} else if (!strcasecmp(field.name, "FREQ"))
				rec._freq = wxString((const char *)field.data, wxConvLocal);
			else if (!strcasecmp(field.name, "FREQ_RX"))
				rec._rxfreq = wxString((const char *)field.data, wxConvLocal);
			else if (!strcasecmp(field.name, "PROP_MODE"))
				rec._propmode = wxString((const char *)field.data, wxConvLocal);
			else if (!strcasecmp(field.name, "SAT_NAME"))
				rec._satellite = wxString((const char *)field.data, wxConvLocal);
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
					rec._time.second = (strlen(cp) > 4) ? atoi(cp+4) : 0;
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
			WriteQSOFile(recs, file.mb_str());
	} catch (TQSLException& x) {
		wxLogError(wxString(x.what(), wxConvLocal));
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
		wxLogError(wxString(x.what(), wxConvLocal));
	}
}

bool
MyFrame::ConvertLogFile(tQSL_Location loc, wxString& infile, wxString& outfile,
	bool compressed, bool suppressdate, const char *password) {
	static const char *iam = "TQSL V" VERSION;
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
		wxMessageBox(wxT("No matching certs for QSO"), ErrorTitle, wxOK|wxCENTRE, this);
		return false;
	}

	wxLogMessage(wxT("Signing using CALL=%s, DXCC=%d"), wxString(callsign, wxConvLocal).c_str(),
		dxcc);

	init_modes();
	init_contests();

	if (compressed)
		gout = gzopen(outfile.mb_str(), "wb9");
	else
		out.open(outfile.mb_str(), ios::out|ios::trunc|ios::binary);

	if ((compressed && !gout) || (!compressed && !out)) {
		wxMessageBox(wxString(wxT("Unable to open ")) + outfile, ErrorTitle, wxOK|wxCENTRE, this);
		return false;
	}

	ConvertingDialog *conv_dial = new ConvertingDialog(this, name.mb_str());
	int n = 0;
	bool cancelled = false;
	int lineno = 0;
	int out_of_range = 0;
   	try {
   		if (tqsl_beginCabrilloConverter(&conv, infile.mb_str(), certlist, ncerts, loc)) {
			if (tQSL_Error != TQSL_CABRILLO_ERROR || tQSL_Cabrillo_Error != TQSL_CABRILLO_NO_START_RECORD)
				check_tqsl_error(1);	// A bad error
			lineno = 0;
	   		check_tqsl_error(tqsl_beginADIFConverter(&conv, infile.mb_str(), certlist, ncerts, loc));
		}
		bool range = true;
		config->Read(wxT("DateRange"), &range);
		if (range && !suppressdate) {
			DateRangeDialog dial(this);
			if (dial.ShowModal() != wxOK) {
				wxLogMessage(wxT("Cancelled"));
				return false;
			}
			tqsl_setADIFConverterDateFilter(conv, &dial.start, &dial.end);
		}
		bool allow = false;
		config->Read(wxT("BadCalls"), &allow);
		tqsl_setConverterAllowBadCall(conv, allow);
		wxSplitPath(infile, 0, &name, &ext);
		if (ext != wxT(""))
			name += wxT(".") + ext;
		// Only display windows if notin batch mode -- KD6PAG
		if (this) {
			conv_dial->Show(TRUE);
			this->Enable(FALSE);
		}
		bool ignore_err = false;
		int major = 0, minor = 0, config_major = 0, config_minor = 0;
		tqsl_getVersion(&major, &minor);
		tqsl_getConfigVersion(&config_major, &config_minor);
		wxString ident = wxString::Format(wxT("%s Lib: V%d.%d Config: V%d.%d"), wxString(iam, wxConvLocal).c_str(),
			major, minor, config_major, config_minor);
		wxString gabbi_ident = wxString::Format(wxT("<TQSL_IDENT:%d>%s"), (int)ident.length(), ident.c_str());
		gabbi_ident += wxT("\n");
		if (compressed)
			gzwrite(gout, (const char *)gabbi_ident.mb_str(), gabbi_ident.length());
		else
			out << gabbi_ident << endl;
   		do {
   	   		while ((cp = tqsl_getConverterGABBI(conv)) != 0) {
  					wxSafeYield(conv_dial);
  					if (!conv_dial->running)
  						break;
					++n;
					if ((n % 10) == 0)
		   	   			conv_dial->msg->SetLabel(wxString::Format(wxT("%d"), n));
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
	   				if ((rval = tqsl_beginSigning(cert, (char *)password, getPassword, cert)) == 0)
						break;
					if (tQSL_Error == TQSL_PASSWORD_ERROR) {
						wxLogMessage(wxT("Password error"));
						if (password)
							free((void *)password);
						password = NULL;
					}
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
					wxString msg = wxString(x.what(), wxConvLocal);
					if (lineno)
						msg += wxString::Format(wxT(" on line %d"), lineno);
					const char *bad_text = tqsl_getConverterRecordText(conv);
					if (bad_text)
						msg += wxString(wxT("\n")) + wxString(bad_text, wxConvLocal);
					wxLogError(msg);
					// Only ask if not in batch mode or ignoring errors - KD6PAG
					if (!ignore_err && this) {
						if (wxMessageBox(wxString(wxT("Error: ")) + msg + wxT("\n\nIgnore errors?"), wxT("Error"), wxYES_NO, this) == wxNO)
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
   		if (this)
			this->Enable(TRUE);
		if (compressed)
			gzclose(gout);
		else
			out.close();
   		if (cancelled) {
   			wxLogWarning(wxT("Signing cancelled"));
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
		if (this)
	   		this->Enable(TRUE);
   		delete conv_dial;
		string msg = x.what();
		tqsl_getConverterLine(conv, &lineno);
   		tqsl_endConverter(&conv);
		if (lineno)
			msg += wxString::Format(wxT(" on line %d"), lineno).mb_str();
		unlink(outfile.mb_str());
		wxLogError(wxT("Signing aborted due to errors"));
   		throw TQSLException(msg.c_str());
   	}
	if (out_of_range > 0)
		wxLogMessage(wxString::Format(wxT("%s: %d QSO records were outside the selected date range"),
			infile.c_str(), out_of_range));
	if (cancelled || n == 0)
		wxLogMessage(wxT("No records output"));
	else
	   	wxLogMessage(wxString::Format(wxT("%s: wrote %d records to %s"), infile.c_str(), n,
			outfile.c_str()));
	if (!cancelled && n > 0)
		wxLogMessage(outfile + wxT(" is ready to be emailed or uploaded"));
	else
		unlink(outfile.mb_str());
	return cancelled;
}

tQSL_Location
MyFrame::SelectStationLocation(const wxString& title, const wxString& okLabel, bool editonly) {
   	int rval;
   	tQSL_Location loc;
   	wxString selname;
   	do {
   		TQSLGetStationNameDialog station_dial(this, &help, wxDefaultPosition, false, title, okLabel, editonly);
   		if (selname != wxT(""))
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
   		   		check_tqsl_error(tqsl_getStationLocation(&loc, station_dial.Selected().mb_str()));
   				selname = run_station_wizard(this, loc, &help, wxT("Edit Station Location"), station_dial.Selected());
   				check_tqsl_error(tqsl_endStationLocationCapture(&loc));
   				break;
   			case wxID_OK:
   		   		check_tqsl_error(tqsl_getStationLocation(&loc, station_dial.Selected().mb_str()));
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
		newidx = str.find(wxT(" "), idx);
		if (newidx != wxString::npos) {
			toks.push_back(str.Mid(idx, newidx - idx));
			idx = newidx + 1;
		}
	} while (newidx != wxString::npos);
	if (str.Mid(idx) != wxT(""))
		toks.push_back(str.Mid(idx));
}

void
MyFrame::ImportQSODataFile(wxCommandEvent& event) {
	wxString infile;
	try {
		bool compressed = (event.GetId() == tm_f_import_compress);
		tQSL_Location loc = SelectStationLocation(wxT("Select Station Location for Signing"));
		if (loc == 0)
			return;
   		// Get input file
		wxString path = wxConfig::Get()->Read(wxT("ImportPath"), wxString(wxT("")));
		// Construct filter string for file-open dialog
		wxString filter = wxT("All files (*.*)|*.*");
		vector<wxString> exts;
		wxString file_exts = wxConfig::Get()->Read(wxT("ADIFFiles"), wxString(DEFAULT_ADIF_FILES));
		wx_tokens(file_exts, exts);
		for (int i = 0; i < (int)exts.size(); i++) {
			filter += wxT("|ADIF files (*.") + exts[i] + wxT(")|*.") + exts[i];
		}
		exts.clear();
		file_exts = wxConfig::Get()->Read(wxT("CabrilloFiles"), wxString(DEFAULT_CABRILLO_FILES));
		wx_tokens(file_exts, exts);
		for (int i = 0; i < (int)exts.size(); i++) {
			filter += wxT("|Cabrillo files (*.") + exts[i] + wxT(")|*.") + exts[i];
		}
		infile = wxFileSelector(wxT("Select file to Sign"), path, wxT(""), wxT("adi"), filter,
			wxOPEN|wxFILE_MUST_EXIST, this);
   		if (infile == wxT(""))
   			return;
		wxConfig::Get()->Write(wxT("ImportPath"), wxPathOnly(infile));
		// Get output file
		wxString basename;
		wxSplitPath(infile.c_str(), 0, &basename, 0);
		path = wxConfig::Get()->Read(wxT("ExportPath"), wxString(wxT("")));
		wxString deftype = compressed ? wxT("tq8") : wxT("tq7");
		filter = compressed ? wxT("TQSL compressed data files (*.tq8)|*.tq8")
			: wxT("TQSL data files (*.tq7)|*.tq7");
   		wxString outfile = wxFileSelector(wxT("Select file to write to"),
   			path, basename, wxT("tq7"), filter + wxT("|All files (*.*)|*.*"),
   			wxSAVE|wxOVERWRITE_PROMPT, this);
   		if (outfile == wxT(""))
   			return;
		wxConfig::Get()->Write(wxT("ExportPath"), wxPathOnly(outfile));
		char callsign[40];
		int dxccnum;
		check_tqsl_error(tqsl_getLocationCallSign(loc, callsign, sizeof callsign));
		check_tqsl_error(tqsl_getLocationDXCCEntity(loc, &dxccnum));
		DXCC dxcc;
		dxcc.getByEntity(dxccnum);
		if (wxMessageBox(wxString::Format(wxT("The file (%s) will be signed using:\n"
			"Call sign: %s\nDXCC: %s\nIs this correct?"), infile.c_str(),
			wxString(callsign, wxConvLocal).c_str(), wxString(dxcc.name(), wxConvLocal).c_str()),
			wxT("TQSL - Confirm signing"), wxYES_NO, this) == wxYES)
			ConvertLogFile(loc, infile, outfile, compressed);
		else
			wxLogMessage(wxT("Signing abandoned"));
	}
	catch (TQSLException& x) {
		wxString s;
		if (infile != wxT(""))
			s = infile + wxT(": ");
		s += wxString(x.what(), wxConvLocal);
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
		wxString(uncompress ? "uncompressed" : "compressed", wxConvLocal).c_str(),
		outfile.c_str()));
}

#endif // ALLOW_UNCOMPRESSED

void MyFrame::OnPreferences(wxCommandEvent& WXUNUSED(event)) {
	Preferences dial(this, &help);
	dial.ShowModal();
}

QSLApp::QSLApp() : wxApp() {
#ifdef __WXMAC__	// Tell wx to put these items on the proper menu
	wxApp::s_macAboutMenuItemId = long(tm_h_about);
	wxApp::s_macPreferencesMenuItemId = long(tm_f_preferences);
	wxApp::s_macExitMenuItemId = long(tm_f_exit);
#endif

	wxConfigBase::Set(new wxConfig(wxT("tqslapp")));
}

QSLApp::~QSLApp() {
	wxConfigBase *c = wxConfigBase::Set(0);
	if (c)
		delete c;
}

/*
wxLog *
QSLApp::CreateLogTarget() {
cerr << "called" << endl;
	MyFrame *mf = (MyFrame *)GetTopWindow();
	if (mf)
		return new LogList(mf);
	return 0;
}
*/

MyFrame *
QSLApp::GUIinit() {
	MyFrame *frame = new MyFrame(wxT("TQSL"), 50, 50, 550, 400);
	frame->Show(true);
	SetTopWindow(frame);

	LogList *log = new LogList(frame);
	wxLog::SetActiveTarget(log);
	return frame;
}

bool
QSLApp::OnInit() {
	MyFrame *frame = 0;

	tQSL_Location loc = 0;
	bool locsw = false;
	bool suppressdate = false;
	bool pwdsw = false;
	bool outflsw = false;
	bool quiet = false;
	char *password = NULL;
	wxString infile(wxT(""));
	wxString outfile(wxT(""));

	// Send errors to 'stderr' if in batch mode. -- KD6PAG
	for (int i = 1; i < argc; i++) {
		if (!strcasecmp(wxString(argv[i]).mb_str(), "-x") ||
		    !strcasecmp(wxString(argv[i]).mb_str(), "-q")) {
			quiet = true;
		}
	}
	if (quiet) {
		wxLog::SetActiveTarget(new wxLogStderr(NULL));
	} else {
		frame = GUIinit();
	}
	for (int i = 1; i < argc; i++) {
		if (locsw) {
			if (loc)
				tqsl_endStationLocationCapture(&loc);
			if (tqsl_getStationLocation(&loc, wxString(argv[i]).mb_str())) {
				wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), ErrorTitle, wxOK|wxCENTRE, frame);
				return false;
			}
			locsw = false;
		} else if (pwdsw) {
			password = strdup(wxString(argv[i]).mb_str());
			pwdsw = false;
		} else if (outflsw) {
			outfile = wxString(argv[i]);
			outflsw = false;
		} else if (!strcasecmp(wxString(argv[i]).mb_str(), "-l")) {
			locsw = true;
		} else if (!strcasecmp(wxString(argv[i]).mb_str(), "-o")) {
			outflsw = true;
		} else if (!strcasecmp(wxString(argv[i]).mb_str(), "-d")) {
			suppressdate = true;
		} else if (!strcasecmp(wxString(argv[i]).mb_str(), "-s")) {
			// Add/Edit station location
			if (!frame)
				frame = GUIinit();
			if (loc == 0) {
				check_tqsl_error(tqsl_initStationLocationCapture(&loc));
				AddEditStationLocation(loc);
			} else
				AddEditStationLocation(loc, wxT("Edit Station Location"));
		} else if (!strcasecmp(wxString(argv[i]).mb_str(), "-x") ||
		           !strcasecmp(wxString(argv[i]).mb_str(), "-q")) {
			continue;	// Already handled
		} else if (!strcasecmp(wxString(argv[i]).mb_str(), "-p")) {
			pwdsw = true;
		} else {		// Must be a log file to sign
			infile = wxString(argv[i]);
		}
	}
	if (wxIsEmpty(infile)) {	// Nothing to sign
		if (argc == 1)		// No extra args, bring up GUI
			return true;
		wxLogError(wxT("No logfile to sign!"));
		return false;
	}
	if (loc == 0) {
		if (!frame)
			frame = GUIinit();
		loc = frame->SelectStationLocation(wxT("Select Station Location for Signing"));
	}
	if (loc == 0)
		return false;
	wxString path, name, ext;
	if (!wxIsEmpty(outfile)) {
		path = outfile;
	} else {
		wxSplitPath(infile, &path, &name, &ext);
		if (!wxIsEmpty(path))
			path += wxT("/");
		path += name + wxT(".tq8");
	}
	try {
		frame->ConvertLogFile(loc, infile, path, true, suppressdate, password);
	} catch (TQSLException& x) {
		wxString s;
		if (infile)
			s = infile + wxT(": ");
		s += wxString(x.what(), wxConvLocal);
		wxLogError(s);
	}
	return false;
}
