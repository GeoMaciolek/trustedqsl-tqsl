/***************************************************************************
                          tqslcert.cpp  -  description
                             -------------------
    begin                : Wed Jun 12 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include <iostream>

#include "tqslapp.h"
#include "tqslcert.h"
#include "crqwiz.h"
#include "tqslcert_prefs.h"
#include "dxcc.h"
#include "tqsllib.h"
#include "tqslerrno.h"
#include <map>

#include "openssl/err.h"

#include "util.h"

#define VERSION "0.9"

/// GEOMETRY

#define LABEL_HEIGHT 15
#define LABEL_WIDTH 100

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_MENU(tc_Quit, MyFrame::OnQuit)
	EVT_MENU(tc_CRQWizard, MyFrame::CRQWizard)
	EVT_MENU(tc_Load, MyFrame::OnLoadCertificateFile)
	EVT_MENU(tc_Preferences, MyFrame::OnPreferences)
	EVT_MENU(tc_c_Properties, MyFrame::OnCertProperties)
	EVT_MENU(tc_c_Sign, MyFrame::OnSign)
	EVT_MENU(tc_c_Renew, MyFrame::CRQWizardRenew)
	EVT_MENU(tc_h_Contents, MyFrame::OnHelpContents)
	EVT_MENU(tc_h_About, MyFrame::OnHelpAbout)
	EVT_TREE_SEL_CHANGED(tc_CertTree, MyFrame::OnTreeSel)
END_EVENT_TABLE()

static int notifyCount;
static int notifyImport(int type, const char *message);

/////////// Application //////////////

IMPLEMENT_APP(CertApp)

CertApp::CertApp() : wxApp() {
	wxConfigBase::Set(new wxConfig("tqslcert"));
}

CertApp::~CertApp() {
	wxConfigBase *c = wxConfigBase::Set(0);
	if (c)
		delete c;
}

bool
CertApp::OnInit() {
	MyFrame *frame = new MyFrame("tQSL Certificates", 50, 50, 450, 400);
	frame->Show(TRUE);
	SetTopWindow(frame);
	notifyCount = 0;
	for (int i = 1; i < argc; i++) {
		if (tqsl_importCertFile(argv[i], notifyImport))
			wxMessageBox(tqsl_getErrorString(), "Error");
		else
			wxMessageBox(wxString::Format("%d certificates loaded", notifyCount), "Notice");
	}
	int ncerts = frame->cert_tree->Build();
	if (ncerts == 0 && wxMessageBox("You have no certificate with which to sign log submissions. "
		"Would you like to request a certificate now?", "Alert", wxYES_NO) == wxYES) {
		wxCommandEvent e;
		frame->CRQWizard(e);
	}
	return TRUE;
}

wxMenu *
makeCertificateMenu(bool enable) {
	wxMenu *cert_menu = new wxMenu;
	cert_menu->Append(tc_c_Properties, "&Properties");
	cert_menu->Enable(tc_c_Properties, enable);
	cert_menu->Append(tc_c_Sign, "&Sign File");
	cert_menu->Enable(tc_c_Sign, enable);
	cert_menu->Append(tc_c_Renew, "&Renew Certificate");
	cert_menu->Enable(tc_c_Renew, enable);
	return cert_menu;
}

/////////// Frame /////////////

MyFrame::MyFrame(const wxString& title, int x, int y, int w, int h) :
	wxFrame(0, -1, title, wxPoint(x, y), wxSize(w, h)),
	cert_tree(0), req(0) {

	wxMenu *file_menu = new wxMenu;

	file_menu->Append(tc_CRQWizard, "&New Certificate Request...");
	file_menu->AppendSeparator();
	file_menu->Append(tc_Load, "&Load Certificate File");
	file_menu->AppendSeparator();
	file_menu->Append(tc_Preferences, "&Preferences...");
	file_menu->AppendSeparator();
	file_menu->Append(tc_Quit, "E&xit\tAlt-X");

	cert_menu = makeCertificateMenu(false);

	wxMenu *help_menu = new wxMenu;

	help_menu->Append(tc_h_Contents, "&Contents");
	help_menu->AppendSeparator();
	help_menu->Append(tc_h_About, "&About");

	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, "&File");
	menu_bar->Append(cert_menu, "&Certificate");
	menu_bar->Append(help_menu, "&Help");
	SetMenuBar(menu_bar);

	cert_tree = new CertTree(this, tc_CertTree, wxDefaultPosition,
		wxDefaultSize, wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);

	cert_tree->SetBackgroundColour(wxColour(255, 255, 255));
	cert_tree->Build();

	help.UseConfig(wxConfig::Get());
	help.SetTempDir(".");
	wxString hhp = "tqslcert_help/tqslcert.hhp";
	if (!::wxFileExists(hhp))
		hhp = "tqsllib-apps/tqslcert_help/tqslcert.hhp";
	help.AddBook(hhp);
}

MyFrame::~MyFrame() {
}

void MyFrame::OnQuit(wxCommandEvent& WXUNUSED(event)) {
	Close(TRUE);
}

void MyFrame::OnPreferences(wxCommandEvent& WXUNUSED(event)) {
	Preferences dial(this);
	dial.ShowModal();
}

void MyFrame::OnHelpContents(wxCommandEvent& WXUNUSED(event)) {
	help.DisplayContents();
}

void MyFrame::OnHelpAbout(wxCommandEvent& WXUNUSED(event)) {
	wxMessageBox("tQSLCert V" VERSION "\n2002\nAmerican Radio Relay League",
		"About");
}

static int notifyImport(int type, const char *message) {
	if (TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_ERROR)
		return 1;
	if (TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_WARNING) {
		wxMessageBox(message, "Certificate Warning");
		return 0;
	}
	if (TQSL_CERT_CB_RESULT_TYPE(type) != TQSL_CERT_CB_PROMPT)
		return 0;
	const char *nametype = 0;
	const char *configkey = 0;
	bool default_prompt = false;
	switch (TQSL_CERT_CB_CERT_TYPE(type)) {
		case TQSL_CERT_CB_ROOT:
			nametype = "Trusted root";
			configkey = "NotifyRoot";
			default_prompt = true;
			break;
		case TQSL_CERT_CB_CA:
			nametype = "Certificate Authority";
			configkey = "NotifyCA";
			break;
		case TQSL_CERT_CB_USER:
			nametype = "User";
			configkey = "NotifyUser";
			break;
	}
	if (!nametype)
		return 0;
	wxConfig *config = (wxConfig *)wxConfig::Get();
	bool b;
	config->Read(configkey, &b, default_prompt);
	if (!b) {
		notifyCount++;
		return 0;
	}
	wxString s("Okay to install ");
	s = s + nametype + " certificate?\n\n" + message;
	if (wxMessageBox(s, "Install Certificate", wxYES_NO) == wxYES) {
		notifyCount++;
		return 0;
	}
	return 1;
}

void MyFrame::OnLoadCertificateFile(wxCommandEvent& WXUNUSED(event)) {
	notifyCount = 0;
	wxString file = wxFileSelector("Load file", "", "",
		TQSL_CRQ_FILE_EXT,
		"tQSL Certificate files (*." TQSL_CERT_FILE_EXT ")|*." TQSL_CERT_FILE_EXT
			"|All files (" ALLFILESWILD ")|" ALLFILESWILD,
		wxOPEN | wxFILE_MUST_EXIST);
	if (file.IsEmpty())
		return;
	if (tqsl_importCertFile(file.c_str(), notifyImport))
		wxMessageBox(tqsl_getErrorString(), "Error");
	else
		wxMessageBox(wxString::Format("%d certificates loaded", notifyCount), "Notice");
	cert_tree->Build();
}

void MyFrame::CRQWizardRenew(wxCommandEvent& event) {
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	req = 0;
	tQSL_Cert cert;
	wxString callSign, name, address1, address2, city, state, postalCode,
		country, emailAddress;
	if (data != NULL && (cert = data->getCert()) != 0) {	// Should be true
		char buf[256];
		req = new TQSL_CERT_REQ;
		if (!tqsl_getCertificateCallSign(cert, buf, sizeof buf)) {
			callSign = buf;
			req->callSign = callSign.c_str();
		}
		if (!tqsl_getCertificateAROName(cert, buf, sizeof buf)) {
			name = buf;
			req->name = name.c_str();
		}
		tqsl_getCertificateDXCCEntity(cert, &(req->dxccEntity));
		tqsl_getCertificateQSONotBeforeDate(cert, &(req->qsoNotBefore));
		tqsl_getCertificateQSONotAfterDate(cert, &(req->qsoNotAfter));
		if (!tqsl_getCertificateEmailAddress(cert, buf, sizeof buf)) {
			emailAddress = buf;
			req->emailAddress = emailAddress.c_str();
		}
		if (!tqsl_getCertificateRequestAddress1(cert, buf, sizeof buf)) {
			address1 = buf;
			req->address1 = address1.c_str();
		}
		if (!tqsl_getCertificateRequestAddress2(cert, buf, sizeof buf)) {
			address2 = buf;
			req->address2 = address2.c_str();
		}
		if (!tqsl_getCertificateRequestCity(cert, buf, sizeof buf)) {
			city = buf;
			req->city = city.c_str();
		}
		if (!tqsl_getCertificateRequestState(cert, buf, sizeof buf)) {
			state = buf;
			req->state = state.c_str();
		}
		if (!tqsl_getCertificateRequestPostalCode(cert, buf, sizeof buf)) {
			postalCode = buf;
			req->postalCode = postalCode.c_str();
		}
		if (!tqsl_getCertificateRequestCountry(cert, buf, sizeof buf)) {
			country = buf;
			req->country = country.c_str();
		}
	}
	CRQWizard(event);
	if (req)
		delete req;
	req = 0;
}

void MyFrame::CRQWizard(wxCommandEvent& event) {
	char renew = (req != 0) ? 1 : 0;
	wxWizard *wiz = wxWizard::Create(this, -1, "Certificate Request");
	CRQ_IntroPage *intro = new CRQ_IntroPage(wiz, req);
	CRQ_NamePage *name = new CRQ_NamePage(wiz, req);
	CRQ_EmailPage *email = new CRQ_EmailPage(wiz, req);
	wxSize size = intro->GetSize();
	if (name->GetSize().GetWidth() > size.GetWidth())
		size = name->GetSize();
	if (email->GetSize().GetWidth() > size.GetWidth())
		size = email->GetSize();
	CRQ_SignPage *sign = new CRQ_SignPage(wiz, size);
	wxWizardPageSimple::Chain(intro, name);
	wxWizardPageSimple::Chain(name, email);
	if (renew)
		sign->cert = ((CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection()))->getCert();
	else
		wxWizardPageSimple::Chain(email, sign);

	wiz->SetPageSize(size);

	if (wiz->RunWizard(intro)) {
		// Where to put it?
		wxString file = wxFileSelector("Save request", "", "request." TQSL_CRQ_FILE_EXT,
			TQSL_CRQ_FILE_EXT,
			"tQSL Cert Request files (*." TQSL_CRQ_FILE_EXT ")|*." TQSL_CRQ_FILE_EXT
				"|All files (" ALLFILESWILD ")|" ALLFILESWILD,
			wxSAVE | wxOVERWRITE_PROMPT);
		if (file.IsEmpty())
			wxMessageBox("Request cancelled", "Cancel");
		else {
			TQSL_CERT_REQ req;
			req.callSign = intro->callsign.c_str();
			req.name = name->name.c_str();
			req.address1 = name->addr1.c_str();
			req.address2 = name->addr2.c_str();
			req.city = name->city.c_str();
			req.state = name->state.c_str();
			req.postalCode = name->zip.c_str();
			if (name->country.IsEmpty())
				req.country = "USA";
			else
				req.country = name->country.c_str();
			req.emailAddress = email->email.c_str();
			req.password = "";
			req.dxccEntity = intro->dxcc;
			req.qsoNotBefore = intro->qsonotbefore;
			req.qsoNotAfter = intro->qsonotafter;
			req.signer = sign->cert;
			if (req.signer) {
				while (tqsl_beginSigning(req.signer, 0, getPassword)) {
					if (tQSL_Error != TQSL_PASSWORD_ERROR) {
						wxMessageBox(tqsl_getErrorString(), "Error");
						return;
					}
				}
			}
			req.renew = renew ? 1 : 0;
			if (tqsl_createCertRequest(file.c_str(), &req, 0))
				wxMessageBox(tqsl_getErrorString(), "Error");
			if (req.signer)
				tqsl_endSigning(req.signer);
		}
	}
	wiz->Destroy();
}

void MyFrame::OnTreeSel(wxTreeEvent& event) {
	wxTreeItemId id = event.GetItem();
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(id);
	cert_menu->Enable(tc_c_Properties, (data != NULL));
	cert_menu->Enable(tc_c_Sign, (data != NULL));
	cert_menu->Enable(tc_c_Renew, (data != NULL));
}

void MyFrame::OnCertProperties(wxCommandEvent& WXUNUSED(event)) {
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	if (data != NULL)
		displayCertProperties(data);
}

void MyFrame::OnSign(wxCommandEvent& WXUNUSED(event)) {
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	if (data == NULL)
		return;	 // No cert selected
	wxString tosign = wxFileSelector("Select the file to sign");
	if (tosign == "")
		return;
	char buf[4000];
	FILE *in = fopen(tosign.c_str(), "r");
	if (in == NULL) {
		wxMessageBox("Unable to open file", tosign);
		return;
	}
	int n = fread(buf, 1, sizeof buf, in);
	wxString sig = wxFileSelector("Select the file to contain the signature",
		"", "", "", ALLFILESWILD, wxSAVE|wxOVERWRITE_PROMPT);
	if (sig == "")
		return;
	tQSL_Cert cert = data->getCert();
	if (tqsl_beginSigning(cert, NULL, getPassword)) {
		if (tQSL_Error != TQSL_OPERATOR_ABORT)
			wxMessageBox(tqsl_getErrorString(), "Error");
		return;
	}
	int maxsig = 0;
	tqsl_getMaxSignatureSize(cert, &maxsig);
	int sigsize = maxsig;
	unsigned char *sigbuf = new unsigned char[maxsig];
	if (tqsl_signDataBlock(cert, (unsigned char *)buf, n, sigbuf, &sigsize))
		wxMessageBox(tqsl_getErrorString(), "Error");
	else {
wxMessageBox(wxString::Format("Signature is %d bytes", sigsize), "Okay");
		char *b64sig = new char[2*maxsig];
		if (tqsl_encodeBase64(sigbuf, sigsize, b64sig, 2*maxsig))
			wxMessageBox(tqsl_getErrorString(), "Error");
		else {
			FILE *out = fopen(sig, "w");
			fputs(b64sig, out);
			fclose(out);
		}
		delete[] b64sig;
	}
	delete[] sig;
	tqsl_endSigning(cert);
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
		wxDialog(parent, -1, "Certificate Properties", wxDefaultPosition, wxSize(400, 15 * LABEL_HEIGHT))
{
	char *labels[] = {
		"Begins: ", "Expires: ", "Organization: ", "", "Operator: ",
		"Call sign: ", "DXCC Entity: ", "QSO Start Date: ", "QSO End Date: ", "Key: "
	};

	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
	wxBoxSizer *prop_sizer = new wxBoxSizer(wxVERTICAL);

	int y = 10;
	for (int i = 0; i < int(sizeof labels / sizeof labels[0]); i++) {
		wxBoxSizer *line_sizer = new wxBoxSizer(wxHORIZONTAL);
		wxStaticText *st = new wxStaticText(this, -1, labels[i],
			wxDefaultPosition, wxSize(LABEL_WIDTH, LABEL_HEIGHT), wxALIGN_RIGHT);
		line_sizer->Add(st);
		char buf[128] = "";
		tQSL_Date date;
		int dxcc;
		DXCC DXCC;
		switch (i) {
			case 0:
				if (!tqsl_getCertificateNotBeforeDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 1:
				if (!tqsl_getCertificateNotAfterDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 2:
				tqsl_getCertificateIssuerOrganization(cert, buf, sizeof buf);
				break;
			case 3:
				tqsl_getCertificateIssuerOrganizationalUnit(cert, buf, sizeof buf);
				break;
			case 4:
				tqsl_getCertificateAROName(cert, buf, sizeof buf);
				break;
			case 5:
				tqsl_getCertificateCallSign(cert, buf, sizeof buf);
				break;
			case 6:
				tqsl_getCertificateDXCCEntity(cert, &dxcc);
				DXCC.getByEntity(dxcc);
				strncpy(buf, DXCC.name(), sizeof buf);
				break;
			case 7:
				if (!tqsl_getCertificateQSONotBeforeDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 8:
				if (!tqsl_getCertificateQSONotAfterDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 9:
				switch (tqsl_getCertificatePrivateKeyType(cert)) {
					case TQSL_PK_TYPE_ERR:
						wxMessageBox(tqsl_getErrorString(), "Error");
						strcpy(buf, "<ERROR>");
						break;
					case TQSL_PK_TYPE_NONE:
						strcpy(buf, "None");
						break;
					case TQSL_PK_TYPE_UNENC:
						strcpy(buf, "Unencrypted");
						break;
					case TQSL_PK_TYPE_ENC:
						strcpy(buf, "Password protected");
						break;
				}
				break;
		}
		line_sizer->Add(
			new wxStaticText(this, -1, buf)
		);
		prop_sizer->Add(line_sizer);
		y += LABEL_HEIGHT;
	}
	topsizer->Add(prop_sizer, 0, wxALL, 10);
	topsizer->Add(
		new wxButton(this, tc_CertPropDialButton, "Close"),
		0, wxALIGN_CENTER | wxALL, 10
	);
	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
}

void
displayCertProperties(CertTreeItemData *item) {

	if (item != NULL) {
		CertPropDial dial(item->getCert());
		dial.ShowModal();
	}
}

int
getPassword(char *buf, int bufsiz) {
	wxString pw = wxGetPasswordFromUser("Enter the password to unlock the private key", "Enter password");
	if (pw == "")
		return 1;
	strncpy(buf, pw.c_str(), bufsiz);
	return 0;
}

void
displayTQSLError(const char *pre) {
	wxString s(pre);
	s += ":\n";
	s += tqsl_getErrorString();
	wxMessageBox(s, "Error");
}

