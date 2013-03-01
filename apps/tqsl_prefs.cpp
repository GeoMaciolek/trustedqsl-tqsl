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
#include "wx/sizer.h"
#include "wx/button.h"
#include "wx/stattext.h"
#include "wx/statbox.h"
#include "wx/config.h"
#include "tqsllib.h"

#include <iostream>
#include <utility>

using namespace std;

#if defined(__APPLE__)
  #define HEIGHT_ADJ(x) ((x)*4/2)
#else
  #define HEIGHT_ADJ(x) ((x)*3/2)
#endif

BEGIN_EVENT_TABLE(Preferences, wxDialog)
	EVT_BUTTON(ID_OK_BUT, Preferences::OnOK)
	EVT_BUTTON(ID_CAN_BUT, Preferences::OnCancel)
	EVT_BUTTON(ID_HELP_BUT, Preferences::OnHelp)
END_EVENT_TABLE()

Preferences::Preferences(wxWindow *parent, wxHtmlHelpController *help)
	: wxDialog(parent, -1, wxString(wxT("Preferences"))), _help(help) {
	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);

	notebook = new wxNotebook(this, -1);
	topsizer->Add(notebook, 1, wxGROW);
	fileprefs = new FilePrefs(notebook);

	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton *button = new wxButton(this, ID_HELP_BUT, wxT("Help") );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_OK_BUT, wxT("OK") );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_CAN_BUT, wxT("Cancel") );
	butsizer->Add(button, 0, wxALIGN_LEFT | wxALL, 10);

	topsizer->Add(butsizer, 0, wxALIGN_CENTER);

	notebook->AddPage(fileprefs, wxT("Options"));

	modemap = new ModeMap(notebook);
	notebook->AddPage(modemap, wxT("ADIF Modes"));

	contestmap = new ContestMap(notebook);
	notebook->AddPage(contestmap, wxT("Cabrillo Specs"));

	//don't let the user play with these
	//onlinePrefs=new OnlinePrefs(notebook);
	//notebook->AddPage(onlinePrefs, wxT("Server Setup"));

	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	SetWindowStyle(GetWindowStyle() | wxWS_EX_VALIDATE_RECURSIVELY);
	CenterOnParent();
}

void Preferences::OnOK(wxCommandEvent& WXUNUSED(event)) {
	if (!fileprefs->TransferDataFromWindow() /* || !onlinePrefs->TransferDataFromWindow()*/)
		return;
	Close(true);
}

void Preferences::OnHelp(wxCommandEvent& WXUNUSED(event)) {
	if (_help) {
		wxString file(wxT("pref.htm"));
		int idx = notebook->GetSelection();
		if (idx >= 0)
			file = ((PrefsPanel *)(notebook->GetPage(idx)))->HelpFile();
		_help->Display(file);
	}
}

BEGIN_EVENT_TABLE(ModeMap, PrefsPanel)
	EVT_BUTTON(ID_PREF_MODE_DELETE, ModeMap::OnDelete)
	EVT_BUTTON(ID_PREF_MODE_ADD, ModeMap::OnAdd)
END_EVENT_TABLE()

#define MODE_TEXT_WIDTH 15

ModeMap::ModeMap(wxWindow *parent) : PrefsPanel(parent, wxT("pref-adi.htm")) {
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT('M'), MODE_TEXT_WIDTH), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, wxT("Custom ADIF mode mappings:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

	map = new wxListBox(this, ID_PREF_MODE_MAP, wxDefaultPosition, wxSize(char_width,(char_height*10)));
	hsizer->Add(map, 1, wxEXPAND, 0);

	wxBoxSizer *vsizer = new wxBoxSizer(wxVERTICAL);

	vsizer->Add(new wxButton(this, ID_PREF_MODE_ADD, wxT("Add...")), 0, wxBOTTOM, 10);
	delete_but = new wxButton(this, ID_PREF_MODE_DELETE, wxT("Delete"));
	vsizer->Add(delete_but, 0);

	hsizer->Add(vsizer, 0, wxLEFT, 10);

	sizer->Add(hsizer, 1, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, 10);
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
	config->SetPath(wxT("/modeMap"));
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, wxT(""));
		modemap.insert(make_pair(key, value));
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath(wxT("/"));
	for (ModeSet::iterator it = modemap.begin(); it != modemap.end(); it++) {
		map->Append(it->first + wxT(" -> ") + it->second, (void*)it->first.c_str()); //char_str on >=2.9
	}
	if (map->GetCount() > 0)
		map->SetSelection(0);
	delete_but->Enable(map->GetSelection() >= 0);
}

void ModeMap::OnDelete(wxCommandEvent &) {
	int sel = map->GetSelection();
	if (sel >= 0) {
		const char *keystr = (const char *)map->GetClientData(sel);
		if (keystr) {
			wxConfig *config = (wxConfig *)wxConfig::Get();
			config->SetPath(wxT("/modeMap"));
			config->DeleteEntry(wxString(keystr, wxConvLocal), true);
			config->Flush(false);
			SetModeList();
		}
	}
}

void ModeMap::OnAdd(wxCommandEvent &) {
	AddMode add_dial(this);
	int val = add_dial.ShowModal();
	if (val == ID_OK_BUT && add_dial.key != wxT("") && add_dial.value != wxT("")) {
		wxConfig *config = (wxConfig *)wxConfig::Get();
		config->SetPath(wxT("/modeMap"));
		config->Write(add_dial.key, add_dial.value);
		config->Flush(false);
		SetModeList();
	}
}

bool ModeMap::TransferDataFromWindow() {
	return true;
}

BEGIN_EVENT_TABLE(AddMode, wxDialog)
	EVT_BUTTON(ID_OK_BUT, AddMode::OnOK)
	EVT_BUTTON(ID_CAN_BUT, AddMode::OnCancel)
END_EVENT_TABLE()

AddMode::AddMode(wxWindow *parent) : wxDialog(parent, -1, wxString(wxT("Add ADIF mode"))) {
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT('M'), MODE_TEXT_WIDTH), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, wxT("Add ADIF mode mapping:")), 0, wxALL, 10);

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

	hsizer->Add(new wxStaticText(this, -1, wxT("ADIF Mode:")), 0);
		
	adif = new wxTextCtrl(this, ID_PREF_ADD_ADIF, wxT(""), wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	hsizer->Add(adif, 0, wxLEFT, 5);

	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Resulting TQSL mode:")), 0, wxLEFT|wxRIGHT, 10);

	modelist = new wxListBox(this, ID_PREF_ADD_MODES, wxDefaultPosition, wxSize(char_width,(char_height*10)));
	sizer->Add(modelist, 0, wxLEFT|wxRIGHT, 10);

	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton *button = new wxButton(this, ID_OK_BUT, wxT("OK") );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_CAN_BUT, wxT("Cancel") );
	butsizer->Add(button, 0, wxALIGN_LEFT | wxALL, 10);

	sizer->Add(butsizer, 0, wxALIGN_CENTER);

	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();

	int n;
	if (tqsl_getNumMode(&n) == 0) {
		for (int i = 0; i < n; i++) {
			const char *modestr;
			if (tqsl_getMode(i, &modestr, 0) == 0) {
				modelist->Append(wxString(modestr, wxConvLocal));
			}
		}
	}
}

void AddMode::OnOK(wxCommandEvent& WXUNUSED(event)) {
	key = adif->GetValue().Trim(true).Trim(false).MakeUpper();
	int sel = modelist->GetSelection();
	if (sel >= 0)
		value = modelist->GetString(sel);
	EndModal(ID_OK_BUT);
}

#define FILE_TEXT_WIDTH 30

FilePrefs::FilePrefs(wxWindow *parent) : PrefsPanel(parent, wxT("pref-opt.htm")) {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT('M'), FILE_TEXT_WIDTH), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, wxT("Cabrillo file extensions:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);
	wxString cab = config->Read(wxT("CabrilloFiles"), DEFAULT_CABRILLO_FILES);
	cabrillo = new wxTextCtrl(this, ID_PREF_FILE_CABRILLO, cab, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(cabrillo, 0, wxLEFT|wxRIGHT, 10);
	sizer->Add(new wxStaticText(this, -1, wxT("ADIF file extensions:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);
	wxString adi = config->Read(wxT("ADIFFiles"), DEFAULT_ADIF_FILES);
	adif = new wxTextCtrl(this, ID_PREF_FILE_ADIF, adi, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(adif, 0, wxLEFT|wxRIGHT, 10);
	badcalls = new wxCheckBox(this, ID_PREF_FILE_BADCALLS, wxT("Allow nonamateur call signs"));
	bool allow = false;
	config->Read(wxT("BadCalls"), &allow);
	badcalls->SetValue(allow);
	sizer->Add(badcalls, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	daterange = new wxCheckBox(this, ID_PREF_FILE_BADCALLS, wxT("Prompt for QSO Date range when signing"));
	allow = true;
	config->Read(wxT("DateRange"), &allow);
	daterange->SetValue(allow);
	sizer->Add(daterange, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
}

static wxString
fix_ext_str(const wxString& oldexts) {
	static const char *delims = ".,;: ";

	char *str = new char[oldexts.Length() + 1];
	strcpy(str, oldexts.mb_str());
	wxString exts;
	char *tok = strtok(str, delims);
	while (tok) {
		if (exts != wxT(""))
			exts += wxT(" ");
		exts += wxString(tok, wxConvLocal);
		tok = strtok(NULL, delims);
	}
	return exts;
}

bool FilePrefs::TransferDataFromWindow() {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->SetPath(wxT("/"));
	config->Write(wxT("CabrilloFiles"), fix_ext_str(cabrillo->GetValue()));
	config->Write(wxT("ADIFFiles"), fix_ext_str(adif->GetValue()));
	config->Write(wxT("BadCalls"), badcalls->GetValue());
	config->Write(wxT("DateRange"), daterange->GetValue());
	return true;
}

BEGIN_EVENT_TABLE(OnlinePrefs, PrefsPanel)
	EVT_CHECKBOX(ID_PREF_ONLINE_DEFAULT, OnlinePrefs::OnShowHide)
END_EVENT_TABLE()

OnlinePrefs::OnlinePrefs(wxWindow *parent) : PrefsPanel(parent, wxT("pref-opt.htm")) {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->SetPath(wxT("/LogUpload"));
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT('M'), FILE_TEXT_WIDTH), &char_width, &char_height);
	
	wxString uplURL = config->Read(wxT("UploadURL"), DEFAULT_UPL_URL);
	wxString uplPOST = config->Read(wxT("PostField"), DEFAULT_UPL_FIELD);
	wxString uplStatusRE = config->Read(wxT("StatusRegex"), DEFAULT_UPL_STATUSRE);
	wxString uplStatOK = config->Read(wxT("StatusSuccess"), DEFAULT_UPL_STATUSOK);
	wxString uplMsgRE = config->Read(wxT("MessageRegex"), DEFAULT_UPL_MESSAGERE);
	bool uplVerifyCA=config->Read(wxT("VerifyCA"), DEFAULT_UPL_VERIFYCA);

	defaults=(
		(uplURL==DEFAULT_UPL_URL) &&
		(uplPOST==DEFAULT_UPL_FIELD) &&
		(uplStatusRE==DEFAULT_UPL_STATUSRE) &&
		(uplStatOK==DEFAULT_UPL_STATUSOK) &&
		(uplMsgRE==DEFAULT_UPL_MESSAGERE) &&
		(uplVerifyCA==DEFAULT_UPL_VERIFYCA));




	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	useDefaults=new wxCheckBox(this, ID_PREF_ONLINE_DEFAULT, wxT("Use Defaults"));
	useDefaults->SetValue(defaults);
	sizer->Add(useDefaults, 0, wxTop|wxCENTER|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Upload URL:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	
	uploadURL = new wxTextCtrl(this, ID_PREF_ONLINE_URL, uplURL, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(uploadURL, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("HTTP POST Field:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	
	postField = new wxTextCtrl(this, ID_PREF_ONLINE_FIELD, uplPOST, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(postField, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Status RegEx:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	statusRegex = new wxTextCtrl(this, ID_PREF_ONLINE_STATUSRE, uplStatusRE, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(statusRegex, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Successful Status Message:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	statusSuccess = new wxTextCtrl(this, ID_PREF_ONLINE_STATUSOK, uplStatOK, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(statusSuccess, 0, wxLEFT|wxRIGHT, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("Message RegEx:")), 0, wxTOP|wxLEFT|wxRIGHT|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);
	messageRegex= new wxTextCtrl(this, ID_PREF_ONLINE_MESSAGERE, uplMsgRE, wxPoint(0, 0),
		wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(messageRegex, 0, wxLEFT|wxRIGHT, 10);

	verifyCA = new wxCheckBox(this, ID_PREF_ONLINE_VERIFYCA, wxT("Verify server certificate"));
	verifyCA->SetValue(uplVerifyCA);
	sizer->Add(verifyCA, 0, wxLEFT|wxRIGHT|wxTOP|wxRESERVE_SPACE_EVEN_IF_HIDDEN, 10);

	config->SetPath(wxT("/"));
	
	

	SetSizer(sizer);

	sizer->Fit(this);
	sizer->SetSizeHints(this);
	ShowHide();
}

void OnlinePrefs::ShowHide() {
	defaults=useDefaults->GetValue();
	for (int i=1; i<12; i++) GetSizer()->Show(i, !defaults); //12 items in sizer; hide all but checkbox

	Layout();
  //wxNotebook caches best size
	GetParent()->InvalidateBestSize();
  GetParent()->Fit();
	GetGrandParent()->Fit();
}

bool OnlinePrefs::TransferDataFromWindow() {
	wxConfig *config = (wxConfig *)wxConfig::Get();

	if (defaults) {
		config->DeleteGroup(wxT("/LogUpload"));
	} else {
		config->SetPath(wxT("/LogUpload"));
		config->Write(wxT("UploadURL"), uploadURL->GetValue());
		config->Write(wxT("PostField"), postField->GetValue());
		config->Write(wxT("StatusRegex"), statusRegex->GetValue());
		config->Write(wxT("StatusSuccess"), statusSuccess->GetValue());
		config->Write(wxT("MessageRegex"), messageRegex->GetValue());
		config->Write(wxT("VerifyCA"), verifyCA->GetValue());
		config->SetPath(wxT("/"));
	}

	return true;
}

BEGIN_EVENT_TABLE(ContestMap, PrefsPanel)
	EVT_BUTTON(ID_PREF_CAB_DELETE, ContestMap::OnDelete)
	EVT_BUTTON(ID_PREF_CAB_ADD, ContestMap::OnAdd)
	EVT_BUTTON(ID_PREF_CAB_EDIT, ContestMap::OnEdit)
END_EVENT_TABLE()

ContestMap::ContestMap(wxWindow *parent) : PrefsPanel(parent, wxT("pref-cab.htm")) {
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT("M")), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	sizer->Add(new wxStaticText(this, -1, wxT("Cabrillo CONTEST definitions:")), 0, wxTOP|wxLEFT|wxRIGHT, 10);
	
	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);

	grid = new wxGrid(this, -1, wxDefaultPosition, wxDefaultSize);

	grid->CreateGrid(10, 3);
	grid->SetLabelSize(wxHORIZONTAL, HEIGHT_ADJ(char_height));
	grid->SetLabelSize(wxVERTICAL, 0);
	grid->SetColumnWidth(0, char_width*15);
	grid->SetColumnWidth(1, char_width*4);
	grid->SetColumnWidth(2, char_width*3);
	grid->SetLabelValue(wxHORIZONTAL, wxT("CONTEST"), 0);
	grid->SetLabelValue(wxHORIZONTAL, wxT("Type"), 1);
	grid->SetLabelValue(wxHORIZONTAL, wxT("Field"), 2);
	grid->SetEditable(false);
	grid->SetDividerPen(wxNullPen);

	grid->SetSize(1, grid->GetRowHeight(0) * grid->GetRows());
	hsizer->Add(grid, 1, wxLEFT|wxRIGHT|wxEXPAND, 10);

	wxBoxSizer *vsizer = new wxBoxSizer(wxVERTICAL);
	vsizer->Add(new wxButton(this, ID_PREF_CAB_ADD, wxT("Add...")), 0, wxBOTTOM, 10);
	edit_but = new wxButton(this, ID_PREF_CAB_EDIT, wxT("Edit..."));
	vsizer->Add(edit_but, 0, wxBOTTOM, 10);
	delete_but = new wxButton(this, ID_PREF_CAB_DELETE, wxT("Delete"));
	vsizer->Add(delete_but, 0);

	hsizer->Add(vsizer, 0, wxRIGHT, 10);

	sizer->Add(hsizer, 1, wxBOTTOM|wxEXPAND, 10);

	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);

	SetContestList();
}

void ContestMap::Buttons() {
	bool editable = grid->GetCursorRow() >= 0;
	delete_but->Enable(editable);
	edit_but->Enable(editable);
}

void ContestMap::SetContestList() {
	wxConfig *config = (wxConfig *)wxConfig::Get();
	wxString key, value;
	long cookie;

	contestmap.clear();
	if (grid->GetRows() > 0)
		grid->DeleteRows(0, grid->GetRows());
	
	config->SetPath(wxT("/cabrilloMap"));
	bool stat = config->GetFirstEntry(key, cookie);
	while (stat) {
		value = config->Read(key, wxT(""));
		int contest_type = atoi(value.mb_str());
		int fieldnum = atoi(value.AfterFirst(wxT(';')).mb_str());
		contestmap.insert(make_pair(key, make_pair(contest_type, fieldnum)));
		stat = config->GetNextEntry(key, cookie);
	}
	config->SetPath(wxT("/"));
	int vsize = contestmap.size();
//	if (vsize < 10)
//		vsize = 10;
	if (vsize)
		grid->AppendRows(vsize);
	int idx = 0;
	for (ContestSet::iterator it = contestmap.begin(); it != contestmap.end(); it++) {
		grid->SetCellValue(it->first, idx, 0);
		grid->SetCellValue(it->second.first == 1 ? wxT("VHF") : wxT("HF"), idx, 1);
		grid->SetCellValue(wxString::Format(wxT("%d"), it->second.second), idx, 2);
		++idx;
	}
	config->SetPath(wxT("/"));
	Buttons();
}

bool ContestMap::TransferDataFromWindow() {
	return true;
}

void ContestMap::OnDelete(wxCommandEvent &) {
	int row = grid->GetCursorRow();
	if (row >= 0) {
		wxString contest = grid->GetCellValue(row, 0);
		if (contest != wxT("")) {
			wxConfig *config = (wxConfig *)wxConfig::Get();
			config->SetPath(wxT("/cabrilloMap"));
			config->DeleteEntry(contest, true);
			config->Flush(false);
			SetContestList();
		}
	}
}

void ContestMap::OnAdd(wxCommandEvent &) {
	EditContest dial(this, wxT("Add"));
	if (dial.ShowModal() == ID_OK_BUT) {
		wxConfig *config = (wxConfig *)wxConfig::Get();
		config->SetPath(wxT("/cabrilloMap"));
		config->Write(dial.contest, wxString::Format(wxT("%d;%d"), dial.contest_type, dial.callsign_field));
		config->Flush(false);
		SetContestList();
	}
}

void ContestMap::OnEdit(wxCommandEvent &) {
	wxString contest;
	int contest_type = 0, callsign_field = TQSL_MIN_CABRILLO_MAP_FIELD;
	int row = grid->GetCursorRow();
	if (row >= 0) {
		contest = grid->GetCellValue(row, 0);
		if (contest != wxT("")) {
			wxConfig *config = (wxConfig *)wxConfig::Get();
			config->SetPath(wxT("/cabrilloMap"));
			wxString val;
			if (config->Read(contest, &val)) {
				contest_type = atoi(val.mb_str());
				callsign_field = atoi(val.AfterFirst(wxT(';')).mb_str());
			}
			config->SetPath(wxT("/"));
		}
	}
	EditContest dial(this, wxT("Edit"), contest, contest_type, callsign_field);
	if (dial.ShowModal() == ID_OK_BUT) {
		wxConfig *config = (wxConfig *)wxConfig::Get();
		config->SetPath(wxT("/cabrilloMap"));
		wxString val = wxString::Format(wxT("%d;%d"), dial.contest_type, dial.callsign_field);
		config->Write(dial.contest, val);
		config->Flush(false);
		if (dial.contest != contest && contest != wxT("")) {
			config->SetPath(wxT("/cabrilloMap"));
			config->DeleteEntry(contest, true);
			config->Flush(false);
		}
		SetContestList();
	}
}

BEGIN_EVENT_TABLE(EditContest, wxDialog)
	EVT_BUTTON(ID_OK_BUT, EditContest::OnOK)
	EVT_BUTTON(ID_CAN_BUT, EditContest::OnCancel)
END_EVENT_TABLE()

EditContest::EditContest(wxWindow *parent, wxString ctype, wxString _contest,
		int _contest_type, int _callsign_field)
		: wxDialog(parent, -1, ctype + wxT(" Contest")), contest(_contest),
		contest_type(_contest_type), callsign_field(_callsign_field) {
	SetAutoLayout(true);

	wxClientDC dc(this);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT("M")), &char_width, &char_height);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, -1, ctype + wxT(" Cabrillo CONTEST")), 0, wxALL, 10);

	sizer->Add(new wxStaticText(this, -1, wxT("CONTEST Name:")), 0, wxLEFT|wxRIGHT, 10);
	name = new wxTextCtrl(this, -1, contest, wxDefaultPosition, wxSize(char_width, HEIGHT_ADJ(char_height)));
	sizer->Add(name, 0, wxLEFT|wxRIGHT|wxEXPAND, 10);

	static wxString choices[] = { wxT("HF"), wxT("VHF") };

	type = new wxRadioBox(this, -1, wxT("Contest type"), wxDefaultPosition, wxDefaultSize,
		2, choices, 2, wxRA_SPECIFY_COLS);
	sizer->Add(type, 0, wxALL|wxEXPAND, 10);
	type->SetSelection(contest_type);
	
	sizer->Add(new wxStaticText(this, -1, wxT("Call-Worked Field Number:")), 0, wxLEFT|wxRIGHT, 10);
	fieldnum = new wxTextCtrl(this, -1, wxString::Format(wxT("%d"), callsign_field),
		wxDefaultPosition, wxSize(char_width * 3, HEIGHT_ADJ(char_height)));
	sizer->Add(fieldnum, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);

	wxBoxSizer *butsizer = new wxBoxSizer(wxHORIZONTAL);

	wxButton *button = new wxButton(this, ID_OK_BUT, wxT("OK") );
	butsizer->Add(button, 0, wxALIGN_RIGHT | wxALL, 10);

	button = new wxButton(this, ID_CAN_BUT, wxT("Cancel") );
	butsizer->Add(button, 0, wxALIGN_LEFT | wxALL, 10);

	sizer->Add(butsizer, 0, wxALIGN_CENTER);

	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	CenterOnParent();
}

void EditContest::OnOK(wxCommandEvent& WXUNUSED(event)) {
	if (TransferDataFromWindow())
		EndModal(ID_OK_BUT);
}

bool EditContest::TransferDataFromWindow() {
	contest = name->GetValue();
	contest.Trim(false);
	contest.Trim(true);
	contest.MakeUpper();
	if (contest == wxT("")) {
		wxMessageBox(wxT("Contest name cannot be blank"), wxT("Error"), wxOK, this);
		return false;
	}
	contest_type = type->GetSelection();
	callsign_field = atoi(fieldnum->GetValue().mb_str());
	if (callsign_field < TQSL_MIN_CABRILLO_MAP_FIELD) {
		wxMessageBox(wxString::Format(wxT("Call-worked field must be %d or greater"), TQSL_MIN_CABRILLO_MAP_FIELD),
			wxT("Error"), wxOK, this);
		return false;
	}
	return true;
}

