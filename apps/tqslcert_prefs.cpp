/***************************************************************************
                          tqslcert_prefs.cpp  -  description
                             -------------------
    begin                : Mon Jul 1 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include "tqslcert_prefs.h"
#include "wx/notebook.h"
#include "wx/sizer.h"
#include "wx/button.h"
#include "wx/stattext.h"
#include "wx/statbox.h"
#include "wx/config.h"

BEGIN_EVENT_TABLE(Preferences, wxDialog)
	EVT_BUTTON(ID_OK_BUT, Preferences::OnOK)
	EVT_BUTTON(ID_CAN_BUT, Preferences::OnCancel)
END_EVENT_TABLE()

Preferences::Preferences(wxWindow *parent) : wxDialog(parent, -1, wxString(wxT("Preferences"))) {
	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);

	wxNotebook *notebook = new wxNotebook(this, -1);
	topsizer->Add(notebook, 1, wxGROW);
	keyprefs = new KeyPrefs(notebook);
	certprefs = new CertPrefs(notebook);

	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton *button = new wxButton(this, ID_OK_BUT, wxT("OK") );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_CAN_BUT, wxT("Cancel") );
	butsizer->Add(button, 0, wxALIGN_LEFT | wxALL, 10);

	topsizer->Add(butsizer, 0, wxALIGN_CENTER);

	notebook->AddPage(keyprefs, wxT("Import"));
	notebook->AddPage(certprefs, wxT("Certificates"));

	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	SetWindowStyle(GetWindowStyle() | wxWS_EX_VALIDATE_RECURSIVELY);
	
}

void Preferences::OnOK(wxCommandEvent& WXUNUSED(event)) {
	if (!keyprefs->TransferDataFromWindow())
		return;
	if (!certprefs->TransferDataFromWindow())
		return;
	Close(TRUE);
}

KeyPrefs::KeyPrefs(wxWindow *parent) : wxPanel(parent, -1) {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	bool b;
	SetAutoLayout(TRUE);
	wxBoxSizer *sizer = new wxStaticBoxSizer(
		new wxStaticBox(this, -1, wxT("Import Notification")),
		wxVERTICAL);
	root_cb = new wxCheckBox(this, ID_PREF_ROOT_CB, wxT("Trusted root certificates"));
	sizer->Add(root_cb);
	config->Read(wxT("NotifyRoot"), &b, true);
	root_cb->SetValue(b);
	ca_cb = new wxCheckBox(this, ID_PREF_CA_CB, wxT("Certificate Authority certificates"));
	sizer->Add(ca_cb);
	config->Read(wxT("NotifyCA"), &b, false);
	ca_cb->SetValue(b);
	user_cb = new wxCheckBox(this, ID_PREF_USER_CB, wxT("User certificates"));
	sizer->Add(user_cb);
	config->Read(wxT("NotifyUser"), &b, false);
	user_cb->SetValue(b);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
}

bool KeyPrefs::TransferDataFromWindow() {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->Write(wxT("NotifyRoot"), root_cb->GetValue());
	config->Write(wxT("NotifyCA"), ca_cb->GetValue());
	config->Write(wxT("NotifyUser"), user_cb->GetValue());
	return TRUE;
}

CertPrefs::CertPrefs(wxWindow *parent) : wxPanel(parent, -1) {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	bool b;
	SetAutoLayout(TRUE);
	wxBoxSizer *sizer = new wxStaticBoxSizer(
		new wxStaticBox(this, -1, wxT("Certificates to display")),
		wxVERTICAL);
	showSuperceded_cb = new wxCheckBox(this, ID_PREF_ALLCERT_CB, wxT("Renewed certificates"));
	sizer->Add(showSuperceded_cb);
	config->Read(wxT("ShowSuperceded"), &b, false);
	showSuperceded_cb->SetValue(b);
	showExpired_cb = new wxCheckBox(this, ID_PREF_ALLCERT_CB, wxT("Expired certificates"));
	sizer->Add(showExpired_cb);
	config->Read(wxT("ShowExpired"), &b, false);
	showExpired_cb->SetValue(b);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
}

bool CertPrefs::TransferDataFromWindow() {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->Write(wxT("ShowSuperceded"), showSuperceded_cb->GetValue());
	config->Write(wxT("ShowExpired"), showExpired_cb->GetValue());
	return TRUE;
}

