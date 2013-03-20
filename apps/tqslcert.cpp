/***************************************************************************
                          tqslcert.cpp  -  description
                             -------------------
    begin                : Wed Jun 12 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: tqslcert.cpp,v 1.15 2013/03/01 13:02:41 k1mu Exp $
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
#include "tqslbuild.h"
#include "tqslerrno.h"
#include "util.h"
#include <map>

#include "util.h"

#include "key.xpm"

using namespace std;

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
static int showAllCerts(void);


/////////// Application //////////////

IMPLEMENT_APP(CertApp)

CertApp::CertApp() : wxApp() {
#ifdef __WXMAC__	// Tell wx to put these items on the proper menu
	wxApp::s_macAboutMenuItemId = long(tc_h_About);
	wxApp::s_macPreferencesMenuItemId = long(tc_Preferences);
	wxApp::s_macExitMenuItemId = long(tc_Quit);
#endif

	wxConfigBase::Set(new wxConfig(wxT("tqslcert")));
}

CertApp::~CertApp() {
	wxConfigBase *c = wxConfigBase::Set(0);
	if (c)
		delete c;
}

bool
CertApp::OnInit() {
	int major, minor;
	if (tqsl_getConfigVersion(&major, &minor)) {
		wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"), wxOK);
		exit (1);
	}
	MyFrame *frame = new MyFrame(wxT("tQSL Certificates"), 50, 50, 450, 400);
	frame->Show(TRUE);
	SetTopWindow(frame);
	if (argc > 1) {
		notifyData nd;
		for (int i = 1; i < argc; i++) {
			if (tqsl_importTQSLFile(wxString(argv[i]).mb_str(wxConvLocal), notifyImport, &nd))
				wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"), wxOK, frame);
			else
				wxConfig::Get()->Write(wxT("RequestPending"), wxT(""));
		}
		wxMessageBox(nd.Message(), wxT("Load Certificates"), wxOK, frame);
	}
	if (wxConfig::Get()->Read(wxT("HasRun")) == wxT("")) {
		wxConfig::Get()->Write(wxT("HasRun"), wxT("yes"));
		frame->DisplayHelp();
		wxMessageBox(wxT("Please review the introductory documentation before using this program."),
			wxT("Notice"), wxOK, frame);
	}
	int ncerts = frame->cert_tree->Build(CERTLIST_FLAGS | showAllCerts());
	if (ncerts == 0 && wxMessageBox(wxT("You have no certificate with which to sign log submissions.\n")
		wxT("Would you like to request a certificate now?"), wxT("Alert"), wxYES_NO, frame) == wxYES) {
		wxCommandEvent e;
		frame->CRQWizard(e);
	}
	wxString pend = wxConfig::Get()->Read(wxT("RequestPending"));
	if (pend != wxT("")) {
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
			wxConfig::Get()->Write(wxT("RequestPending"), wxT(""));
		} else {
			if (wxMessageBox(wxT("Are you ready to load your new certificate for ") + pend + wxT(" from LoTW now?"), wxT("Alert"), wxYES_NO, frame) == wxYES) {
				wxCommandEvent e;
				frame->OnLoadCertificateFile(e);
			}
		}
	}

	if (ncerts > 0) {
		TQ_WXCOOKIE cookie;
		wxTreeItemId it = frame->cert_tree->GetFirstChild(frame->cert_tree->GetRootItem(), cookie);
		while (it.IsOk()) {
			if (frame->cert_tree->GetItemText(it) == wxT("Test Certificate Authority")) {
				wxMessageBox(wxT("You must delete your beta-test certificates (the ones\n")
					wxT("listed under \"Test Certificate Authority\") to ensure proprer\n")
					wxT("operation of the TrustedQSL software."), wxT("Warning"), wxOK, frame);
				break;
			}
			it = frame->cert_tree->GetNextChild(frame->cert_tree->GetRootItem(), cookie);
		}

	}
	return TRUE;
}

wxMenu *
makeCertificateMenu(bool enable, bool keyonly) {
	wxMenu *cert_menu = new wxMenu;
	cert_menu->Append(tc_c_Properties, wxT("&Properties"));
	cert_menu->Enable(tc_c_Properties, enable);
	cert_menu->Append(tc_c_Export, wxT("&Save"));
	cert_menu->Enable(tc_c_Export, enable);
	cert_menu->Append(tc_c_Delete, wxT("&Delete"));
	cert_menu->Enable(tc_c_Delete, enable);
	if (!keyonly) {
//		cert_menu->Append(tc_c_Sign, "S&ign File");
//		cert_menu->Enable(tc_c_Sign, enable);
		cert_menu->Append(tc_c_Renew, wxT("&Renew Certificate"));
		cert_menu->Enable(tc_c_Renew, enable);
	}
	return cert_menu;
}

/////////// Frame /////////////

MyFrame::MyFrame(const wxString& title, int x, int y, int w, int h) :
	wxFrame(0, -1, title, wxPoint(x, y), wxSize(w, h)),
	cert_tree(0), req(0) {

	DocPaths docpaths(wxT("tqslcert"));
	wxMenu *file_menu = new wxMenu;

	file_menu->Append(tc_CRQWizard, wxT("&New Certificate Request..."));
	file_menu->AppendSeparator();
	file_menu->Append(tc_Load, wxT("&Load Certificate File"));
#ifndef __WXMAC__	// On Mac, Preferences not on File menu
	file_menu->AppendSeparator();
#endif
	file_menu->Append(tc_Preferences, wxT("&Preferences..."));
#ifndef __WXMAC__	// On Mac, Exit not on File menu
	file_menu->AppendSeparator();
#endif
	file_menu->Append(tc_Quit, wxT("E&xit\tAlt-X"));

	cert_menu = makeCertificateMenu(false);

	wxMenu *help_menu = new wxMenu;

	help.UseConfig(wxConfig::Get());
	wxString hhp = docpaths.FindAbsoluteValidPath(wxT("tqslcert.hhp"));
//cerr << "Help: " << wxFileNameFromPath(hhp) << endl;
	if (wxFileNameFromPath(hhp) != wxT("")) {
		if (help.AddBook(hhp)) {
			help_menu->Append(tc_h_Contents, wxT("&Contents"));
#ifndef __WXMAC__	// On Mac, About not on Help menu
			help_menu->AppendSeparator();
#endif
		}
	}
	help_menu->Append(tc_h_About, wxT("&About"));

	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, wxT("&File"));
	menu_bar->Append(cert_menu, wxT("&Certificate"));
	menu_bar->Append(help_menu, wxT("&Help"));
	SetMenuBar(menu_bar);

	SetIcon(wxIcon(key_xpm));

	cert_tree = new CertTree(this, tc_CertTree, wxDefaultPosition,
		wxDefaultSize, wxTR_DEFAULT_STYLE); //wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);

	cert_tree->SetBackgroundColour(wxColour(255, 255, 255));
	cert_tree->Build(CERTLIST_FLAGS | showAllCerts());
}

MyFrame::~MyFrame() {
}

void MyFrame::OnQuit(wxCommandEvent& WXUNUSED(event)) {
	Close(TRUE);
}

void MyFrame::OnPreferences(wxCommandEvent& WXUNUSED(event)) {
	Preferences dial(this);
	dial.ShowModal();
	cert_tree->Build(CERTLIST_FLAGS | showAllCerts());
}

void MyFrame::OnHelpContents(wxCommandEvent& WXUNUSED(event)) {
	DisplayHelp();
}

void MyFrame::OnHelpAbout(wxCommandEvent& WXUNUSED(event)) {
	wxString msg = wxT("TQSLCert V")  wxT(VERSION) wxT(" build ") wxT(BUILD) wxT("\n(c) 2001-2013\nAmerican Radio Relay League\n\n");
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
	wxMessageBox(msg, wxT("About"), wxOK|wxCENTRE|wxICON_INFORMATION, this);
}

void MyFrame::OnLoadCertificateFile(wxCommandEvent& WXUNUSED(event)) {
	LoadCertWiz lcw(this, &help, wxT("Load Certificate File"));
	lcw.RunWizard();
	cert_tree->Build(CERTLIST_FLAGS | showAllCerts());
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
		wxString file = wxFileSelector(wxT("Save request"), wxT(""), flattenCallSign(wiz.callsign) + wxT(".") wxT(TQSL_CRQ_FILE_EXT),
			wxT(TQSL_CRQ_FILE_EXT),
			wxT("tQSL Cert Request files (*.") wxT(TQSL_CRQ_FILE_EXT) wxT(")|*.") wxT(TQSL_CRQ_FILE_EXT)
				wxT("|All files (") wxT(ALLFILESWILD) wxT(")|") wxT(ALLFILESWILD),
			wxSAVE | wxOVERWRITE_PROMPT);
		if (file.IsEmpty())
			wxMessageBox(wxT("Request cancelled"), wxT("Cancel"));
		else {
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
						wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
						return;
					}
				}
			}
			req.renew = renew ? 1 : 0;
			if (tqsl_createCertRequest(file.mb_str(), &req, 0, 0))
				wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
			else {
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
				wxMessageBox(msg, wxT("tQSLCert"));
				wxConfig::Get()->Write(wxT("RequestPending"),wiz.callsign);
			}
			if (req.signer)
				tqsl_endSigning(req.signer);
			cert_tree->Build(CERTLIST_FLAGS | showAllCerts());
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
		wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
		return;
	}
	wxString file_default = flattenCallSign(wxString(call, wxConvLocal));
	int ko = 0;
	tqsl_getCertificateKeyOnly(data->getCert(), &ko);
	if (ko)
		file_default += wxT("-key-only");
	file_default += wxT(".p12");
	wxString path = wxConfig::Get()->Read(wxT("CertFilePath"), wxT(""));
	wxString filename = wxFileSelector(wxT("Enter PKCS#12 file to save to"), path,
		file_default, wxT(".p12"), wxT("PKCS#12 files (*.p12)|*.p12|All files (*.*)|*.*"),
		wxSAVE|wxOVERWRITE_PROMPT, this);
	if (filename == wxT(""))
		return;
	wxConfig::Get()->Write(wxT("CertFilePath"), wxPathOnly(filename));
	GetNewPasswordDialog dial(this, wxT("PKCS#12 Password"),
wxT("Enter the password for the .p12 file.\n\n")
wxT("If you are using a computer system that is shared\n")
wxT("with others, you should specify a password to\n")
wxT("protect this certificate. However, if you are using\n")
wxT("a computer in a private residence, no password need be specified.\n\n")
wxT("You will have to enter the password any time you\n")
wxT("load the file into TQSLCert (or any other PKCS#12\n")
wxT("compliant software)\n\n")
wxT("Leave the password blank and click 'Ok' unless you want to\n")
wxT("use a password.\n\n"), true, &help, wxT("save.htm"));
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
			wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"), wxOK, this);
		}
	} while (terr);
	if (tqsl_exportPKCS12File(data->getCert(), filename.mb_str(), dial.Password().mb_str()))
		wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"), wxOK, this);
	else
		wxMessageBox(wxString(wxT("Certificate saved in file ")) + filename, wxT("Notice"), wxOK, this);
	tqsl_endSigning(data->getCert());
}

void MyFrame::OnCertDelete(wxCommandEvent& WXUNUSED(event)) {
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	if (data == NULL)	// "Never happens"
		return;

	if (wxMessageBox(
wxT("WARNING! BE SURE YOU REALLY WANT TO DO THIS!\n\n")
wxT("This will permanently remove the certificate from your system.\n")
wxT("You will NOT be able to recover it by loading a .TQ6 file.\n")
wxT("You WILL be able to recover it from a .P12 file only if you\n")
wxT("have created one via the Certificate menu's Save command.\n\n")
wxT("ARE YOU SURE YOU WANT TO DELETE THE CERTIFICATE?"), wxT("Warning"), wxYES_NO|wxICON_QUESTION, this) == wxYES) {
		if (tqsl_deleteCertificate(data->getCert()))
			wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
		cert_tree->Build(CERTLIST_FLAGS | showAllCerts());
	}
}

/*

//	Just for testing

void MyFrame::OnSign(wxCommandEvent& WXUNUSED(event)) {
	CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
	if (data == NULL)
		return;	 // No cert selected
	wxString tosign = wxFileSelector("Select the file to sign");
	if (tosign == wxT(""))
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
	if (tqsl_beginSigning(cert, NULL, getPassword, 0)) {
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
		wxDialog(parent, -1, wxT("Certificate Properties"), wxDefaultPosition, wxSize(400, 15 * LABEL_HEIGHT))
{
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
							wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
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

	if (item != NULL) {
		CertPropDial dial(item->getCert(), parent);
		dial.ShowModal();
	}
}

int
getPassword(char *buf, int bufsiz, void *callsign) {
	wxString prompt(wxT("Enter the password to unlock the private key"));
	
	if (callsign) 
	    prompt = prompt + wxT(" for ") + wxString((const char *)callsign, wxConvLocal);

	GetPasswordDialog dial(wxGetApp().GetTopWindow(), wxT("Enter password"), prompt);
	if (dial.ShowModal() != wxID_OK)
		return 1;
	strncpy(buf, dial.Password().mb_str(), bufsiz);
	buf[bufsiz-1] = 0;
	return 0;
}

void
displayTQSLError(const char *pre) {
	wxString s(pre, wxConvLocal);
	s += wxT(":\n");
	s += wxString(tqsl_getErrorString(), wxConvLocal);
	wxMessageBox(s, wxT("Error"));
}


static wxString
flattenCallSign(const wxString& call) {
	wxString flat = call;
	size_t idx;
	while ((idx = flat.find_first_not_of(wxT("ABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_"))) != wxString::npos)
		flat[idx] = '_';
	return flat;
}

static int
showAllCerts(void) {

	wxConfig *config = (wxConfig *)wxConfig::Get();
	int ret = 0;
	bool b;
	config->Read(wxT("ShowSuperceded"), &b, false);
	if (b) 
		ret |= TQSL_SELECT_CERT_SUPERCEDED;
	config->Read(wxT("ShowExpired"), &b, false);
	if (b) 
		ret |= TQSL_SELECT_CERT_EXPIRED;
	return ret;
}

