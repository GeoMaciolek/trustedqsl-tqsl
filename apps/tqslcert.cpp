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
#include "tqslpaths.h"
#include "crqwiz.h"
#include "tqslcert_prefs.h"
#include "getpassword.h"
#include "loadcertwiz.h"
#include "dxcc.h"
#include "tqsllib.h"
#include "tqslerrno.h"
#include "util.h"
#include <map>

#include "openssl/err.h"

#include "util.h"

#include "version.h"

#include "tqslcertbuild.h"

/// GEOMETRY

#define LABEL_HEIGHT 15
#define LABEL_WIDTH 100

#define CERTLIST_FLAGS TQSL_SELECT_CERT_WITHKEYS

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_MENU(tc_Quit, MyFrame::OnQuit)
	EVT_MENU(tc_CRQWizard, MyFrame::CRQWizard)
	EVT_MENU(tc_Load, MyFrame::OnLoadCertificateFile)
	EVT_MENU(tc_Preferences, MyFrame::OnPreferences)
	EVT_MENU(tc_c_Properties, MyFrame::OnCertProperties)
	EVT_MENU(tc_c_Export, MyFrame::OnCertExport)
	EVT_MENU(tc_c_Delete, MyFrame::OnCertDelete)
//	EVT_MENU(tc_c_Import, MyFrame::OnCertImport)
//	EVT_MENU(tc_c_Sign, MyFrame::OnSign)
	EVT_MENU(tc_c_Renew, MyFrame::CRQWizardRenew)
	EVT_MENU(tc_h_Contents, MyFrame::OnHelpContents)
	EVT_MENU(tc_h_About, MyFrame::OnHelpAbout)
	EVT_TREE_SEL_CHANGED(tc_CertTree, MyFrame::OnTreeSel)
END_EVENT_TABLE()

static wxString flattenCallSign(const wxString& call);

static DocPaths docpaths("tqslcert");

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
	if (argc > 1) {
		notifyData nd;
		for (int i = 1; i < argc; i++) {
			if (tqsl_importTQSLFile(argv[i], notifyImport, &nd))
				wxMessageBox(tqsl_getErrorString(), "Error", wxOK, frame);
		}
		wxMessageBox(nd.Message(), "Load Certificates", wxOK, frame);
	}
	if (wxConfig::Get()->Read("HasRun") == "") {
		wxConfig::Get()->Write("HasRun", "yes");
		frame->DisplayHelp();
		wxMessageBox("Please review the introductory documentation before using this program.",
			"Notice", wxOK, frame);
	}
	int ncerts = frame->cert_tree->Build(CERTLIST_FLAGS);
	if (ncerts == 0 && wxMessageBox("You have no certificate with which to sign log submissions. "
		"Would you like to request a certificate now?", "Alert", wxYES_NO, frame) == wxYES) {
		wxCommandEvent e;
		frame->CRQWizard(e);
	}
	return TRUE;
}

wxMenu *
makeCertificateMenu(bool enable, bool keyonly) {
	wxMenu *cert_menu = new wxMenu;
	cert_menu->Append(tc_c_Properties, "&Properties");
	cert_menu->Enable(tc_c_Properties, enable);
	cert_menu->Append(tc_c_Export, "&Save");
	cert_menu->Enable(tc_c_Export, enable);
	cert_menu->Append(tc_c_Delete, "&Delete");
	cert_menu->Enable(tc_c_Delete, enable);
	if (!keyonly) {
//		cert_menu->Append(tc_c_Sign, "S&ign File");
//		cert_menu->Enable(tc_c_Sign, enable);
		cert_menu->Append(tc_c_Renew, "&Renew Certificate");
		cert_menu->Enable(tc_c_Renew, enable);
	}
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

	help.UseConfig(wxConfig::Get());
	wxString hhp = docpaths.FindAbsoluteValidPath("tqslcert.hhp");
//cerr << "Help: " << wxFileNameFromPath(hhp) << endl;
	if (wxFileNameFromPath(hhp) != "") {
		if (help.AddBook(hhp)) {
			help_menu->Append(tc_h_Contents, "&Contents");
			help_menu->AppendSeparator();
		}
	}
	help_menu->Append(tc_h_About, "&About");

	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, "&File");
	menu_bar->Append(cert_menu, "&Certificate");
	menu_bar->Append(help_menu, "&Help");
	SetMenuBar(menu_bar);

	cert_tree = new CertTree(this, tc_CertTree, wxDefaultPosition,
		wxDefaultSize, wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);

	cert_tree->SetBackgroundColour(wxColour(255, 255, 255));
	cert_tree->Build(CERTLIST_FLAGS);

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
	DisplayHelp();
}

void MyFrame::OnHelpAbout(wxCommandEvent& WXUNUSED(event)) {
	wxString msg = "TQSLCert V" VERSION "." BUILD "\n(c) 2001-2003\nAmerican Radio Relay League\n\n";
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

void MyFrame::OnLoadCertificateFile(wxCommandEvent& WXUNUSED(event)) {
	LoadCertWiz lcw(this, &help, "Load Certificate File");
	lcw.RunWizard();
	cert_tree->Build(CERTLIST_FLAGS);
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
		if (!tqsl_getCertificateIssuerOrganization(cert, buf, sizeof buf))
			strncpy(req->providerName, buf, sizeof req->providerName);
		if (!tqsl_getCertificateIssuerOrganizationalUnit(cert, buf, sizeof buf))
			strncpy(req->providerUnit, buf, sizeof req->providerUnit);
		if (!tqsl_getCertificateCallSign(cert, buf, sizeof buf)) {
			callSign = buf;
			strncpy(req->callSign, callSign.c_str(), sizeof (req->callSign));
		}
		if (!tqsl_getCertificateAROName(cert, buf, sizeof buf)) {
			name = buf;
			strncpy(req->name, name.c_str(), sizeof (req->name));
		}
		tqsl_getCertificateDXCCEntity(cert, &(req->dxccEntity));
		tqsl_getCertificateQSONotBeforeDate(cert, &(req->qsoNotBefore));
		tqsl_getCertificateQSONotAfterDate(cert, &(req->qsoNotAfter));
		if (!tqsl_getCertificateEmailAddress(cert, buf, sizeof buf)) {
			emailAddress = buf;
			strncpy(req->emailAddress, emailAddress.c_str(), sizeof (req->emailAddress));
		}
		if (!tqsl_getCertificateRequestAddress1(cert, buf, sizeof buf)) {
			address1 = buf;
			strncpy(req->address1, address1.c_str(), sizeof (req->address1));
		}
		if (!tqsl_getCertificateRequestAddress2(cert, buf, sizeof buf)) {
			address2 = buf;
			strncpy(req->address2, address2.c_str(), sizeof (req->address2));
		}
		if (!tqsl_getCertificateRequestCity(cert, buf, sizeof buf)) {
			city = buf;
			strncpy(req->city, city.c_str(), sizeof (req->city));
		}
		if (!tqsl_getCertificateRequestState(cert, buf, sizeof buf)) {
			state = buf;
			strncpy(req->state, state.c_str(), sizeof (req->state));
		}
		if (!tqsl_getCertificateRequestPostalCode(cert, buf, sizeof buf)) {
			postalCode = buf;
			strncpy(req->postalCode, postalCode.c_str(), sizeof (req->postalCode));
		}
		if (!tqsl_getCertificateRequestCountry(cert, buf, sizeof buf)) {
			country = buf;
			strncpy(req->country, country.c_str(), sizeof (req->country));
		}
	}
	CRQWizard(event);
	if (req)
		delete req;
	req = 0;
}

void MyFrame::CRQWizard(wxCommandEvent& event) {
	char renew = (req != 0) ? 1 : 0;
	tQSL_Cert cert = (renew ? ((CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection()))->getCert() : 0);
	CRQWiz wiz(req, cert, this, &help);
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
		// Where to put it?
		wxString file = wxFileSelector("Save request", "", flattenCallSign(wiz.callsign) + "." TQSL_CRQ_FILE_EXT,
			TQSL_CRQ_FILE_EXT,
			"tQSL Cert Request files (*." TQSL_CRQ_FILE_EXT ")|*." TQSL_CRQ_FILE_EXT
				"|All files (" ALLFILESWILD ")|" ALLFILESWILD,
			wxSAVE | wxOVERWRITE_PROMPT);
		if (file.IsEmpty())
			wxMessageBox("Request cancelled", "Cancel");
		else {
			TQSL_CERT_REQ req;
			strncpy(req.providerName, wiz.provider.organizationName, sizeof req.providerName);
			strncpy(req.providerUnit, wiz.provider.organizationalUnitName, sizeof req.providerUnit);
			strncpy(req.callSign, wiz.callsign.c_str(), sizeof req.callSign);
			strncpy(req.name, wiz.name.c_str(), sizeof req.name);
			strncpy(req.address1, wiz.addr1.c_str(), sizeof req.address1);
			strncpy(req.address2, wiz.addr2.c_str(), sizeof req.address2);
			strncpy(req.city, wiz.city.c_str(), sizeof req.city);
			strncpy(req.state, wiz.state.c_str(), sizeof req.state);
			strncpy(req.postalCode, wiz.zip.c_str(), sizeof req.postalCode);
			if (wiz.country.IsEmpty())
				strncpy(req.country, "USA", sizeof req.country);
			else
				strncpy(req.country, wiz.country.c_str(), sizeof req.country);
			strncpy(req.emailAddress, wiz.email.c_str(), sizeof req.emailAddress);
			strncpy(req.password, wiz.password.c_str(), sizeof req.password);
			req.dxccEntity = wiz.dxcc;
			req.qsoNotBefore = wiz.qsonotbefore;
			req.qsoNotAfter = wiz.qsonotafter;
			req.signer = wiz.cert;
			if (req.signer) {
				while (tqsl_beginSigning(req.signer, 0, getPassword, 0)) {
					if (tQSL_Error != TQSL_PASSWORD_ERROR) {
						wxMessageBox(tqsl_getErrorString(), "Error");
						return;
					}
				}
			}
			req.renew = renew ? 1 : 0;
			if (tqsl_createCertRequest(file.c_str(), &req, 0, 0))
				wxMessageBox(tqsl_getErrorString(), "Error");
			else {
				wxString msg = "Your may now send your new certificate request (";
				msg += file ;
				msg += ")";
				if (wiz.provider.emailAddress[0] != 0)
					msg += wxString("\nto:\n   ") + wiz.provider.emailAddress;
				if (wiz.provider.url[0] != 0) {
					msg += "\n";
					if (wiz.provider.emailAddress[0] != 0)
						msg += "or ";
					msg += wxString("see:\n   ") + wiz.provider.url;
				}
				wxMessageBox(msg, "tQSLCert");
			}
			if (req.signer)
				tqsl_endSigning(req.signer);
			cert_tree->Build(CERTLIST_FLAGS);
		}
	}
}

void MyFrame::OnTreeSel(wxTreeEvent& event) {
	wxTreeItemId id = event.GetItem();
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(id);
	cert_menu->Enable(tc_c_Properties, (data != NULL));
	cert_menu->Enable(tc_c_Export, (data != NULL));
	cert_menu->Enable(tc_c_Delete, (data != NULL));
	int keyonly = 0;
	if (data != NULL)
		tqsl_getCertificateKeyOnly(data->getCert(), &keyonly);
//	cert_menu->Enable(tc_c_Sign, (data != NULL) && !keyonly);
	cert_menu->Enable(tc_c_Renew, (data != NULL) && !keyonly);
}

void MyFrame::OnCertProperties(wxCommandEvent& WXUNUSED(event)) {
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	if (data != NULL)
		displayCertProperties(data, this);
}

void MyFrame::OnCertExport(wxCommandEvent& WXUNUSED(event)) {
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	if (data == NULL)	// "Never happens"
		return;

	char call[40];
	if (tqsl_getCertificateCallSign(data->getCert(), call, sizeof call)) {
		wxMessageBox(tqsl_getErrorString(), "Error");
		return;
	}
	wxString file_default = flattenCallSign(wxString(call));
	int ko = 0;
	tqsl_getCertificateKeyOnly(data->getCert(), &ko);
	if (ko)
		file_default += "-key-only";
	file_default += ".p12";
	wxString path = wxConfig::Get()->Read("CertFilePath", "");
	wxString filename = wxFileSelector("Enter PKCS#12 file to save to", path,
		file_default, ".p12", "PKCS#12 files (*.p12)|*.p12|All files (*.*)|*.*",
		wxSAVE|wxOVERWRITE_PROMPT, this);
	if (filename == "")
		return;
	wxConfig::Get()->Write("CertFilePath", wxPathOnly(filename));
	GetNewPasswordDialog dial(this, "PKCS#12 Password",
"Enter password for the PKCS#12 file.\n\n"
"You will have to enter this password any time you\n"
"load the file into TQSLCert (or any other PKCS#12\n"
"compliant software)", true, &help, "save.htm");
	if (dial.ShowModal() != wxID_OK)
		return;	// Cancelled
	int terr;
	do {
		terr = tqsl_beginSigning(data->getCert(), 0, getPassword, 0);
		if (terr) {
			if (tQSL_Error == TQSL_PASSWORD_ERROR)
				continue;
			if (tQSL_Error == TQSL_OPERATOR_ABORT)
				return;
			wxMessageBox(tqsl_getErrorString(), "Error", wxOK, this);
		}
	} while (terr);
	if (tqsl_exportPKCS12File(data->getCert(), filename.c_str(), dial.Password().c_str()))
		wxMessageBox(tqsl_getErrorString(), "Error", wxOK, this);
	else
		wxMessageBox(wxString("Certificate saved in file ") + filename, "Notice", wxOK, this);
	tqsl_endSigning(data->getCert());
}

void MyFrame::OnCertDelete(wxCommandEvent& WXUNUSED(event)) {
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	if (data == NULL)	// "Never happens"
		return;

	if (wxMessageBox(
"This will permanently remove the certificate from your system.\n\n"
"Are you sure this is what you want to do?", "Warning", wxYES_NO|wxICON_QUESTION, this) == wxYES) {
		if (tqsl_deleteCertificate(data->getCert()))
			wxMessageBox(tqsl_getErrorString(), "Error");
		cert_tree->Build(CERTLIST_FLAGS);
	}
}

/*

//	Just for testing

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

*/

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
		"Begins: ", "Expires: ", "Organization: ", "", "Serial: ", "Operator: ",
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
		int dxcc, keyonly;
		DXCC DXCC;
		long serial;
		switch (i) {
			case 0:
				tqsl_getCertificateKeyOnly(cert, &keyonly);
				if (keyonly)
					strcpy(buf, "N/A");
				else if (!tqsl_getCertificateNotBeforeDate(cert, &date))
					tqsl_convertDateToText(&date, buf, sizeof buf);
				break;
			case 1:
				tqsl_getCertificateKeyOnly(cert, &keyonly);
				if (keyonly)
					strcpy(buf, "N/A");
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
					strcpy(buf, "N/A");
				else {
					tqsl_getCertificateSerial(cert, &serial);
					sprintf(buf, "%ld", serial);
				}
				break;
			case 5:
				tqsl_getCertificateKeyOnly(cert, &keyonly);
				if (keyonly)
					strcpy(buf, "N/A");
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
//					strcpy(buf, "N/A");
//				else {
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
//				}
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
	CenterOnParent();
}

void
displayCertProperties(CertTreeItemData *item, wxWindow *parent) {

	if (item != NULL) {
		CertPropDial dial(item->getCert(), parent);
		dial.ShowModal();
	}
}

int
getPassword(char *buf, int bufsiz, void *) {
	GetPasswordDialog dial(wxGetApp().GetTopWindow(), "Enter password", "Enter the password to unlock the private key");
	if (dial.ShowModal() != wxID_OK)
		return 1;
	strncpy(buf, dial.Password().c_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

void
displayTQSLError(const char *pre) {
	wxString s(pre);
	s += ":\n";
	s += tqsl_getErrorString();
	wxMessageBox(s, "Error");
}


static wxString
flattenCallSign(const wxString& call) {
	wxString flat = call;
	size_t idx;
	while ((idx = flat.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_")) != wxString::npos)
		flat[idx] = '_';
	return flat;
}
