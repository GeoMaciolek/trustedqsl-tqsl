/***************************************************************************
                          loadcertwiz.cpp  -  description
                             -------------------
    begin                : Wed Aug 6 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "loadcertwiz.h"
#include "getpassword.h"
#include "tqslcertctrls.h"
#include "tqsllib.h"
#include "tqslerrno.h"
#include "tqslcert.h"

wxString
notifyData::Message() const {
	
	wxString msgs = status;
	if (!wxIsEmpty(status))
		msgs = status + wxT("\n");

	return wxString::Format(
		wxT("%s")
		wxT("Root Certificates:\t\tLoaded: %d  Duplicate: %d  Error: %d\n")
		wxT("CA Certificates:\t\tLoaded: %d  Duplicate: %d  Error: %d\n")
		wxT("User Certificates:\t\tLoaded: %d  Duplicate: %d  Error: %d\n")
		wxT("Private Keys:\t\t\tLoaded: %d  Duplicate: %d  Error: %d\n")
		wxT("Configuration Data:\tLoaded: %d  Duplicate: %d  Error: %d"),
		msgs.c_str(),
		root.loaded, root.duplicate, root.error,
		ca.loaded, ca.duplicate, ca.error,
		user.loaded, user.duplicate, user.error,
		pkey.loaded, pkey.duplicate, pkey.error,
		config.loaded, config.duplicate, config.error);
}

int
notifyImport(int type, const char *message, void *data) {
	if (TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_PROMPT) {
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
		config->Read(wxString(configkey, wxConvLocal), &b, default_prompt);
		if (!b)
			return 0;
		wxString s(wxT("Okay to install "));
		s = s + wxString(nametype, wxConvLocal) + wxT(" certificate?\n\n") + wxString(message, wxConvLocal);
		if (wxMessageBox(s, wxT("Install Certificate"), wxYES_NO) == wxYES)
			return 0;
		return 1;
	} // end TQSL_CERT_CB_PROMPT
	if (TQSL_CERT_CB_CALL_TYPE(type) == TQSL_CERT_CB_RESULT && data) {
		// Keep count
		notifyData *nd = (notifyData *)data;
		notifyData::counts *counts = 0;
		switch (TQSL_CERT_CB_CERT_TYPE(type)) {
			case TQSL_CERT_CB_ROOT:
				counts = &(nd->root);
				break;
			case TQSL_CERT_CB_CA:
				counts = &(nd->ca);
				break;
			case TQSL_CERT_CB_USER:
				counts = &(nd->user);
				break;
			case TQSL_CERT_CB_PKEY:
				counts = &(nd->pkey);
				break;
			case TQSL_CERT_CB_CONFIG:
				counts = &(nd->config);
				break;
		}
		if (counts) {
			if (message) nd->status = nd->status + wxString(message, wxConvLocal) + wxT("\n");
			switch (TQSL_CERT_CB_RESULT_TYPE(type)) {
				case TQSL_CERT_CB_DUPLICATE:
					counts->duplicate++;
					break;
				case TQSL_CERT_CB_ERROR:
					counts->error++;
					// wxMessageBox(wxString(message, wxConvLocal), wxT("Error"));
					break;
				case TQSL_CERT_CB_LOADED:
					counts->loaded++;
					break;
			}
		}
	}
	if (TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_ERROR)
		return 1;	// Errors get posted later
	if (TQSL_CERT_CB_CALL_TYPE(type) == TQSL_CERT_CB_RESULT
		|| TQSL_CERT_CB_RESULT_TYPE(type) == TQSL_CERT_CB_DUPLICATE) {
//		wxMessageBox(message, "Certificate Notice");
		return 0;
	}
	return 1;
}

static wxHtmlHelpController *pw_help = 0;
static wxString pw_helpfile;

static int
GetNewPassword(char *buf, int bufsiz, void *) {
	GetNewPasswordDialog dial(0, wxT("New Password"),
wxT("Enter a password for this private key.\n\n")
wxT("If you are using a computer system that is\n")
wxT("shared with others, you should specify a\n")
wxT("password to protect this key. However, if\n")
wxT("you are using a computer in a private residence\n")
wxT("no password need be specified.\n\n")
wxT("This password will have to be entered each time\n")
wxT("you use the key/certificate for signing or when\n")
wxT("saving the key.\n\n")
wxT("Leave the password blank and click 'Ok' unless you want to\n")
wxT("use a password.\n\n"), true, pw_help, pw_helpfile);
	if (dial.ShowModal() == wxID_OK) {
		strncpy(buf, dial.Password().mb_str(), bufsiz);
		buf[bufsiz-1] = 0;
		return 0;
	}
	return 1;
}


LoadCertWiz::LoadCertWiz(wxWindow *parent, wxHtmlHelpController *help, const wxString& title) :
	ExtWizard(parent, help, title), _nd(0) {

	LCW_FinalPage *final = new LCW_FinalPage(this);
	LCW_IntroPage *intro = new LCW_IntroPage(this, final);
	LCW_P12PasswordPage *p12pw = new LCW_P12PasswordPage(this);
	wxWizardPageSimple::Chain(intro, p12pw);
	wxWizardPageSimple::Chain(p12pw, final);
	_first = intro;
	_parent = parent;
	_final = final;
	_p12pw = p12pw;
	AdjustSize();
	CenterOnParent();
}

LCW_IntroPage::LCW_IntroPage(LoadCertWiz *parent, LCW_Page *tq6next)
	: LCW_Page(parent), _tq6next(tq6next) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("Select the type of certificate file to load:"));
	sizer->Add(st, 0, wxALL, 10);

	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);
	_p12but = new wxRadioButton(this, ID_LCW_P12, wxT(""), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	_p12but->SetValue(true);
	butsizer->Add(_p12but, 0, wxALIGN_TOP, 0);
	butsizer->Add(new wxStaticText(this, -1,
wxT("PKCS#12 (.p12) certificate file - A file you've saved that contains\n")
wxT("a certificate and/or private key. This file may be password\n")
wxT("protected and you'll need to provide a password in order to open it.")),
		0, wxLEFT, 5);

	sizer->Add(butsizer, 0, wxALL, 10);

	butsizer = new wxBoxSizer(wxHORIZONTAL);
	butsizer->Add(new wxRadioButton(this, ID_LCW_TQ6, wxT(""), wxDefaultPosition, wxDefaultSize, 0),
		0, wxALIGN_TOP, 0);
	butsizer->Add(new wxStaticText(this, -1,
wxT("TQSL (.tq6) certificate file - A file you received from a certificate\n")
wxT("issuer that contains the issued certificate and/or configuration data.")),
		0, wxLEFT, 5);

	sizer->Add(butsizer, 0, wxALL, 10);
	AdjustPage(sizer, wxT("lcf.htm"));
}

static void
export_new_cert(ExtWizard *_parent, const char *filename) {
	long newserial;
	if (!tqsl_getSerialFromTQSLFile(filename, &newserial)) {

		MyFrame *frame = (MyFrame *)(((LoadCertWiz *)_parent)->Parent());
		TQ_WXCOOKIE cookie;
		wxTreeItemId root = frame->cert_tree->GetRootItem();
		wxTreeItemId prov = frame->cert_tree->GetFirstChild(root, cookie); // First child is the providers
		wxTreeItemId item = frame->cert_tree->GetFirstChild(prov, cookie);// Then it's certs
		while (item.IsOk()) {
			tQSL_Cert cert;
			CertTreeItemData *id = frame->cert_tree->GetItemData(item);
			if (id && (cert = id->getCert())) {
				long serial;
				if (!tqsl_getCertificateSerial(cert, &serial)) {
					if (serial == newserial) {
						wxCommandEvent e;
	    					if (wxMessageBox(
wxT("You will not be able to use this tq6 file to recover your\n")
wxT("certificate if it gets lost. For security purposes, you should\n")
wxT("back up your certificate on removable media for safe-keeping.\n\n")
wxT("Would you like to back up your certificate now?"), wxT("Warning"), wxYES_NO|wxICON_QUESTION, _parent) == wxNO) {
							return;
						}
						frame->cert_tree->SelectItem(item);
						frame->OnCertExport(e);
						break;
					}
				}
			}
			item = frame->cert_tree->GetNextChild(prov, cookie);
		}
	}
}

bool
LCW_IntroPage::TransferDataFromWindow() {
	wxString ext(wxT("p12"));
	wxString wild(wxT("PKCS#12 certificate files (*.p12)|*.p12"));
	if (!_p12but->GetValue()) {
		ext = wxT("tq6");
		wild = wxT("TQSL certificate files (*.tq6)|*.tq6");
	}
	wild += wxT("|All files (*.*)|*.*");

	wxString path = wxConfig::Get()->Read(wxT("CertFilePath"), wxT(""));
	wxString filename = wxFileSelector(wxT("Select Certificate File"), path,
		wxT(""), ext, wild, wxOPEN|wxFILE_MUST_EXIST);
	if (filename == wxT("")) {
		// Cancelled!
		SetNext(0);
	} else {
		((LoadCertWiz *)_parent)->ResetNotifyData();
		wxConfig::Get()->Write(wxT("CertFilePath"), wxPathOnly(filename));
	 	if (!_p12but->GetValue()) {
			SetNext(_tq6next);
			_tq6next->SetPrev(this);
			if (tqsl_importTQSLFile(filename.mb_str(), notifyImport,
				((LoadCertWiz *)_parent)->GetNotifyData()))
				wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
			else {
				wxConfig::Get()->Write(wxT("RequestPending"), wxT(""));
				export_new_cert(_parent, filename.mb_str());
			}
		} else {
			// First try with no password
			if (!tqsl_importPKCS12File(filename.mb_str(), "", 0, GetNewPassword, notifyImport, 
				((LoadCertWiz *)_parent)->GetNotifyData())) {
				SetNext(((LoadCertWiz *)_parent)->Final());
				((LoadCertWiz *)_parent)->Final()->SetPrev(this);
			} else {
				SetNext(((LoadCertWiz *)_parent)->P12PW());
				((LoadCertWiz *)_parent)->P12PW()->SetPrev(this);
				((LCW_P12PasswordPage*)GetNext())->SetFilename(filename);
			}
		}
	}
	return true;
}

LCW_FinalPage::LCW_FinalPage(LoadCertWiz *parent) : LCW_Page(parent) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("Loading complete"));
	sizer->Add(st, 0, wxALL, 10);
	wxSize tsize = getTextSize(this);
	tc_status = new wxTextCtrl(this, -1, wxT(""),wxDefaultPosition, tsize.Scale(40, 16), wxTE_MULTILINE|wxTE_READONLY);
	sizer->Add(tc_status, 1, wxALL|wxEXPAND, 10);
	AdjustPage(sizer);
}

void
LCW_FinalPage::refresh() {
	const notifyData *nd = ((LoadCertWiz *)_parent)->GetNotifyData();
	if (nd)
		tc_status->SetValue(nd->Message());
	else
		tc_status->SetValue(wxT("No status information available"));
}

LCW_P12PasswordPage::LCW_P12PasswordPage(LoadCertWiz *parent) : LCW_Page(parent) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("Enter the password to unlock the .p12 file:"));
	sizer->Add(st, 0, wxALL, 10);

	_pwin = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
	sizer->Add(_pwin, 0, wxLEFT|wxRIGHT|wxEXPAND, 10);
	tc_status = new wxStaticText(this, -1, wxT(""));
	sizer->Add(tc_status, 0, wxALL, 10);

	AdjustPage(sizer, wxT("lcf1.htm"));
}

bool
LCW_P12PasswordPage::TransferDataFromWindow() {
	wxString _pw = _pwin->GetValue();
	pw_help = Parent()->GetHelp();
	pw_helpfile = wxT("lcf2.htm");
	if (tqsl_importPKCS12File(_filename.mb_str(), _pw.mb_str(), 0, GetNewPassword, notifyImport,
		((LoadCertWiz *)_parent)->GetNotifyData())) {
		if (tQSL_Error == TQSL_PASSWORD_ERROR) {
			tc_status->SetLabel(wxT("Password error"));
			return false;
		} else
			wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
	}
	tc_status->SetLabel(wxT(""));
	return true;
}
