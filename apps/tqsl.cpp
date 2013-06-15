/***************************************************************************
                          tqsl.cpp  -  description
                             -------------------
    begin                : Mon Nov 4 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: tqsl.cpp,v 1.13 2010/03/19 21:31:02 k1mu Exp $
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif



#include <curl/curl.h> // has to be before something else in this list
#include <stdlib.h>
#include <errno.h>
#include <expat.h>

#include <wx/wxprec.h>
#include <wx/object.h>
#include <wx/wxchar.h>
#include <wx/config.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>
#include <wx/hashmap.h>
#include <wx/hyperlink.h>
#include <wx/cmdline.h>

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

#ifdef _MSC_VER //could probably do a more generic check here...
// stdint exists on vs2012 and (I think) 2010, but not 2008 or its platform
  #define uint8_t unsigned char
#else
#include <stdint.h> //for uint8_t; should be cstdint but this is C++11 and not universally supported
#endif

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
#include "tqslbuild.h"

#include "winstrdefs.h"

#include "key.xpm"

using namespace std;

#undef ALLOW_UNCOMPRESSED


enum {
	tm_f_import = 7000,
	tm_f_import_compress,
	tm_f_upload,
	tm_f_compress,
	tm_f_uncompress,
	tm_f_preferences,
	tm_f_loadconfig,
	tm_f_saveconfig,
	tm_f_new,
	tm_f_edit,
	tm_f_exit,
	tm_s_add,
	tm_s_edit,
	tm_h_contents,
	tm_h_about,
	tm_h_diag,
	tm_h_update
};

// Action values
enum {
	TQSL_ACTION_ASK = 0,
	TQSL_ACTION_ABORT = 1,
	TQSL_ACTION_NEW = 2,
	TQSL_ACTION_ALL = 3,
	TQSL_ACTION_UNSPEC = 4
};

// Exit codes
enum {
	TQSL_EXIT_SUCCESS = 0,
	TQSL_EXIT_CANCEL = 1,
	TQSL_EXIT_REJECTED = 2,
	TQSL_EXIT_UNEXP_RESP = 3,
	TQSL_EXIT_TQSL_ERROR = 4,
	TQSL_EXIT_LIB_ERROR = 5,
	TQSL_EXIT_ERR_OPEN_INPUT = 6,
	TQSL_EXIT_ERR_OPEN_OUTPUT = 7,
	TQSL_EXIT_NO_QSOS = 8,
	TQSL_EXIT_QSOS_SUPPRESSED = 9,
	TQSL_EXIT_COMMAND_ERROR = 10,
	TQSL_EXIT_CONNECTION_FAILED = 11,
	TQSL_EXIT_UNKNOWN = 12
};

#define TQSL_CD_MSG TQSL_ID_LOW
#define TQSL_CD_CANBUT TQSL_ID_LOW+1

static wxString ErrorTitle(wxT("TQSL Error"));

static void exitNow(int status, bool quiet) {
	const char *errors[] = { "Success",
				 "User Cancelled",
				 "Upload Rejected",
				 "Unexpected LoTW Response",
				 "TQSL Error",
				 "TQSLLib Error",
				 "Error opening input file",
				 "Error opening output file",
				 "No QSOs written",
				 "Some QSOs suppressed",
				 "Commmand Syntax Error",
				 "LoTW Connection Failed",
				 "Unknown"
				};
	int stat = status;
	if (stat > TQSL_EXIT_UNKNOWN || stat < 0) stat = TQSL_EXIT_UNKNOWN;
	if (quiet)
		wxLogMessage(wxT("Final Status: %hs (%d)"), errors[stat], status);
	else
		cerr << "Final Status: " << errors[stat] << "(" << status << ")" << endl;
	exit(status);
}

/////////// Application //////////////

class QSLApp : public wxApp {
public:
	QSLApp();
	virtual ~QSLApp();
	class MyFrame *GUIinit(bool checkUpdates);
	bool OnInit();
//	virtual wxLog *CreateLogTarget();
};

IMPLEMENT_APP(QSLApp)


static wxString lastPW;

static int
getPassword(char *buf, int bufsiz, tQSL_Cert cert) {
	char call[TQSL_CALLSIGN_MAX+1] = "";
	int dxcc = 0;
	const char *dxccname = "Unknown";
	tqsl_getCertificateCallSign(cert, call, sizeof call);
	tqsl_getCertificateDXCCEntity(cert, &dxcc);
	tqsl_getDXCCEntityName(dxcc, &dxccname);
	
	wxString message = wxString::Format(wxT("Enter the password to unlock the private key for\n"
		wxT("%hs -- %hs\n(This is the password you made up when you\nrequested the certificate.)")),
		call, dxccname);

	wxWindow* top = wxGetApp().GetTopWindow();
	top->SetFocus();
	lastPW = wxGetPasswordFromUser(message, wxT("Enter password"), wxT(""), top);
	if (lastPW.IsEmpty())
		return 1;
	strncpy(buf, lastPW.mb_str(), bufsiz);
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

#define TQSL_PROG_CANBUT TQSL_ID_LOW

DECLARE_EVENT_TYPE(wxEVT_LOGUPLOAD_DONE, -1)
DEFINE_EVENT_TYPE(wxEVT_LOGUPLOAD_DONE)

class UploadDialog : public wxDialog {
public:
	UploadDialog(wxWindow *parent);
	void OnCancel(wxCommandEvent&);
	void OnDone(wxCommandEvent&);
	int doUpdateProgress(double dltotal, double dlnow, double ultotal, double ulnow);
	static int UpdateProgress(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow)
		{ return ((UploadDialog*)clientp)->doUpdateProgress(dltotal, dlnow, ultotal, ulnow); }
private:
	wxButton *canbut;
	wxGauge* progress;
	bool cancelled;
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(UploadDialog, wxDialog)
	EVT_BUTTON(TQSL_PROG_CANBUT, UploadDialog::OnCancel)
	EVT_COMMAND(wxID_ANY, wxEVT_LOGUPLOAD_DONE, UploadDialog::OnDone)
END_EVENT_TABLE()

void
UploadDialog::OnCancel(wxCommandEvent&) {
	cancelled=true;
	canbut->Enable(false);
}

UploadDialog::UploadDialog(wxWindow *parent)
	: wxDialog(parent, -1, wxString(wxT("Uploading Signed Data"))), cancelled(false) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	wxString label = wxString(wxT("Uploading signed log data..."));
	sizer->Add(new wxStaticText(this, -1, label), 0, wxALL|wxALIGN_CENTER, 10);
	
	progress=new wxGauge(this, -1, 100);
	progress->SetValue(0);
	sizer->Add(progress, 0, wxALL|wxEXPAND);

	canbut = new wxButton(this, TQSL_PROG_CANBUT, wxT("Cancel"));
	sizer->Add(canbut, 0, wxALL|wxEXPAND, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

void UploadDialog::OnDone(wxCommandEvent&) {
  EndModal(1);
}

int UploadDialog::doUpdateProgress(double dltotal, double dlnow, double ultotal, double ulnow) {
	if (cancelled) return 1;
	if (ultotal>0.0000001) progress->SetValue((int)(100*(ulnow/ultotal)));
	return 0;
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
		wxT("You may set the starting and/or ending QSO dates\n")
		wxT("in order to select QSOs from the input file.\n\n")
		wxT("QSOs prior to the starting date or after the ending\n")
		wxT("date will not be signed or included in the output file.\n\n")
		wxT("You may leave either date (or both dates) blank.")
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

#define TQSL_DP_OK TQSL_ID_LOW
#define TQSL_DP_CAN TQSL_ID_LOW+1
#define TQSL_DP_ALLOW TQSL_ID_LOW+2

class DupesDialog : public wxDialog {
public:
	DupesDialog(wxWindow *parent = 0, int qso_count=0, int dupes=0, int action=TQSL_ACTION_ASK);
private:
	void OnOk(wxCommandEvent&);	
	void OnCancel(wxCommandEvent&);
	void OnAllow(wxCommandEvent&);
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(DupesDialog, wxDialog)
	EVT_BUTTON(TQSL_DP_OK, DupesDialog::OnOk)
	EVT_BUTTON(TQSL_DP_CAN, DupesDialog::OnCancel)
	EVT_BUTTON(TQSL_DP_ALLOW, DupesDialog::OnAllow)
END_EVENT_TABLE()

DupesDialog::DupesDialog(wxWindow *parent, int qso_count, int dupes, int action) :
		wxDialog(parent, -1, wxString(wxT("Duplicate QSOs Detected")), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	wxString message;

	if (qso_count == dupes) {
		message = wxString::Format(wxT("This log contains %d QSO(s) which appear ")
		wxT("to have already been signed for upload to LoTW, and no new QSOs.\n\n")
		wxT("Click 'Cancel' to abandon processing this log file (Recommended).\n")
		wxT("Click 'Allow Duplicates' to re-process this ")
		wxT("log while allowing duplicate QSOs."),
			qso_count);

	} else {
		int newq = qso_count - dupes;
		message = wxString::Format(wxT("This log contains %d QSO(s) which appear ")
		wxT("to have already been signed for upload to LoTW, and ")
		wxT("%d QSO%hs new.\n\n")
		wxT("Click 'Exclude duplicates' to sign normally, without the duplicate QSOs (Recommended).\n")
		wxT("Click 'Cancel' to abandon processing this log file.\n")
		wxT("Click 'Allow duplicates' to re-process this log ")
		wxT("while allowing duplicate QSOs."),
			dupes, newq, 
			(newq == 1) ? " which is" : "s which are");
	}

	if (action==TQSL_ACTION_UNSPEC) {
		if (qso_count==dupes) {
			message+=
				wxT("\n\nThe log file you are uploading using your QSO Logging system consists entirely of previously uploaded")
				wxT(" QSOs (duplicates) that create unnecessary work for LoTW. There may be a more recent version of your QSO")
				wxT(" Logging system that would prevent this. Please check with your QSO Logging system's vendor for an updated version.\n")
				wxT("In the meantime, please note that some loggers may exhibit strange behavior if an option other than 'Allow duplicates'")
				wxT(" is clicked. Choosing 'Cancel' is usually safe, but a defective logger not checking the status messages reported by TrustedQSL may produce")
				wxT(" strange (but harmless) behavior such as attempting to upload an empty file or marking all chosen QSOs as 'sent'");
		} else {
			message+=
				wxT("\n\nThe log file you are uploading using your QSO Logging system includes some previously uploaded")
				wxT(" QSOs (duplicates) that create unnecessary work for LoTW. There may be a more recent version of your")
				wxT(" QSO Logging system that would prevent this. Please check with your QSO Logging system's vendor for an updated version.\n")
				wxT("In the meantime, please note that some loggers may exhibit strange behavior if an option other than 'Allow duplicates'")
				wxT(" is clicked. 'Exclude duplicates' is recommended, but a logger that does its own duplicate tracking may incorrectly")
				wxT(" set the status in this case. A logger that doesn't track duplicates should be unaffected by choosing 'Exclude duplicates'")
				wxT(" and if it tracks 'QSO sent' status, will correctly mark all selected QSOs as sent - they are in your account even though")
				wxT(" they would not be in this specific batch\n")
				wxT("Choosing 'Cancel' is usually safe, but a defective logger not checking the status messages reported by TrustedQSL may produce")
				wxT(" strange (but harmless) behavior such as attempting to upload an empty file or marking all chosen QSOs as 'sent'");
		}
	}
	sizer->Add(new wxStaticText(this, -1, message), 0, wxALL|wxALIGN_CENTER, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	if (qso_count!=dupes) hsizer->Add(new wxButton(this, TQSL_DP_OK, wxT("Exclude duplicates")), 0, wxRIGHT, 5);
	hsizer->Add(new wxButton(this, TQSL_DP_CAN, wxT("Cancel")), 0, wxLEFT, 10);
	hsizer->Add(new wxButton(this, TQSL_DP_ALLOW, wxT("Allow duplicates")), 0, wxLEFT, 20);
	sizer->Add(hsizer, 0, wxALIGN_CENTER|wxALL, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

void
DupesDialog::OnOk(wxCommandEvent&) {
	EndModal(TQSL_DP_OK);
}

void
DupesDialog::OnCancel(wxCommandEvent&) {
	EndModal(TQSL_DP_CAN);
}

void
DupesDialog::OnAllow(wxCommandEvent&) {
	if (wxMessageBox(wxT("The only reason to re-sign duplicate QSOs is if a previous upload")
		wxT(" went unprocessed by LoTW, either because it was never uploaded, or there was a server failure\n\n")
		wxT("Are you sure you want to proceed? Click 'No' to review the choices"), wxT("Are you sure?"), wxYES_NO|wxICON_EXCLAMATION, this)==wxYES) {
		EndModal(TQSL_DP_ALLOW);
	}
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
		int contest_type = strtol(value.mb_str(), NULL, 10);
		int callsign_field = strtol(value.AfterFirst(';').mb_str(), NULL, 10);
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
get_certlist(string callsign, int dxcc, bool expired) {
	free_certlist();
	tqsl_selectCertificates(&certlist, &ncerts,
		(callsign == "") ? 0 : callsign.c_str(), dxcc, 0, 0, 
		expired ? TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED :
			  TQSL_SELECT_CERT_WITHKEYS);
}

class MyFrame : public wxFrame {
public:
	MyFrame(const wxString& title, int x, int y, int w, int h, bool checkUpdates);

	void AddStationLocation(wxCommandEvent& event);
	void EditStationLocation(wxCommandEvent& event);
	void EnterQSOData(wxCommandEvent& event);
	void EditQSOData(wxCommandEvent& event);
	void ImportQSODataFile(wxCommandEvent& event);
	void UploadQSODataFile(wxCommandEvent& event);
	void OnExit(TQ_WXCLOSEEVENT& event);
	void DoExit(wxCommandEvent& event);
	void OnHelpAbout(wxCommandEvent& event);
	void OnHelpContents(wxCommandEvent& event);
	void OnHelpDiagnose(wxCommandEvent& event);
#ifdef ALLOW_UNCOMPRESSED
	void OnFileCompress(wxCommandEvent& event);
#endif
	void OnPreferences(wxCommandEvent& event);
	void OnSaveConfig(wxCommandEvent& event);
	void OnLoadConfig(wxCommandEvent& event);
	int ConvertLogFile(tQSL_Location loc, wxString& infile, wxString& outfile, bool compress = false, bool suppressdate = false, int action = TQSL_ACTION_ASK, const char *password = NULL);
	tQSL_Location SelectStationLocation(const wxString& title = wxT(""), const wxString& okLabel = wxT("Ok"), bool editonly = false);
	int ConvertLogToString(tQSL_Location loc, wxString& infile, wxString& output, int& n, tQSL_Converter& converter, bool suppressdate=false, int action = TQSL_ACTION_ASK, const char* password=NULL);
	int UploadLogFile(tQSL_Location loc, wxString& infile, bool compress=false, bool suppressdate=false, int action = TQSL_ACTION_ASK, const char* password=NULL);
	void WriteQSOFile(QSORecordList& recs, const char *fname = 0, bool force = false);

	void CheckForUpdates(wxCommandEvent&);
	void DoCheckForUpdates(bool quiet);

	wxTextCtrl *logwin;
	wxHtmlHelpController help;
	FILE *diagFile;

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
	if (wxString(szString).StartsWith(wxT("Debug:")))
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
	EVT_MENU(tm_f_upload, MyFrame::UploadQSODataFile)
#ifdef ALLOW_UNCOMPRESSED
	EVT_MENU(tm_f_compress, MyFrame::OnFileCompress)
	EVT_MENU(tm_f_uncompress, MyFrame::OnFileCompress)
#endif
	EVT_MENU(tm_f_exit, MyFrame::DoExit)
	EVT_MENU(tm_f_preferences, MyFrame::OnPreferences)
	EVT_MENU(tm_f_loadconfig, MyFrame::OnLoadConfig)
	EVT_MENU(tm_f_saveconfig, MyFrame::OnSaveConfig)
	EVT_MENU(tm_h_contents, MyFrame::OnHelpContents)
	EVT_MENU(tm_h_about, MyFrame::OnHelpAbout)
	EVT_MENU(tm_h_diag, MyFrame::OnHelpDiagnose)

	EVT_MENU(tm_h_update, MyFrame::CheckForUpdates)

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

MyFrame::MyFrame(const wxString& title, int x, int y, int w, int h, bool checkUpdates)
	: wxFrame(0, -1, title, wxPoint(x, y), wxSize(w, h)) {

	DocPaths docpaths(wxT("tqslapp"));
	// File menu
	wxMenu *file_menu = new wxMenu;
	file_menu->Append(tm_f_upload, wxT("Sign and &upload ADIF or Cabrillo File..."));
	file_menu->Append(tm_f_import_compress, wxT("&Sign and save ADIF or Cabrillo file..."));
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
	file_menu->Append(tm_f_saveconfig, wxT("&Backup TQSL Configuration..."));
	file_menu->Append(tm_f_loadconfig, wxT("&Restore TQSL Configuration..."));
	file_menu->AppendSeparator();
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

	help_menu->Append(tm_h_update, wxT("Check for &Updates..."));

	help_menu->Append(tm_h_about, wxT("&About"));
	help_menu->AppendSeparator();
	help_menu->Append(tm_h_diag, wxT("Dia&gnostic Mode"));

	// Main menu
	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, wxT("&File"));
	menu_bar->Append(stn_menu, wxT("&Station"));
	menu_bar->Append(help_menu, wxT("&Help"));

	SetMenuBar(menu_bar);

	logwin = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxSize(400, 200),
		wxTE_MULTILINE|wxTE_READONLY);

	//app icon
	SetIcon(wxIcon(key_xpm));

	
	LogList *log = new LogList(this);
	wxLog::SetActiveTarget(log);

	//check for updates

	if (checkUpdates) {
		wxConfig *config = (wxConfig *)wxConfig::Get();
		if (config->Read(wxT("AutoUpdateCheck"), true)) {
			DoCheckForUpdates(true); //TODO: in a thread?
		}
	}
}

static wxString
run_station_wizard(wxWindow *parent, tQSL_Location loc, wxHtmlHelpController *help = 0,
	bool expired = false, wxString title = wxT("Add Station Location"), wxString dataname = wxT("")) {
	wxString rval(wxT(""));
	get_certlist("", 0, expired);
	if (ncerts == 0)
		throw TQSLException("No certificates available");
	TQSLWizard *wiz = new TQSLWizard(loc, parent, help, title, expired);
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

// Return the "About" string
//
static wxString getAbout() {
	wxString msg = wxT("TQSL V") wxT(VERSION) wxT(" build ") wxT(BUILD) wxT("\n(c) 2001-2013\nAmerican Radio Relay League\n\n");
	int major, minor;
	if (tqsl_getVersion(&major, &minor))
		wxLogError(wxT("%hs"), tqsl_getErrorString());
	else
		msg += wxString::Format(wxT("TrustedQSL library V%d.%d\n"), major, minor);
	if (tqsl_getConfigVersion(&major, &minor))
		wxLogError(wxT("%hs"), tqsl_getErrorString());
	else
		msg += wxString::Format(wxT("Configuration data V%d.%d\n"), major, minor);
	msg += wxVERSION_STRING;
#ifdef wxUSE_UNICODE
	if (wxUSE_UNICODE)
		msg += wxT(" (Unicode)");
#endif
	return msg;
}

void
MyFrame::OnHelpAbout(wxCommandEvent& WXUNUSED(event)) {
	wxMessageBox(getAbout(), wxT("About"), wxOK|wxCENTRE|wxICON_INFORMATION, this);
}

void
MyFrame::OnHelpDiagnose(wxCommandEvent& WXUNUSED(event)) {
	wxString s_fname;

	if (diagFile) {
		fclose(diagFile);
		diagFile = NULL;
		wxMessageBox(wxT("Diagnostic log closed"), wxT("Diagnostics"), wxOK|wxCENTRE|wxICON_INFORMATION, this);
		return;
	}
	s_fname = wxFileSelector(wxT("Log File"), wxT(""), wxT("tqsldiag.log"), wxT("log"),
			wxT("Log files (*.log)|*.log|All files (*.*)|*.*"),
			wxSAVE|wxOVERWRITE_PROMPT, this);
	if (s_fname == wxT(""))
		return;
	diagFile = fopen(s_fname.mb_str(), "wb");
	if (!diagFile) {
		wxString errmsg = wxString::Format(wxT("Error opening diagnostic log %s: %hs"), s_fname.c_str(), strerror(errno));
		wxMessageBox(errmsg, wxT("Log File Error"), wxOK|wxICON_EXCLAMATION);
		return;
	}
	wxString about = getAbout();
	fprintf(diagFile, "TQSL Diagnostics\n%s\n\n", (const char *)about.mb_str());
}

static void
AddEditStationLocation(tQSL_Location loc, bool expired = false, const wxString& title = wxT("Add Station Location")) {
	try {
		MyFrame *frame = (MyFrame *)wxGetApp().GetTopWindow();
		run_station_wizard(frame, loc, &(frame->help), expired, title);
	}
	catch (TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
}

void
MyFrame::AddStationLocation(wxCommandEvent& WXUNUSED(event)) {
	tQSL_Location loc;
	if (tqsl_initStationLocationCapture(&loc)) {
		wxLogError(wxT("%hs"), tqsl_getErrorString());
	}
	AddEditStationLocation(loc, false);
	if (tqsl_endStationLocationCapture(&loc)) {
		wxLogError(wxT("%hs"), tqsl_getErrorString());
	}
}

void
MyFrame::EditStationLocation(wxCommandEvent& WXUNUSED(event)) {
	try {
		SelectStationLocation(wxT("Edit Station Locations"), wxT("Close"), true);
	}
	catch (TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
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
	if (tqsl_beginADIF(&adif, file.mb_str())) {
		wxLogError(wxT("%hs"), tqsl_getErrorString());
	}
	QSORecord rec;
	do {
		if (tqsl_getADIFField(adif, &field, &stat, fielddefs, defined_types, adif_alloc)) {
			wxLogError(wxT("%hs"), tqsl_getErrorString());
		}
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
					rec._date.day = strtol(cp+6, NULL, 10);
					*(cp+6) = '\0';
					rec._date.month = strtol(cp+4, NULL, 10);
					*(cp+4) = '\0';
					rec._date.year = strtol(cp, NULL, 10);
				}
			} else if (!strcasecmp(field.name, "TIME_ON")) {
				char *cp = (char *)field.data;
				if (strlen(cp) >= 4) {
					rec._time.second = (strlen(cp) > 4) ? strtol(cp+4, NULL, 10) : 0;
					*(cp+4) = 0;
					rec._time.minute = strtol(cp+2, NULL, 10);
					*(cp+2) = '\0';
					rec._time.hour = strtol(cp, NULL, 10);
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
		wxLogError(wxT("%hs"), x.what());
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
		wxLogError(wxT("%hs"), x.what());
	}
}

int MyFrame::ConvertLogToString(tQSL_Location loc, wxString& infile, wxString& output, int& n, tQSL_Converter& conv, bool suppressdate, int action, const char* password) {
	static const char *iam = "TQSL V" VERSION;
	const char *cp;
	char callsign[40];
	int dxcc;
	wxString name, ext;
	bool allow_dupes = false;
	bool restarting = false;
	const char *dxccname = "Unknown";

	wxConfig *config = (wxConfig *)wxConfig::Get();

	check_tqsl_error(tqsl_getLocationCallSign(loc, callsign, sizeof callsign));
	check_tqsl_error(tqsl_getLocationDXCCEntity(loc, &dxcc));

	tqsl_getDXCCEntityName(dxcc, &dxccname);

	get_certlist(callsign, dxcc, false);
	if (ncerts == 0) {
		wxString msg = wxString::Format(wxT("There are no valid certificates for callsign %hs.\nSigning aborted.\n"), callsign);
		throw TQSLException(msg.mb_str());
		return TQSL_EXIT_TQSL_ERROR;
	}

	wxLogMessage(wxT("Signing using Callsign %hs, DXCC Entity %hs"), callsign, dxccname);

	init_modes();
	init_contests();

	DateRangeDialog dial(this);

restart:

	ConvertingDialog *conv_dial = new ConvertingDialog(this, infile.mb_str());
	n=0;
	bool cancelled = false;
	int lineno = 0;
	int out_of_range = 0;
	int duplicates = 0;
	int processed = 0;
	int errors = 0;
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
			if (!restarting) {
				if (dial.ShowModal() != wxOK) {
					wxLogMessage(wxT("Cancelled"));
					return TQSL_EXIT_CANCEL;
				}
			}
			tqsl_setADIFConverterDateFilter(conv, &dial.start, &dial.end);
		}
		bool allow = false;
		config->Read(wxT("BadCalls"), &allow);
		tqsl_setConverterAllowBadCall(conv, allow);
		tqsl_setConverterAllowDuplicates(conv, allow_dupes);
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
		wxString ident = wxString::Format(wxT("%hs Lib: V%d.%d Config: V%d.%d AllowDupes: %hs"), iam,
			major, minor, config_major, config_minor,
			allow_dupes ? "true" : "false");
		wxString gabbi_ident = wxString::Format(wxT("<TQSL_IDENT:%d>%s"), (int)ident.length(), ident.c_str());
		gabbi_ident += wxT("\n");

		output = gabbi_ident;

   		do {
   	   		while ((cp = tqsl_getConverterGABBI(conv)) != 0) {
  					wxSafeYield(conv_dial);
  					if (!conv_dial->running)
  						break;
					// Only count QSO records
					if (strstr(cp, "tCONTACT")) {
						++n;
						++processed;
					}
					if ((n % 10) == 0)
		   	   			conv_dial->msg->SetLabel(wxString::Format(wxT("%d"), n));

					output<<(wxString(cp, wxConvLocal)+wxT("\n"));
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
				processed++;
				out_of_range++;
				continue;
			}
			if (tQSL_Error == TQSL_DUPLICATE_QSO) {
				processed++;
				duplicates++;
				continue;
			}
			bool has_error = (tQSL_Error != TQSL_NO_ERROR);
			if (has_error) {
				errors++;
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

					if (!this) { // No GUI
						switch (action) {
						 	case TQSL_ACTION_ABORT:
								cancelled = true;
								ignore_err = true;
								goto abortSigning;
							case TQSL_ACTION_NEW:		// For ALL or NEW, let the signing proceed
							case TQSL_ACTION_ALL:
								ignore_err = true;
								break;
							case TQSL_ACTION_ASK:
							case TQSL_ACTION_UNSPEC:
								break;			// The error will show as a popup
						}
					}
					wxLogError(wxT("%s"), msg.c_str());
					if (!ignore_err) {
						if (wxMessageBox(wxString(wxT("Error: ")) + msg + wxT("\n\nIgnore errors?"), wxT("Error"), wxYES_NO, this) == wxNO) {
							cancelled = true;
							goto abortSigning;
						}
						ignore_err = true;
					}
				}
			}
			tqsl_getErrorString();	// Clear error			
			if (has_error && ignore_err)
				continue;
   			break;
		} while (1);
   		cancelled = !conv_dial->running;

abortSigning:
   		if (this)
			this->Enable(TRUE);

   		if (cancelled) {
   			wxLogWarning(wxT("Signing cancelled"));
   		} else if (tQSL_Error != TQSL_NO_ERROR) {
   			check_tqsl_error(1);
		}
   		delete conv_dial;
   	} catch (TQSLException& x) {

		if (this)
	   		this->Enable(TRUE);
   		delete conv_dial;
		string msg = x.what();
		tqsl_getConverterLine(conv, &lineno);
		tqsl_converterRollBack(conv);
   		tqsl_endConverter(&conv);
		if (lineno)
			msg += wxString::Format(wxT(" on line %d"), lineno).mb_str();

		wxLogError(wxT("Signing aborted due to errors"));
   		throw TQSLException(msg.c_str());
   	}
	if (out_of_range > 0)
		wxLogMessage(wxT("%s: %d QSO records were outside the selected date range"),
			infile.c_str(), out_of_range);
	if (duplicates > 0) {
		if (cancelled) {
			tqsl_converterRollBack(conv);
			tqsl_endConverter(&conv);
			return TQSL_EXIT_CANCEL;
		}
		if (this || action == TQSL_ACTION_ASK || action==TQSL_ACTION_UNSPEC) { //if GUI or want to ask the user
			DupesDialog dial(this, processed, duplicates, action);
			int choice = dial.ShowModal();
			if (choice == TQSL_DP_CAN) {
				wxLogMessage(wxT("Cancelled"));
				tqsl_converterRollBack(conv);
				tqsl_endConverter(&conv);
				return TQSL_EXIT_CANCEL;
			}
			if (choice == TQSL_DP_ALLOW) {
				allow_dupes = true;
				tqsl_converterRollBack(conv);
				tqsl_endConverter(&conv);
				restarting = true;
				goto restart;
			}
		} else {
			if (action == TQSL_ACTION_ABORT) {
				if (processed==duplicates) {
					wxLogMessage(wxT("All QSOs are duplicates; aborted"));
					tqsl_converterRollBack(conv);
					tqsl_endConverter(&conv);
					n = 0;
					return TQSL_EXIT_NO_QSOS;
				} else {
					wxLogMessage(wxT("%d of %d QSOs are duplicates; aborted"), duplicates, processed);
					tqsl_converterRollBack(conv);
					tqsl_endConverter(&conv);
					n = 0;
					return TQSL_EXIT_NO_QSOS;
				}
			} else if (action == TQSL_ACTION_ALL) {
				allow_dupes = true;
				tqsl_converterRollBack(conv);
				tqsl_endConverter(&conv);
				restarting = true;
				goto restart;
			}
			// Otherwise it must be TQSL_ACTION_NEW, so fall through
			// and output the new records.
		}
		wxLogMessage(wxT("%s: %d QSO records were duplicates"),
			infile.c_str(), duplicates);
	}
	//if (!cancelled) tqsl_converterCommit(conv);
	if (cancelled || processed == 0) { 
		tqsl_converterRollBack(conv);
		tqsl_endConverter(&conv);
	}
	if (cancelled) return TQSL_EXIT_CANCEL;
	if (duplicates > 0 || out_of_range > 0 || errors > 0) return TQSL_EXIT_QSOS_SUPPRESSED;
	return TQSL_EXIT_SUCCESS;
}

int
MyFrame::ConvertLogFile(tQSL_Location loc, wxString& infile, wxString& outfile,
	bool compressed, bool suppressdate, int action, const char *password) {
	
	gzFile gout = 0;
	ofstream out;

	if (compressed)
		gout = gzopen(outfile.mb_str(), "wb9");
	else
		out.open(outfile.mb_str(), ios::out|ios::trunc|ios::binary);

	if ((compressed && !gout) || (!compressed && !out)) {
		wxLogError(wxT("Unable to open %s for output"), outfile.c_str());
		return TQSL_EXIT_ERR_OPEN_OUTPUT;
	}

	wxString output;
	int numrecs=0;
	tQSL_Converter conv=0;
	int status = this->ConvertLogToString(loc, infile, output, numrecs, conv, suppressdate, action, password);

	if (status == TQSL_EXIT_CANCEL || numrecs==0) {
		wxLogMessage(wxT("No records output"));
		if (numrecs == 0) status = TQSL_EXIT_NO_QSOS;
	} else {
		if(compressed) { 
			if (0>=gzwrite(gout, output.mb_str(), output.size()) || Z_OK!=gzclose(gout)) {
				tqsl_converterRollBack(conv);
				tqsl_endConverter(&conv);
				return TQSL_EXIT_LIB_ERROR;
			}

		} else {
			out<<output;
			out.close();
			if (out.fail()) {
				tqsl_converterRollBack(conv);
				tqsl_endConverter(&conv);
				return TQSL_EXIT_LIB_ERROR;
			}
			
			
		}
		
		tqsl_converterCommit(conv);
		tqsl_endConverter(&conv);

		wxLogMessage(wxT("%s: wrote %d records to %s"), infile.c_str(), numrecs,
			outfile.c_str());
		wxLogMessage(wxT("%s is ready to be emailed or uploaded.\nNote: TQSL assumes that this file will be uploaded to LoTW.\nResubmitting these QSOs will cause them to be reported as duplicates."), outfile.c_str());
	}

	return status;
	
}

class FileUploadHandler {
public:
	string s;
	FileUploadHandler(): s() { s.reserve(2000); }

	size_t internal_recv( char *ptr, size_t size, size_t nmemb) {
		s.append(ptr, size*nmemb);
		return size*nmemb;
	}

	static size_t recv( char *ptr, size_t size, size_t nmemb, void *userdata) { 
		return ((FileUploadHandler*)userdata)->internal_recv(ptr, size, nmemb);
	}
};

long compressToBuf(string& buf, const char* input) {
	const size_t TBUFSIZ=128*1024;
	uint8_t* tbuf=new uint8_t[TBUFSIZ];

	//vector<uint8_t> buf;
	z_stream stream;
	stream.zalloc=0;
	stream.zfree=0;
	stream.next_in=(Bytef*)input;
	stream.avail_in=strlen(input);
	stream.next_out=tbuf;
	stream.avail_out=TBUFSIZ;
	
	//deflateInit(&stream, Z_BEST_COMPRESSION);
	deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 16+15, 9, Z_DEFAULT_STRATEGY); //use gzip header

	while (stream.avail_in) { //while still some left
		int res=deflate(&stream, Z_NO_FLUSH);
		assert(res==Z_OK);
		if (!stream.avail_out) {
			buf.insert(buf.end(), tbuf, tbuf+TBUFSIZ);
			stream.next_out=tbuf;
			stream.avail_out=TBUFSIZ;
		}
	}

	do {
		if (stream.avail_out==0) {
			buf.insert(buf.end(), tbuf, tbuf+TBUFSIZ);
			stream.next_out=tbuf;
			stream.avail_out=TBUFSIZ;
		}
	} while (deflate(&stream, Z_FINISH)==Z_OK);

	buf.insert(buf.end(), tbuf, tbuf+TBUFSIZ-stream.avail_out);
	deflateEnd(&stream);

	delete tbuf;

	return buf.length();
}


class UploadThread: public wxThread
{
public:
  UploadThread(CURL* handle_, wxDialog* dial_): wxThread(wxTHREAD_JOINABLE),
						handle(handle_), dial(dial_)

	{
		wxThread::Create();
	}
protected:
	CURL* handle;
	wxDialog* dial;
	virtual wxThread::ExitCode Entry()
	{
		int ret=curl_easy_perform(handle);
		wxCommandEvent evt(wxEVT_LOGUPLOAD_DONE, wxID_ANY);
		dial->AddPendingEvent(evt);
		return (wxThread::ExitCode)((intptr_t)ret);
	}
};

int MyFrame::UploadLogFile(tQSL_Location loc, wxString& infile, bool compressed, bool suppressdate, int action, const char* password) {
	int numrecs=0;
	wxString signedOutput;
	tQSL_Converter conv=0;

	int status = this->ConvertLogToString(loc, infile, signedOutput, numrecs, conv, suppressdate, action, password);

	if (status == TQSL_EXIT_CANCEL || numrecs==0) {
		wxLogMessage(wxT("No records to upload"));
		if (numrecs == 0) status = TQSL_EXIT_NO_QSOS;
		return status;
	}
	else {
		//upload the file

		//get the url from the config, can be overridden by an installer
		//defaults are valid for LoTW as of 1/31/2013

		wxConfig *config = (wxConfig *)wxConfig::Get();
		config->SetPath(wxT("/LogUpload"));
		wxString uploadURL=config->Read(wxT("UploadURL"), DEFAULT_UPL_URL);
		wxString uploadField=config->Read(wxT("PostField"), DEFAULT_UPL_FIELD);
		wxString uplStatus=config->Read(wxT("StatusRegex"), DEFAULT_UPL_STATUSRE);
		wxString uplStatusSuccess=config->Read(wxT("StatusSuccess"), DEFAULT_UPL_STATUSOK).Lower();
		wxString uplMessage=config->Read(wxT("MessageRegex"), DEFAULT_UPL_MESSAGERE);
		bool uplVerifyCA=config->Read(wxT("VerifyCA"), DEFAULT_UPL_VERIFYCA);
		config->SetPath(wxT("/"));

		// Copy the strings so they remain around
		char *urlstr = strdup(uploadURL.mb_str());
		char *cpUF = strdup(uploadField.mb_str());

		//compress the upload
		string compressed;
		long compressedSize=compressToBuf(compressed, (const char*)signedOutput.mb_str());
		//ofstream f; f.open("testzip.tq8", ios::binary); f<<compressed; f.close(); //test of compression routine
		if (compressedSize<0) { 
			wxLogMessage(wxT("Error compressing before upload")); 
			free(urlstr);
			free(cpUF);
			return TQSL_EXIT_TQSL_ERROR;
		}

retry_upload:

		CURL* req=curl_easy_init();

		if (!req) {
			wxLogMessage(wxT("Error: Could not upload file (CURL Init error)"));
			free(urlstr);
			free(cpUF);
			return TQSL_EXIT_TQSL_ERROR; 
		}


		//debug
		if (diagFile) {
			curl_easy_setopt(req, CURLOPT_VERBOSE, 1);
			curl_easy_setopt(req, CURLOPT_STDERR, diagFile);
			fprintf(diagFile, "Upload Log:\n");
		}


		//set up options
		curl_easy_setopt(req, CURLOPT_URL, urlstr);

		if(!uplVerifyCA) curl_easy_setopt(req, CURLOPT_SSL_VERIFYPEER, 0);

		
		//follow redirects
		curl_easy_setopt(req, CURLOPT_FOLLOWLOCATION, 1);

		//the following allow us to write our log and read the result

		FileUploadHandler handler;

		curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
		curl_easy_setopt(req, CURLOPT_WRITEDATA, &handler);

		char errorbuf[CURL_ERROR_SIZE];
		curl_easy_setopt(req, CURLOPT_ERRORBUFFER, errorbuf);

		//set up the file

		wxDateTime now=wxDateTime::Now().ToUTC();

		wxString name, ext;
		wxFileName::SplitPath(infile, 0, &name, &ext);
		if (!ext.IsEmpty()) name+=wxT(".")+ext;



		//unicode mess. can't just use mb_str directly because it's a temp ptr
		// and the curl form expects it to still be there during perform() so 
		// we have to do all this copying around to please the unicode gods

		char filename[1024];
		strncpy(filename, wxString::Format(wxT("<TQSLUpl %s-%s> %s"),
			now.Format(wxT("%Y%m%d")).c_str(),
			now.Format(wxT("%H%M")).c_str(),
			name.c_str()).mb_str(), 1023);

		struct curl_httppost* post=NULL, *lastitem=NULL;

		curl_formadd(&post, &lastitem,
			CURLFORM_PTRNAME, cpUF,
			CURLFORM_BUFFER, filename,
			CURLFORM_BUFFERPTR, compressed.c_str(),
			CURLFORM_BUFFERLENGTH, compressedSize,
			CURLFORM_END);


		curl_easy_setopt(req, CURLOPT_HTTPPOST, post);

		
		intptr_t retval;

		UploadDialog* upload;

		wxLogMessage(wxT("Attempting to upload %d QSO%hs"), numrecs, numrecs == 1 ? "" : "s");

		if(this) {
			upload=new UploadDialog(this);

			curl_easy_setopt(req, CURLOPT_PROGRESSFUNCTION, &UploadDialog::UpdateProgress);
			curl_easy_setopt(req, CURLOPT_PROGRESSDATA, upload);
			curl_easy_setopt(req, CURLOPT_NOPROGRESS, 0);

			UploadThread thread(req, upload);
			if (thread.Run() != wxTHREAD_NO_ERROR) {
				wxLogError(wxT("Could not spawn upload thread!"));
			        upload->Destroy();
				free(urlstr);
				free(cpUF);
				return TQSL_EXIT_TQSL_ERROR;
			}

			upload->ShowModal();
			retval=((intptr_t)thread.Wait());
		} else { retval=curl_easy_perform(req); }

		if (retval==0) { //success

			//check the result

			wxString uplresult=wxString::FromAscii(handler.s.c_str());

			wxRegEx uplStatusRE(uplStatus);
			wxRegEx uplMessageRE(uplMessage);

			if (uplStatusRE.Matches(uplresult)) //we can make sense of the error
			{
				//sometimes has leading/trailing spaces
				if (uplStatusRE.GetMatch(uplresult, 1).Lower().Strip(wxString::both)==uplStatusSuccess) //success
				{
					if (uplMessageRE.Matches(uplresult)) { //and a message
						wxLogMessage(wxT("%s: Log uploaded successfully with result \"%s\"!\nAfter reading this message, you may close this program."), 
							infile.c_str(), uplMessageRE.GetMatch(uplresult, 1).c_str());

					} else { // no message we could find
						wxLogMessage(wxT("%s: Log uploaded successfully!\nAfter reading this message, you may close this program."), infile.c_str());
					}

					retval=TQSL_EXIT_SUCCESS;
				} else { // failure, but site is working

					if (uplMessageRE.Matches(uplresult)) { //and a message
						wxLogMessage(wxT("%s: Log upload was rejected with result \"%s\""), 
							infile.c_str(), uplMessageRE.GetMatch(uplresult, 1).c_str());

					} else { // no message we could find
						wxLogMessage(wxT("%s: Log upload was rejected!"), infile.c_str());
					}

					retval=TQSL_EXIT_REJECTED;
				}
			} else { //site isn't working
				wxLogMessage(wxT("%s: Got an unexpected response on log upload! Maybe the site is down?"), infile.c_str());
				retval=TQSL_EXIT_UNEXP_RESP;
			}

		} else if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			wxLogMessage(wxT("%s: Unable to upload - either your Internet connection is down or LoTW is unreachable.\nPlease try uploading these QSOs later."), infile.c_str());
			retval=TQSL_EXIT_CONNECTION_FAILED;
		} else if (retval==CURLE_ABORTED_BY_CALLBACK) { //cancelled.
			wxLogMessage(wxT("%s: Upload cancelled"), infile.c_str());
			retval=TQSL_EXIT_CANCEL;
		} else { //error
			//don't know why the conversion from char* -> wxString -> char* is necessary but it 
			// was turned into garbage otherwise
			wxLogMessage(wxT("%s: Couldn't upload the file: CURL returned \"%hs\" (%hs)"), infile.c_str(), curl_easy_strerror((CURLcode)retval), errorbuf);
			retval=TQSL_EXIT_TQSL_ERROR;
		}
		if (this) upload->Destroy();

		curl_formfree(post);
		curl_easy_cleanup(req);

		// If there's a GUI and we didn't successfully upload and weren't cancelled,
		// ask the user if we should retry the upload.
		if (this && retval != TQSL_EXIT_CANCEL && retval != TQSL_EXIT_SUCCESS) {
			if (wxMessageBox(wxT("Your upload appears to have failed. Should TQSL try again?"), wxT("Retry?"), wxYES_NO, this) == wxYES)
				goto retry_upload;
		}

		if (retval==0)
			tqsl_converterCommit(conv);
		else
			tqsl_converterRollBack(conv);

		tqsl_endConverter(&conv);

		if (urlstr) free(urlstr);
		if (cpUF) free (cpUF);
		return retval;
	}


}

// Verify that a certificate exists for this station location
// before allowing the location to be edited
static bool verify_cert(tQSL_Location loc) {
	char call[128];
	tQSL_Cert *certlist;
	int ncerts;
	// Get the callsign from the location
	check_tqsl_error(tqsl_getLocationCallSign(loc, call, sizeof(call)));
	// See if there is a certificate for that call
	tqsl_selectCertificates(&certlist, &ncerts, call, 0, 0, 0, TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED);	
	if (ncerts == 0) {
		wxMessageBox(wxString::Format(wxT("There are no certificates for callsign %hs. This location cannot be edited."), call), wxT("No Certificate"), wxOK|wxICON_EXCLAMATION);
		return false;
	}
	for (int i = 0; i  < ncerts; i++)
		tqsl_freeCertificate(certlist[i]);
	return true;
}

tQSL_Location
MyFrame::SelectStationLocation(const wxString& title, const wxString& okLabel, bool editonly) {
   	int rval;
   	tQSL_Location loc;
   	wxString selname;
	char errbuf[512];
   	do {
   		TQSLGetStationNameDialog station_dial(this, &help, wxDefaultPosition, false, title, okLabel, editonly);
   		if (selname != wxT(""))
   			station_dial.SelectName(selname);
   		rval = station_dial.ShowModal();
   		switch (rval) {
   			case wxID_CANCEL:	// User hit Close
   				return 0;
   			case wxID_APPLY:	// User hit New
   				check_tqsl_error(tqsl_initStationLocationCapture(&loc));
   				selname = run_station_wizard(this, loc, &help, false);
   				check_tqsl_error(tqsl_endStationLocationCapture(&loc));
   				break;
   			case wxID_MORE:		// User hit Edit
   		   		check_tqsl_error(tqsl_getStationLocation(&loc, station_dial.Selected().mb_str()));
				if (verify_cert(loc)) {		// Check if there is a certificate before editing
					check_tqsl_error(tqsl_getStationLocationErrors(loc, errbuf, sizeof(errbuf)));
					if (strlen(errbuf) > 0) {
						wxMessageBox(wxString::Format(wxT("%hs\nThe invalid data was ignored."), errbuf), wxT("Location data error"), wxOK|wxICON_EXCLAMATION, this);
					}
					char loccall[512];
					check_tqsl_error(tqsl_getLocationCallSign(loc, loccall, sizeof loccall));
					selname = run_station_wizard(this, loc, &help, true, wxString::Format(wxT("Edit Station Location : %hs - %s"), loccall, station_dial.Selected().c_str()), station_dial.Selected());
   					check_tqsl_error(tqsl_endStationLocationCapture(&loc));
				}
   				break;
   			case wxID_OK:		// User hit OK
   		   		check_tqsl_error(tqsl_getStationLocation(&loc, station_dial.Selected().mb_str()));
				check_tqsl_error(tqsl_getStationLocationErrors(loc, errbuf, sizeof(errbuf)));
				if (strlen(errbuf) > 0) {
					wxMessageBox(wxString::Format(wxT("%hs\nThis should be corrected before signing a log file."), errbuf), wxT("Location data error"), wxOK|wxICON_EXCLAMATION, this);
				}
   				break;
   		}
   	} while (rval != wxID_OK);
	return loc;
}

void MyFrame::CheckForUpdates(wxCommandEvent&) {
	DoCheckForUpdates(false);
}

wxString GetUpdatePlatformString() {
	wxString ret;
#if defined(_WIN32)
	#if defined(_WIN64)
		//this is 64 bit code already; if we are running we support 64
		ret=wxT("win64 win32");

	#else // this is not 64 bit code, but we are on windows
		// are we 64-bit compatible? if so prefer it
		BOOL val=false;
		typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
		LPFN_ISWOW64PROCESS fnIsWow64Process = 
			(LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")),"IsWow64Process");
		if (fnIsWow64Process!=NULL) {
			fnIsWow64Process(GetCurrentProcess(), &val);
		}
		if(val) //32 bit running on 64 bit
			ret=wxT("win64 win32");
		else //just 32-bit only
			ret=wxT("win32");
	#endif

#elif defined(__APPLE__) && defined(__MACH__) //osx has universal binaries
	ret=wxT("osx");

#elif defined(__gnu_linux__)
	#if defined(__amd64__)
		ret=wxT("linux64 linux32 source");
	#elif defined(__i386__)
		ret=wxT("linux32 source");
	#else
		ret=wxT("source"); //source distribution is kosher on linux
	#endif
#else
	ret=wxT(""); //give them a homepage
#endif
	return ret;
}

class UpdateDialogMsgBox: public wxDialog
 {
public:
	UpdateDialogMsgBox(wxWindow* parent, wxString newVersion, wxString platformURL, wxString homepage) :
			wxDialog(parent, (wxWindowID)wxID_ANY, wxT("Update Available"), wxDefaultPosition, wxDefaultSize)
	{
		wxSizer* overall=new wxBoxSizer(wxVERTICAL);
		wxSizer* buttons=CreateButtonSizer(wxOK);
		overall->Add(new wxStaticText(this, wxID_ANY, wxString::Format(wxT("A new release (%s) is available!"), newVersion.c_str())), 0, wxALIGN_CENTER_HORIZONTAL);
		
		if (!platformURL.IsEmpty()) {
			wxSizer* thisline=new wxBoxSizer(wxHORIZONTAL);
			thisline->Add(new wxStaticText(this, wxID_ANY, wxT("Download from:")));
			thisline->Add(new wxHyperlinkCtrl(this, wxID_ANY, platformURL, platformURL));

			overall->AddSpacer(10);
			overall->Add(thisline);
		}

		if (!homepage.IsEmpty()) {
			wxSizer* thisline=new wxBoxSizer(wxHORIZONTAL);
			thisline->Add(new wxStaticText(this, wxID_ANY, wxT("More details at:")));
			thisline->Add(new wxHyperlinkCtrl(this, wxID_ANY, homepage, homepage));

			overall->AddSpacer(10);
			overall->Add(thisline);
		}

		if (buttons) { //should always be here but documentation says to check
			overall->AddSpacer(10);
			overall->Add(buttons, 0, wxALIGN_CENTER_HORIZONTAL);
		}

		wxSizer* padding=new wxBoxSizer(wxVERTICAL);
		padding->Add(overall, 0, wxALL, 10);
		SetSizer(padding);
		Fit();
	}
private:

};


void MyFrame::DoCheckForUpdates(bool silent) {
	wxConfig* config=(wxConfig*)wxConfig::Get();
	CURL* req=curl_easy_init();

	wxString lastUpdateTime=config->Read(wxT("UpdateCheckTime"));
	int numdays=config->Read(wxT("UpdateCheckInterval"), 1); // in days

	bool check=true;
	if (!lastUpdateTime.IsEmpty()) {
		wxDateTime lastcheck; lastcheck.ParseFormat(lastUpdateTime, wxT("%Y-%m-%d"));
		lastcheck+=wxDateSpan::Days(numdays); // x days from when we checked
		if (lastcheck>wxDateTime::Today()) //if we checked less than x days ago
			check=false;  // don't check again
	} // else no stored value, means check 

	if (!silent) check=true; //unless the user explicitly asked

	if (!check) return; //if we really weren't supposed to check, get out of here

	wxString updateURL=config->Read(wxT("UpdateURL"), DEFAULT_UPD_URL);

	curl_easy_setopt(req, CURLOPT_URL, (const char*)updateURL.mb_str());

	if (diagFile) {
		curl_easy_setopt(req, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(req, CURLOPT_STDERR, diagFile);
		fprintf(diagFile, "Upload Log:\n");
	}


	//follow redirects
	curl_easy_setopt(req, CURLOPT_FOLLOWLOCATION, 1);

	//the following allow us to analyze our file

	FileUploadHandler handler;

	curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
	curl_easy_setopt(req, CURLOPT_WRITEDATA, &handler);

	if(silent) { // if there's a problem, we don't want the program to hang while we're starting it
		curl_easy_setopt(req, CURLOPT_CONNECTTIMEOUT, 3);
		curl_easy_setopt(req, CURLOPT_TIMEOUT, 3); 
	}

	curl_easy_setopt(req, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	curl_easy_setopt(req, CURLOPT_ERRORBUFFER, errorbuf);

	if (curl_easy_perform(req) == CURLE_OK) {
		wxString result=wxString::FromAscii(handler.s.c_str());
		wxString url;
		
		wxStringTokenizer urls(result, wxT("\n"));
		wxString header=urls.GetNextToken();
		wxString onlinever;
		if (header.StartsWith(wxT("TQSLVERSION;"), &onlinever)) {
			if (onlinever.Cmp(wxT(VERSION))>0) {
				//online string is lexicographically before ours... this will suffice for a few versions
				// eventually we will have to compare major, then minor... but this will work for a while
				WX_DECLARE_STRING_HASH_MAP(wxString, URLHashMap);
				URLHashMap map;

				wxString ourPlatURL; //empty by default (we check against this later)

				while (urls.HasMoreTokens()) {
					wxString tok=urls.GetNextToken().Trim();
					if (tok.IsEmpty()) continue; //blank line
					if (tok[0]=='#') continue; //comments

					int sep=tok.Find(';'); //; is invalid in URLs
					if (sep==wxNOT_FOUND) continue; //malformed string
					wxString plat=tok.Left(sep);
					wxString url=tok.Right(tok.size()-sep-1);
					map[plat]=url;
				}
			
				wxStringTokenizer plats(GetUpdatePlatformString(), wxT(" "));
				while(plats.HasMoreTokens()) {
					wxString tok=plats.GetNextToken();
					//see if this token is here
					if (map.count(tok)) { ourPlatURL=map[tok]; break; }

				}
				//will create ("homepage"->"") if none there, which is what we'd be checking for anyway
				UpdateDialogMsgBox msg(this, onlinever, ourPlatURL, map[wxT("homepage")]);

				msg.ShowModal();

			} else {
				if (!silent)
					wxMessageBox(wxString::Format(wxT("Version %hs is the newest available"), VERSION), wxT("No Updates"), wxOK|wxICON_INFORMATION, this);
			}
		} else {
			if(!silent)
			wxMessageBox(wxT("Malformed update file - not your fault"), wxT("Update"), wxOK|wxICON_EXCLAMATION, this);
		}
		
	} else {
		if(!silent)
			wxMessageBox(wxString::Format(wxT("Error checking for updates:\n%hs"), errorbuf), wxT("Update"), wxOK|wxICON_EXCLAMATION, this);
	}

	// we checked today, and whatever the result, no need to (automatically) check again until the next interval

	config->Write(wxT("UpdateCheckTime"), wxDateTime::Today().FormatISODate());

	curl_easy_cleanup(req);
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
		
		wxConfig *config = (wxConfig *)wxConfig::Get();
   		// Get input file

		wxString path = config->Read(wxT("ImportPath"), wxString(wxT("")));
		wxString defext = config->Read(wxT("ImportExtension"), wxString(wxT("adi"))).Lower();
		bool defFound = false;
		// Construct filter string for file-open dialog
		wxString filter = wxT("All files (*.*)|*.*");
		vector<wxString> exts;
		wxString file_exts = config->Read(wxT("ADIFFiles"), wxString(DEFAULT_ADIF_FILES));
		wx_tokens(file_exts, exts);
		for (int i = 0; i < (int)exts.size(); i++) {
			filter += wxT("|ADIF files (*.") + exts[i] + wxT(")|*.") + exts[i];
			if (exts[i] == defext)
				defFound = true;
		}
		exts.clear();
		file_exts = config->Read(wxT("CabrilloFiles"), wxString(DEFAULT_CABRILLO_FILES));
		wx_tokens(file_exts, exts);
		for (int i = 0; i < (int)exts.size(); i++) {
			filter += wxT("|Cabrillo files (*.") + exts[i] + wxT(")|*.") + exts[i];
			if (exts[i] == defext)
				defFound = true;
		}
		if (defext.IsEmpty() || !defFound)
			defext = wxString(wxT("adi"));
		infile = wxFileSelector(wxT("Select file to Sign"), path, wxT(""), defext, filter,
			wxOPEN|wxFILE_MUST_EXIST, this);
   		if (infile == wxT(""))
   			return;
		wxString inPath;
		wxString inExt;
		wxSplitPath(infile.c_str(), &inPath, NULL, &inExt);
		inExt.Lower();
		config->Write(wxT("ImportPath"), inPath);
		config->Write(wxT("ImportExtension"), inExt);
		// Get output file
		wxString basename;
		wxSplitPath(infile.c_str(), 0, &basename, 0);
		path = wxConfig::Get()->Read(wxT("ExportPath"), wxString(wxT("")));
		wxString deftype = compressed ? wxT("tq8") : wxT("tq7");
		filter = compressed ? wxT("TQSL compressed data files (*.tq8)|*.tq8")
			: wxT("TQSL data files (*.tq7)|*.tq7");
		basename += wxT(".") + deftype;
   		wxString outfile = wxFileSelector(wxT("Select file to write to"),
   			path, basename, deftype, filter + wxT("|All files (*.*)|*.*"),
   			wxSAVE|wxOVERWRITE_PROMPT, this);
   		if (outfile == wxT(""))
   			return;
		config->Write(wxT("ExportPath"), wxPathOnly(outfile));

		tQSL_Location loc = SelectStationLocation(wxT("Select Station Location for Signing"));
		if (loc == 0)
			return;
		
		
		char callsign[40];
		char loc_name[256];
		int dxccnum;
		check_tqsl_error(tqsl_getLocationCallSign(loc, callsign, sizeof callsign));
		check_tqsl_error(tqsl_getLocationDXCCEntity(loc, &dxccnum));
		check_tqsl_error(tqsl_getStationLocationCaptureName(loc, loc_name, sizeof loc_name));
		DXCC dxcc;
		dxcc.getByEntity(dxccnum);
		if (wxMessageBox(wxString::Format(wxT("The file (%s) will be signed using:\n"
                                          wxT("Station Location: %hs\nCall sign: %hs\nDXCC: %s\nIs this correct?")), infile.c_str(), loc_name,
			callsign, wxString(dxcc.name(), wxConvLocal).c_str()),
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
		wxLogError(wxT("%s"), (const char *)s.c_str());
	}
	free_certlist();
	return;
}

void
MyFrame::UploadQSODataFile(wxCommandEvent& event) {
	wxString infile;
	try {
		
		wxConfig *config = (wxConfig *)wxConfig::Get();
   		// Get input file
		wxString path = config->Read(wxT("ImportPath"), wxString(wxT("")));
		wxString defext = config->Read(wxT("ImportExtension"), wxString(wxT("adi"))).Lower();
		bool defFound = false;

		// Construct filter string for file-open dialog
		wxString filter = wxT("All files (*.*)|*.*");
		vector<wxString> exts;
		wxString file_exts = config->Read(wxT("ADIFFiles"), wxString(DEFAULT_ADIF_FILES));
		wx_tokens(file_exts, exts);
		for (int i = 0; i < (int)exts.size(); i++) {
			filter += wxT("|ADIF files (*.") + exts[i] + wxT(")|*.") + exts[i];
			if (exts[i] == defext)
				defFound = true;
		}
		exts.clear();
		file_exts = config->Read(wxT("CabrilloFiles"), wxString(DEFAULT_CABRILLO_FILES));
		wx_tokens(file_exts, exts);
		for (int i = 0; i < (int)exts.size(); i++) {
			filter += wxT("|Cabrillo files (*.") + exts[i] + wxT(")|*.") + exts[i];
			if (exts[i] == defext)
				defFound = true;
		}
		if (defext.IsEmpty() || !defFound)
			defext = wxString(wxT("adi"));
		infile = wxFileSelector(wxT("Select file to Sign"), path, wxT(""), defext, filter,
			wxOPEN|wxFILE_MUST_EXIST, this);
   		if (infile == wxT(""))
   			return;
		wxString inPath;
		wxString inExt;
		wxSplitPath(infile.c_str(), &inPath, NULL, &inExt);
		inExt.Lower();
		config->Write(wxT("ImportPath"), inPath);
		config->Write(wxT("ImportExtension"), inExt);

		// Get output file
		tQSL_Location loc = SelectStationLocation(wxT("Select Station Location for Signing"));
		if (loc == 0)
			return;


		char callsign[40];
		char loc_name[256];
		int dxccnum;
		check_tqsl_error(tqsl_getLocationCallSign(loc, callsign, sizeof callsign));
		check_tqsl_error(tqsl_getLocationDXCCEntity(loc, &dxccnum));
		check_tqsl_error(tqsl_getStationLocationCaptureName(loc, loc_name, sizeof loc_name));
		DXCC dxcc;
		dxcc.getByEntity(dxccnum);
		if (wxMessageBox(wxString::Format(wxT("The file (%s) will be signed and uploaded using:\n"
                                          wxT("Station Location: %hs\nCall sign: %hs\nDXCC: %s\nIs this correct?")), infile.c_str(), loc_name,
			callsign, wxString(dxcc.name(), wxConvLocal).c_str()),
			wxT("TQSL - Confirm signing"), wxYES_NO, this) == wxYES)
			UploadLogFile(loc, infile);
		else
			wxLogMessage(wxT("Signing abandoned"));
	}
	catch (TQSLException& x) {
		wxString s;
		if (infile != wxT(""))
			s = infile + wxT(": ");
		s += wxString(x.what(), wxConvLocal);
		wxLogError(wxT("%s"), (const char *)s.c_str());
	}
	free_certlist();
}

#ifdef ALLOW_UNCOMPRESSED

void
MyFrame::OnFileCompress(wxCommandEvent& event) {
	bool uncompress = (event.m_id == tm_f_uncompress);
	wxString defin = uncompress ? wxT("tq8") : wxT("tq7");
	wxString defout = uncompress ? wxT("tq7") : wxT("tq8");
	wxString filt_uncompressed = wxT("TQSL data files (*.tq7)|*.tq7|All files (*.*)|*.*");
	wxString filt_compressed = wxT("TQSL compressed data files (*.tq8)|*.tq8|All files (*.*)|*.*");
	wxString filtin = uncompress ? filt_compressed : filt_uncompressed;
	wxString filtout = uncompress ? filt_uncompressed : filt_compressed;

	wxString infile;
   	// Get input file
	wxString path = wxConfig::Get()->Read(wxT("ExportPath"), wxT(""));
	infile = wxFileSelector(wxString(wxT("Select file to ")) + (uncompress ? wxT("Uncompress") : wxT("Compress")),
   		path, wxT(""), defin, filtin,
   		wxOPEN|wxFILE_MUST_EXIST, this);
   	if (infile.IsEmpty())
   		return;
	ifstream in;
	gzFile gin = 0;
	if (uncompress)
		gin = gzopen(infile.mb_str(), "rb");
	else
		in.open(infile.mb_str(), ios::in | ios::binary);
	if ((uncompress && !gin) || (!uncompress && !in)) {
		wxMessageBox(wxString(wxT("Unable to open ")) + infile, ErrorTitle, wxOK|wxCENTRE, this);
		return;
	}
	wxConfig::Get()->Write(wxT("ExportPath"), wxPathOnly(infile));
	// Get output file
	wxString basename;
	wxSplitPath(infile.c_str(), 0, &basename, 0);
	path = wxConfig::Get()->Read(wxT("ExportPath"), wxT(""));
	wxString deftype = uncompress ? wxT("tq8") : wxT("tq7");
	wxString filter = uncompress ? wxT("TQSL compressed data files (*.tq8)|*.tq8")
		: wxT("TQSL data files (*.tq7)|*.tq7");
   	wxString outfile = wxFileSelector(wxT("Select file to write to"),
   		path, basename, defout, filtout,
   		wxSAVE|wxOVERWRITE_PROMPT, this);
   	if (outfile.IsEmpty())
   		return;
	wxConfig::Get()->Write(wxT("ExportPath"), wxPathOnly(outfile));

	gzFile gout = 0;
	ofstream out;

	if (uncompress)
		out.open(outfile.mb_str(), ios::out | ios::trunc | ios::binary);
	else
		gout = gzopen(outfile.mb_str(), "wb9");
	if ((uncompress && !out) || (!uncompress && !gout)) {
		wxMessageBox(wxString(wxT("Unable to open ")) + outfile, ErrorTitle, wxOK|wxCENTRE, this);
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
	wxLogMessage(wxString::Format(wxT("%s written to %hs file %s"), infile.c_str(),
		uncompress ? "uncompressed" : "compressed",
		outfile.c_str()));
}

#endif // ALLOW_UNCOMPRESSED

void MyFrame::OnPreferences(wxCommandEvent& WXUNUSED(event)) {
	Preferences dial(this, &help);
	dial.ShowModal();
}

class TQSLConfig {
public:
        TQSLConfig() {
                callSign = "";
		certpwd[0] = '\0';
		elementBody = wxT("");
		locstring = wxT("");
		config = NULL;
		outstr = NULL;
		conv = NULL;
        }
	void SaveSettings (gzFile &out, wxString appname);
	void RestoreConfig (gzFile& in);
	void ParseLocations (const char *loc, gzFile* out);
	wxConfig *config;
	char certpwd[80];
	string callSign;
	wxString elementBody;
	wxString locstring;
	gzFile* outstr;
	tQSL_Converter conv;

private:
	static void xml_restore_start(void *data, const XML_Char *name, const XML_Char **atts);
	static void xml_restore_end(void *data, const XML_Char *name);
	static void xml_text(void *data, const XML_Char *text, int len);
	static void xml_location_start(void *data, const XML_Char *name, const XML_Char **atts);
	static void xml_location_end(void *data, const XML_Char *name);
};

// Save the user's configuration settings - appname is the
// application name (tqslapp/tqslcert)

void TQSLConfig::SaveSettings (gzFile &out, wxString appname) {
	config = new wxConfig(appname);
	wxString name, gname;
	long	context;
	wxString svalue;
	long	lvalue;
	bool	bvalue;
	double	dvalue;
	wxArrayString groupNames;

	groupNames.Add(wxT("/"));
	bool more = config->GetFirstGroup(gname, context);
	while (more) {
		groupNames.Add(wxT("/") + gname);
		more = config->GetNextGroup(gname, context);
	}

	for (unsigned i = 0; i < groupNames.GetCount(); i++) {
		config->SetPath(groupNames[i]);
		more = config->GetFirstEntry(name, context);
		while (more) {
			gzprintf(out, "<Setting name=\"%s\" group=\"%s\" ", (const char *)name.mb_str(), (const char *)groupNames[i].mb_str());
			wxConfigBase::EntryType etype = config->GetEntryType(name);
			switch (etype) {
				case wxConfigBase::Type_Unknown:
				case wxConfigBase::Type_String:
					config->Read(name, &svalue);
					svalue.Replace(wxT("&"), wxT("&amp;"), true); 
					svalue.Replace(wxT("<"), wxT("&lt;"), true); 
					svalue.Replace(wxT(">"), wxT("&gt;"), true); 
					gzprintf(out, "Type=\"String\" Value=\"%s\"/>\n", (const char *)svalue.mb_str());
					break;
				case wxConfigBase::Type_Boolean:
					config->Read(name, &bvalue);
					if (bvalue)
						gzprintf(out, "Type=\"Bool\" Value=\"true\"/>\n", (const char *)svalue.mb_str());
					else
						gzprintf(out, "Type=\"Bool\" Value=\"false\"/>\n", (const char *)svalue.mb_str());
					break;
				case wxConfigBase::Type_Integer:
					config->Read(name, &lvalue);
					gzprintf(out, "Type=\"Int\" Value=\"%d\"/>\n", lvalue);
					break;
				case wxConfigBase::Type_Float:
					config->Read(name, &dvalue);
					gzprintf(out, "Type=\"Float\" Value=\"%f\"/>\n", dvalue);
					break;
			}
			more = config->GetNextEntry(name, context);
		}
	}
	config->SetPath(wxT("/"));
	
	return;
}

void
MyFrame::OnSaveConfig(wxCommandEvent& WXUNUSED(event)) {

	try {
		wxString file_default = wxT("userconfig.tq9");
		wxString filename = wxFileSelector(wxT("Enter file to save to"), wxT(""),
			file_default, wxT(".tq9"), wxT("Configuration files (*.tq9)|*.tq9|All files (*.*)|*.*"),
			wxSAVE|wxOVERWRITE_PROMPT, this);
		if (filename == wxT(""))
			return;

		gzFile out = 0;
		out = gzopen(filename.mb_str(), "wb9");
		if (!out) {
			wxLogError(wxT("Error opening save file %s: %hs"), filename.c_str(), strerror(errno));
			return;
		}
		TQSLConfig* conf = new TQSLConfig();

		gzprintf(out, "<TQSL_Configuration>\n");
		gzprintf(out, "<Warning>If you directly edit this file, you are responsible for its content.\n");
		gzprintf(out, "The ARRL's LoTW Help Desk will be unable to assist you.</Warning>\n");
		gzprintf(out, "<Certificates>\n");

		wxLogMessage(wxT("Saving certificates"));
		int ncerts;
		tqsl_selectCertificates(&certlist, &ncerts, 0, 0, 0, 0, TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED);
		char *password = NULL;
		char buf[8192];
		for (int i = 0; i < ncerts; i++) {
			char callsign[64];
			int rval;
			check_tqsl_error(tqsl_getCertificateCallSign(certlist[i], callsign, sizeof(callsign)));
			do {
   				if ((rval = tqsl_beginSigning(certlist[i], password, getPassword, certlist[i])) == 0)
					break;
				if (tQSL_Error == TQSL_PASSWORD_ERROR) {
					wxLogMessage(wxT("Password error"));
					if (password)
						free((void *)password);
					password = NULL;
				}
			} while (tQSL_Error == TQSL_PASSWORD_ERROR);
			check_tqsl_error(rval);

			char pwbuf[80];
			if (wxIsEmpty(lastPW)) {
				pwbuf[0] = '\0';
			} else {
				const char *pwd = strdup(lastPW.mb_str());
				tqsl_encodeBase64((unsigned char *)pwd, strlen(pwd), pwbuf, sizeof pwbuf);
				int end = strlen(pwbuf);

				while (pwbuf[end - 1] == '\n') {
					pwbuf[--end] = '\0'; // Strip the newline
				}
			}
			wxLogMessage(wxT("\tSaving certificate for %hs"), callsign);
			gzprintf(out, "<Cert CallSign=\"%s\" password=\"%s\">\n", callsign, pwbuf);
			lastPW = wxT("");
			check_tqsl_error(tqsl_exportPKCS12Base64(certlist[i], buf, sizeof(buf), ""));
			tqsl_endSigning(certlist[i]);
			gzwrite(out, buf, strlen(buf));
			gzprintf(out, "\n</Cert>\n");
		}
		gzprintf(out, "</Certificates>\n");
		gzprintf(out, "<Locations>\n");
		wxLogMessage(wxT("Saving Locations"));
		char *sdbuf = NULL;
		check_tqsl_error(tqsl_getStationData(&sdbuf));
		TQSLConfig* parser = new TQSLConfig();
		parser->ParseLocations(sdbuf, &out);
		free(sdbuf);
		gzprintf(out, "</Locations>\n");

		gzprintf(out, "<TQSLSettings>\n");
		conf->SaveSettings(out, wxT("tqslapp"));
		gzprintf(out, "</TQSLSettings>\n");

		gzprintf(out, "<TQSLCertSettings>\n");
		conf->SaveSettings(out, wxT("tqslcert"));
		gzprintf(out, "</TQSLCertSettings>\n");
		wxLogMessage(wxT("Saving QSOs"));
	
		tQSL_Converter conv = 0;
		check_tqsl_error(tqsl_beginConverter(&conv));
		gzprintf(out, "<DupeDb>\n");

		while (true) {
			char dupekey[256];
			char dupedata[10];
			int status = tqsl_getDuplicateRecords(conv, dupekey, dupedata, sizeof(dupekey));
			if (status == -1) break;
			check_tqsl_error(status);
			gzprintf(out, "<Dupe key=\"%s\" />\n", dupekey);
		}
		gzprintf(out, "</DupeDb>\n");

		gzprintf(out, "</TQSL_Configuration>\n");

		gzclose(out);
		wxLogMessage(wxT("Save operation complete."));
	}
	catch (TQSLException& x) {
		wxLogError(wxT("Backup operation failed: %hs"), x.what());
	}
}


void
TQSLConfig::xml_restore_start(void *data, const XML_Char *name, const XML_Char **atts) {
	TQSLConfig* loader = (TQSLConfig *) data;
	int i;

	loader->elementBody = wxT("");
	if (strcmp(name, "Cert") == 0) {
		for (int i = 0; atts[i]; i+=2) {
			if (strcmp(atts[i], "CallSign") == 0) {
				loader->callSign = atts[i + 1];
			} else if (strcmp(atts[i], "password") == 0) {
				if (strlen(atts[i+1]) == 0) {
					loader->certpwd[0] = '\0';
				} else {
					int dlen = sizeof loader->certpwd;
					char pwdstring[128];		// Have to append a newline for decode to work
					strncpy(pwdstring, atts[i+1], sizeof(pwdstring));
					strncat(pwdstring, "\n", sizeof(pwdstring) - strlen(pwdstring));
					tqsl_decodeBase64(pwdstring, (unsigned char *)&loader->certpwd, &dlen);
					if (dlen > 0)
						loader->certpwd[dlen] = '\0';
				}
			}
		}
	} else if (strcmp(name, "TQSLSettings") == 0) {
		wxLogMessage(wxT("Restoring Preferences"));
		loader->config = new wxConfig(wxT("tqslapp"));
	} else if (strcmp(name, "TQSLCertSettings") == 0) {
		loader->config = new wxConfig(wxT("tqslcert"));
	} else if (strcmp(name, "Setting") == 0) {
		wxString sname;
		wxString sgroup;
		wxString stype;
		wxString svalue;
		for (i = 0; atts[i]; i+=2) {
			if (strcmp(atts[i], "name") == 0) {
				sname = wxString(atts[i+1], wxConvLocal);
			} else if (strcmp(atts[i], "group") == 0) {
				sgroup = wxString(atts[i+1], wxConvLocal);
			} else if (strcmp(atts[i], "Type") == 0) {
				stype = wxString(atts[i+1], wxConvLocal);
			} else if (strcmp(atts[i], "Value") == 0) {
				svalue = wxString(atts[i+1], wxConvLocal);
			}
		}
		loader->config->SetPath(sgroup);
		if (stype == wxT("String")) {
			svalue.Replace(wxT("&lt;"), wxT("<"), true);
			svalue.Replace(wxT("&gt;"), wxT(">"), true);
			svalue.Replace(wxT("&amp;"), wxT("&"), true);
			loader->config->Write(sname, svalue);
		} else if (stype == wxT("Bool")) {
			loader->config->Write(sname, (svalue == wxT("true")));
		} else if (stype == wxT("Int")) {
			loader->config->Write(sname, strtol(svalue.mb_str(), NULL, 10));
		} else if (stype == wxT("Float")) {
			loader->config->Write(sname, strtod(svalue.mb_str(), NULL));
		}
	} else if (strcmp(name, "Locations") == 0) {
		wxLogMessage(wxT("Restoring Station Locations"));
		loader->locstring = wxT("<StationDataFile>\n");
	} else if (strcmp(name, "Location") == 0) {
		for (i = 0; atts[i]; i+=2) {
			if (strcmp(atts[i], "name") == 0) {
				loader->locstring += wxT("<StationData name=\"") + wxString(atts[i+1], wxConvLocal) + wxT("\">\n");
				break;
			}
		}
		for (i = 0; atts[i]; i+=2) {
			if (strcmp(atts[i], "name") != 0) {
				loader->locstring += wxT("<") + wxString(atts[i], wxConvLocal) + wxT(">") +
					wxString(atts[i+1], wxConvLocal) + wxT("</") +
					wxString(atts[i], wxConvLocal) + wxT(">\n");
			}
		}
	} else if (strcmp(name, "DupeDb") == 0) {
		wxLogMessage(wxT("Restoring QSO records"));
		check_tqsl_error(tqsl_beginConverter(&loader->conv));
	} else if (strcmp(name, "Dupe") == 0) {
		for (i = 0; atts[i]; i+=2) {
			if (strcmp(atts[i], "key") == 0) {
				int status = tqsl_putDuplicateRecord(loader->conv,  atts[i+1], "D", strlen(atts[i+1]));
				if (status >= 0) check_tqsl_error(status);
			}
		}
	}
}

void
TQSLConfig::xml_restore_end(void *data, const XML_Char *name) {
	TQSLConfig* loader = (TQSLConfig *) data;
	if (strcmp(name, "Cert") == 0) {
		const char *pwd = NULL;
		if (strlen((char *)loader->certpwd) > 0)
			pwd = (char *)&loader->certpwd;
		wxLogMessage(wxT("\tRestoring certificate for %hs"), loader->callSign.c_str());
		if (tqsl_importPKCS12Base64(loader->elementBody.mb_str(), "", pwd, NULL, NULL, NULL) != 0) {
			wxLogError(wxT("\tError importing certificate for %hs: %hs"), loader->callSign.c_str(), tqsl_getErrorString());
		}
	} else if (strcmp(name, "Location") == 0) {
		loader->locstring += wxT("</StationData>\n");
	} else if (strcmp(name, "Locations") == 0) {
		loader->locstring += wxT("</StationDataFile>\n");
		if (tqsl_mergeStationLocations(loader->locstring.mb_str()) != 0) {
			wxLogError(wxT("\tError importing locations: %hs"), tqsl_getErrorString());
		}
	} else if (strcmp(name, "TQSLSettings") == 0 || 
		   strcmp(name, "TQSLCertSettings") == 0) {
		loader->config->Flush(false);
	} else if (strcmp(name, "DupeDb") == 0) {
		check_tqsl_error(tqsl_converterCommit(loader->conv));
		check_tqsl_error(tqsl_endConverter(&loader->conv));
	}
	loader->elementBody = wxT("");
}

void
TQSLConfig::xml_location_start(void *data, const XML_Char *name, const XML_Char **atts) {
	TQSLConfig* parser = (TQSLConfig *) data;

	if (strcmp(name, "StationDataFile") == 0)
		return;
	if (strcmp(name, "StationData") == 0) {
		gzprintf(*parser->outstr, "<Location name=\"%s\"", atts[1]);
	}
}
void
TQSLConfig::xml_location_end(void *data, const XML_Char *name) {
	TQSLConfig* parser = (TQSLConfig *) data;
	if (strcmp(name, "StationDataFile") == 0) 
		return;
	if (strcmp(name, "StationData") == 0) {
		gzprintf(*parser->outstr ," />\n");
		return;
	}
	// Anything else is a station attribute. Add it to the definition.
	parser->elementBody.Trim(false);
	parser->elementBody.Trim(true);
	gzprintf(*parser->outstr,  " %s=\"%s\"", name, (const char *)parser->elementBody.mb_str());
	parser->elementBody = wxT("");
}

void
TQSLConfig::xml_text(void *data, const XML_Char *text, int len) {
	TQSLConfig* loader = (TQSLConfig *) data;
	char buf[512];
	memcpy(buf, text, len);
	buf[len] = '\0';
	loader->elementBody += wxString(buf, wxConvLocal);
}

void
TQSLConfig::RestoreConfig (gzFile& in) {
        XML_Parser xp = XML_ParserCreate(0);
	XML_SetUserData(xp, (void *) this);
        XML_SetStartElementHandler(xp, &TQSLConfig::xml_restore_start);
        XML_SetEndElementHandler(xp, &TQSLConfig::xml_restore_end);
        XML_SetCharacterDataHandler(xp, &TQSLConfig::xml_text);

	char buf[4096];
	wxLogMessage(wxT("Restoring Certificates"));
	int rcount = 0;
	do {
		rcount = gzread(in, buf, sizeof(buf));
		if (rcount > 0) {
			if (XML_Parse(xp, buf, rcount, 0) == 0) {
				wxLogError(wxT("Error parsing saved configuration file: %hs"), XML_ErrorString(XML_GetErrorCode(xp)));
				XML_ParserFree(xp);
				gzclose(in);
				return;
			}
		}
	} while (rcount > 0);
	if (!gzeof(in)) {
		int gerr;
		wxLogError(wxT("Error parsing saved configuration file: %hs"), gzerror(in, &gerr));
		XML_ParserFree(xp);
		return;
	}
	if (XML_Parse(xp, "", 0, 1) == 0) {
		wxLogError(wxT("Error parsing saved configuration file: %hs"), XML_ErrorString(XML_GetErrorCode(xp)));
		XML_ParserFree(xp);
		return;
	}
	wxLogMessage(wxT("Restore Complete."));
}

void
TQSLConfig::ParseLocations (const char *loc, gzFile* out) {

        XML_Parser xp = XML_ParserCreate(0);
	XML_SetUserData(xp, (void *) this);
        XML_SetStartElementHandler(xp, &TQSLConfig::xml_location_start);
        XML_SetEndElementHandler(xp, &TQSLConfig::xml_location_end);
        XML_SetCharacterDataHandler(xp, &TQSLConfig::xml_text);
	outstr = out;

	if (XML_Parse(xp, loc, strlen(loc), 1) == 0) {
		wxLogError(wxT("Error parsing station location file: %hs"), XML_ErrorString(XML_GetErrorCode(xp)));
		XML_ParserFree(xp);
		return;
	}
}

void
MyFrame::OnLoadConfig(wxCommandEvent& WXUNUSED(event)) {
	wxString filename = wxFileSelector(wxT("Select saved configuration file"), wxT(""),
					   wxT(""), wxT("p12"), wxT("Saved configuration files (*.tq9)|*.tq9"),
					   wxOPEN|wxFILE_MUST_EXIST);
	if (filename == wxT(""))
		return;

	gzFile in = 0;
	try {
		in = gzopen(filename.mb_str(), "rb");
		if (!in) {
			wxLogError(wxT("Error opening save file %s: %hs"), filename.c_str(), strerror(errno));
			return;
		}

		TQSLConfig* loader = new TQSLConfig();
		loader->RestoreConfig(in);
		gzclose(in);
	}
	catch (TQSLException& x) {
		wxLogError(wxT("Restore operation failed: %hs"), x.what());
		gzclose(in);
	}
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
QSLApp::GUIinit(bool checkUpdates) {
	MyFrame *frame = new MyFrame(wxT("TQSL"), 50, 50, 550, 400, checkUpdates);
	frame->Show(true);
	SetTopWindow(frame);

	return frame;
}

bool
QSLApp::OnInit() {
	MyFrame *frame = 0;
	bool quiet = false;

	int major, minor;
	if (tqsl_getConfigVersion(&major, &minor)) {
		wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"), wxOK);
		exitNow (TQSL_EXIT_TQSL_ERROR, quiet);
	}
	//short circuit if no arguments

	if (argc<=1) {
		GUIinit(true);
		return true;
	}

	tQSL_Location loc = 0;
	wxString locname;
	bool suppressdate = false;
	int action = TQSL_ACTION_UNSPEC;
	bool upload=false;
	char *password = NULL;
	wxString infile(wxT(""));
	wxString outfile(wxT(""));

	wxCmdLineParser parser;

	static const wxCmdLineEntryDesc cmdLineDesc[] = {
		{ wxCMD_LINE_OPTION, wxT("a"), wxT("action"),	wxT("Specify dialog action - abort, all, compliant or ask") },
		{ wxCMD_LINE_SWITCH, wxT("d"), wxT("nodate"),	wxT("Suppress date range dialog") },
		{ wxCMD_LINE_OPTION, wxT("l"), wxT("location"),	wxT("Selects Station Location") },
		{ wxCMD_LINE_SWITCH, wxT("s"), wxT("editlocation"), wxT("Edit (if used with -l) or create Station Location") },
		{ wxCMD_LINE_OPTION, wxT("o"), wxT("output"),	wxT("Output file name (defaults to input name minus extension plus .tq8") },
		{ wxCMD_LINE_SWITCH, wxT("u"), wxT("upload"),	wxT("Upload after signing instead of saving") },
		{ wxCMD_LINE_SWITCH, wxT("x"), wxT("batch"),	wxT("Exit after processing log (otherwise start normally)") },
		{ wxCMD_LINE_OPTION, wxT("p"), wxT("password"),	wxT("Password for the signing key") },
		{ wxCMD_LINE_SWITCH, wxT("q"), wxT("quiet"),	wxT("Quiet Mode - same behavior as -x") },
		{ wxCMD_LINE_SWITCH, wxT("v"), wxT("version"),  wxT("Display the version information and exit") },
		{ wxCMD_LINE_SWITCH, wxT("h"), wxT("help"),	wxT("Display command line help"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
		{ wxCMD_LINE_PARAM,  NULL,     NULL,		wxT("Input ADIF or Cabrillo log file to sign"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
		{ wxCMD_LINE_NONE }
	};

	// Lowercase command options
	for (int i = 1; i < argc; i++)
		if (argv[i][0] == wxT('-') || argv[i][0] == wxT('/')) 
			if (wxIsalpha(argv[i][1]) && wxIsupper(argv[i][1])) 
				argv[i][1] = wxTolower(argv[i][1]);
		
	parser.SetCmdLine(argc, argv);
	parser.SetDesc(cmdLineDesc);
	// only allow "-" for options, otherwise "/path/something.adif" 
	// is parsed as "-path"
	//parser.SetSwitchChars(wxT("-")); //by default, this is '-' on Unix, or '-' or '/' on Windows. We should respect the Win32 conventions, but allow the cross-platform Unix one for cross-plat loggers
	if (parser.Parse(true)!=0)  {
		exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
	}

	if (parser.Found(wxT("x")) || parser.Found(wxT("q"))) {
		quiet = true;
		wxLog::SetActiveTarget(new wxLogStderr(NULL));
	}

	// print version and exit
	if (parser.Found(wxT("v"))) { 
		wxMessageOutput::Set(new wxMessageOutputStderr);
		wxMessageOutput::Get()->Printf(wxT("TQSL Version " VERSION " " BUILD "\n"));
		return false;
	}

	// check for logical command switches
	if (parser.Found(wxT("o")) && parser.Found(wxT("u"))) {
		cerr << "Option -o cannot be combined with -u" << endl;
		exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
	}
	if ((parser.Found(wxT("o")) || parser.Found(wxT("u"))) && parser.Found(wxT("s"))) { 
		cerr << "Option -s cannot be combined with -u or -o" << endl;
		exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
	}
	if (parser.Found(wxT("s")) && parser.GetParamCount() > 0) {
		cerr << "Option -s cannot be combined with an input file" << endl;
		exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
	}

	if (quiet) {
		wxLog::SetActiveTarget(new wxLogStderr(NULL));
	} else {
		frame = GUIinit(true);
	}
	if (parser.Found(wxT("l"), &locname)) {
		tqsl_endStationLocationCapture(&loc);
		if (tqsl_getStationLocation(&loc, locname.mb_str())) {
			if (quiet) {
				wxLogError(wxT("%hs"), tqsl_getErrorString());
				exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
			}
			else {
				wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), ErrorTitle, wxOK|wxCENTRE, frame);
				return false;
			}
		}
	}
	wxString pwd;
	if (parser.Found(wxT("p"), &pwd)) {
		password = strdup(pwd.mb_str());
	}
	parser.Found(wxT("o"), &outfile);
	if (parser.Found(wxT("d"))) {
		suppressdate = true;
	}
	wxString act;
	if (parser.Found(wxT("a"), &act)) {
		if (!act.CmpNoCase(wxT("abort")))
			action = TQSL_ACTION_ABORT;
		else if (!act.CmpNoCase(wxT("compliant")))
			action = TQSL_ACTION_NEW;
		else if (!act.CmpNoCase(wxT("all")))
			action = TQSL_ACTION_ALL;
		else if (!act.CmpNoCase(wxT("ask")))
			action = TQSL_ACTION_ASK;
		else {
			char tmp[100];
			strncpy(tmp, (const char *)act.mb_str(wxConvUTF8), sizeof tmp);
			tmp[sizeof tmp -1] = '\0';
			if (quiet)
				wxLogMessage(wxT("The -a parameter %hs is not recognized"), tmp);
			else
				cerr << "The action parameter " << tmp << " is not recognized" << endl;
			exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
		}
	}
	if (parser.Found(wxT("u"))) {
		upload=true;
	}
	if (parser.Found(wxT("s"))) {
		// Add/Edit station location
		if (!frame)
			frame = GUIinit(!quiet);
		if (loc == 0) {
			if (tqsl_initStationLocationCapture(&loc)) {
				wxLogError(wxT("%hs"), tqsl_getErrorString());
			}
			AddEditStationLocation(loc, true);
		} else
			AddEditStationLocation(loc, false, wxT("Edit Station Location"));
		return false;
	}
	if (parser.GetParamCount()>0) {
		infile = parser.GetParam(0);
	}
	// We need a logfile, else there's nothing to do.
	if (wxIsEmpty(infile)) {	// Nothing to sign
		wxLogError(wxT("No logfile to sign!"));
		if (quiet)
			exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
		return false;
	}
	if (loc == 0) {
		if (!frame)
			frame = GUIinit(!quiet);
		try {
			loc = frame->SelectStationLocation(wxT("Select Station Location for Signing"));
		}
		catch (TQSLException& x) {
			wxLogError(wxT("%hs"), x.what());
			if (quiet)
				exitNow(TQSL_EXIT_CANCEL, quiet);
		}
	}
	// If no location specified and not chosen, can't sign. Exit.
	if (loc == 0) {
		if (quiet)
			exitNow(TQSL_EXIT_CANCEL, quiet);
		return false;
	}
	wxString path, name, ext;
	if (!wxIsEmpty(outfile)) {
		path = outfile;
	} else {
		wxSplitPath(infile, &path, &name, &ext);
		if (!wxIsEmpty(path))
			path += wxT("/");
		path += name + wxT(".tq8");
	}
	if (upload) {
		try {
			int val=frame->UploadLogFile(loc, infile, true, suppressdate, action, password);
			if (quiet)
				exitNow(val, quiet);
			else
				return true;	// Run the GUI
		} catch (TQSLException& x) {
			wxString s;
			if (!infile.empty())
				s = infile + wxT(": ");
			s += wxString(x.what(), wxConvLocal);
			wxLogError(wxT("%s"), (const char*)s.c_str());
			if (quiet)
				exitNow(TQSL_EXIT_LIB_ERROR, quiet);
			else
				return true;
		}
	} else {
		try {
			int val = frame->ConvertLogFile(loc, infile, path, true, suppressdate, action, password);
			if (quiet)
				exitNow(val, quiet);
			else
				return true;
		} catch (TQSLException& x) {
			wxString s;
			if (!infile.empty())
				s = infile + wxT(": ");
			s += wxString(x.what(), wxConvLocal);
			wxLogError(wxT("%s"), (const char*)s.c_str());
			if (quiet)
				exitNow(TQSL_EXIT_LIB_ERROR, quiet);
			else
				return true;
		}
	}
	if (quiet)
		exitNow(TQSL_EXIT_SUCCESS, quiet);
	return true;
}

