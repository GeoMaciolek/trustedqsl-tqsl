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

Preferences::Preferences(wxWindow *parent) : wxDialog(parent, -1, wxString("Preferences")) {
	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);

	wxNotebook *notebook = new wxNotebook(this, -1);
	wxNotebookSizer *nbs = new wxNotebookSizer(notebook);
	topsizer->Add(nbs, 1, wxGROW);
	keyprefs = new KeyPrefs(notebook);
	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton *button = new wxButton(this, ID_OK_BUT, "OK" );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_CAN_BUT, "Cancel" );
	butsizer->Add(button, 0, wxALIGN_LEFT | wxALL, 10);

	topsizer->Add(butsizer, 0, wxALIGN_CENTER);

	notebook->AddPage(keyprefs, "Import");

	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	SetWindowStyle(GetWindowStyle() | wxWS_EX_VALIDATE_RECURSIVELY);
	
}

void Preferences::OnOK(wxCommandEvent& WXUNUSED(event)) {
	if (!keyprefs->TransferDataFromWindow())
		return;
	Close(TRUE);
}

KeyPrefs::KeyPrefs(wxWindow *parent) : wxPanel(parent, -1) {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	bool b;
	SetAutoLayout(TRUE);
	wxBoxSizer *sizer = new wxStaticBoxSizer(
		new wxStaticBox(this, -1, "Import Notification"),
		wxVERTICAL);
	root_cb = new wxCheckBox(this, ID_PREF_ROOT_CB, "Trusted root certificates");
	sizer->Add(root_cb);
	config->Read("NotifyRoot", &b, true);
	root_cb->SetValue(b);
	ca_cb = new wxCheckBox(this, ID_PREF_CA_CB, "Certificate Authority certificates");
	sizer->Add(ca_cb);
	config->Read("NotifyCA", &b, false);
	ca_cb->SetValue(b);
	user_cb = new wxCheckBox(this, ID_PREF_USER_CB, "User certificates");
	sizer->Add(user_cb);
	config->Read("NotifyUser", &b, false);
	user_cb->SetValue(b);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
}

bool KeyPrefs::TransferDataFromWindow() {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->Write("NotifyRoot", root_cb->GetValue());
	config->Write("NotifyCA", ca_cb->GetValue());
	config->Write("NotifyUser", user_cb->GetValue());
	return TRUE;
}
