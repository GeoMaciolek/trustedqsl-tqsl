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

wxString
notifyData::Message() const {
	return wxString::Format(
		wxT("Root Certificates:\n     Loaded: %d  Duplicate: %d  Error: %d\n"
		"CA Certificates:\n     Loaded: %d  Duplicate: %d  Error: %d\n"
		"User Certificates:\n     Loaded: %d  Duplicate: %d  Error: %d\n"
		"Private Keys:\n     Loaded: %d  Duplicate: %d  Error: %d\n"
		"Configuration Data:\n     Loaded: %d  Duplicate: %d  Error: %d"),
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
			switch (TQSL_CERT_CB_RESULT_TYPE(type)) {
				case TQSL_CERT_CB_DUPLICATE:
					counts->duplicate++;
					break;
				case TQSL_CERT_CB_ERROR:
					counts->error++;
					wxMessageBox(wxString(message, wxConvLocal), wxT("Error"));
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
wxT("Enter password for private key.\n\n"
"This password will have to be entered each time\n"
"you use the key/certificate for signing or when\n"
"saving the key."), true, pw_help, pw_helpfile);
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
	butsizer->Add(_p12but, 0, wxALIGN_TOP, 0);
	butsizer->Add(new wxStaticText(this, -1,
wxT("PKCS#12 (.p12) certificate file - A file you've saved that contains\n"
"a certificate and/or private key. This file is typically password\n"
"protected and you'll need to provide a password in order to open it.")),
		0, wxLEFT, 5);

	sizer->Add(butsizer, 0, wxALL, 10);

	butsizer = new wxBoxSizer(wxHORIZONTAL);
	butsizer->Add(new wxRadioButton(this, ID_LCW_TQ6, wxT(""), wxDefaultPosition, wxDefaultSize, 0),
		0, wxALIGN_TOP, 0);
	butsizer->Add(new wxStaticText(this, -1,
wxT("TQSL (.tq6) certificate file - A file you received from a certificate\n"
"issuer that contains the issued certificate and/or configuration data.")),
		0, wxLEFT, 5);

	sizer->Add(butsizer, 0, wxALL, 10);
	AdjustPage(sizer, wxT("lcf.htm"));
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
		} else
			((LCW_P12PasswordPage*)GetNext())->SetFilename(filename);
	}
	return true;
}

LCW_FinalPage::LCW_FinalPage(LoadCertWiz *parent) : LCW_Page(parent) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("Loading complete"));
	sizer->Add(st, 0, wxALL, 10);
	tc_status = new wxStaticText(this, -1, wxT(""));
	sizer->Add(tc_status, 1, wxALL|wxEXPAND, 10);
	AdjustPage(sizer);
}

void
LCW_FinalPage::refresh() {
	const notifyData *nd = ((LoadCertWiz *)_parent)->GetNotifyData();
	if (nd)
		tc_status->SetLabel(nd->Message());
	else
		tc_status->SetLabel(wxT("No status information available"));
}

LCW_P12PasswordPage::LCW_P12PasswordPage(LoadCertWiz *parent) : LCW_Page(parent) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("Enter password to unlock PKCS#12 file:"));
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
