/***************************************************************************
                          tqslcert_prefs.cpp  -  description
                             -------------------
    begin                : Mon Jul 1 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include "tqsl_prefs.h"
#include "wx/notebook.h"
#include "wx/sizer.h"
#include "wx/button.h"
#include "wx/stattext.h"
#include "wx/statbox.h"
#include "wx/config.h"
#include "tqsllib.h"

#include <iostream>

BEGIN_EVENT_TABLE(Preferences, wxDialog)
	EVT_BUTTON(ID_OK_BUT, Preferences::OnOK)
	EVT_BUTTON(ID_CAN_BUT, Preferences::OnCancel)
END_EVENT_TABLE()

Preferences::Preferences(wxWindow *parent) : wxDialog(parent, -1, wxString("Preferences")) {
	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);

	wxNotebook *notebook = new wxNotebook(this, -1);
	wxNotebookSizer *nbs = new wxNotebookSizer(notebook);
	topsizer->Add(nbs, 1, wxGROW);
	fileprefs = new FilePrefs(notebook);

	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton *button = new wxButton(this, ID_OK_BUT, "OK" );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_CAN_BUT, "Cancel" );
	butsizer->Add(button, 0, wxALIGN_LEFT | wxALL, 10);

	topsizer->Add(butsizer, 0, wxALIGN_CENTER);

	notebook->AddPage(fileprefs, "Files");

	modemap = new ModeMap(notebook);

	notebook->AddPage(modemap, "ADIF Modes");

	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	SetWindowStyle(GetWindowStyle() | wxWS_EX_VALIDATE_RECURSIVELY);
	
}

void Preferences::OnOK(wxCommandEvent& WXUNUSED(event)) {
	if (!fileprefs->TransferDataFromWindow())
		return;
	Close(TRUE);
}

BEGIN_EVENT_TABLE(ModeMap, wxPanel)
	EVT_BUTTON(ID_PREF_MODE_DELETE, ModeMap::OnDelete)
	EVT_BUTTON(ID_PREF_MODE_ADD, ModeMap::OnAdd)
END_EVENT_TABLE()

#define MODE_TEXT_WIDTH 15

ModeMap::ModeMap(wxWindow *parent) : wxPanel(parent, -1) {
	SetAutoLayout(TRUE);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString('M', MODE_TEXT_WIDTH), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, "Custom ADIF mode mappings:"), 0, wxTOP|wxLEFT|wxRIGHT, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

	map = new wxListBox(this, ID_PREF_MODE_MAP, wxDefaultPosition, wxSize(char_width,(char_height*10)));
	hsizer->Add(map, 0);

	wxBoxSizer *vsizer = new wxBoxSizer(wxVERTICAL);

	vsizer->Add(new wxButton(this, ID_PREF_MODE_DELETE, "Delete"), 0, wxBOTTOM, 10);
	vsizer->Add(new wxButton(this, ID_PREF_MODE_ADD, "Add..."), 0);

	hsizer->Add(vsizer, 0, wxLEFT, 10);

	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 10);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);

	SetModeList();
}

void ModeMap::SetModeList() {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	wxString key, value;
	long cookie;

	modemap.clear();
	map->Clear();
	config->SetPath("/modeMap");
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, "");
		modemap.insert(make_pair(key, value));
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath("/");
	for (ModeSet::iterator it = modemap.begin(); it != modemap.end(); it++) {
		map->Append(it->first + " -> " + it->second, (void *)it->first.c_str());
	}
}

void ModeMap::OnDelete(wxCommandEvent &) {
	int sel = map->GetSelection();
	if (sel >= 0) {
		const char *keystr = (const char *)map->GetClientData(sel);
		if (keystr) {
			wxConfig *config = (wxConfig *)wxConfig::Get();
			config->SetPath("/modeMap");
			config->DeleteEntry(keystr, TRUE);
			config->Flush(FALSE);
			SetModeList();
		}
	}
}

void ModeMap::OnAdd(wxCommandEvent &) {
	AddMode add_dial(this);
	int val = add_dial.ShowModal();
	if (val == ID_OK_BUT && add_dial.key != "" && add_dial.value != "") {
		wxConfig *config = (wxConfig *)wxConfig::Get();
		config->SetPath("/modeMap");
		config->Write(add_dial.key, add_dial.value);
		config->Flush(FALSE);
		SetModeList();
	}
}

bool ModeMap::TransferDataFromWindow() {
	return TRUE;
}

BEGIN_EVENT_TABLE(AddMode, wxDialog)
	EVT_BUTTON(ID_OK_BUT, AddMode::OnOK)
	EVT_BUTTON(ID_CAN_BUT, AddMode::OnCancel)
END_EVENT_TABLE()

AddMode::AddMode(wxWindow *parent) : wxDialog(parent, -1, wxString("Add ADIF mode")) {
	SetAutoLayout(TRUE);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString('M', MODE_TEXT_WIDTH), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, "Add ADIF mode mapping:"), 0, wxALL, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

	hsizer->Add(new wxStaticText(this, -1, "ADIF Mode:"), 0);
		
	adif = new wxTextCtrl(this, ID_PREF_ADD_ADIF, "", wxPoint(0, 0),
		wxSize(char_width,(char_height*3)/2));
	hsizer->Add(adif, 0, wxLEFT, 5);

	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, "Resulting TQSL mode:"), 0, wxLEFT|wxRIGHT, 10);

	modelist = new wxListBox(this, ID_PREF_ADD_MODES, wxDefaultPosition, wxSize(char_width,(char_height*10)));
	sizer->Add(modelist, 0, wxLEFT|wxRIGHT, 10);

	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton *button = new wxButton(this, ID_OK_BUT, "OK" );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_CAN_BUT, "Cancel" );
	butsizer->Add(button, 0, wxALIGN_LEFT | wxALL, 10);

	sizer->Add(butsizer, 0, wxALIGN_CENTER);

	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	int n;
	if (tqsl_getNumMode(&n) == 0) {
		for (int i = 0; i < n; i++) {
			const char *modestr;
			if (tqsl_getMode(i, &modestr, 0) == 0) {
				modelist->Append(modestr);
			}
		}
	}
}

void AddMode::OnOK(wxCommandEvent& WXUNUSED(event)) {
	key = adif->GetValue().Trim(TRUE).Trim(FALSE).MakeUpper();
	int sel = modelist->GetSelection();
	if (sel >= 0)
		value = modelist->GetString(sel);
	EndModal(ID_OK_BUT);
}

#define FILE_TEXT_WIDTH 30

FilePrefs::FilePrefs(wxWindow *parent) : wxPanel(parent, -1) {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	SetAutoLayout(TRUE);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString('M', FILE_TEXT_WIDTH), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, "Cabrillo file extensions:"), 0, wxTOP|wxLEFT|wxRIGHT, 10);
	wxString cab = config->Read("CabrilloFiles", DEFAULT_CABRILLO_FILES);
	cabrillo = new wxTextCtrl(this, ID_PREF_FILE_CABRILLO, cab, wxPoint(0, 0),
		wxSize(char_width,(char_height*3)/2));
	sizer->Add(cabrillo, 0, wxLEFT|wxRIGHT, 10);
	sizer->Add(new wxStaticText(this, -1, "ADIF file extensions:"), 0, wxTOP|wxLEFT|wxRIGHT, 10);
	wxString adi = config->Read("ADIFFiles", DEFAULT_ADIF_FILES);
	adif = new wxTextCtrl(this, ID_PREF_FILE_ADIF, adi, wxPoint(0, 0),
		wxSize(char_width,(char_height*3)/2));
	sizer->Add(adif, 0, wxLEFT|wxRIGHT, 10);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
}

static wxString
fix_ext_str(const wxString& oldexts) {
	static char *delims = ".,;: ";

	char *str = new char[oldexts.Length() + 1];
	strcpy(str, oldexts.c_str());
	wxString exts;
	char *tok = strtok(str, delims);
	while (tok) {
		if (exts != "")
			exts += " ";
		exts += tok;
		tok = strtok(NULL, delims);
	}
	return exts;
}

bool FilePrefs::TransferDataFromWindow() {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->SetPath("/");
	config->Write("CabrilloFiles", fix_ext_str(cabrillo->GetValue()));
	config->Write("ADIFFiles", fix_ext_str(adif->GetValue()));
	return TRUE;
}
