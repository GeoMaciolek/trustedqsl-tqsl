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
#include <wx/notebook.h>
#include <wx/statline.h>
#include <wx/app.h>

#include "tqslhelp.h"
#include "tqsltrace.h"

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

#include "crqwiz.h"
#include "getpassword.h"
#include "loadcertwiz.h"
#include "tqsllib.h"
#include <map>
#include "util.h"


#ifdef _MSC_VER //could probably do a more generic check here...
// stdint exists on vs2012 and (I think) 2010, but not 2008 or its platform
  #define uint8_t unsigned char
#else
#include <stdint.h> //for uint8_t; should be cstdint but this is C++11 and not universally supported
#endif

#ifdef _WIN32
	#include <io.h>
#endif
#include <zlib.h>
#include <openssl/opensslv.h> // only for version info!
#include <db.h> //only for version info!
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
#include "tqslapp.h"

#include "winstrdefs.h"

#ifdef _WIN32
#include "key.xpm"
#else
#include "key-new.xpm"
#endif
#include "save.xpm"
#include "upload.xpm"
#include "upload_dis.xpm"
#include "file_edit.xpm"
#include "file_edit_dis.xpm"
#include "loc_add.xpm"
#include "loc_add_dis.xpm"
#include "delete.xpm"
#include "delete_dis.xpm"
#include "edit.xpm"
#include "edit_dis.xpm"
#include "download.xpm"
#include "download_dis.xpm"
#include "properties.xpm"
#include "properties_dis.xpm"
#include "import.xpm"
#include "lotw.xpm"

using namespace std;

/// GEOMETRY

#define LABEL_HEIGHT 20
#define LABEL_WIDTH 100

#define CERTLIST_FLAGS TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_SUPERCEDED | TQSL_SELECT_CERT_EXPIRED

static wxMenu *stn_menu;

static wxString flattenCallSign(const wxString& call);

static wxString ErrorTitle(wxT("TQSL Error"));
FILE *diagFile = NULL;
static wxString origCommandLine = wxT("");
static MyFrame *frame = 0;

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
	class MyFrame *GUIinit(bool checkUpdates, bool quiet=false);
	bool OnInit();
	virtual int OnRun();
//	virtual wxLog *CreateLogTarget();
};

QSLApp::~QSLApp() {
	wxConfigBase *c = wxConfigBase::Set(0);
	if (c)
		delete c;
	if (diagFile)
		fclose(diagFile);
}

IMPLEMENT_APP(QSLApp)


static int
getCertPassword(char *buf, int bufsiz, tQSL_Cert cert) {
	tqslTrace("getCertPassword", "buf = %lx, bufsiz=%d, cert=%lx", buf, bufsiz, cert);
	char call[TQSL_CALLSIGN_MAX+1] = "";
	int dxcc = 0;
	tqsl_getCertificateCallSign(cert, call, sizeof call);
	tqsl_getCertificateDXCCEntity(cert, &dxcc);
	DXCC dx;
	dx.getByEntity(dxcc);
	
	wxString message = wxString::Format(wxT("Enter the password to unlock the callsign certificate for\n"
		wxT("%hs -- %hs\n(This is the password you made up when you\ninstalled the callsign certificate.)")),
		call, dx.name());

	wxWindow* top = wxGetApp().GetTopWindow();
	if (frame->IsQuiet()) {
		frame->Show(true);
	}
	top->SetFocus();
	top->Raise();

	wxString pwd = wxGetPasswordFromUser(message, wxT("Enter password"), wxT(""), top);
	if (frame->IsQuiet()) {
		frame->Show(false);
		wxSafeYield(frame);
	}
	if (pwd.IsEmpty())
		return 1;
	strncpy(buf, pwd.mb_str(), bufsiz);
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
	msg = new wxStaticText(this, TQSL_CD_MSG, wxT(" "));
	sizer->Add(msg, 0, wxALL|wxALIGN_LEFT, 10);
	canbut = new wxButton(this, TQSL_CD_CANBUT, wxT("Cancel"));
	sizer->Add(canbut, 0, wxALL|wxEXPAND, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

#define TQSL_PROG_CANBUT TQSL_ID_LOW+30

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
	tqslTrace("UploadDialog::UploadDialog", "parent = %lx", parent);
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
	tqslTrace("UploadDialog::OnDone");
	EndModal(1);
}

int UploadDialog::doUpdateProgress(double dltotal, double dlnow, double ultotal, double ulnow) {
	tqslTrace("UploadDialog::doUpdaeProgresss", "dltotal=%f, dlnow=%f, ultotal=%f, ulnow=%f", dltotal, dlnow, ultotal, ulnow);
	if (cancelled) return 1;
	if (ultotal>0.0000001) progress->SetValue((int)(100*(ulnow/ultotal)));
	return 0;
}

#define TQSL_DR_START TQSL_ID_LOW+10
#define TQSL_DR_END TQSL_ID_LOW+11
#define TQSL_DR_OK TQSL_ID_LOW+12
#define TQSL_DR_CAN TQSL_ID_LOW+13
#define TQSL_DR_MSG TQSL_ID_LOW+14

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
	tqslTrace("DateRangeDialog::DateRangeDialog");
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
	tqslTrace("DateRangeDialog::TransferDataFromWindow");
	wxString text = start_tc->GetValue();
	tqslTrace("DateRangeDialog::TransferDataFromWindow: start=%s",_S(text));
	if (text.Trim() == wxT(""))
		start.year = start.month = start.day = 0;
	else if (tqsl_initDate(&start, text.mb_str()) || !tqsl_isDateValid(&start)) {
		msg->SetLabel(wxT("Start date is invalid"));
		return false;
	}
	text = end_tc->GetValue();
	tqslTrace("DateRangeDialog::TransferDataFromWindow: end=%s",_S(text));
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
	tqslTrace("DateRangeDialog::OnOk");
	if (TransferDataFromWindow())
		EndModal(wxOK);
}

void
DateRangeDialog::OnCancel(wxCommandEvent&) {
	tqslTrace("DateRangeDialog::OnCancel");
	EndModal(wxCANCEL);
}

#define TQSL_DP_OK TQSL_ID_LOW+20
#define TQSL_DP_CAN TQSL_ID_LOW+21
#define TQSL_DP_ALLOW TQSL_ID_LOW+22

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
	tqslTrace("DupesDialog::DupesDialog", "qso_count = %d, dupes =%d, action= =%d",qso_count, dupes, action);
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
				wxT("\n\nThe log file you are uploading using your QSO Logging system consists entirely of previously uploaded\n")
				wxT("QSOs (duplicates) that create unnecessary work for LoTW. There may be a more recent version of your QSO\n")
				wxT("Logging system that would prevent this. Please check with your QSO Logging system's vendor for an updated version.\n")
				wxT("In the meantime, please note that some loggers may exhibit strange behavior if an option other than 'Allow duplicates'\n")
				wxT("is clicked. Choosing 'Cancel' is usually safe, but a defective logger not checking the status messages reported by TrustedQSL may produce\n")
				wxT("strange (but harmless) behavior such as attempting to upload an empty file or marking all chosen QSOs as 'sent'");
		} else {
			message+=
				wxT("\n\nThe log file you are uploading using your QSO Logging system includes some previously uploaded\n")
				wxT("QSOs (duplicates) that create unnecessary work for LoTW. There may be a more recent version of your\n")
				wxT("QSO Logging system that would prevent this. Please check with your QSO Logging system's vendor for an updated version.\n")
				wxT("In the meantime, please note that some loggers may exhibit strange behavior if an option other than 'Allow duplicates'\n")
				wxT("is clicked. 'Exclude duplicates' is recommended, but a logger that does its own duplicate tracking may incorrectly\n")
				wxT("set the status in this case. A logger that doesn't track duplicates should be unaffected by choosing 'Exclude duplicates'\n")
				wxT("and if it tracks 'QSO sent' status, will correctly mark all selected QSOs as sent - they are in your account even though\n")
				wxT("they would not be in this specific batch\n")
				wxT("Choosing 'Cancel' is usually safe, but a defective logger not checking the status messages reported by TrustedQSL may produce\n")
				wxT("strange (but harmless) behavior such as attempting to upload an empty file or marking all chosen QSOs as 'sent'");
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
	tqslTrace("DupesDialog::OnOk");
	EndModal(TQSL_DP_OK);
}

void
DupesDialog::OnCancel(wxCommandEvent&) {
	tqslTrace("DupesDialog::OnCancel");
	EndModal(TQSL_DP_CAN);
}

void
DupesDialog::OnAllow(wxCommandEvent&) {
	tqslTrace("DupesDialog::OnAllow");

	if (wxMessageBox(wxT("The only reason to re-sign duplicate QSOs is if a previous upload")
		wxT(" went unprocessed by LoTW, either because it was never uploaded, or there was a server failure\n\n")
		wxT("Are you sure you want to proceed? Click 'No' to review the choices"), wxT("Are you sure?"), wxYES_NO|wxICON_EXCLAMATION, this)==wxYES) {
		EndModal(TQSL_DP_ALLOW);
	}
}

#define TQSL_AE_OK TQSL_ID_LOW+40
#define TQSL_AE_CAN TQSL_ID_LOW+41

class ErrorsDialog : public wxDialog {
public:
	ErrorsDialog(wxWindow *parent = 0, wxString msg = wxT(""));
private:
	void OnOk(wxCommandEvent&);	
	void OnCancel(wxCommandEvent&);
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ErrorsDialog, wxDialog)
	EVT_BUTTON(TQSL_AE_OK, ErrorsDialog::OnOk)
	EVT_BUTTON(TQSL_AE_CAN, ErrorsDialog::OnCancel)
END_EVENT_TABLE()

ErrorsDialog::ErrorsDialog(wxWindow *parent, wxString msg) :
		wxDialog(parent, -1, wxString(wxT("Errors Detected")), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE) {
	tqslTrace("ErrorsDialog::ErrorsDialog", "msg=%s", _S(msg));
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	sizer->Add(new wxStaticText(this, -1, msg), 0, wxALL|wxALIGN_CENTER, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxButton(this, TQSL_AE_OK, wxT("Ignore")), 0, wxRIGHT, 5);
	hsizer->Add(new wxButton(this, TQSL_AE_CAN, wxT("Cancel")), 0, wxLEFT, 10);
	sizer->Add(hsizer, 0, wxALIGN_CENTER|wxALL, 10);
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

void
ErrorsDialog::OnOk(wxCommandEvent&) {
	tqslTrace("ErrorsDialog::OnOk");
	EndModal(TQSL_AE_OK);
}

void
ErrorsDialog::OnCancel(wxCommandEvent&) {
	tqslTrace("ErrorsDialog::OnCancel");
	EndModal(TQSL_AE_CAN);
}

static void
init_modes() {
	tqslTrace("init_modes");
	tqsl_clearADIFModes();
	wxConfig *config = (wxConfig *)wxConfig::Get();
	long cookie;
	wxString key, value;
	vector<wxString> badModes;
	config->SetPath(wxT("/modeMap"));
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, wxT(""));
		bool newMode = true;
		int numModes; 
		if (tqsl_getNumMode(&numModes) == 0) {
			for (int i = 0; i < numModes; i++) {
				const char *modestr;
				if (tqsl_getMode(i, &modestr, NULL) == 0) {
					if (strcasecmp(key.mb_str(), modestr) == 0) {
						wxLogWarning(wxT("Your custom mode map %s conflicts with the standard mode definition for %hs and was deleted."), key.c_str(), modestr);
						newMode = false;
						badModes.push_back(key);
						break;
					}
				}
			}
		}
		if (newMode)
			tqsl_setADIFMode(key.mb_str(), value.mb_str());
		stat = config->GetNextEntry(key, cookie);
	}
	// Delete the conflicting entries
	for (int i = 0; i < (int)badModes.size(); i++) {
		config->DeleteEntry(badModes[i]);
	}
	config->SetPath(wxT("/"));
}

static void
init_contests() {
	tqslTrace("init_contests");
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
	tqslTrace("check_tqsl_error", "rval=%d", rval);
	const char *msg = tqsl_getErrorString();
	tqslTrace("check_tqsl_error", "msg=%s", msg);
	throw TQSLException(msg);
}

static tQSL_Cert *certlist = 0;
static int ncerts;

static void
free_certlist() {
	tqslTrace("free_certlist");
	if (certlist) {
		for (int i = 0; i < ncerts; i++)
			tqsl_freeCertificate(certlist[i]);
		certlist = 0;
	}
}

static void
get_certlist(string callsign, int dxcc, bool expired, bool superceded) {
	tqslTrace("get_certlist", "callsign=%s, dxcc=%d, expired=%d, superceded=%d",callsign.c_str(), dxcc, expired, superceded);
	free_certlist();
	int select = TQSL_SELECT_CERT_WITHKEYS;
	if (expired) select |= TQSL_SELECT_CERT_EXPIRED;
	if (superceded) select |= TQSL_SELECT_CERT_SUPERCEDED;
	tqsl_selectCertificates(&certlist, &ncerts,
		(callsign == "") ? 0 : callsign.c_str(), dxcc, 0, 0, select);
}

class LogList : public wxLog {
public:
	LogList(MyFrame *frame) : wxLog(), _frame(frame) {}
	virtual void DoLogString(const wxChar *szString, time_t t);
private:
	MyFrame *_frame;
};

void LogList::DoLogString(const wxChar *szString, time_t) {
	wxTextCtrl *_logwin = 0;

	if (diagFile)  {
#ifdef _WIN32
		fprintf(diagFile, "%hs\n", szString);
#else
//		const char *cstr = wxString(szString).mb_str();
//		fprintf(diagFile, "%s\n", cstr);
		fprintf(diagFile, "%ls\n", szString);
#endif
	}
	if (wxString(szString).StartsWith(wxT("Debug:")))
		return;
	if (wxString(szString).StartsWith(wxT("Error: Unable to open requested HTML document:")))
		return;
	if (_frame != 0)
		_logwin = _frame->logwin;
	if (_logwin == 0) {
#ifdef _WIN32
		cerr << szString << endl;
#else
		fprintf(stderr, "%ls\n", szString);
#endif
		return;
	}
	_logwin->AppendText(szString);
	_logwin->AppendText(wxT("\n"));
}

class LogStderr : public wxLog {
public:
	LogStderr(void) : wxLog() {}
	virtual void DoLogString(const wxChar *szString, time_t t);
};

void LogStderr::DoLogString(const wxChar *szString, time_t) {

	if (diagFile) {
#ifdef _WIN32
		fprintf(diagFile, "%hs\n", szString);
#else
		fprintf(diagFile, "%ls\n", szString);
//		const char *cstr = wxString(szString).mb_str();
//		fprintf(diagFile, "%s\n", cstr);
#endif
	}
	if (wxString(szString).StartsWith(wxT("Debug:")))
		return;
#ifdef _WIN32
	cerr << szString << endl;
#else
	fprintf(stderr, "%ls\n", szString);
#endif
	return;
}

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_MENU(tm_s_add, MyFrame::AddStationLocation)
	EVT_BUTTON(tl_AddLoc, MyFrame::AddStationLocation)
	EVT_MENU(tm_s_edit, MyFrame::EditStationLocation)
	EVT_BUTTON(tl_EditLoc, MyFrame::EditStationLocation)
	EVT_MENU(tm_f_new, MyFrame::EnterQSOData)
	EVT_BUTTON(tl_Edit, MyFrame::EnterQSOData)
	EVT_MENU(tm_f_edit, MyFrame::EditQSOData)
	EVT_MENU(tm_f_import_compress, MyFrame::ImportQSODataFile)
	EVT_BUTTON(tl_Save, MyFrame::ImportQSODataFile)
	EVT_MENU(tm_f_upload, MyFrame::UploadQSODataFile)
	EVT_BUTTON(tl_Upload, MyFrame::UploadQSODataFile)
	EVT_MENU(tm_f_exit, MyFrame::DoExit)
	EVT_MENU(tm_f_preferences, MyFrame::OnPreferences)
	EVT_MENU(tm_f_loadconfig, MyFrame::OnLoadConfig)
	EVT_MENU(tm_f_saveconfig, MyFrame::OnSaveConfig)
	EVT_MENU(tm_h_contents, MyFrame::OnHelpContents)
	EVT_MENU(tm_h_about, MyFrame::OnHelpAbout)
	EVT_MENU(tm_f_diag, MyFrame::OnHelpDiagnose)

	EVT_MENU(tm_h_update, MyFrame::CheckForUpdates)

	EVT_CLOSE(MyFrame::OnExit)

	EVT_MENU(tc_CRQWizard, MyFrame::CRQWizard)
	EVT_MENU(tc_c_New, MyFrame::CRQWizard)
	EVT_MENU(tc_c_Load, MyFrame::OnLoadCertificateFile)
	EVT_BUTTON(tc_Load, MyFrame::OnLoadCertificateFile)
	EVT_MENU(tc_c_Properties, MyFrame::OnCertProperties)
	EVT_BUTTON(tc_CertProp, MyFrame::OnCertProperties)
	EVT_MENU(tc_c_Export, MyFrame::OnCertExport)
	EVT_BUTTON(tc_CertSave, MyFrame::OnCertExport)
	EVT_MENU(tc_c_Delete, MyFrame::OnCertDelete)
//	EVT_MENU(tc_c_Import, MyFrame::OnCertImport)
//	EVT_MENU(tc_c_Sign, MyFrame::OnSign)
	EVT_MENU(tc_c_Renew, MyFrame::CRQWizardRenew)
	EVT_BUTTON(tc_CertRenew, MyFrame::CRQWizardRenew)
	EVT_MENU(tc_h_Contents, MyFrame::OnHelpContents)
	EVT_MENU(tc_h_About, MyFrame::OnHelpAbout)
	EVT_MENU(tl_c_Properties, MyFrame::OnLocProperties)
	EVT_MENU(tm_s_Properties, MyFrame::OnLocProperties)
	EVT_BUTTON(tl_PropLoc, MyFrame::OnLocProperties)
	EVT_MENU(tl_c_Delete, MyFrame::OnLocDelete)
	EVT_BUTTON(tl_DeleteLoc, MyFrame::OnLocDelete)
	EVT_MENU(tl_c_Edit, MyFrame::OnLocEdit)
	EVT_BUTTON(tl_Login, MyFrame::OnLoginToLogbook)
	EVT_TREE_SEL_CHANGED(tc_CertTree, MyFrame::OnCertTreeSel)
	EVT_TREE_SEL_CHANGED(tc_LocTree, MyFrame::OnLocTreeSel)

	EVT_TIMER(tm_f_autoupdate, MyFrame::DoUpdateCheck)
END_EVENT_TABLE()

void
MyFrame::OnExit(TQ_WXCLOSEEVENT& WXUNUSED(event)) {
	int x, y, w, h;
	// Don't save window size/position if minimized or too small
	wxConfig *config = (wxConfig *)wxConfig::Get();
	if (!IsIconized()) {
		GetPosition(&x, &y);
		GetSize(&w, &h);
		if (w >= MAIN_WINDOW_MIN_WIDTH && h >= MAIN_WINDOW_MIN_HEIGHT) {
			config->Write(wxT("MainWindowX"), x);
			config->Write(wxT("MainWindowY"), y);
			config->Write(wxT("MainWindowWidth"), w);
			config->Write(wxT("MainWindowHeight"), h);
			config->Flush(false);
		}
	}
	Destroy();		// close the window
	bool ab;
	config->Read(wxT("AutoBackup"), &ab, DEFAULT_AUTO_BACKUP);
	if (ab) {
		wxString bdir = config->Read(wxT("BackupFolder"), wxString(tQSL_BaseDir, wxConvLocal));
#ifdef _WIN32
		bdir += wxT("\\tqslconfig.tbk");
#else
		bdir += wxT("/tqslconfig.tbk");
#endif
		BackupConfig(bdir, true);
	}
}

void
MyFrame::DoExit(wxCommandEvent& WXUNUSED(event)) {
	Close();
	Destroy();
}

void
MyFrame::DoUpdateCheck(wxTimerEvent& WXUNUSED(event)) {
	//check for updates
	wxConfig *config = (wxConfig *)wxConfig::Get();
	if (config->Read(wxT("AutoUpdateCheck"), true)) {
	wxLogMessage(wxT("Checking for TQSL updates..."));
		wxSafeYield();
		wxBusyCursor wait;
		wxSafeYield();
		DoCheckForUpdates(true);
		logwin->SetValue(wxT(""));              // Clear the checking message
	}
}

MyFrame::MyFrame(const wxString& title, int x, int y, int w, int h, bool checkUpdates, bool quiet)
	: wxFrame(0, -1, title, wxPoint(x, y), wxSize(w, h)) {

	_quiet = quiet;

	DocPaths docpaths(wxT("tqslapp"));
	wxBitmap savebm(save_xpm);
	wxBitmap uploadbm(upload_xpm);
	wxBitmap upload_disbm(upload_dis_xpm);
	wxBitmap file_editbm(file_edit_xpm);
	wxBitmap file_edit_disbm(file_edit_dis_xpm);
	wxBitmap locaddbm(loc_add_xpm);
	wxBitmap locadd_disbm(loc_add_dis_xpm);
	wxBitmap editbm(edit_xpm);
	wxBitmap edit_disbm(edit_dis_xpm);
	wxBitmap deletebm(delete_xpm);
	wxBitmap delete_disbm(delete_dis_xpm);
	wxBitmap downloadbm(download_xpm);
	wxBitmap download_disbm(download_dis_xpm);
	wxBitmap propertiesbm(properties_xpm);
	wxBitmap properties_disbm(properties_dis_xpm);
	wxBitmap importbm(import_xpm);
	wxBitmap lotwbm(lotw_xpm);
	loc_edit_button = NULL;
	cert_save_label = NULL;
	req = NULL;

	// File menu
	file_menu = new wxMenu;
	file_menu->Append(tm_f_upload, wxT("Sign and &upload ADIF or Cabrillo File..."));
	file_menu->Append(tm_f_import_compress, wxT("&Sign and save ADIF or Cabrillo file..."));
	file_menu->AppendSeparator();
	file_menu->Append(tm_f_saveconfig, wxT("&Backup Station Locations, Certificates, and Preferences..."));
	file_menu->Append(tm_f_loadconfig, wxT("&Restore Station Locations, Certificates, and Preferences..."));
	file_menu->AppendSeparator();
	file_menu->Append(tm_f_new, wxT("Create &New ADIF file..."));
	file_menu->Append(tm_f_edit, wxT("&Edit existing ADIF file..."));
	file_menu->AppendSeparator();
#ifdef __WXMAC__	// On Mac, Preferences not on File menu
	file_menu->Append(tm_f_preferences, wxT("&Preferences..."));
#else
	file_menu->Append(tm_f_preferences, wxT("Display or Modify &Preferences..."));
#endif
	file_menu->AppendSeparator();
	file_menu->AppendCheckItem(tm_f_diag, wxT("Dia&gnostic Mode"));
	file_menu->Check(tm_f_diag, false);
#ifndef __WXMAC__	// On Mac, Exit not on File menu
	file_menu->AppendSeparator();
#endif
	file_menu->Append(tm_f_exit, wxT("E&xit TQSL\tAlt-X"));

	cert_menu = makeCertificateMenu(false);
	// Station menu
	stn_menu = new wxMenu;
	stn_menu->Append(tm_s_Properties, wxT("&Display Station Location Properties"));
	stn_menu->Enable(tm_s_Properties, false);
	stn_menu->Append(tm_s_edit, wxT("&Edit Station Location"));
	stn_menu->Append(tm_s_add, wxT("&Add Station Location"));

	// Help menu
	help = new wxHtmlHelpController(wxHF_DEFAULT_STYLE | wxHF_OPEN_FILES);
	help_menu = new wxMenu;
	help->UseConfig(wxConfig::Get());
	wxString hhp = docpaths.FindAbsoluteValidPath(wxT("tqslapp.hhp"));
	if (wxFileNameFromPath(hhp) != wxT("")) {
		if (help->AddBook(hhp))
#ifdef __WXMAC__
		help_menu->Append(tm_h_contents, wxT("&Contents"));
#else
		help_menu->Append(tm_h_contents, wxT("Display &Documentation"));
		help_menu->AppendSeparator();
#endif
	}

	help_menu->Append(tm_h_update, wxT("Check for &Updates..."));

	help_menu->Append(tm_h_about, wxT("&About"));
	// Main menu
	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, wxT("&File"));
	menu_bar->Append(stn_menu, wxT("&Station Location"));
	menu_bar->Append(cert_menu, wxT("Callsign &Certificate"));
	menu_bar->Append(help_menu, wxT("&Help"));

	SetMenuBar(menu_bar);

	wxPanel* topPanel = new wxPanel(this);
	wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
	topPanel->SetSizer(topSizer);

	// Log operations

	//topSizer->Add(new wxStaticText(topPanel, -1, wxT("")), 0, wxEXPAND | wxALL, 1);
	topSizer->AddSpacer(10);

	wxNotebook* notebook = new wxNotebook(topPanel, -1, wxDefaultPosition, wxDefaultSize, wxNB_TOP /* | wxNB_FIXEDWIDTH*/, wxT("Log Operations"));

	topSizer->Add(notebook, 0, wxEXPAND | wxALL, 1);

	topSizer->Add(new wxStaticText(topPanel, -1, wxT("Status Log")), 0, wxEXPAND | wxALL, 1);

	logwin = new wxTextCtrl(topPanel, -1, wxT(""), wxDefaultPosition, wxSize(400, 200),
		wxTE_MULTILINE|wxTE_READONLY);
	topSizer->Add(logwin, 50, wxEXPAND | wxALL, 1);

	wxPanel* buttons = new wxPanel(notebook, -1);
	buttons->SetBackgroundColour(wxColour(255, 255, 255));

	wxBoxSizer* bsizer = new wxBoxSizer(wxVERTICAL);
	buttons->SetSizer(bsizer);

	wxPanel* b1Panel = new wxPanel(buttons);
	b1Panel->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* b1sizer = new wxBoxSizer(wxHORIZONTAL);
	b1Panel->SetSizer(b1sizer);
	
	wxBitmapButton *up = new wxBitmapButton(b1Panel, tl_Upload, uploadbm);
	up->SetBitmapDisabled(upload_disbm);
	b1sizer->Add(up, 0, wxALL, 1);
	b1sizer->Add(new wxStaticText(b1Panel, -1, wxT("\nSign a log and upload it automatically to LoTW")), 1, wxALL, 1);
	bsizer->Add(b1Panel, 0, wxALL, 1);

	wxPanel* b2Panel = new wxPanel(buttons);
	wxBoxSizer* b2sizer = new wxBoxSizer(wxHORIZONTAL);
	b2Panel->SetBackgroundColour(wxColour(255, 255, 255));
	b2Panel->SetSizer(b2sizer);
	b2sizer->Add(new wxBitmapButton(b2Panel, tl_Save, savebm), 0, wxALL, 1);
	b2sizer->Add(new wxStaticText(b2Panel, -1, wxT("\nSign a log and save it for uploading later")), 1, wxALL, 1);
	bsizer->Add(b2Panel, 0, wxALL, 1);

	wxPanel* b3Panel = new wxPanel(buttons);
	wxBoxSizer* b3sizer = new wxBoxSizer(wxHORIZONTAL);
	b3Panel->SetBackgroundColour(wxColour(255, 255, 255));
	b3Panel->SetSizer(b3sizer);
	wxBitmapButton *fed = new wxBitmapButton(b3Panel, tl_Edit, file_editbm);
	fed->SetBitmapDisabled(file_edit_disbm);
	b3sizer->Add(fed, 0, wxALL, 1);
	b3sizer->Add(new wxStaticText(b3Panel, -1, wxT("\nCreate an ADIF file for signing and uploading")), 1, wxALL, 1);
	bsizer->Add(b3Panel, 0, wxALL, 1);

	wxPanel* b4Panel = new wxPanel(buttons);
	wxBoxSizer* b4sizer = new wxBoxSizer(wxHORIZONTAL);
	b4Panel->SetBackgroundColour(wxColour(255, 255, 255));
	b4Panel->SetSizer(b4sizer);
	b4sizer->Add(new wxBitmapButton(b4Panel, tl_Login, lotwbm), 0, wxALL, 1);
	b4sizer->Add(new wxStaticText(b4Panel, -1, wxT("\nLog in to the Logbook of the World Site")), 1, wxALL, 1);
	bsizer->Add(b4Panel, 0, wxALL, 1);

	notebook->AddPage(buttons, wxT("Log Operations"));

//	notebook->InvalidateBestSize();
//	logwin->FitInside();

	// Location tab 

	wxPanel* loctab = new wxPanel(notebook, -1);

	wxBoxSizer* locsizer = new wxBoxSizer(wxHORIZONTAL);
	loctab->SetSizer(locsizer);

	wxPanel* locgrid = new wxPanel(loctab, -1);
	locgrid->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* lgsizer = new wxBoxSizer(wxVERTICAL);
	locgrid->SetSizer(lgsizer);

	loc_tree = new LocTree(locgrid, tc_LocTree, wxDefaultPosition,
		wxDefaultSize, wxTR_DEFAULT_STYLE | wxBORDER_NONE);

	loc_tree->SetBackgroundColour(wxColour(255, 255, 255));
	loc_tree->Build();
	LocTreeReset();
	lgsizer->Add(loc_tree, 1, wxEXPAND);

	loc_select_label = new wxStaticText(locgrid, -1, wxT("\nSelect a Station Location to process      "));
	lgsizer->Add(loc_select_label, 0, wxALL, 1);

	locsizer->Add(locgrid, 50, wxEXPAND);

	wxStaticLine *locsep =new wxStaticLine(loctab, -1, wxDefaultPosition, wxSize(2, -1), wxLI_VERTICAL);
	locsizer->Add(locsep, 0, wxEXPAND);

	wxPanel* lbuttons = new wxPanel(loctab, -1);
	lbuttons->SetBackgroundColour(wxColour(255, 255, 255));
	locsizer->Add(lbuttons, 50, wxEXPAND);
	wxBoxSizer* lbsizer = new wxBoxSizer(wxVERTICAL);
	lbuttons->SetSizer(lbsizer);

	wxPanel* lb1Panel = new wxPanel(lbuttons);
	lb1Panel->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* lb1sizer = new wxBoxSizer(wxHORIZONTAL);
	lb1Panel->SetSizer(lb1sizer);
	
	loc_add_button = new wxBitmapButton(lb1Panel, tl_AddLoc, locaddbm);
	loc_add_button->SetBitmapDisabled(locadd_disbm);
	lb1sizer->Add(loc_add_button, 0, wxALL, 1);
	// Note - the doubling below is to size the label to allow the control to stretch later
	loc_add_label = new wxStaticText(lb1Panel, -1, wxT("\nCreate a new Station LocationCreate a new Station\n"));
	lb1sizer->Add(loc_add_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	lbsizer->Add(lb1Panel, 1, wxALL, 1);
	int tw, th;
	loc_add_label->GetSize(&tw, &th);
	loc_add_label->SetLabel(wxT("\nCreate a new Station Location"));

	wxPanel* lb2Panel = new wxPanel(lbuttons);
	lb2Panel->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* lb2sizer = new wxBoxSizer(wxHORIZONTAL);
	lb2Panel->SetSizer(lb2sizer);
	
	loc_edit_button = new wxBitmapButton(lb2Panel, tl_EditLoc, editbm);
	loc_edit_button->SetBitmapDisabled(edit_disbm);
	loc_edit_button->Enable(false);
	lb2sizer->Add(loc_edit_button, 0, wxALL, 1);
	loc_edit_label = new wxStaticText(lb2Panel, -1, wxT("\nEdit a Station Location"), wxDefaultPosition, wxSize(tw, th));
	lb2sizer->Add(loc_edit_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	lbsizer->Add(lb2Panel, 1, wxALL, 1);

	wxPanel* lb3Panel = new wxPanel(lbuttons);
	lb3Panel->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* lb3sizer = new wxBoxSizer(wxHORIZONTAL);
	lb3Panel->SetSizer(lb3sizer);
	
	loc_delete_button = new wxBitmapButton(lb3Panel, tl_DeleteLoc, deletebm);
	loc_delete_button->SetBitmapDisabled(delete_disbm);
	loc_delete_button->Enable(false);
	lb3sizer->Add(loc_delete_button, 0, wxALL, 1);
	loc_delete_label = new wxStaticText(lb3Panel, -1, wxT("\nDelete a Station Location"), wxDefaultPosition, wxSize(tw, th));
	lb3sizer->Add(loc_delete_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	lbsizer->Add(lb3Panel, 1, wxALL, 1);

	wxPanel* lb4Panel = new wxPanel(lbuttons);
	lb4Panel->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* lb4sizer = new wxBoxSizer(wxHORIZONTAL);
	lb4Panel->SetSizer(lb4sizer);
	
	loc_prop_button = new wxBitmapButton(lb4Panel, tl_PropLoc, propertiesbm);
	loc_prop_button->SetBitmapDisabled(properties_disbm);
	loc_prop_button->Enable(false);
	lb4sizer->Add(loc_prop_button, 0, wxALL, 1);
	loc_prop_label = new wxStaticText(lb4Panel, -1, wxT("\nDisplay Station Location Properties"), wxDefaultPosition, wxSize(tw, th));
	lb4sizer->Add(loc_prop_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	lbsizer->Add(lb4Panel, 1, wxALL, 1);

	notebook->AddPage(loctab, wxT("Station Locations"));

	// Certificates tab

	wxPanel* certtab = new wxPanel(notebook, -1);

	wxBoxSizer* certsizer = new wxBoxSizer(wxHORIZONTAL);
	certtab->SetSizer(certsizer);

	wxPanel* certgrid = new wxPanel(certtab, -1);
	certgrid->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* cgsizer = new wxBoxSizer(wxVERTICAL);
	certgrid->SetSizer(cgsizer);

	cert_tree = new CertTree(certgrid, tc_CertTree, wxDefaultPosition,
		wxDefaultSize, wxTR_DEFAULT_STYLE | wxBORDER_NONE); //wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);

	cert_tree->SetBackgroundColour(wxColour(255, 255, 255));
	cert_tree->Build(CERTLIST_FLAGS);
	CertTreeReset();
	cgsizer->Add(cert_tree, 1, wxEXPAND);

	cert_select_label = new wxStaticText(certgrid, -1, wxT("\nSelect a Callsign Certificate to process"));
	cgsizer->Add(cert_select_label, 0, wxALL, 1);

	certsizer->Add(certgrid, 50, wxEXPAND);

	wxStaticLine *certsep =new wxStaticLine(certtab, -1, wxDefaultPosition, wxSize(2, -1), wxLI_VERTICAL);
	certsizer->Add(certsep, 0, wxEXPAND);

	wxPanel* cbuttons = new wxPanel(certtab, -1);
	cbuttons->SetBackgroundColour(wxColour(255, 255, 255));
	certsizer->Add(cbuttons, 50, wxEXPAND, 0);

	wxBoxSizer* cbsizer = new wxBoxSizer(wxVERTICAL);
	cbuttons->SetSizer(cbsizer);

	wxPanel* cb1Panel = new wxPanel(cbuttons);
	cb1Panel->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* cb1sizer = new wxBoxSizer(wxHORIZONTAL);
	cb1Panel->SetSizer(cb1sizer);
	
	cert_load_button = new wxBitmapButton(cb1Panel, tc_Load, importbm);
	cert_load_button->SetBitmapDisabled(delete_disbm);
	cb1sizer->Add(cert_load_button, 0, wxALL, 1);
	cert_load_label = new wxStaticText(cb1Panel, -1, wxT("\nLoad a Callsign Certificate"), wxDefaultPosition, wxSize(tw, th));
	cb1sizer->Add(cert_load_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	cbsizer->Add(cb1Panel, 1, wxALL, 1);

	wxPanel* cb2Panel = new wxPanel(cbuttons);
	cb2Panel->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* cb2sizer = new wxBoxSizer(wxHORIZONTAL);
	cb2Panel->SetSizer(cb2sizer);
	
	cert_save_button = new wxBitmapButton(cb2Panel, tc_CertSave, downloadbm);
	cert_save_button->SetBitmapDisabled(download_disbm);
	cert_save_button->Enable(false);
	cb2sizer->Add(cert_save_button, 0, wxALL, 1);
	cert_save_label = new wxStaticText(cb2Panel, -1, wxT("\nSave a Callsign Certificate"), wxDefaultPosition, wxSize(tw, th));
	cb2sizer->Add(cert_save_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	cbsizer->Add(cb2Panel, 1, wxALL, 1);

	wxPanel* cb3Panel = new wxPanel(cbuttons);
	cb3Panel->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* cb3sizer = new wxBoxSizer(wxHORIZONTAL);
	cb3Panel->SetSizer(cb3sizer);
	
	cert_renew_button = new wxBitmapButton(cb3Panel, tc_CertRenew, uploadbm);
	cert_renew_button->SetBitmapDisabled(upload_disbm);
	cert_renew_button->Enable(false);
	cb3sizer->Add(cert_renew_button, 0, wxALL, 1);
	cert_renew_label = new wxStaticText(cb3Panel, -1, wxT("\nRenew a Callsign Certificate"), wxDefaultPosition, wxSize(tw, th));
	cb3sizer->Add(cert_renew_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	cbsizer->Add(cb3Panel, 1, wxALL, 1);

	wxPanel* cb4Panel = new wxPanel(cbuttons);
	cb4Panel->SetBackgroundColour(wxColour(255, 255, 255));
	wxBoxSizer* cb4sizer = new wxBoxSizer(wxHORIZONTAL);
	cb4Panel->SetSizer(cb4sizer);
	
	cert_prop_button = new wxBitmapButton(cb4Panel, tc_CertProp, propertiesbm);
	cert_prop_button->SetBitmapDisabled(properties_disbm);
	cert_prop_button->Enable(false);
	cb4sizer->Add(cert_prop_button, 0, wxALL, 1);
	cert_prop_label = new wxStaticText(cb4Panel, -1, wxT("\nDisplay a Callsign Certificate's Properties"), wxDefaultPosition, wxSize(tw, th));
	cb4sizer->Add(cert_prop_label, 1, wxFIXED_MINSIZE | wxALL, 1);
	cbsizer->Add(cb4Panel, 1, wxALL, 1);

	notebook->AddPage(certtab, wxT("Callsign Certificates"));

	//app icon
	SetIcon(wxIcon(key_xpm));

	if (checkUpdates) {
		LogList *log = new LogList(this);
		wxLog::SetActiveTarget(log);
                // Start the autoupdate check 2 seconds after the page shows up
		_timer = new wxTimer(this, tm_f_autoupdate);
		_timer->Start(2000, true);
	}
}

static wxString
run_station_wizard(wxWindow *parent, tQSL_Location loc, wxHtmlHelpController *help = 0,
	bool expired = false, wxString title = wxT("Add Station Location"), wxString dataname = wxT(""), wxString callsign = wxT("")) {
	tqslTrace("run_station_wizard", "loc=%lx, expired=%d, title=%s, dataname=%s, callsign=%s", loc, expired, _S(title), _S(dataname), _S(callsign));
	wxString rval(wxT(""));
	get_certlist("", 0, expired, false);
	if (ncerts == 0)
		throw TQSLException("No certificates available");
	TQSLWizard *wiz = new TQSLWizard(loc, parent, help, title, expired);
	wiz->SetDefaultCallsign(callsign);
	wiz->GetPage(true);
	TQSLWizCertPage *cpage = (TQSLWizCertPage*) wiz->GetPage();
	if (cpage && !callsign.IsEmpty())
		cpage->UpdateFields(0);
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
	help->Display(wxT("main.htm"));
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
		msg += wxString::Format(wxT("\nConfiguration data V%d.%d\n\n"), major, minor);
	msg += wxVERSION_STRING;
#ifdef wxUSE_UNICODE
	if (wxUSE_UNICODE)
		msg += wxT(" (Unicode)");
#endif
        msg+=wxString::Format(wxT("\nlibcurl V%hs\n"), LIBCURL_VERSION);
        msg+=wxString::Format(wxT("%hs\n"), OPENSSL_VERSION_TEXT);
        msg+=wxString::Format(wxT("zlib V%hs\n"), ZLIB_VERSION);
	msg+=wxString::Format(wxT("%hs"), DB_VERSION_STRING);
	return msg;
}

void
MyFrame::OnHelpAbout(wxCommandEvent& WXUNUSED(event)) {
	wxMessageBox(getAbout(), wxT("About"), wxOK|wxCENTRE|wxICON_INFORMATION, this);
}

void
MyFrame::OnHelpDiagnose(wxCommandEvent& event) {
	wxString s_fname;

	if (diagFile) {
		file_menu->Check(tm_f_diag, false);
		fclose(diagFile);
		diagFile = NULL;
		wxMessageBox(wxT("Diagnostic log closed"), wxT("Diagnostics"), wxOK|wxCENTRE|wxICON_INFORMATION, this);
		return;
	}
	s_fname = wxFileSelector(wxT("Log File"), wxT(""), wxT("tqsldiag.log"), wxT("log"),
			wxT("Log files (*.log)|*.log|All files (*.*)|*.*"),
			wxSAVE|wxOVERWRITE_PROMPT, this);
	if (s_fname == wxT("")) {
		file_menu->Check(tm_f_diag, false); //would be better to not check at all, but no, apparently that's a crazy thing to want
		return;
	}
	diagFile = fopen(s_fname.mb_str(), "wb");
	if (!diagFile) {
		wxString errmsg = wxString::Format(wxT("Error opening diagnostic log %s: %hs"), s_fname.c_str(), strerror(errno));
		wxMessageBox(errmsg, wxT("Log File Error"), wxOK|wxICON_EXCLAMATION);
		return;
	}
	file_menu->Check(tm_f_diag, true);
	wxString about = getAbout();
	fprintf(diagFile, "TQSL Diagnostics\n%s\n\n", (const char *)about.mb_str());
	fprintf(diagFile, "Command Line: %s\n", (const char *)origCommandLine.mb_str());
	fprintf(diagFile, "Working Directory:%s\n", tQSL_BaseDir);
}

static void
AddEditStationLocation(tQSL_Location loc, bool expired = false, const wxString& title = wxT("Add Station Location"), const wxString& callsign = wxT("")) {
	tqslTrace("AddEditStationLocation", "loc=%lx, expired=%lx, title=%s, callsign=%s", loc, expired, _S(title), _S(callsign));
	try {
		run_station_wizard(frame, loc, frame->help, expired, title, wxT(""), callsign);
		frame->loc_tree->Build();
	}
	catch (TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
}

void
MyFrame::AddStationLocation(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::AddStationLocation");
	wxTreeItemId root = loc_tree->GetRootItem();
	wxTreeItemId id = loc_tree->GetSelection();
	wxString call;
	if (id != root && id.IsOk()) {		// If something selected
		LocTreeItemData *data = (LocTreeItemData *)loc_tree->GetItemData(id);
		if (data) {
			call = data->getCallSign();		// Station location
		} else {
			call = loc_tree->GetItemText(id);	// Callsign folder selected
		}
		tqslTrace("MyFrame::AddStationLocation", "Call selected is %s", _S(call));
	}
	tQSL_Location loc;
	if (tqsl_initStationLocationCapture(&loc)) {
		wxLogError(wxT("%hs"), tqsl_getErrorString());
	}
	AddEditStationLocation(loc, false, wxT("Add Station Location"), call);
	if (tqsl_endStationLocationCapture(&loc)) {
		wxLogError(wxT("%hs"), tqsl_getErrorString());
	}
	loc_tree->Build();
	LocTreeReset();
}

void
MyFrame::EditStationLocation(wxCommandEvent& event) {
	tqslTrace("MyFrame::EditStationLocation");
	if (event.GetId() == tl_EditLoc) {
		LocTreeItemData *data = (LocTreeItemData *)loc_tree->GetItemData(loc_tree->GetSelection());
		tQSL_Location loc;
		wxString selname;
		char errbuf[512];

		if (data == NULL) return;

		check_tqsl_error(tqsl_getStationLocation(&loc, data->getLocname().mb_str()));
		check_tqsl_error(tqsl_getStationLocationErrors(loc, errbuf, sizeof(errbuf)));
		if (strlen(errbuf) > 0) {
			wxMessageBox(wxString::Format(wxT("%hs\nThe invalid data was ignored."), errbuf), wxT("Location data error"), wxOK|wxICON_EXCLAMATION, this);
		}
		char loccall[512];
		check_tqsl_error(tqsl_getLocationCallSign(loc, loccall, sizeof loccall));
		selname = run_station_wizard(this, loc, help, true, wxString::Format(wxT("Edit Station Location : %hs - %s"), loccall, data->getLocname().c_str()), data->getLocname());
		check_tqsl_error(tqsl_endStationLocationCapture(&loc));
		loc_tree->Build();
		LocTreeReset();
		return;
	}
	try {
		SelectStationLocation(wxT("Edit Station Location"), wxT("Close"), true);
		loc_tree->Build();
		LocTreeReset();
	}
	catch (TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
}

void
MyFrame::WriteQSOFile(QSORecordList& recs, const char *fname, bool force) {
	tqslTrace("MyFrame::writeQSOFile", "fname=%s, force=%d", fname, force);
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
	tqslTrace("MyFrame::EditQSOData");
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
		QSODataDialog dial(this, help, &recs);
		if (dial.ShowModal() == wxID_OK)
			WriteQSOFile(recs, file.mb_str());
	} catch (TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
}

void
MyFrame::EnterQSOData(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::EnterQSOData");
	QSORecordList recs;
	try {
		QSODataDialog dial(this, help, &recs);
		if (dial.ShowModal() == wxID_OK)
			WriteQSOFile(recs);
	} catch (TQSLException& x) {
		wxLogError(wxT("%hs"), x.what());
	}
}

int MyFrame::ConvertLogToString(tQSL_Location loc, wxString& infile, wxString& output, int& n, tQSL_Converter& conv, bool suppressdate, tQSL_Date* startdate, tQSL_Date* enddate, int action, const char* password) {
	tqslTrace("MyFrame::ConvertLogToString", "loc = %lx, infile=%s, output=%s n=%d conv=%lx, suppressdate=%d, startdate=0x%lx, enddate=0x%lx, action=%d",(void*)loc, _S(infile), _S(output), (void *)conv, suppressdate, (void *)startdate, (void *)enddate, action);
	static const char *iam = "TQSL V" VERSION;
	const char *cp;
	char callsign[40];
	int dxcc;
	wxString name, ext;
	bool allow_dupes = false;
	bool restarting = false;
	bool ignore_err = false;

	wxConfig *config = (wxConfig *)wxConfig::Get();

	check_tqsl_error(tqsl_getLocationCallSign(loc, callsign, sizeof callsign));
	check_tqsl_error(tqsl_getLocationDXCCEntity(loc, &dxcc));
	DXCC dx;
	dx.getByEntity(dxcc);

	get_certlist(callsign, dxcc, false, false);
	if (ncerts == 0) {
		wxString msg = wxString::Format(wxT("There are no valid callsign certificates for callsign %hs in entity %hs.\nSigning aborted.\n"), callsign, dx.name());
		throw TQSLException(msg.mb_str());
		return TQSL_EXIT_TQSL_ERROR;
	}

	wxLogMessage(wxT("Signing using Callsign %hs, DXCC Entity %hs"), callsign, dx.name());

	init_modes();
	init_contests();

restart:

	ConvertingDialog *conv_dial = new ConvertingDialog(this, infile.mb_str());
	n=0;
	bool cancelled = false;
	bool aborted = false;
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
		if (range && !suppressdate && !restarting) {
			DateRangeDialog dial(this);
			if (dial.ShowModal() != wxOK) {
				wxLogMessage(wxT("Cancelled"));
				return TQSL_EXIT_CANCEL;
			}
			tqsl_setADIFConverterDateFilter(conv, &dial.start, &dial.end);
			if (this->IsQuiet()) {
				this->Show(false);
				wxSafeYield(this);
			}
		}
		if (startdate || enddate) {
			tqslTrace("MyFrame::ConvertLogToString", "startdate %d/%d/%d, enddate %d/%d/%d",
					startdate ? startdate->year : 0,
					startdate ? startdate->month : 0,
					startdate ? startdate->day : 0,
					enddate ? enddate->year : 0,
					enddate ? enddate->month : 0,
					enddate ? enddate->day : 0);
			tqsl_setADIFConverterDateFilter(conv, startdate, enddate);
		}	
		bool allow = false;
		config->Read(wxT("BadCalls"), &allow);
		tqsl_setConverterAllowBadCall(conv, allow);
		tqsl_setConverterAllowDuplicates(conv, allow_dupes);
		tqsl_setConverterAppName(conv, iam);

		wxSplitPath(infile, 0, &name, &ext);
		if (ext != wxT(""))
			name += wxT(".") + ext;
		// Only display windows if notin batch mode -- KD6PAG
		if (!this->IsQuiet()) {
			conv_dial->Show(TRUE);
		}
		this->Enable(FALSE);

		output = wxT("");

   		do {
   	   		while ((cp = tqsl_getConverterGABBI(conv)) != 0) {
				if (!this->IsQuiet())
  					wxSafeYield(conv_dial);
  				if (!conv_dial->running)
  					break;
				// Only count QSO records
				if (strstr(cp, "tCONTACT")) {
					++n;
					++processed;
				}
				if ((processed % 10) == 0) {
					wxString progress = wxString::Format(wxT("QSOs: %d"), processed);
					if (duplicates > 0) 
						progress += wxString::Format(wxT(" Duplicates: %d"), duplicates);
					if (errors > 0 || out_of_range > 0) 
						progress += wxString::Format(wxT(" Errors: %d"), errors + out_of_range);
		   	   		conv_dial->msg->SetLabel(progress);
				}
				output<<(wxString(cp, wxConvLocal)+wxT("\n"));
   			}
			if ((processed % 10) == 0) {
				wxString progress = wxString::Format(wxT("QSOs: %d"), processed);
				if (duplicates > 0) 
					progress += wxString::Format(wxT(" Duplicates: %d"), duplicates);
				if (errors > 0 || out_of_range > 0) 
					progress += wxString::Format(wxT(" Errors: %d"), errors + out_of_range);
   	   			conv_dial->msg->SetLabel(progress);
			}
			if (cp == 0) {
				if (!this->IsQuiet())
					wxSafeYield(conv_dial);
				if (!conv_dial->running)
					break;
			}
   			if (tQSL_Error == TQSL_SIGNINIT_ERROR) {
   				tQSL_Cert cert;
				int rval;
   				check_tqsl_error(tqsl_getConverterCert(conv, &cert));
				do {
	   				if ((rval = tqsl_beginSigning(cert, (char *)password, getCertPassword, cert)) == 0)
						break;
					if (tQSL_Error == TQSL_PASSWORD_ERROR) {
						wxLogMessage(wxT("Password error"));
						if (password)
							free((void *)password);
						password = NULL;
					}
				} while (tQSL_Error == TQSL_PASSWORD_ERROR);
   				check_tqsl_error(rval);
				if (this->IsQuiet()) {
					this->Show(false);
					wxSafeYield(this);
				}
   				continue;
   			}
			if (tQSL_Error == TQSL_DATE_OUT_OF_RANGE) {
				processed++;
				out_of_range++;
				continue;
			}
			if (tQSL_Error == TQSL_CERT_DATE_MISMATCH) {
				processed++;
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

					wxLogError(wxT("%s"), msg.c_str());
					if (frame->IsQuiet()) { 
						switch (action) {
						 	case TQSL_ACTION_ABORT:
								aborted = true;
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
					if (!ignore_err) {
						tqslTrace("MyFrame::ConvertLogToString", "Error: %s/asking for action", _S(msg));
						ErrorsDialog dial(this, msg + wxT("\nClick 'Ignore' to continue signing this log while ignoring errors.\n")
								wxT("Click 'Cancel' to abandon signing this log."));
						int choice = dial.ShowModal();
						if (choice == TQSL_AE_CAN) {
							cancelled = true;
							goto abortSigning;
						}
						if (choice == TQSL_AE_OK) {
							ignore_err = true;
							if (this->IsQuiet()) {
								this->Show(false);
								wxSafeYield(this);
							}
						}
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
		this->Enable(TRUE);

   		if (cancelled) {
   			wxLogWarning(wxT("Signing cancelled"));
			n = 0;
   		} else if (aborted) {
   			wxLogWarning(wxT("Signing aborted"));
			n = 0;
   		} else if (tQSL_Error != TQSL_NO_ERROR) {
   			check_tqsl_error(1);
		}
   		delete conv_dial;
   	} catch (TQSLException& x) {

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
	if (!cancelled && out_of_range > 0)
		wxLogMessage(wxT("%s: %d QSO records were outside the selected date range"),
			infile.c_str(), out_of_range);
	if (duplicates > 0) {
		if (cancelled) {
			tqsl_converterRollBack(conv);
			tqsl_endConverter(&conv);
			return TQSL_EXIT_CANCEL;
		}
		if (action == TQSL_ACTION_ASK || action==TQSL_ACTION_UNSPEC) { // want to ask the user
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
				if (this->IsQuiet()) {
					this->Show(false);
					wxSafeYield(this);
				}
				goto restart;
			}
		} else if (action == TQSL_ACTION_ABORT) {
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
		wxLogMessage(wxT("%s: %d QSO records were duplicates"),
			infile.c_str(), duplicates);
	}
	//if (!cancelled) tqsl_converterCommit(conv);
	if (cancelled || processed == 0) { 
		tqsl_converterRollBack(conv);
		tqsl_endConverter(&conv);
	}
	if (cancelled)
		return TQSL_EXIT_CANCEL;
	if (processed == 0)
		return TQSL_EXIT_NO_QSOS;
	if (aborted || duplicates > 0 || out_of_range > 0 || errors > 0)
		return TQSL_EXIT_QSOS_SUPPRESSED;
	return TQSL_EXIT_SUCCESS;
}

int
MyFrame::ConvertLogFile(tQSL_Location loc, wxString& infile, wxString& outfile,
	bool compressed, bool suppressdate, tQSL_Date* startdate, tQSL_Date* enddate, int action, const char *password) {
	tqslTrace("MyFrame::ConvertLogFile", "loc=%lx, infile=%s, outfile=%s, compressed=%d, suppressdate=%d, startdate=0x%lx enddate=0x%lx action=%d", (void *)loc, _S(infile), _S(outfile), compressed, suppressdate, (void *)startdate, (void*) enddate, action);	

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
	int status = this->ConvertLogToString(loc, infile, output, numrecs, conv, suppressdate, startdate, enddate, action, password);

	if (numrecs == 0) {
		wxLogMessage(wxT("No records output"));
		if (compressed) {
			gzclose(gout);
		} else {
			out.close();
		}
		unlink(outfile.mb_str());
		if (status == TQSL_EXIT_CANCEL || TQSL_EXIT_QSOS_SUPPRESSED)
			return status;
		else
			return TQSL_EXIT_NO_QSOS;
	} else {
		if(compressed) { 
			if (0>=gzwrite(gout, output.mb_str(), output.size()) || Z_OK!=gzclose(gout)) {
				tqsl_converterRollBack(conv);
				tqsl_endConverter(&conv);
				return TQSL_EXIT_LIB_ERROR;
			}
		} else {
			out<<output;
			if (out.fail()) {
				tqsl_converterRollBack(conv);
				tqsl_endConverter(&conv);
				return TQSL_EXIT_LIB_ERROR;
			}
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

class ConfigFileDownloadHandler {
public:
	void *s;
	size_t allocated;
	size_t used;
	ConfigFileDownloadHandler(size_t _size): s() {
		allocated = _size;
		s = malloc(_size);
		used = 0;
	}
	~ConfigFileDownloadHandler(void);
	size_t internal_recv( char *ptr, size_t size, size_t nmemb) {
		size_t newlen = used + (size * nmemb);
		if (newlen > allocated) {
			s = realloc(s, newlen + 2000);
			allocated += newlen + 2000;
		}
		memcpy((char *)s+used, ptr, size * nmemb);
		used += size*nmemb;
		return size*nmemb;
	}

	static size_t recv( char *ptr, size_t size, size_t nmemb, void *userdata) { 
		return ((ConfigFileDownloadHandler*)userdata)->internal_recv(ptr, size, nmemb);
	}
};

ConfigFileDownloadHandler::~ConfigFileDownloadHandler(void) {
	if (s) free(s);
}

long compressToBuf(string& buf, const char* input) {
	tqslTrace("compressToBuf");
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

int MyFrame::UploadLogFile(tQSL_Location loc, wxString& infile, bool compressed, bool suppressdate, tQSL_Date* startdate, tQSL_Date* enddate, int action, const char* password) {
	tqslTrace("MyFrame::UploadLogFile", "loc=%lx, infile=%s, compressed=%d, suppressdate=%d, startdate=0x%lx, enddate=0x%lx action=%d", (void *)loc, _S(infile), compressed, suppressdate, (void *)startdate, (void *)enddate, action);
	int numrecs=0;
	wxString signedOutput;
	tQSL_Converter conv=0;

	int status = this->ConvertLogToString(loc, infile, signedOutput, numrecs, conv, suppressdate, startdate, enddate, action, password);

	if (numrecs == 0) {
		wxLogMessage(wxT("No records to upload"));
		if (status == TQSL_EXIT_CANCEL || TQSL_EXIT_QSOS_SUPPRESSED)
			return status;
		else
			return TQSL_EXIT_NO_QSOS;
	} else {
		//compress the upload
		string compressed;
		size_t compressedSize=compressToBuf(compressed, (const char*)signedOutput.mb_str());
		//ofstream f; f.open("testzip.tq8", ios::binary); f<<compressed; f.close(); //test of compression routine
		if (compressedSize<0) { 
			wxLogMessage(wxT("Error compressing before upload")); 
			return TQSL_EXIT_TQSL_ERROR;
		}

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
			name.c_str()).mb_str(), sizeof filename);
		filename[sizeof filename - 1] = '\0';

		wxString fileType(wxT("Log"));
		int retval = UploadFile(infile, filename, numrecs, (void *)compressed.c_str(), compressedSize, fileType);

		if (retval==0)
			tqsl_converterCommit(conv);
		else
			tqsl_converterRollBack(conv);

		tqsl_endConverter(&conv);

		return retval;
	}
}

static CURL*
tqsl_curl_init(const char *logTitle, const char *url, FILE **logFile, bool newFile) {

	CURL* req=curl_easy_init();
	if (!req)
		return NULL;

	if (diagFile) {
		curl_easy_setopt(req, CURLOPT_VERBOSE, 1);
		curl_easy_setopt(req, CURLOPT_STDERR, diagFile);
		fprintf(diagFile, "%s\n", logTitle);
	} else {
		if (logFile) {
			wxString filename;
#ifdef _WIN32
			filename.Printf(wxT("%hs\\curl.log"), tQSL_BaseDir);
#else
			filename.Printf(wxT("%hs/curl.log"), tQSL_BaseDir);
#endif
			*logFile = fopen(filename.mb_str(), newFile ? "wb" : "ab");
			if (*logFile) {
				curl_easy_setopt(req, CURLOPT_VERBOSE, 1);
				curl_easy_setopt(req, CURLOPT_STDERR, *logFile);
				fprintf(*logFile, "%s:\n", logTitle);
			}
		}
	}
	//set up options
	curl_easy_setopt(req, CURLOPT_URL, url);

	// Get the proxy configuration and pass it to cURL
        wxConfig *config = (wxConfig *)wxConfig::Get();
        config->SetPath(wxT("/Proxy"));

	bool enabled;
        config->Read(wxT("proxyEnabled"), &enabled, false);
	wxString pHost = config->Read(wxT("proxyHost"), wxT(""));
	wxString pPort = config->Read(wxT("proxyPort"), wxT(""));
	wxString pType = config->Read(wxT("proxyType"), wxT(""));
	config->SetPath(wxT("/"));

	if (!enabled) return req;	// No proxy defined

	long port = strtol(pPort.mb_str(), NULL, 10);
	if (port == 0 || pHost.IsEmpty())
		return req;		// Invalid proxy. Ignore it.

	curl_easy_setopt(req, CURLOPT_PROXY, (const char *)pHost.mb_str());
	curl_easy_setopt(req, CURLOPT_PROXYPORT, port);
	if (pType == wxT("HTTP")) {
		curl_easy_setopt(req, CURLOPT_PROXYTYPE, CURLPROXY_HTTP);	// Default is HTTP
	} else if (pType == wxT("Socks4")) {
		curl_easy_setopt(req, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS4);
	} else if (pType == wxT("Socks5")) {
		curl_easy_setopt(req, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
	}
	return req;
}

int MyFrame::UploadFile(wxString& infile, const char* filename, int numrecs, void *content, size_t clen, wxString& fileType) {
	tqslTrace("MyFrame::UploadFile", "infile=%s, filename=%s, numrecs=%d, content=0x%lx, clen=%d fileType=%s",  _S(infile), filename, numrecs, (void *)content, clen, _S(fileType));

	FILE *logFile = NULL; 
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
	bool uplVerifyCA;
	config->Read(wxT("VerifyCA"), &uplVerifyCA, DEFAULT_UPL_VERIFYCA);
	config->SetPath(wxT("/"));

	// Copy the strings so they remain around
	char *urlstr = strdup(uploadURL.mb_str());
	char *cpUF = strdup(uploadField.mb_str());

retry_upload:

	CURL* req=tqsl_curl_init("Upload Log", urlstr, &logFile, true);

	if (!req) {
		wxLogMessage(wxT("Error: Could not upload file (CURL Init error)"));
		free(urlstr);
		free(cpUF);
		return TQSL_EXIT_TQSL_ERROR; 
	}

	//the following allow us to write our log and read the result

	FileUploadHandler handler;

	curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
	curl_easy_setopt(req, CURLOPT_WRITEDATA, &handler);
	curl_easy_setopt(req, CURLOPT_SSL_VERIFYPEER, uplVerifyCA);

	char errorbuf[CURL_ERROR_SIZE];
	curl_easy_setopt(req, CURLOPT_ERRORBUFFER, errorbuf);

	struct curl_httppost* post=NULL, *lastitem=NULL;

	curl_formadd(&post, &lastitem,
		CURLFORM_PTRNAME, cpUF,
		CURLFORM_BUFFER, filename,
		CURLFORM_BUFFERPTR, content,
		CURLFORM_BUFFERLENGTH, clen,
		CURLFORM_END);

	curl_easy_setopt(req, CURLOPT_HTTPPOST, post);

	intptr_t retval;

	UploadDialog* upload;

	if (numrecs > 0)
		wxLogMessage(wxT("Attempting to upload %d QSO%hs"), numrecs, numrecs == 1 ? "" : "s");
	else
		wxLogMessage(wxT("Attempting to upload %s"), fileType.c_str());

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
			if (logFile) fclose(logFile);
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
					wxString lotwmessage = uplMessageRE.GetMatch(uplresult, 1).Trim();
					if (fileType == wxT("Log")) {
						wxLogMessage(wxT("%s: Log uploaded successfully with result \"%s\"!\nAfter reading this message, you may close this program."), 
							infile.c_str(), lotwmessage.c_str());

					} else {
						wxLogMessage(wxT("%s uploaded with result \"%s\"!"), 
							fileType.c_str(), lotwmessage.c_str());
					}
				} else { // no message we could find
					if (fileType == wxT("Log")) {
						wxLogMessage(wxT("%s: Log uploaded successfully!\nAfter reading this message, you may close this program."), infile.c_str());
					} else {
						wxLogMessage(wxT("%s uploaded successfully!"), fileType.c_str());
					}
				}

				retval=TQSL_EXIT_SUCCESS;
			} else { // failure, but site is working

				if (uplMessageRE.Matches(uplresult)) { //and a message
					wxLogMessage(wxT("%s: %s upload was rejected with result \"%s\""), 
						infile.c_str(), fileType.c_str(), uplMessageRE.GetMatch(uplresult, 1).c_str());

				} else { // no message we could find
					wxLogMessage(wxT("%s: %s upload was rejected!"), infile.c_str(), fileType.c_str());
				}

				retval=TQSL_EXIT_REJECTED;
			}
		} else { //site isn't working
			wxLogMessage(wxT("%s: Got an unexpected response on %s upload! Maybe the site is down?"), infile.c_str(), fileType.c_str());
			retval=TQSL_EXIT_UNEXP_RESP;
		}

	} else {
		if (diagFile) {
			fprintf(diagFile, "cURL Error: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		}
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			wxLogMessage(wxT("%s: Unable to upload - either your Internet connection is down or LoTW is unreachable.\nPlease try uploading the %s later."), infile.c_str(), fileType.c_str());
			retval=TQSL_EXIT_CONNECTION_FAILED;
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			wxLogMessage(wxT("%s: Unable to upload. The nework is down or the LoTW site is too busy.\nPlease try uploading the %s later."), infile.c_str(), fileType.c_str());
			retval=TQSL_EXIT_CONNECTION_FAILED;
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			wxLogMessage(wxT("%s: Unable to connect to the upload site.\nPlease try uploading the %s later."), infile.c_str(), fileType.c_str());
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

	if (urlstr) free(urlstr);
	if (cpUF) free (cpUF);
	if (logFile) fclose(logFile);
	return retval;
}

// Verify that a certificate exists for this station location
// before allowing the location to be edited
static bool verify_cert(tQSL_Location loc) {
	tqslTrace("verify_cert", "loc=%lx", (void *)loc);
	char call[128];
	tQSL_Cert *certlist;
	int ncerts;
	// Get the callsign from the location
	check_tqsl_error(tqsl_getLocationCallSign(loc, call, sizeof(call)));
	// See if there is a certificate for that call
	tqsl_selectCertificates(&certlist, &ncerts, call, 0, 0, 0, TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED);	
	if (ncerts == 0) {
		wxMessageBox(wxString::Format(wxT("There are no callsign certificates for callsign %hs. This station location cannot be edited."), call), wxT("No Certificate"), wxOK|wxICON_EXCLAMATION);
		return false;
	}
	for (int i = 0; i  < ncerts; i++)
		tqsl_freeCertificate(certlist[i]);
	return true;
}

tQSL_Location
MyFrame::SelectStationLocation(const wxString& title, const wxString& okLabel, bool editonly) {
	tqslTrace("MyFrame::SelectStationLocation", "title=%s, okLabel=%s, editonly=%d", _S(title), _S(okLabel), editonly);
   	int rval;
   	tQSL_Location loc;
   	wxString selname;
	char errbuf[512];
   	do {
   		TQSLGetStationNameDialog station_dial(this, help, wxDefaultPosition, false, title, okLabel, editonly);
   		if (selname != wxT(""))
   			station_dial.SelectName(selname);
   		rval = station_dial.ShowModal();
   		switch (rval) {
   			case wxID_CANCEL:	// User hit Close
   				return 0;
   			case wxID_APPLY:	// User hit New
   				check_tqsl_error(tqsl_initStationLocationCapture(&loc));
   				selname = run_station_wizard(this, loc, help, false);
   				check_tqsl_error(tqsl_endStationLocationCapture(&loc));
   				break;
   			case wxID_MORE:		// User hit Edit
   		   		check_tqsl_error(tqsl_getStationLocation(&loc, station_dial.Selected().mb_str()));
				if (verify_cert(loc)) {		// Check if there is a certificate before editing
					check_tqsl_error(tqsl_getStationLocationErrors(loc, errbuf, sizeof(errbuf)));
					if (strlen(errbuf) > 0) {
						wxMessageBox(wxString::Format(wxT("%hs\nThe invalid data was ignored."), errbuf), wxT("Station Location data error"), wxOK|wxICON_EXCLAMATION, this);
					}
					char loccall[512];
					check_tqsl_error(tqsl_getLocationCallSign(loc, loccall, sizeof loccall));
					selname = run_station_wizard(this, loc, help, true, wxString::Format(wxT("Edit Station Location : %hs - %s"), loccall, station_dial.Selected().c_str()), station_dial.Selected());
   					check_tqsl_error(tqsl_endStationLocationCapture(&loc));
				}
   				break;
   			case wxID_OK:		// User hit OK
   		   		check_tqsl_error(tqsl_getStationLocation(&loc, station_dial.Selected().mb_str()));
				check_tqsl_error(tqsl_getStationLocationErrors(loc, errbuf, sizeof(errbuf)));
				if (strlen(errbuf) > 0) {
					wxMessageBox(wxString::Format(wxT("%hs\nThis should be corrected before signing a log file."), errbuf), wxT("Station Location data error"), wxOK|wxICON_EXCLAMATION, this);
				}
   				break;
   		}
   	} while (rval != wxID_OK);
	return loc;
}

void MyFrame::CheckForUpdates(wxCommandEvent&) {
	tqslTrace("MyFrame::CheckForUpdates");
	DoCheckForUpdates(false);
}

wxString GetUpdatePlatformString() {
	tqslTrace("GetUpdatePlatformString");
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

// Class for encapsulating version information
class revLevel
{
public:
	revLevel(long _major = 0, long _minor = 0, long _patch = 0) {
		major = _major;
		minor = _minor;
		patch = _patch;
	}
        revLevel(const wxString& _value) {
		wxString str = _value;
		str.Trim(true);
		str.Trim(false);
		wxStringTokenizer vers(str, wxT("."));
		wxString majorVer = vers.GetNextToken();
		wxString minorVer = vers.GetNextToken();	
		wxString patchVer = vers.GetNextToken();	
		if (majorVer.IsNumber()) {
			majorVer.ToLong(&major);
		} else {
			major = -1;
		}
		if (minorVer.IsNumber()) {
			minorVer.ToLong(&minor);
		} else {
			minor = -1;
		}
		if (patchVer.IsNumber()) {
			patchVer.ToLong(&patch);
		} else {
			patch = 0;
		}
        }
	wxString Value(void) {
		if (patch > 0) 
			return wxString::Format(wxT("%d.%d.%d"), major, minor, patch);
		else
			return wxString::Format(wxT("%d.%d"), major, minor);
	}
        long major;
	long minor;
	long patch;
        bool operator >(const revLevel& other) {
		if (major > other.major) return true;
		if (major == other.major) {
			if (minor > other.minor) return true;
			if (minor == other.minor) {
				if (patch > other.patch) return true;
			}
		}
		return false;
	}
};

class UpdateDialogMsgBox: public wxDialog
 {
public:
	UpdateDialogMsgBox(wxWindow* parent, bool newProg, bool newConfig, revLevel* currentProgRev, revLevel* newProgRev,
			revLevel* currentConfigRev, revLevel* newConfigRev, wxString platformURL, wxString homepage) :
			wxDialog(parent, (wxWindowID)wxID_ANY, wxT("TQSL Update Available"), wxDefaultPosition, wxDefaultSize)
	{
		tqslTrace("UpdateDialogMsgBox::UpdateDialogMsgBox", "parent=%lx, newProg=%d, newConfig=%d, currentProgRev %s, newProgRev %s, newConfigRev %s, currentConfigRev=%s, platformURL=%s, homepage=%s", (void*)parent, newProg, newConfig, _S(currentProgRev->Value()), _S(newProgRev->Value()), _S(currentConfigRev->Value()), _S(newConfigRev->Value()), _S(platformURL), _S(homepage));
		wxSizer* overall=new wxBoxSizer(wxVERTICAL);
		long flags = wxOK;
		if (newConfig)
			flags |= wxCANCEL;

		wxSizer* buttons=CreateButtonSizer(flags);
		wxString notice;
		if (newProg)
			notice = wxString::Format(wxT("A new TQSL release (V%s) is available!"), newProgRev->Value().c_str());
		else if (newConfig)
			notice = wxString::Format(wxT("A new TrustedQSL configuration file (V%s) is available!"), newConfigRev->Value().c_str());

		overall->Add(new wxStaticText(this, wxID_ANY, notice), 0, wxALIGN_CENTER_HORIZONTAL);
		
		if (newProg) {
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
		}
		if (newConfig) {
			overall->AddSpacer(10);
			overall->Add(new wxStaticText(this, wxID_ANY, wxT("Click 'OK' to install the new configuration file, or Cancel to ignore it.")));
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

void MyFrame::UpdateConfigFile() {
	tqslTrace("MyFrame::UpdateConfigFile()");
	wxConfig* config=(wxConfig*)wxConfig::Get();
	wxString newConfigURL = config->Read(wxT("NewConfigURL"), DEFAULT_CONFIG_FILE_URL);
	FILE *logFile = NULL; 
	CURL* req=tqsl_curl_init("Config File Download Log", (const char *)newConfigURL.mb_str(), &logFile, false);

	ConfigFileDownloadHandler handler(40000);

	curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, &ConfigFileDownloadHandler::recv);
	curl_easy_setopt(req, CURLOPT_WRITEDATA, &handler);

	curl_easy_setopt(req, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	curl_easy_setopt(req, CURLOPT_ERRORBUFFER, errorbuf);
	int retval = curl_easy_perform(req);
	if (retval == CURLE_OK) {
		char *newconfig = (char *)handler.s;
		wxString filename;
#ifdef _WIN32
		filename.Printf(wxT("%hs\\/config.tq6"), tQSL_BaseDir);
#else
		filename.Printf(wxT("%hs/config.tq6"), tQSL_BaseDir);
#endif
		FILE *configFile = fopen(filename.mb_str(), "wb");
		if (!configFile) {
			wxMessageBox(wxString::Format(wxT("Can't open new configuration file %s: %hs"), filename.c_str(), strerror(errno)));
			curl_easy_cleanup(req);
			if (logFile) fclose(logFile);
			return;
		}
		size_t left = handler.used;
		while (left > 0) {
			size_t written = fwrite(newconfig, 1, left, configFile);
			if (written == 0) {
				wxMessageBox(wxString::Format(wxT("Can't write new configuration file %s: %hs"), filename.c_str(), strerror(errno)));
				curl_easy_cleanup(req);
				if (logFile) fclose(logFile);
				return;
			}
			left -= written;
		}
		if (fclose(configFile)) {
			wxMessageBox(wxString::Format(wxT("Error writing new configuration file %s: %hs"), filename.c_str(), strerror(errno)));
			curl_easy_cleanup(req);
			if (logFile) fclose(logFile);
			return;
		}
		notifyData nd;
		if (tqsl_importTQSLFile(filename.mb_str(), notifyImport, &nd)) {
			wxLogError(wxT("%hs"), tqsl_getErrorString());
		} else {
			wxMessageBox(wxT("Configuration file successfully updated"), wxT("Update Completed"), wxOK|wxICON_INFORMATION, this);
		}
	} else {
		if (diagFile) {
			fprintf(diagFile, "cURL Error during config file download: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		}
		if (logFile) {
			fprintf(logFile, "cURL Error during config file download: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		}
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			wxLogMessage(wxT("Unable to update - either your Internet connection is down or LoTW is unreachable.\nPlease try again later."));
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			wxLogMessage(wxT("Unable to update. The nework is down or the LoTW site is too busy.\nPlease try again later."));
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			wxLogMessage(wxT("Unable to connect to the update site.\nPlease try again later."));
		} else { // some other error
			wxMessageBox(wxString::Format(wxT("Error downloading new configuration file:\n%hs"), errorbuf), wxT("Update"), wxOK|wxICON_EXCLAMATION, this);
		}
	}
	curl_easy_cleanup(req);
	if (logFile) fclose(logFile);
}

// Check for certificates expiring in the next nn (default 60) days
void
MyFrame::DoCheckExpiringCerts(bool noGUI) {
	get_certlist("", 0, false, false);
	if (ncerts == 0) return;

	long expireDays = DEFAULT_CERT_WARNING;
	wxConfig::Get()->Read(wxT("CertWarnDays"), &expireDays);
	// Get today's date
	time_t t = time(0);
	struct tm *tm = gmtime(&t);
	tQSL_Date d;
	d.year = tm->tm_year + 1900;
	d.month = tm->tm_mon + 1;
	d.day = tm->tm_mday;
	tQSL_Date exp;

	for (int i = 0; i < ncerts; i ++) {
		if (0 == tqsl_getCertificateNotAfterDate(certlist[i], &exp)) {
			int days_left;
			tqsl_subtractDates(&d, &exp, &days_left);
			if (days_left < expireDays) {
				char callsign[64];
				check_tqsl_error(tqsl_getCertificateCallSign(certlist[i], callsign, sizeof callsign));
				if (noGUI) {
					wxLogMessage(wxT("The certificate for %hs expires in %d days."),
						callsign, days_left);
				} else {
					if (wxMessageBox(
						wxString::Format(wxT("The certificate for %hs expires in %d days\n")
							wxT("Do you want to renew it now?"), callsign, days_left), wxT("Certificate Expiring"), wxYES_NO|wxICON_QUESTION, this) == wxYES) {
							// Select the certificate in the tree
							cert_tree->SelectCert(certlist[i]);
							// Then start the renewal
							wxCommandEvent dummy;
							CRQWizardRenew(dummy);
					}
				}
			}
		}
	}
	free_certlist();
	if (noGUI) {
		wxString pend = wxConfig::Get()->Read(wxT("RequestPending"));
		if (!pend.IsEmpty()){
			if (pend.Find(wxT(",")) != wxNOT_FOUND)
				wxLogMessage(wxT("Callsign Certificate Requests are pending for %s."), pend.c_str());
			else
				wxLogMessage(wxT("A Callsign Certificate Request for %s is pending."), pend.c_str());
		}
	}
	return;
}

void
MyFrame::DoCheckForUpdates(bool silent, bool noGUI) {
	tqslTrace("MyFrame::DoCheckForUpdates", "silent=%d, noGUI=%d", silent, noGUI);
	FILE *logFile = NULL; 
	wxConfig* config=(wxConfig*)wxConfig::Get();

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

	if (!check) return;	//if we really weren't supposed to check, get out of here

	revLevel* programRev = new revLevel(wxT(VERSION));

	int currentConfigMajor, currentConfigMinor;
	tqsl_getConfigVersion(&currentConfigMajor, &currentConfigMinor);
	revLevel* configRev = new revLevel(currentConfigMajor, currentConfigMinor, 0);		// config files don't have patch levels

	wxString updateURL=config->Read(wxT("UpdateURL"), DEFAULT_UPD_URL);

	CURL* req=tqsl_curl_init("Version Check Log", (const char*)updateURL.mb_str(), &logFile, true);

	//the following allow us to analyze our file

	FileUploadHandler handler;

	curl_easy_setopt(req, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
	curl_easy_setopt(req, CURLOPT_WRITEDATA, &handler);

	if(silent) { // if there's a problem, we don't want the program to hang while we're starting it
		curl_easy_setopt(req, CURLOPT_CONNECTTIMEOUT, 10);
	}

	curl_easy_setopt(req, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	curl_easy_setopt(req, CURLOPT_ERRORBUFFER, errorbuf);

	int retval = curl_easy_perform(req);
	if (retval == CURLE_OK) {
		tqslTrace("MyFrame::DoCheckForUpdates", "Program rev returns %d chars, %s", handler.s.size(), handler.s.c_str());
		// Add the config.xml text to the result
		wxString configURL=config->Read(wxT("ConfigFileVerURL"), DEFAULT_UPD_CONFIG_URL);
		curl_easy_setopt(req, CURLOPT_URL, (const char*)configURL.mb_str());

		retval = curl_easy_perform(req);
		if (retval == CURLE_OK) {
			tqslTrace("MyFrame::DoCheckForUpdates", "Prog + Config rev returns %d chars, %s", handler.s.size(), handler.s.c_str());
			wxString result=wxString::FromAscii(handler.s.c_str());
			wxString url;
			WX_DECLARE_STRING_HASH_MAP(wxString, URLHashMap);
			URLHashMap map;
			revLevel *newProgramRev = NULL;
			revLevel *newConfigRev = NULL;
		
			wxStringTokenizer urls(result, wxT("\n"));
			wxString onlinever;
			while(urls.HasMoreTokens()) {
				wxString header=urls.GetNextToken().Trim();
				if (header.StartsWith(wxT("TQSLVERSION;"), &onlinever)) {
					newProgramRev = new revLevel(onlinever);

				} else if (header.IsEmpty()) continue; //blank line
				else if (header[0]=='#') continue; //comments

				else if (header.StartsWith(wxT("config.xml"), &onlinever)) {
					onlinever.Replace(wxT(":"), wxT(""));
					onlinever.Replace(wxT("Version"), wxT(""));
					onlinever.Trim(true);
					onlinever.Trim(false);
					newConfigRev = new revLevel(onlinever);
				} else {
					int sep=header.Find(';'); //; is invalid in URLs
					if (sep==wxNOT_FOUND) continue; //malformed string
					wxString plat=header.Left(sep);
					wxString url=header.Right(header.size()-sep-1);
					map[plat]=url;
				}
			}
			bool newProgram = newProgramRev ? (*newProgramRev > *programRev) : false;
			bool newConfig = newConfigRev ? (*newConfigRev > *configRev) : false;

			if (newProgram) {

				wxString ourPlatURL; //empty by default (we check against this later)

				wxStringTokenizer plats(GetUpdatePlatformString(), wxT(" "));
				while(plats.HasMoreTokens()) {
					wxString tok=plats.GetNextToken();
					//see if this token is here
					if (map.count(tok)) { ourPlatURL=map[tok]; break; }

				}
				if (noGUI) {
					wxLogMessage(wxT("A new TQSL release (V%s) is available."), newProgramRev->Value().c_str());
				} else {
					//will create ("homepage"->"") if none there, which is what we'd be checking for anyway
					UpdateDialogMsgBox msg(this, true, false, programRev, newProgramRev,
							configRev, newConfigRev, ourPlatURL, map[wxT("homepage")]);

					msg.ShowModal();
				}
			}
			if (newConfig) {

				if( noGUI) {
					wxLogMessage(wxT("A new TrustedQSL configuration file (V%s) is available."), newConfigRev->Value().c_str());
				} else {
					UpdateDialogMsgBox msg(this, false, true, programRev, newProgramRev,
							configRev, newConfigRev, wxT(""), wxT(""));

					if (msg.ShowModal() == wxID_OK) {
						UpdateConfigFile();
					}
				}
			} 

			if (!newProgram && !newConfig) 
				if (!silent && !noGUI) 
					wxMessageBox(wxString::Format(wxT("Your system is up to date!\nTQSL Version %hs and Configuration Version %s\nare the newest available"), VERSION, configRev->Value().c_str()), wxT("No Updates"), wxOK|wxICON_INFORMATION, this);
		} else {
			if (diagFile) {
				fprintf(diagFile, "cURL Error during config file version check: %d : %s (%s)\n", retval, curl_easy_strerror((CURLcode)retval), errorbuf);
			}
			if (logFile) {
				fprintf(logFile, "cURL Error during config file version check: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
			}
			if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
				if (!silent && !noGUI)
					wxLogMessage(wxT("Unable to check for updates - either your Internet connection is down or LoTW is unreachable.\nPlease try again later."));
			} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
				if (!silent && !noGUI)
					wxLogMessage(wxT("Unable to check for updates. The nework is down or the LoTW site is too busy.\nPlease try again later."));
			} else if (retval == CURLE_SSL_CONNECT_ERROR) {
				if (!silent && !noGUI)
					wxLogMessage(wxT("Unable to connect to the update site.\nPlease try again later."));
			} else { // some other error
				if (!silent && !noGUI)
					wxMessageBox(wxString::Format(wxT("Error downloading new version information:\n%hs"), errorbuf), wxT("Update"), wxOK|wxICON_EXCLAMATION, this);
			}
		}
	} else {
		if (diagFile) {
			fprintf(diagFile, "cURL Error during program revision check: %d: %s (%s)\n", retval, curl_easy_strerror((CURLcode)retval), errorbuf);
		}
		if (logFile) {
			fprintf(logFile, "cURL Error during program revision check: %s (%s)\n", curl_easy_strerror((CURLcode)retval), errorbuf);
		}
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			if (!silent && !noGUI)
				wxLogMessage(wxT("Unable to check for updates - either your Internet connection is down or LoTW is unreachable.\nPlease try again later."));
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			if (!silent && !noGUI)
				wxLogMessage(wxT("Unable to check for updates. The nework is down or the LoTW site is too busy.\nPlease try again later."));
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			if (!silent && !noGUI)
				wxLogMessage(wxT("Unable to connect to the update site.\nPlease try again later."));
		} else { // some other error
			if (!silent && !noGUI)
				wxMessageBox(wxString::Format(wxT("Error downloading update version information:\n%hs"), errorbuf), wxT("Update"), wxOK|wxICON_EXCLAMATION, this);
		}
	}

	// we checked today, and whatever the result, no need to (automatically) check again until the next interval

	config->Write(wxT("UpdateCheckTime"), wxDateTime::Today().FormatISODate());

	curl_easy_cleanup(req);
	if (logFile) fclose(logFile);
	
	// After update check, validate user certificates
	DoCheckExpiringCerts(noGUI);
	return;
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
	tqslTrace("MyFrame::ImportQSODataFile");
	wxString infile;

	// Does the user have any certificates?
	get_certlist("", 0, false, false);
	free_certlist();
	if (ncerts == 0) {
		wxMessageBox(wxT("You have no callsign certificates to use to sign a log file.\n")
			   wxT("Please install a callsign certificate then try again."), wxT("No Callsign Certificates"),
			   wxOK|wxICON_EXCLAMATION, this);
		return;
	}
	try {
		bool compressed = (event.GetId() == tm_f_import_compress || event.GetId() == tl_Save);
		
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
                                          wxT("Station Location: %hs\nCall sign: %hs\nDXCC: %hs\nIs this correct?")), infile.c_str(), loc_name,
			callsign, dxcc.name()), wxT("TQSL - Confirm signing"), wxYES_NO, this) == wxYES)
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
	tqslTrace("MyFrame::UploadQSODataFile");
	wxString infile;
	// Does the user have any certificates?
	get_certlist("", 0, false, false);
	free_certlist();
	if (ncerts == 0) {
		wxMessageBox(wxT("You have no callsign certificates to use to sign a log file.\n")
			   wxT("Please install a callsign certificate then try again."), wxT("No Callsign Certificates"),
			   wxOK|wxICON_EXCLAMATION, this);
		return;
	}
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
                                          wxT("Station Location: %hs\nCall sign: %hs\nDXCC: %hs\nIs this correct?")), infile.c_str(), loc_name,
			callsign, dxcc.name()), wxT("TQSL - Confirm signing"), wxYES_NO, this) == wxYES)
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

void MyFrame::OnPreferences(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnPreferences");
	Preferences* dial = new Preferences(this, help);
	dial->Show(true);
	file_menu->Enable(tm_f_preferences, false);
}

class TQSLConfig {
public:
        TQSLConfig() {
                callSign = "";
		serial = 0;
		dxcc = 0;
		elementBody = wxT("");
		locstring = wxT("");
		config = NULL;
		outstr = NULL;
		conv = NULL;
        }
	void SaveSettings (gzFile* out, wxString appname);
	void RestoreCert (void);
	void RestoreConfig (gzFile& in);
	void ParseLocations (gzFile* out, const tQSL_StationDataEnc loc);
	wxConfig *config;
	long serial;
	int dxcc;
	string callSign;
	wxString signedCert;
	wxString privateKey;
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
// application name (tqslapp)

void TQSLConfig::SaveSettings (gzFile* out, wxString appname) {
	tqslTrace("TQSLConfig::SaveSettings", "appname=%s", _S(appname));
	config = new wxConfig(appname);
	wxString name, gname;
	long	context;
	wxString svalue;
	long	lvalue;
	bool	bvalue;
	double	dvalue;
	wxArrayString groupNames;

	tqslTrace("TQSLConfig::SaveSettings", "... init groups");
	groupNames.Add(wxT("/"));
	bool more = config->GetFirstGroup(gname, context);
	while (more) {
		tqslTrace("TQSLConfig::SaveSettings", "... add group %s", _S(name));
		groupNames.Add(wxT("/") + gname);
		more = config->GetNextGroup(gname, context);
	}
	tqslTrace("TQSLConfig::SaveSettings", "... groups done.");

	for (unsigned i = 0; i < groupNames.GetCount(); i++) {
		tqslTrace("TQSLConfig::SaveSettings", "Group %d setting path %s", i, _S(groupNames[i]));
		config->SetPath(groupNames[i]);
		more = config->GetFirstEntry(name, context);
		while (more) {
			tqslTrace("TQSLConfig::SaveSettings", "name=%s", _S(name));
			if (name.IsEmpty()) {
				more = config->GetNextEntry(name, context);
				continue;
			}
			gzprintf(*out, "<Setting name=\"%s\" group=\"%s\" ", (const char *)name.mb_str(), (const char *)groupNames[i].mb_str());
			wxConfigBase::EntryType etype = config->GetEntryType(name);
			switch (etype) {
				case wxConfigBase::Type_Unknown:
				case wxConfigBase::Type_String:
					config->Read(name, &svalue);
					svalue.Replace(wxT("&"), wxT("&amp;"), true); 
					svalue.Replace(wxT("<"), wxT("&lt;"), true); 
					svalue.Replace(wxT(">"), wxT("&gt;"), true); 
					gzprintf(*out, "Type=\"String\" Value=\"%s\"/>\n", (const char *)svalue.mb_str());
					break;
				case wxConfigBase::Type_Boolean:
					config->Read(name, &bvalue);
					if (bvalue)
						gzprintf(*out, "Type=\"Bool\" Value=\"true\"/>\n");
					else
						gzprintf(*out, "Type=\"Bool\" Value=\"false\"/>\n");
					break;
				case wxConfigBase::Type_Integer:
					config->Read(name, &lvalue);
					gzprintf(*out, "Type=\"Int\" Value=\"%d\"/>\n", lvalue);
					break;
				case wxConfigBase::Type_Float:
					config->Read(name, &dvalue);
					gzprintf(*out, "Type=\"Float\" Value=\"%f\"/>\n", dvalue);
					break;
			}
			more = config->GetNextEntry(name, context);
		}
	}
	tqslTrace("TQSLConfig::SaveSettings", "Done.");
	config->SetPath(wxT("/"));
	
	return;
}

void
MyFrame::BackupConfig(wxString& filename, bool quiet) {
	tqslTrace("MyFrame::BackupConfig", "filename=%s, quiet=%d", _S(filename), quiet);
	int i;
	try {
		gzFile out = 0;
		out = gzopen(filename.mb_str(), "wb9");
		if (!out) {
			wxLogError(wxT("Error opening save file %s: %hs"), filename.c_str(), strerror(errno));
			return;
		}
		TQSLConfig* conf = new TQSLConfig();

		gzprintf(out, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<TQSL_Configuration>\n");
		gzprintf(out, "<!-- Warning! If you directly edit this file, you are responsible for its content.\n");
		gzprintf(out, "The ARRL's LoTW Help Desk will be unable to assist you. -->\n");
		gzprintf(out, "<Certificates>\n");

		if (!quiet) {
			wxLogMessage(wxT("Saving callsign certificates"));
		} else {
			tqslTrace("MyFrame::BackupConfig", "Saving callsign certificates");
		}
		int ncerts;
		char buf[8192];
		// Save root certificates
		check_tqsl_error(tqsl_selectCACertificates(&certlist, &ncerts, "root"));
		for (i = 0; i < ncerts; i++) {
			gzprintf(out, "<RootCert>\n");
			check_tqsl_error(tqsl_getCertificateEncoded(certlist[i], buf, sizeof buf));
			gzwrite(out, buf, strlen(buf));
			gzprintf(out, "</RootCert>\n");
			tqsl_freeCertificate(certlist[i]);
		}
		// Save CA certificates
		check_tqsl_error(tqsl_selectCACertificates(&certlist, &ncerts, "authorities"));
		for (i = 0; i < ncerts; i++) {
			gzprintf(out, "<CACert>\n");
			check_tqsl_error(tqsl_getCertificateEncoded(certlist[i], buf, sizeof buf));
			gzwrite(out, buf, strlen(buf));
			gzprintf(out, "</CACert>\n");
			tqsl_freeCertificate(certlist[i]);
		}
		tqsl_selectCertificates(&certlist, &ncerts, 0, 0, 0, 0, TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED | TQSL_SELECT_CERT_SUPERCEDED);
		for (i = 0; i < ncerts; i++) {
			char callsign[64];
			long serial = 0;
			int dxcc = 0;
			int keyonly;
			check_tqsl_error(tqsl_getCertificateKeyOnly(certlist[i], &keyonly));
			check_tqsl_error(tqsl_getCertificateCallSign(certlist[i], callsign, sizeof callsign));
			if (!keyonly) {
				check_tqsl_error(tqsl_getCertificateSerial(certlist[i], &serial));
			} 
			check_tqsl_error(tqsl_getCertificateDXCCEntity(certlist[i], &dxcc));
			if (!quiet) {
				wxLogMessage(wxT("\tSaving callsign certificate for %hs"), callsign);
			}
			gzprintf(out, "<UserCert CallSign=\"%s\" dxcc=\"%d\" serial=\"%d\">\n", callsign, dxcc, serial);
			if (!keyonly) {
				gzprintf(out, "<SignedCert>\n");
				check_tqsl_error(tqsl_getCertificateEncoded(certlist[i], buf, sizeof buf));
				gzwrite(out, buf, strlen(buf));
				gzprintf(out, "</SignedCert>\n");
			}
			gzprintf(out, "<PrivateKey>\n");
			check_tqsl_error(tqsl_getKeyEncoded(certlist[i], buf, sizeof buf));
			gzwrite(out, buf, strlen(buf));
			gzprintf(out, "</PrivateKey>\n</UserCert>\n");
			tqsl_freeCertificate(certlist[i]);
		}
		gzprintf(out, "</Certificates>\n");
		gzprintf(out, "<Locations>\n");
		if (!quiet) {
			wxLogMessage(wxT("Saving Station Locations"));
		} else {
			tqslTrace("MyFrame::BackupConfig", "Saving Station Locations");
		}
		tQSL_StationDataEnc sdbuf = NULL;
		check_tqsl_error(tqsl_getStationDataEnc(&sdbuf));
		TQSLConfig* parser = new TQSLConfig();
		if (sdbuf)
			parser->ParseLocations(&out, sdbuf);
		check_tqsl_error(tqsl_freeStationDataEnc(sdbuf));
		gzprintf(out, "</Locations>\n");

		if (!quiet) {
			wxLogMessage(wxT("Saving TQSL Preferences"));
		} else {
			tqslTrace("MyFrame::BackupConfig", "Saving TQSL Preferences - out=0x%lx", (void *)out);
		}
		gzprintf(out, "<TQSLSettings>\n");
		conf->SaveSettings(&out, wxT("tqslapp"));
		tqslTrace("MyFrame::BackupConfig", "Done with settings. out=0x%lx", (void *)out);
		gzprintf(out, "</TQSLSettings>\n");

		if (!quiet) {
			wxLogMessage(wxT("Saving QSOs"));
		} else {
			tqslTrace("MyFrame::BackupConfig", "Saving QSOs");
		}
	
		tQSL_Converter conv = 0;
		check_tqsl_error(tqsl_beginConverter(&conv));
		tqslTrace("MyFrame::BackupConfig", "beginConverter call success");
		gzprintf(out, "<DupeDb>\n");

		while (true) {
			char dupekey[256];
			char dupedata[10];
			int status = tqsl_getDuplicateRecords(conv, dupekey, dupedata, sizeof(dupekey));
			if (status == -1)		// End of file
				break;
			check_tqsl_error(status);
			gzprintf(out, "<Dupe key=\"%s\" />\n", dupekey);
		}
		gzprintf(out, "</DupeDb>\n");
		tqslTrace("MyFrame::BackupConfig", "Dupes db saved OK");

		gzprintf(out, "</TQSL_Configuration>\n");

		gzclose(out);
		if (!quiet) {
			wxLogMessage(wxT("Save operation complete."));
		} else {
			tqslTrace("MyFrame::BackupConfig", "Save operation complete.");
		}
	}
	catch (TQSLException& x) {
		if (quiet) {
			wxString errmsg = wxString::Format(wxT("Error performing automatic backup: %hs"), x.what());
			wxMessageBox(errmsg, wxT("Backup Error"), wxOK|wxICON_EXCLAMATION);
		} else {
			wxLogError(wxT("Backup operation failed: %hs"), x.what());
		}
	}
}

void
MyFrame::OnSaveConfig(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnSaveConfig");
	try {
		wxString file_default = wxT("tqslconfig.tbk");
		wxString filename = wxFileSelector(wxT("Enter file to save to"), wxT(""),
			file_default, wxT(".tbk"), wxT("Configuration files (*.tbk)|*.tbk|All files (*.*)|*.*"),
			wxSAVE|wxOVERWRITE_PROMPT, this);
		if (filename == wxT(""))
			return;

		BackupConfig(filename, false);
	}
	catch (TQSLException& x) {
		wxLogError(wxT("Backup operation failed: %hs"), x.what());
	}
}


void
restore_user_cert(TQSLConfig* loader) {
	get_certlist(loader->callSign.c_str(), loader->dxcc, true, true);
	for (int i = 0; i < ncerts; i++) {
		long serial;
		int dxcc;
		int keyonly;
		check_tqsl_error(tqsl_getCertificateKeyOnly(certlist[i], &keyonly));
		check_tqsl_error(tqsl_getCertificateDXCCEntity(certlist[i], &dxcc));
		if (!keyonly) {
			check_tqsl_error(tqsl_getCertificateSerial(certlist[i], &serial));
		} else {
			// There can be only one pending request for
			// a given callsign/entity
			serial = loader->serial;
		}
		if (serial == loader->serial && dxcc == loader->dxcc)
			return;			// This certificate is already installed.
	}
	
	// There is no certificate matching this callsign/entity/serial.
	wxLogMessage(wxT("\tRestoring callsign certificate for %hs"), loader->callSign.c_str());
	check_tqsl_error(tqsl_importKeyPairEncoded(loader->callSign.c_str(), "user", loader->privateKey.mb_str(), loader->signedCert.mb_str()));
}

void
restore_root_cert(TQSLConfig* loader) {
	check_tqsl_error(tqsl_importKeyPairEncoded(NULL, "root", NULL, loader->signedCert.mb_str()));
}

void
restore_ca_cert(TQSLConfig* loader) {
	check_tqsl_error(tqsl_importKeyPairEncoded(NULL, "authorities", NULL, loader->signedCert.mb_str()));
}

void
TQSLConfig::xml_restore_start(void *data, const XML_Char *name, const XML_Char **atts) {
	TQSLConfig* loader = (TQSLConfig *) data;
	int i;

	loader->elementBody = wxT("");
	if (strcmp(name, "UserCert") == 0) {
		for (int i = 0; atts[i]; i+=2) {
			if (strcmp(atts[i], "CallSign") == 0) {
				loader->callSign = atts[i + 1];
			} else if (strcmp(atts[i], "serial") == 0) {
				if (strlen(atts[i+1]) == 0) {
					loader->serial = 0;
				} else {
					loader->serial =  strtol(atts[i+1], NULL, 10);
				}
			} else if (strcmp(atts[i], "dxcc") == 0) {
				if (strlen(atts[i+1]) == 0) {
					loader->dxcc = 0;
				} else {
					loader->dxcc =  strtol(atts[i+1], NULL, 10);
				}
			}
		}
		loader->privateKey = wxT("");
		loader->signedCert = wxT("");
	} else if (strcmp(name, "TQSLSettings") == 0) {
		wxLogMessage(wxT("Restoring Preferences"));
		loader->config = new wxConfig(wxT("tqslapp"));
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
			bool bsw = (svalue == wxT("true"));
			loader->config->Write(sname, bsw);
		} else if (stype == wxT("Int")) {
			long lval = strtol(svalue.mb_str(), NULL, 10);
			loader->config->Write(sname, lval);
		} else if (stype == wxT("Float")) {
			double dval = strtod(svalue.mb_str(), NULL);
			loader->config->Write(sname, dval);
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
	if (strcmp(name, "SignedCert") == 0) {
		loader->signedCert = loader->elementBody.Trim(false);
	} else if (strcmp(name, "PrivateKey") == 0) {
		loader->privateKey = loader->elementBody.Trim(false);
	} else if (strcmp(name, "RootCert") == 0) {
		loader->signedCert = loader->elementBody.Trim(false);
		restore_root_cert(loader);
	} else if (strcmp(name, "CACert") == 0) {
		loader->signedCert = loader->elementBody.Trim(false);
		restore_ca_cert(loader);
	} else if (strcmp(name, "UserCert") == 0) {
		restore_user_cert(loader);
	} else if (strcmp(name, "Location") == 0) {
		loader->locstring += wxT("</StationData>\n");
	} else if (strcmp(name, "Locations") == 0) {
		loader->locstring += wxT("</StationDataFile>\n");
		if (tqsl_mergeStationLocations(loader->locstring.mb_str()) != 0) {
			wxLogError(wxT("\tError importing station locations: %hs"), tqsl_getErrorString());
		}
	} else if (strcmp(name, "TQSLSettings") == 0) {
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
	tqslTrace("TQSLConfig::RestoreConfig");
        XML_Parser xp = XML_ParserCreate(0);
	XML_SetUserData(xp, (void *) this);
        XML_SetStartElementHandler(xp, &TQSLConfig::xml_restore_start);
        XML_SetEndElementHandler(xp, &TQSLConfig::xml_restore_end);
        XML_SetCharacterDataHandler(xp, &TQSLConfig::xml_text);

	char buf[4096];
	wxLogMessage(wxT("Restoring Callsign Certificates"));
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
TQSLConfig::ParseLocations (gzFile* out, const tQSL_StationDataEnc loc) {
	tqslTrace("TQSL::ParseLocations", "loc=%s", loc);
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
	tqslTrace("MyFrame::OnLoadConfig");
	wxString filename = wxFileSelector(wxT("Select saved configuration file"), wxT(""),
					   wxT("tqslconfig.tbk"), wxT("tbk"), wxT("Saved configuration files (*.tbk)|*.tbk"),
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
		cert_tree->Build(CERTLIST_FLAGS);
		loc_tree->Build();
		LocTreeReset();
		CertTreeReset();
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
QSLApp::GUIinit(bool checkUpdates, bool quiet) {
	tqslTrace("QSLApp::GUIinit", "checkUpdates=%d", checkUpdates);
	int x, y, w, h;
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->Read(wxT("MainWindowX"), &x, 50);
	config->Read(wxT("MainWindowY"), &y, 50);
	config->Read(wxT("MainWindowWidth"), &w, 800);
	config->Read(wxT("MainWindowHeight"), &h, 600);

	if (w < MAIN_WINDOW_MIN_WIDTH) w = MAIN_WINDOW_MIN_WIDTH;
	if (h < MAIN_WINDOW_MIN_HEIGHT) w = MAIN_WINDOW_MIN_HEIGHT;

	frame = new MyFrame(wxT("TQSL"), x, y, w, h, checkUpdates, quiet);
	frame->SetMinSize(wxSize(MAIN_WINDOW_MIN_WIDTH, MAIN_WINDOW_MIN_HEIGHT));
	if (checkUpdates)
		frame->FirstTime();
	frame->Show(!quiet);
	SetTopWindow(frame);

	return frame;
}

// Override OnRun so we can have a last-chance exception handler
// in case something doesn't handle an error.
int
QSLApp::OnRun() {
	tqslTrace("QSLApp::OnRun");
	try {
		if (m_exitOnFrameDelete == Later)
			m_exitOnFrameDelete = Yes;
		return MainLoop();
	}
	catch (TQSLException& x) {
		string msg = x.what();
		tqslTrace("QSLApp::OnRun", "Last chance handler, string=%s", (const char *)msg.c_str());
		cerr << "An exception has occurred! " << msg << endl;
		wxLogError(wxT("%hs"), x.what());
		exitNow(TQSL_EXIT_TQSL_ERROR, false);
	}
	return 0;
}

bool
QSLApp::OnInit() {
	frame = 0;
	bool quiet = false;

	int major, minor;
	if (tqsl_getConfigVersion(&major, &minor)) {
		wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"), wxOK);
		exitNow (TQSL_EXIT_TQSL_ERROR, quiet);
	}
	wxFileSystem::AddHandler(new tqslInternetFSHandler());

	//short circuit if no arguments

	if (argc<=1) {
		GUIinit(true, quiet);
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
	wxString importfile(wxT(""));
	wxString diagfile(wxT(""));

	wxCmdLineParser parser;

	static const wxCmdLineEntryDesc cmdLineDesc[] = {
		{ wxCMD_LINE_OPTION, wxT("a"), wxT("action"),	wxT("Specify dialog action - abort, all, compliant or ask") },
		{ wxCMD_LINE_OPTION, wxT("b"), wxT("begindate"),wxT("Specify start date for QSOs to sign") },
		{ wxCMD_LINE_OPTION, wxT("e"), wxT("enddate"),	wxT("Specify end date for QSOs to sign") },
		{ wxCMD_LINE_SWITCH, wxT("d"), wxT("nodate"),	wxT("Suppress date range dialog") },
		{ wxCMD_LINE_OPTION, wxT("i"), wxT("import"),	wxT("Import a certificate file (.p12 or .tq6)") },
		{ wxCMD_LINE_OPTION, wxT("l"), wxT("location"),	wxT("Selects Station Location") },
		{ wxCMD_LINE_SWITCH, wxT("s"), wxT("editlocation"), wxT("Edit (if used with -l) or create Station Location") },
		{ wxCMD_LINE_OPTION, wxT("o"), wxT("output"),	wxT("Output file name (defaults to input name minus extension plus .tq8") },
		{ wxCMD_LINE_SWITCH, wxT("u"), wxT("upload"),	wxT("Upload after signing instead of saving") },
		{ wxCMD_LINE_SWITCH, wxT("x"), wxT("batch"),	wxT("Exit after processing log (otherwise start normally)") },
		{ wxCMD_LINE_OPTION, wxT("p"), wxT("password"),	wxT("Password for the signing key") },
		{ wxCMD_LINE_SWITCH, wxT("q"), wxT("quiet"),	wxT("Quiet Mode - same behavior as -x") },
		{ wxCMD_LINE_OPTION, wxT("t"), wxT("diagnose"),	wxT("File name for diagnostic tracking log") },
		{ wxCMD_LINE_SWITCH, wxT("v"), wxT("version"),  wxT("Display the version information and exit") },
		{ wxCMD_LINE_SWITCH, wxT("n"), wxT("updates"),	wxT("Check for updates to tqsl and the configuration file") },
		{ wxCMD_LINE_SWITCH, wxT("h"), wxT("help"),	wxT("Display command line help"), wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
		{ wxCMD_LINE_PARAM,  NULL,     NULL,		wxT("Input ADIF or Cabrillo log file to sign"), wxCMD_LINE_VAL_STRING, wxCMD_LINE_PARAM_OPTIONAL },
		{ wxCMD_LINE_NONE }
	};

	// Lowercase command options
	origCommandLine = argv[0];
	for (int i = 1; i < argc; i++) {
		origCommandLine += wxT(" ");
		origCommandLine += argv[i];
		if (argv[i] && (argv[i][0] == wxT('-') || argv[i][0] == wxT('/')))
			if (wxIsalpha(argv[i][1]) && wxIsupper(argv[i][1])) 
				argv[i][1] = wxTolower(argv[i][1]);
	}
		
	parser.SetCmdLine(argc, argv);
	parser.SetDesc(cmdLineDesc);
	// only allow "-" for options, otherwise "/path/something.adif" 
	// is parsed as "-path"
	//parser.SetSwitchChars(wxT("-")); //by default, this is '-' on Unix, or '-' or '/' on Windows. We should respect the Win32 conventions, but allow the cross-platform Unix one for cross-plat loggers
	int parseStatus = parser.Parse(true);
	if (parseStatus == -1) {	// said "-h"
		return false;
	}
	// Always display TQSL version
	if ((!parser.Found(wxT("n"))) || parser.Found(wxT("v"))) {
		cerr << "TQSL Version " VERSION " " BUILD "\n";
	}
	if (parseStatus != 0)  {
		exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
	}

	// version already displayed - just exit
	if (parser.Found(wxT("v"))) { 
		return false;
	}

	if (parser.Found(wxT("x")) || parser.Found(wxT("q"))) {
		quiet = true;
		wxLog::SetActiveTarget(new LogStderr());
	}

	if (parser.Found(wxT("t"), &diagfile)) {
		diagFile = fopen(diagfile.mb_str(), "wb");
		if (!diagFile) {
			cerr << "Error opening diagnostic log " << diagfile.mb_str() << ": " << strerror(errno) << endl;
		} else {
			wxString about = getAbout();
			fprintf(diagFile, "TQSL Diagnostics\n%s\n\n", (const char *)about.mb_str());
			fprintf(diagFile, "Command Line: %s\n", (const char *)origCommandLine.mb_str());
			tqsl_init();
			fprintf(diagFile, "Working Directory: %s\n", tQSL_BaseDir);
		}
	}

	tqsl_init();	// Init tqsllib
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

	// Request to check for new versions of tqsl/config/certs
	if (parser.Found(wxT("n"))) {
		if (parser.Found(wxT("i")) || parser.Found(wxT("o")) ||
		    parser.Found(wxT("s")) || parser.Found(wxT("u"))) {
			cerr << "Option -n cannot be combined with any other options" << endl;
			exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
		}
		frame = GUIinit(false, true);
		frame->Show(false);
		// Check for updates then bail out.
		wxLog::SetActiveTarget(new LogStderr());
		frame->DoCheckForUpdates(false, true);
		return(false);
	}

	frame =GUIinit(!quiet, quiet);
	if (quiet) {
		wxLog::SetActiveTarget(new LogStderr());
		frame->Show(false);
	}
	if (parser.Found(wxT("i"), &importfile)) {
		importfile.Trim(true).Trim(false);
		notifyData nd;
		if (tqsl_importTQSLFile(importfile.mb_str(), notifyImport, &nd)) {
			wxLogError(wxT("%hs"), tqsl_getErrorString());
		} else {
			wxLogMessage(nd.Message());
			frame->cert_tree->Build(CERTLIST_FLAGS);
			wxString call = wxString(tQSL_ImportCall, wxConvLocal);
			wxString pending = wxConfig::Get()->Read(wxT("RequestPending"));
			pending.Replace(call, wxT(""), true);
			if (pending[0] == ',')
				pending.Replace(wxT(","), wxT(""));
			if (pending.Last() == ',')
				pending.Truncate(pending.Len()-1);
			wxConfig::Get()->Write(wxT("RequestPending"), pending);
		}
		return(true);
	}

	if (parser.Found(wxT("l"), &locname)) {
		locname.Trim(true);			// clean up whitespace
		locname.Trim(false);
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
	wxString start = wxT("");
	wxString end = wxT("");
	tQSL_Date* startdate = NULL;
	tQSL_Date* enddate = NULL;
	tQSL_Date s, e;
	if (parser.Found(wxT("b"), &start)) {
		if (start.Trim() == wxT(""))
			startdate = NULL;
		else if (tqsl_initDate(&s, start.mb_str()) || !tqsl_isDateValid(&s)) {
			if (quiet) {
				wxLogError(wxT("Start date of %s is invalid"), start.c_str());
				exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
			}
			else {
				wxMessageBox(wxString::Format(wxT("Start date of %s is invalid"), start.c_str()), ErrorTitle, wxOK|wxCENTRE, frame);
				return false;
			}
		}
		startdate = &s;
	}
	if (parser.Found(wxT("e"), &end)) {
		if (end.Trim() == wxT(""))
			enddate = NULL;
		else if (tqsl_initDate(&e, end.mb_str()) || !tqsl_isDateValid(&e)) {
			if (quiet) {
				wxLogError(wxT("End date of %s is invalid"), end.c_str());
				exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
			}
			else {
				wxMessageBox(wxString::Format(wxT("End date of %s is invalid"), end.c_str()), ErrorTitle, wxOK|wxCENTRE, frame);
				return false;
			}
		}
		enddate = &e;
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
		if (diagFile)		// Unless there's just a trace log
			return true;	// in which case we let it open.
		wxLogError(wxT("No logfile to sign!"));
		if (quiet)
			exitNow(TQSL_EXIT_COMMAND_ERROR, quiet);
		return false;
	}
	if (loc == 0) {
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
			int val=frame->UploadLogFile(loc, infile, true, suppressdate, startdate, enddate, action, password);
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
			int val = frame->ConvertLogFile(loc, infile, path, true, suppressdate, startdate, enddate, action, password);
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


void MyFrame::FirstTime(void) {
	tqslTrace("MyFrame::FirstTime");
	if (wxConfig::Get()->Read(wxT("HasRun")) == wxT("")) {
		wxConfig::Get()->Write(wxT("HasRun"), wxT("yes"));
		DisplayHelp();
		wxMessageBox(wxT("Please review the introductory documentation before using this program."),
			wxT("Notice"), wxOK, this);
	}
	int ncerts = cert_tree->Build(CERTLIST_FLAGS);
	CertTreeReset();
	if (ncerts == 0 && wxMessageBox(wxT("You have no callsign certificate installed on this computer with which to sign log submissions.\n")
		wxT("Would you like to request a callsign certificate now?"), wxT("Alert"), wxYES_NO, this) == wxYES) {
		wxCommandEvent e;
		CRQWizard(e);
	}
	wxString pending = wxConfig::Get()->Read(wxT("RequestPending"));
	wxStringTokenizer tkz(pending, wxT(","));
	while (tkz.HasMoreTokens()) {
		wxString pend = tkz.GetNextToken();
		bool found = false;
		tQSL_Cert *certs;
		int ncerts = 0;
		if (!tqsl_selectCertificates(&certs, &ncerts, pend.mb_str(), 0, 0, 0, TQSL_SELECT_CERT_WITHKEYS)) {
			for (int i = 0; i < ncerts; i++) {
				int keyonly;
				if (!tqsl_getCertificateKeyOnly(certs[i], &keyonly)) {
					if (!found && keyonly)
						found = true;
				}
				tqsl_freeCertificate(certs[i]);
			}
		}

		if (!found) {
			// Remove this call from the list of pending certificate requests
			wxString p = wxConfig::Get()->Read(wxT("RequestPending"));
			p.Replace(pend, wxT(""), true);
			if (p[0] == ',')
				p.Replace(wxT(","), wxT(""));
			if (p.Last() == ',')
				p.Truncate(pending.Len()-1);
			wxConfig::Get()->Write(wxT("RequestPending"), p);
		} else {
			if (wxMessageBox(wxT("Are you ready to load your new callsign certificate for ") + pend + wxT(" from LoTW now?"), wxT("Alert"), wxYES_NO, this) == wxYES) {
				wxCommandEvent e;
				OnLoadCertificateFile(e);
			}
		}
	}

	if (ncerts > 0) {
		TQ_WXCOOKIE cookie;
		wxTreeItemId it = cert_tree->GetFirstChild(cert_tree->GetRootItem(), cookie);
		while (it.IsOk()) {
			if (cert_tree->GetItemText(it) == wxT("Test Certificate Authority")) {
				wxMessageBox(wxT("You must delete your beta-test certificates (the ones\n")
					wxT("listed under \"Test Certificate Authority\") to ensure proprer\n")
					wxT("operation of the TrustedQSL software."), wxT("Warning"), wxOK, this);
				break;
			}
			it = cert_tree->GetNextChild(cert_tree->GetRootItem(), cookie);
		}

	}
// Copy tqslcert preferences to tqsl unless already done.
	if (wxConfig::Get()->Read(wxT("PrefsMigrated")) == wxT("")) {
		wxConfig::Get()->Write(wxT("PrefsMigrated"), wxT("yes"));
		tqslTrace("MyFrame::FirstTime", "Migrating preferences from tqslcert");
		wxConfig* certconfig = new wxConfig(wxT("tqslcert"));

		wxString name, gname;
		long	context;
		wxString svalue;
		long	lvalue;
		bool	bvalue;
		double	dvalue;
		wxArrayString groupNames;

		groupNames.Add(wxT("/"));
		bool more = certconfig->GetFirstGroup(gname, context);
		while (more) {
			groupNames.Add(wxT("/") + gname);
			more = certconfig->GetNextGroup(gname, context);
		}

		for (unsigned i = 0; i < groupNames.GetCount(); i++) {
			certconfig->SetPath(groupNames[i]);
			wxConfig::Get()->SetPath(groupNames[i]);
			more = certconfig->GetFirstEntry(name, context);
			while (more) {
				wxConfigBase::EntryType etype = certconfig->GetEntryType(name);
				switch (etype) {
					case wxConfigBase::Type_Unknown:
					case wxConfigBase::Type_String:
						certconfig->Read(name, &svalue);
						wxConfig::Get()->Write(name, svalue);
						break;
					case wxConfigBase::Type_Boolean:
						certconfig->Read(name, &bvalue);
						wxConfig::Get()->Write(name, bvalue);
						break;
					case wxConfigBase::Type_Integer:
						certconfig->Read(name, &lvalue);
						wxConfig::Get()->Write(name, lvalue);
						break;
					case wxConfigBase::Type_Float:
						certconfig->Read(name, &dvalue);
						wxConfig::Get()->Write(name, dvalue);
						break;
				}
				more = certconfig->GetNextEntry(name, context);
			}
		}
		delete certconfig;
		wxConfig::Get()->SetPath(wxT("/"));
		wxConfig::Get()->Flush();
	}	
	// Find and report conflicting mode maps
	init_modes();
	return;
}

wxMenu *
makeCertificateMenu(bool enable, bool keyonly) {
	tqslTrace("makeCertificateMenu", "enable=%d, keyonly=%d", enable, keyonly);
	wxMenu *c_menu = new wxMenu;
	c_menu->Append(tc_c_Properties, wxT("Display Callsign Certificate &Properties"));
	c_menu->Enable(tc_c_Properties, enable);
	c_menu->AppendSeparator();
	c_menu->Append(tc_c_Load, wxT("&Load Callsign Certificate from File"));
	c_menu->Append(tc_c_Export, wxT("&Save Callsign Certificate to File..."));
	c_menu->Enable(tc_c_Export, enable);
	if (!keyonly) {
		c_menu->AppendSeparator();
		c_menu->Append(tc_c_New, wxT("Request &New Callsign Certificate..."));
		c_menu->Append(tc_c_Renew, wxT("&Renew Callsign Certificate"));
		c_menu->Enable(tc_c_Renew, enable);
	} else 
	c_menu->AppendSeparator();
	c_menu->Append(tc_c_Delete, wxT("&Delete Callsign Certificate"));
	c_menu->Enable(tc_c_Delete, enable);
	return c_menu;
}

wxMenu *
makeLocationMenu(bool enable) {
	tqslTrace("makeLocationMenu", "enable=%d", enable);
	wxMenu *loc_menu = new wxMenu;
	loc_menu->Append(tl_c_Properties, wxT("&Properties"));
	loc_menu->Enable(tl_c_Properties, enable);
	stn_menu->Enable(tm_s_Properties, enable);
	loc_menu->Append(tl_c_Edit, wxT("&Edit"));
	loc_menu->Enable(tl_c_Edit, enable);
	loc_menu->Append(tl_c_Delete, wxT("&Delete"));
	loc_menu->Enable(tl_c_Delete, enable);
	return loc_menu;
}

/////////// Frame /////////////

void MyFrame::OnLoadCertificateFile(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLoadCertificateFile");
	LoadCertWiz lcw(this, help, wxT("Load Certificate File"));
	lcw.RunWizard();
	cert_tree->Build(CERTLIST_FLAGS);
	CertTreeReset();
}

void MyFrame::CRQWizardRenew(wxCommandEvent& event) {
	tqslTrace("MyFrame::CRQWizardRenew");
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	req = 0;
	tQSL_Cert cert;
	wxString callSign, name, address1, address2, city, state, postalCode,
		country, emailAddress;
	if (data != NULL && (cert = data->getCert()) != 0) {	// Should be true
		char buf[256];
		req = new TQSL_CERT_REQ;
		if (!tqsl_getCertificateIssuerOrganization(cert, buf, sizeof buf))
			strncpy(req->providerName, buf, sizeof req->providerName);
		if (!tqsl_getCertificateIssuerOrganizationalUnit(cert, buf, sizeof buf))
			strncpy(req->providerUnit, buf, sizeof req->providerUnit);
		if (!tqsl_getCertificateCallSign(cert, buf, sizeof buf)) {
			callSign = wxString(buf, wxConvLocal);
			strncpy(req->callSign, callSign.mb_str(), sizeof (req->callSign));
		}
		if (!tqsl_getCertificateAROName(cert, buf, sizeof buf)) {
			name = wxString(buf, wxConvLocal);
			strncpy(req->name, name.mb_str(), sizeof (req->name));
		}
		tqsl_getCertificateDXCCEntity(cert, &(req->dxccEntity));
		tqsl_getCertificateQSONotBeforeDate(cert, &(req->qsoNotBefore));
		tqsl_getCertificateQSONotAfterDate(cert, &(req->qsoNotAfter));
		if (!tqsl_getCertificateEmailAddress(cert, buf, sizeof buf)) {
			emailAddress = wxString(buf, wxConvLocal);
			strncpy(req->emailAddress, emailAddress.mb_str(), sizeof (req->emailAddress));
		}
		if (!tqsl_getCertificateRequestAddress1(cert, buf, sizeof buf)) {
			address1 = wxString(buf, wxConvLocal);
			strncpy(req->address1, address1.mb_str(), sizeof (req->address1));
		}
		if (!tqsl_getCertificateRequestAddress2(cert, buf, sizeof buf)) {
			address2 = wxString(buf, wxConvLocal);
			strncpy(req->address2, address2.mb_str(), sizeof (req->address2));
		}
		if (!tqsl_getCertificateRequestCity(cert, buf, sizeof buf)) {
			city = wxString(buf, wxConvLocal);
			strncpy(req->city, city.mb_str(), sizeof (req->city));
		}
		if (!tqsl_getCertificateRequestState(cert, buf, sizeof buf)) {
			state = wxString(buf, wxConvLocal);
			strncpy(req->state, state.mb_str(), sizeof (req->state));
		}
		if (!tqsl_getCertificateRequestPostalCode(cert, buf, sizeof buf)) {
			postalCode = wxString(buf, wxConvLocal);
			strncpy(req->postalCode, postalCode.mb_str(), sizeof (req->postalCode));
		}
		if (!tqsl_getCertificateRequestCountry(cert, buf, sizeof buf)) {
			country = wxString(buf, wxConvLocal);
			strncpy(req->country, country.mb_str(), sizeof (req->country));
		}
	}
	CRQWizard(event);
	if (req)
		delete req;
	req = 0;
}

void MyFrame::CRQWizard(wxCommandEvent& event) {
	tqslTrace("MyFrame::CRQWizard");
	char renew = (req != 0) ? 1 : 0;
	tQSL_Cert cert = (renew ? ((CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection()))->getCert() : 0);
	CRQWiz wiz(req, cert, this, help, renew ? wxT("Renew a Callsign Certificate") : wxT("Request a new Callsign Certificate"));
	int retval = 0;
/*
	CRQ_ProviderPage *prov = new CRQ_ProviderPage(wiz, req);
	CRQ_IntroPage *intro = new CRQ_IntroPage(wiz, req);
	CRQ_NamePage *name = new CRQ_NamePage(wiz, req);
	CRQ_EmailPage *email = new CRQ_EmailPage(wiz, req);
	wxSize size = prov->GetSize();
	if (intro->GetSize().GetWidth() > size.GetWidth())
		size = intro->GetSize();
	if (name->GetSize().GetWidth() > size.GetWidth())
		size = name->GetSize();
	if (email->GetSize().GetWidth() > size.GetWidth())
		size = email->GetSize();
	CRQ_PasswordPage *pw = new CRQ_PasswordPage(wiz);
	CRQ_SignPage *sign = new CRQ_SignPage(wiz, size, &(prov->provider));
	wxWizardPageSimple::Chain(prov, intro);
	wxWizardPageSimple::Chain(intro, name);
	wxWizardPageSimple::Chain(name, email);
	wxWizardPageSimple::Chain(email, pw);
	if (renew)
		sign->cert = ;
	else
		wxWizardPageSimple::Chain(pw, sign);

	wiz.SetPageSize(size);

*/

	if (wiz.RunWizard()) {
		// Save or upload?
		wxString file = flattenCallSign(wiz.callsign) + wxT(".") + wxT(TQSL_CRQ_FILE_EXT);
		bool upload = false;
		if (wxMessageBox(wxT("Do you want to upload this certificate request to LoTW now?\n"
				     "You do not need an account on LoTW to do this."),
				wxT("Upload"), wxYES_NO|wxICON_QUESTION, this) == wxYES) {
			upload = true;
			// Save it in the working directory
#ifdef _WIN32
			file = wxString(tQSL_BaseDir, wxConvLocal) + wxT("\\") + file;
#else
			file = wxString(tQSL_BaseDir, wxConvLocal) + wxT("/") + file;
#endif
		} else {
			// Where to put it?
			file = wxFileSelector(wxT("Save request"), wxT(""), file, wxT(TQSL_CRQ_FILE_EXT),
				wxT("tQSL Cert Request files (*.") wxT(TQSL_CRQ_FILE_EXT) wxT(")|*.") wxT(TQSL_CRQ_FILE_EXT)
					wxT("|All files (") wxT(ALLFILESWILD) wxT(")|") wxT(ALLFILESWILD),
				wxSAVE | wxOVERWRITE_PROMPT);
			if (file.IsEmpty()) {
				wxLogMessage(wxT("Request cancelled"));
				return;
			}
		}
		TQSL_CERT_REQ req;
		strncpy(req.providerName, wiz.provider.organizationName, sizeof req.providerName);
		strncpy(req.providerUnit, wiz.provider.organizationalUnitName, sizeof req.providerUnit);
		strncpy(req.callSign, wiz.callsign.mb_str(), sizeof req.callSign);
		strncpy(req.name, wiz.name.mb_str(), sizeof req.name);
		strncpy(req.address1, wiz.addr1.mb_str(), sizeof req.address1);
		strncpy(req.address2, wiz.addr2.mb_str(), sizeof req.address2);
		strncpy(req.city, wiz.city.mb_str(), sizeof req.city);
		strncpy(req.state, wiz.state.mb_str(), sizeof req.state);
		strncpy(req.postalCode, wiz.zip.mb_str(), sizeof req.postalCode);
		if (wiz.country.IsEmpty())
			strncpy(req.country, "USA", sizeof req.country);
		else
			strncpy(req.country, wiz.country.mb_str(), sizeof req.country);
		strncpy(req.emailAddress, wiz.email.mb_str(), sizeof req.emailAddress);
		strncpy(req.password, wiz.password.mb_str(), sizeof req.password);
		req.dxccEntity = wiz.dxcc;
		req.qsoNotBefore = wiz.qsonotbefore;
		req.qsoNotAfter = wiz.qsonotafter;
		req.signer = wiz.cert;
		if (req.signer) {
			char buf[40];
			void *call = 0;
			if (!tqsl_getCertificateCallSign(req.signer, buf, sizeof(buf)))
				call = &buf;
			while (tqsl_beginSigning(req.signer, 0, getPassword, call)) {
				if (tQSL_Error != TQSL_PASSWORD_ERROR) {
					wxLogError(wxT("%hs"), tqsl_getErrorString());
					return;
				}
			}
		}
		req.renew = renew ? 1 : 0;
		if (tqsl_createCertRequest(file.mb_str(), &req, 0, 0)) {
			if (req.signer)
				tqsl_endSigning(req.signer);
			wxLogError(wxT("%hs"), tqsl_getErrorString());
			return;
		}
		if (upload) {
			ifstream in(file.mb_str(), ios::in | ios::binary);
			if (!in) {
				wxLogError(wxT("Error opening certificate request file %s: %hs"), file.c_str(), strerror(errno));
			} else {
				string contents;
				in.seekg(0, ios::end);
				contents.resize(in.tellg());
				in.seekg(0, ios::beg);
				in.read(&contents[0], contents.size());
				in.close();

				wxString fileType(wxT("Certificate Request"));
				retval = UploadFile(file, file.mb_str(), 0, (void *)contents.c_str(), contents.size(), fileType);
				if (retval != 0)
					wxLogError(wxT("Your certificate request did not upload properly\nPlease try again."));
			}
		} else {
			wxString msg = wxT("You may now send your new certificate request (");
			msg += file ;
			msg += wxT(")");
			if (wiz.provider.emailAddress[0] != 0)
				msg += wxString(wxT("\nto:\n   ")) + wxString(wiz.provider.emailAddress, wxConvLocal);
			if (wiz.provider.url[0] != 0) {
				msg += wxT("\n");
				if (wiz.provider.emailAddress[0] != 0)
					msg += wxT("or ");
				msg += wxString(wxT("see:\n   ")) + wxString(wiz.provider.url, wxConvLocal);
			}
			wxMessageBox(msg, wxT("TQSL"));
		}
		if (retval == 0) {
			wxString pending = wxConfig::Get()->Read(wxT("RequestPending"));
			if (pending.IsEmpty())
				pending = wiz.callsign;
			else
				pending += wxT(",") + wiz.callsign;
			wxConfig::Get()->Write(wxT("RequestPending"),pending);
		}
		if (req.signer)
			tqsl_endSigning(req.signer);
		cert_tree->Build(CERTLIST_FLAGS);
		CertTreeReset();
	}
}

void
MyFrame::CertTreeReset() {
	if (!cert_save_label) return;
	cert_save_label->SetLabel(wxT("\nSave a Callsign Certificate"));
	cert_renew_label->SetLabel(wxT("\nRenew a Callsign Certificate"));
	cert_prop_label->SetLabel(wxT("\nDisplay a Callsign Certificate"));
	cert_menu->Enable(tc_c_Renew, false);
	cert_renew_button->Enable(false);
	cert_select_label->SetLabel(wxT("\nSelect a Callsign Certificate to process"));
	cert_save_button->Enable(false);
	cert_prop_button->Enable(false);
}

void MyFrame::OnCertTreeSel(wxTreeEvent& event) {
	tqslTrace("MyFrame::OnCertTreeSel");
	wxTreeItemId id = event.GetItem();
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(id);
	if (data) {
		int keyonly = 0;
		int expired = 0;
		int superseded = 0;
		char call[40];
		tqsl_getCertificateCallSign(data->getCert(), call, sizeof call);
		wxString callSign(call, wxConvLocal);
		tqsl_getCertificateKeyOnly(data->getCert(), &keyonly);
		tqsl_isCertificateExpired(data->getCert(), &expired);
		tqsl_isCertificateSuperceded(data->getCert(), &superseded);
		tqslTrace("MyFrame::OnCertTreeSel", "call=%s", call);

		cert_select_label->SetLabel(wxT(""));
		cert_menu->Enable(tc_c_Properties, true);
		cert_menu->Enable(tc_c_Export, true);
		cert_menu->Enable(tc_c_Delete, true);
		cert_menu->Enable(tc_c_Renew, true);
		cert_save_button->Enable(true);
		cert_load_button->Enable(true);
		cert_prop_button->Enable(true);

		int w, h;
		loc_add_label->GetSize(&w, &h);
		cert_save_label->SetLabel(wxT("\nSave the callsign certificate for ") + callSign);
		cert_save_label->Wrap(w - 10);
		cert_prop_label->SetLabel(wxT("\nDisplay the callsign certificate properties for ") + callSign);
		cert_prop_label->Wrap(w - 10);
		if (!(keyonly || expired || superseded)) {
			cert_renew_label->SetLabel(wxT("\nRenew the callsign certificate for ") + callSign);
			cert_renew_label->Wrap(w - 10);
		} else {
			cert_renew_label->SetLabel(wxT("\nRenew a Callsign Certificate"));
		}
		cert_menu->Enable(tc_c_Renew, !(keyonly || expired || superseded));
		cert_renew_button->Enable(!(keyonly || expired || superseded));
	} else {
		CertTreeReset();
	}
}

void MyFrame::OnCertProperties(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnCertProperties");
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	if (data != NULL)
		displayCertProperties(data, this);
}

void MyFrame::OnCertExport(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnCertExport");
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	if (data == NULL)	// "Never happens"
		return;

	char call[40];
	if (tqsl_getCertificateCallSign(data->getCert(), call, sizeof call)) {
		wxLogError(wxT("%hs"), tqsl_getErrorString());
		return;
	}
	tqslTrace("MyFrame::OnCertExport", "call=%s", call);
	wxString file_default = flattenCallSign(wxString(call, wxConvLocal));
	int ko = 0;
	tqsl_getCertificateKeyOnly(data->getCert(), &ko);
	if (ko)
		file_default += wxT("-key-only");
	file_default += wxT(".p12");
	wxString path = wxConfig::Get()->Read(wxT("CertFilePath"), wxT(""));
	wxString filename = wxFileSelector(wxT("Enter the name for the new Certificate Container file"), path,
		file_default, wxT(".p12"), wxT("Certificate Container files (*.p12)|*.p12|All files (*.*)|*.*"),
		wxSAVE|wxOVERWRITE_PROMPT, this);
	if (filename == wxT(""))
		return;
	wxConfig::Get()->Write(wxT("CertFilePath"), wxPathOnly(filename));
	GetNewPasswordDialog dial(this, wxT("Certificate Container Password"),
wxT("Enter the password for the certificate container file.\n\n")
wxT("If you are using a computer system that is shared\n")
wxT("with others, you should specify a password to\n")
wxT("protect this certificate. However, if you are using\n")
wxT("a computer in a private residence, no password need be specified.\n\n")
wxT("You will have to enter the password any time you\n")
wxT("load the file into TrustedQSL\n\n")
wxT("Leave the password blank and click 'Ok' unless you want to\n")
wxT("use a password.\n\n"), true, help, wxT("save.htm"));
	if (dial.ShowModal() != wxID_OK)
		return;	// Cancelled
	int terr;
	do {
		terr = tqsl_beginSigning(data->getCert(), 0, getPassword, (void *)&call);
		if (terr) {
			if (tQSL_Error == TQSL_PASSWORD_ERROR)
				continue;
			if (tQSL_Error == TQSL_OPERATOR_ABORT)
				return;
			wxLogError(wxT("%hs"), tqsl_getErrorString());
		}
	} while (terr);
	if (tqsl_exportPKCS12File(data->getCert(), filename.mb_str(), dial.Password().mb_str()))
		wxLogError(wxT("Export to %s failed: %hs"), filename.c_str(), tqsl_getErrorString());
	else
		wxLogMessage(wxT("Certificate saved in file %s"), filename.c_str());
	tqsl_endSigning(data->getCert());
}

void MyFrame::OnCertDelete(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnCertDelete");
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	if (data == NULL)	// "Never happens"
		return;

	if (wxMessageBox(
wxT("WARNING! BE SURE YOU REALLY WANT TO DO THIS!\n\n")
wxT("This will permanently remove the certificate from your system.\n")
wxT("You will NOT be able to recover it by loading a .TQ6 file.\n")
wxT("You WILL be able to recover it from a container (.p12) file only if you have\n")
wxT("created one via the Callsign Certificate menu's 'Save Callsign Certificate' command.\n\n")
wxT("ARE YOU SURE YOU WANT TO DELETE THE CERTIFICATE?"), wxT("Warning"), wxYES_NO|wxICON_QUESTION, this) == wxYES) {
		char buf[128];
		if (!tqsl_getCertificateCallSign(data->getCert(), buf, sizeof buf)) {
			wxString call = wxString(buf, wxConvLocal);
			wxString pending = wxConfig::Get()->Read(wxT("RequestPending"));
			pending.Replace(call, wxT(""), true);
			if (pending[0] == ',')
				pending.Replace(wxT(","), wxT(""));
			if (pending.Last() == ',')
				pending.Truncate(pending.Len()-1);
			wxConfig::Get()->Write(wxT("RequestPending"), pending);
		}
		if (tqsl_deleteCertificate(data->getCert()))
			wxLogError(wxT("%hs"), tqsl_getErrorString());
		cert_tree->Build(CERTLIST_FLAGS);
		CertTreeReset();
	}
}

void
MyFrame::LocTreeReset() {
	if (!loc_edit_button) return;
	loc_edit_button->Disable();
	loc_delete_button->Disable();
	loc_prop_button->Disable();
	stn_menu->Enable(tm_s_Properties, false);
	loc_edit_label->SetLabel(wxT("\nEdit a Station Location"));
	loc_delete_label->SetLabel(wxT("\nDelete a Station Location"));
	loc_prop_label->SetLabel(wxT("\nDisplay Station Location Properties"));
	loc_select_label->SetLabel(wxT("\nSelect a Station Location to process"));
}

void MyFrame::OnLocTreeSel(wxTreeEvent& event) {
	tqslTrace("MyFrame::OnLocTreeSel");
	wxTreeItemId id = event.GetItem();
	LocTreeItemData *data = (LocTreeItemData *)loc_tree->GetItemData(id);
	if (data) {
		int w, h;
		wxString lname = data->getLocname();
		wxString call = data->getCallSign();
		tqslTrace("MyFrame::OnLocTreeSel", "lname=%s, call=%s", _S(lname), _S(call));

		loc_add_label->GetSize(&w, &h);

		loc_edit_button->Enable();
		loc_delete_button->Enable();
		loc_prop_button->Enable();
		stn_menu->Enable(tm_s_Properties, true);
		loc_edit_label->SetLabel(wxT("Edit Station Location ") + call + wxT(": ") + lname);
		loc_edit_label->Wrap(w - 10);
		loc_delete_label->SetLabel(wxT("Delete Station Location ") + call + wxT(": ") + lname);
		loc_delete_label->Wrap(w - 10);
		loc_prop_label->SetLabel(wxT("Display Station Location Properties for ") + call + wxT(": ") + lname);
		loc_prop_label->Wrap(w - 10);
		loc_select_label->SetLabel(wxT(""));
	} else {
		LocTreeReset();
	}
}

void MyFrame::OnLocProperties(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLocProperties");
	LocTreeItemData *data = (LocTreeItemData *)loc_tree->GetItemData(loc_tree->GetSelection());
	if (data != NULL)
		displayLocProperties(data, this);
}

void MyFrame::OnLocDelete(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLocDelete");
	LocTreeItemData *data = (LocTreeItemData *)loc_tree->GetItemData(loc_tree->GetSelection());
	if (data == NULL)	// "Never happens"
		return;

	if (wxMessageBox(
wxT("This will permanently remove this station location from your system.\n")
wxT("ARE YOU SURE YOU WANT TO DELETE THIS LOCATION?"), wxT("Warning"), wxYES_NO|wxICON_QUESTION, this) == wxYES) {
		if (tqsl_deleteStationLocation(data->getLocname().mb_str()))
			wxLogError(wxT("%hs"), tqsl_getErrorString());
		loc_tree->Build();
		LocTreeReset();
	}
}

void MyFrame::OnLocEdit(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLocEdit");
	LocTreeItemData *data = (LocTreeItemData *)loc_tree->GetItemData(loc_tree->GetSelection());
	if (data == NULL)	// "Never happens"
		return;

   	tQSL_Location loc;
   	wxString selname;
	char errbuf[512];

	check_tqsl_error(tqsl_getStationLocation(&loc, data->getLocname().mb_str()));
	if (verify_cert(loc)) {		// Check if there is a certificate before editing
		check_tqsl_error(tqsl_getStationLocationErrors(loc, errbuf, sizeof(errbuf)));
		if (strlen(errbuf) > 0) {
			wxMessageBox(wxString::Format(wxT("%hs\nThe invalid data was ignored."), errbuf), wxT("Station Location data error"), wxOK|wxICON_EXCLAMATION, this);
		}
		char loccall[512];
		check_tqsl_error(tqsl_getLocationCallSign(loc, loccall, sizeof loccall));
		selname = run_station_wizard(this, loc, help, true, wxString::Format(wxT("Edit Station Location : %hs - %s"), loccall, data->getLocname().c_str()));
		check_tqsl_error(tqsl_endStationLocationCapture(&loc));
	}
	loc_tree->Build();
	LocTreeReset();
}

void MyFrame::OnLoginToLogbook(wxCommandEvent& WXUNUSED(event)) {
	tqslTrace("MyFrame::OnLoginToLogbook");
	wxString url = wxConfig::Get()->Read(wxT("LogbookURL"), DEFAULT_LOTW_LOGIN_URL);
	if (!url.IsEmpty())
		wxLaunchDefaultBrowser(url);
	return;
}

class CertPropDial : public wxDialog {
public:
	CertPropDial(tQSL_Cert cert, wxWindow *parent = 0);
	void closeMe(wxCommandEvent&) { wxWindow::Close(TRUE); }
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(CertPropDial, wxDialog)
	EVT_BUTTON(tc_CertPropDialButton, CertPropDial::closeMe)
END_EVENT_TABLE()

CertPropDial::CertPropDial(tQSL_Cert cert, wxWindow *parent) :
		wxDialog(parent, -1, wxT("Certificate Properties"), wxDefaultPosition, wxSize(400, 15 * LABEL_HEIGHT))
{
	tqslTrace("CertPropDial::CertPropDial", "cert=%lx", (void *)cert);
	const char *labels[] = {
		"Begins: ", "Expires: ", "Organization: ", "", "Serial: ", "Operator: ",
		"Call sign: ", "DXCC Entity: ", "QSO Start Date: ", "QSO End Date: ", "Key: "
	};

	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *prop_sizer = new wxBoxSizer(wxVERTICAL);

	int y = 10;
	for (int i = 0; i < int(sizeof labels / sizeof labels[0]); i++) {
		wxBoxSizer *line_sizer = new wxBoxSizer(wxHORIZONTAL);
		wxStaticText *st = new wxStaticText(this, -1, wxString(labels[i], wxConvLocal),
			wxDefaultPosition, wxSize(LABEL_WIDTH, LABEL_HEIGHT), wxALIGN_RIGHT);
		line_sizer->Add(st);
		char buf[128] = "";
		tQSL_Date date;
		int dxcc, keyonly;
		DXCC DXCC;
		long serial;
		switch (i) {
			case 0:
				tqsl_getCertificateKeyOnly(cert, &keyonly);
				if (keyonly)
					strncpy(buf, "N/A", sizeof buf);
				else if (!tqsl_getCertificateNotBeforeDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 1:
				tqsl_getCertificateKeyOnly(cert, &keyonly);
				if (keyonly)
					strncpy(buf, "N/A", sizeof buf);
				else if (!tqsl_getCertificateNotAfterDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 2:
				tqsl_getCertificateIssuerOrganization(cert, buf, sizeof buf);
				break;
			case 3:
				tqsl_getCertificateIssuerOrganizationalUnit(cert, buf, sizeof buf);
				break;
			case 4:
				tqsl_getCertificateKeyOnly(cert, &keyonly);
				if (keyonly)
					strncpy(buf, "N/A", sizeof buf);
				else {
					tqsl_getCertificateSerial(cert, &serial);
					snprintf(buf, sizeof buf, "%ld", serial);
				}
				break;
			case 5:
				tqsl_getCertificateKeyOnly(cert, &keyonly);
				if (keyonly)
					strncpy(buf, "N/A", sizeof buf);
				else
					tqsl_getCertificateAROName(cert, buf, sizeof buf);
				break;
			case 6:
				tqsl_getCertificateCallSign(cert, buf, sizeof buf);
				break;
			case 7:
				tqsl_getCertificateDXCCEntity(cert, &dxcc);
				DXCC.getByEntity(dxcc);
				strncpy(buf, DXCC.name(), sizeof buf);
				break;
			case 8:
				if (!tqsl_getCertificateQSONotBeforeDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 9:
				if (!tqsl_getCertificateQSONotAfterDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 10:
//				tqsl_getCertificateKeyOnly(cert, &keyonly);
//				if (keyonly)
//					strncpy(buf, "N/A", sizeof buf);
//				else {
					switch (tqsl_getCertificatePrivateKeyType(cert)) {
						case TQSL_PK_TYPE_ERR:
							wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
							strncpy(buf, "<ERROR>", sizeof buf);
							break;
						case TQSL_PK_TYPE_NONE:
							strncpy(buf, "None", sizeof buf);
							break;
						case TQSL_PK_TYPE_UNENC:
							strncpy(buf, "Unencrypted", sizeof buf);
							break;
						case TQSL_PK_TYPE_ENC:
							strncpy(buf, "Password protected", sizeof buf);
							break;
					}
//				}
				break;
		}
		line_sizer->Add(
			new wxStaticText(this, -1, wxString(buf, wxConvLocal))
		);
		prop_sizer->Add(line_sizer);
		y += LABEL_HEIGHT;
	}
	topsizer->Add(prop_sizer, 0, wxALL, 10);
	topsizer->Add(
		new wxButton(this, tc_CertPropDialButton, wxT("Close")),
		0, wxALIGN_CENTER | wxALL, 10
	);
	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	CenterOnParent();
}

void
displayCertProperties(CertTreeItemData *item, wxWindow *parent) {
	tqslTrace("displayCertProperties", "item=%lx", (void *)item);
	if (item != NULL) {
		CertPropDial dial(item->getCert(), parent);
		dial.ShowModal();
	}
}


class LocPropDial : public wxDialog {
public:
	LocPropDial(wxString locname, wxWindow *parent = 0);
	void closeMe(wxCommandEvent&) { wxWindow::Close(TRUE); }
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(LocPropDial, wxDialog)
	EVT_BUTTON(tl_LocPropDialButton, LocPropDial::closeMe)
END_EVENT_TABLE()

LocPropDial::LocPropDial(wxString locname, wxWindow *parent) :
		wxDialog(parent, -1, wxT("Station Location Properties"), wxDefaultPosition, wxSize(1000, 15 * LABEL_HEIGHT))
{
	tqslTrace("LocPropDial", "locname=%s", _S(locname));

	const char *fields[] = { "CALL", "Call sign: ",
				 "DXCC", "DXCC Entity: ",
				 "GRIDSQUARE", "Grid Square: ",
				 "ITUZ", "ITU Zone: ",
				 "CQZ", "CQ Zone: ",
				 "IOTA", "IOTA Locator: ",
				 "US_STATE", "State: ",
				 "US_COUNTY", "County: ",
				 "CA_PROVINCE", "Province: ",
				 "RU_OBLAST", "Oblast: ",
				 "CN_PROVINCE", "Province: ",
				 "AU_STATE", "State: " };

	tQSL_Location loc;
	check_tqsl_error(tqsl_getStationLocation(&loc, locname.mb_str()));

	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *prop_sizer = new wxBoxSizer(wxVERTICAL);

	int y = 10;
	char fieldbuf[512];
	for (int i = 0; i < int(sizeof fields / sizeof fields[0]); i+=2) {
		if (tqsl_getStationLocationField(loc, fields[i], fieldbuf, sizeof fieldbuf) == 0) {
			if (strlen(fieldbuf) > 0) {
				wxBoxSizer *line_sizer = new wxBoxSizer(wxHORIZONTAL);
				wxStaticText *st = new wxStaticText(this, -1, wxString(fields[i+1], wxConvLocal),
					wxDefaultPosition, wxSize(LABEL_WIDTH, LABEL_HEIGHT), wxALIGN_RIGHT);
				line_sizer->Add(st, 30);
				if (!strcmp(fields[i], "DXCC")) {
					int dxcc = strtol(fieldbuf, NULL, 10);
					const char *dxccname = NULL;
					tqsl_getDXCCEntityName(dxcc, &dxccname);
					strncpy(fieldbuf, dxccname, sizeof fieldbuf);
				}
				line_sizer->Add(
					new wxStaticText(this, -1, wxString(fieldbuf, wxConvLocal),
					wxDefaultPosition, wxSize(LABEL_WIDTH, LABEL_HEIGHT)), 70
				);
				prop_sizer->Add(line_sizer);
				y += LABEL_HEIGHT;
			}
		}
	}
	topsizer->Add(prop_sizer, 0, wxALL, 10);
	topsizer->Add(
		new wxButton(this, tl_LocPropDialButton, wxT("Close")),
				0, wxALIGN_CENTER | wxALL, 10
	);
	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	CenterOnParent();
}

void
displayLocProperties(LocTreeItemData *item, wxWindow *parent) {
	tqslTrace("displayLocProperties", "item=%lx", item);
	if (item != NULL) {
		LocPropDial dial(item->getLocname(), parent);
		dial.ShowModal();
	}
}

int
getPassword(char *buf, int bufsiz, void *callsign) {
	tqslTrace("getPassword", "buf=%lx, bufsiz=%d, callsign=%s", buf, bufsiz, callsign ? callsign : "NULL");
	wxString prompt(wxT("Enter the password to unlock the callsign certificate"));
	
	if (callsign) 
	    prompt = prompt + wxT(" for ") + wxString((const char *)callsign, wxConvLocal);

	tqslTrace("getPassword", "Probing for top window");
	wxWindow* top = wxGetApp().GetTopWindow();
	tqslTrace("getPassword", "Top window = 0x%lx", (void *)top);
	top->SetFocus();
	tqslTrace("getPassword", "Focus grabbed. About to pop up password dialog");
	GetPasswordDialog dial(top, wxT("Enter password"), prompt);
	if (dial.ShowModal() != wxID_OK) {
		tqslTrace("getPassword", "Password entry cancelled");
		return 1;
	}
	tqslTrace("getPassword", "Password entered OK");
	strncpy(buf, dial.Password().mb_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

void
displayTQSLError(const char *pre) {
	tqslTrace("displayTQSLError", "pre=%s", pre);
	wxString s(pre, wxConvLocal);
	s += wxT(":\n");
	s += wxString(tqsl_getErrorString(), wxConvLocal);
	wxMessageBox(s, wxT("Error"));
}


static wxString
flattenCallSign(const wxString& call) {
	tqslTrace("flattenCallSign", "call=%s", _S(call));
	wxString flat = call;
	size_t idx;
	while ((idx = flat.find_first_not_of(wxT("ABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_"))) != wxString::npos)
		flat[idx] = '_';
	return flat;
}

void tqslTrace(const char *name, const char *format, ...) {
	va_list ap;
	if (!diagFile) return;

	fprintf(diagFile, "%s: ", name);
	if (!format) {
		fprintf(diagFile, "\r\n");
		fflush(diagFile);
		return;
	}
	va_start (ap, format);
	vfprintf(diagFile, format, ap);
	va_end(ap);
	fprintf(diagFile, "\r\n");
	fflush(diagFile);
}

