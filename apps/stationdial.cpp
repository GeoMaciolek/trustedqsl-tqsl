/***************************************************************************
                          stationdial.cpp  -  description
                             -------------------
    begin                : Mon Nov 11 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#define TQSL_ID_LOW 6000

using namespace std;

#include "stationdial.h"
#include "tqslwiz.h"
#include "tqslexcept.h"
#include <wx/listctrl.h>

#include <algorithm>

#include "tqsllib.h"
#include "wxutil.h"

#define GS_NAMELIST TQSL_ID_LOW
#define GS_OKBUT TQSL_ID_LOW+1
#define GS_CANCELBUT TQSL_ID_LOW+2
#define GS_NAMEENTRY TQSL_ID_LOW+3
#define GS_DELETEBUT TQSL_ID_LOW+4
#define GS_NEWBUT TQSL_ID_LOW+5
#define GS_MODIFYBUT TQSL_ID_LOW+6
#define GS_CMD_PROPERTIES TQSL_ID_LOW+7
#define GS_HELPBUT TQSL_ID_LOW+8

class TQSLStationListBox : public wxListBox {
public:
	TQSLStationListBox(wxWindow* parent, wxWindowID id, const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize, int n = 0, const wxString choices[] = NULL,
		long style = 0) : wxListBox(parent, id, pos, size, n, choices, style) {}
	void OnRightDown(wxMouseEvent& event);

	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(TQSLStationListBox, wxListBox)
	EVT_RIGHT_DOWN(TQSLStationListBox::OnRightDown)
END_EVENT_TABLE()

void
TQSLStationListBox::OnRightDown(wxMouseEvent& event) {
	wxMenu menu;
	menu.Append(GS_CMD_PROPERTIES, "&Properties");
	PopupMenu(&menu, event.GetPosition());
}

class PropList : public wxDialog {
public:
	PropList(wxWindow *parent);
	wxListCtrl *list;
};

PropList::PropList(wxWindow *parent) : wxDialog(parent, -1, "Properties", wxDefaultPosition,
	wxSize(400, 300)) {
	list = new wxListCtrl(this, -1, wxDefaultPosition, wxDefaultSize, wxLC_REPORT|wxLC_SINGLE_SEL);
	list->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 100);
	list->InsertColumn(1, "Value", wxLIST_FORMAT_LEFT, 300);
	wxLayoutConstraints *c = new wxLayoutConstraints;
	c->top.SameAs            (this, wxTop);
	c->left.SameAs        (this, wxLeft);
	c->right.SameAs        (this, wxRight);
	c->height.PercentOf    (this, wxHeight, 66);
	list->SetConstraints(c);
	CenterOnParent();
}

static void
check_tqsl_error(int rval) {
	if (rval == 0)
		return;
	throw TQSLException(tqsl_getErrorString());
}

static bool
itemLess(const item& s1, const item& s2) {
	int i = strcasecmp(s1.call.c_str(), s2.call.c_str());
	if (i == 0)
		i = strcasecmp(s1.name.c_str(), s2.name.c_str());
	return (i < 0);
}

BEGIN_EVENT_TABLE(TQSLGetStationNameDialog, wxDialog)
	EVT_BUTTON(GS_OKBUT, TQSLGetStationNameDialog::OnOk)
	EVT_BUTTON(GS_CANCELBUT, TQSLGetStationNameDialog::OnCancel)
	EVT_BUTTON(GS_DELETEBUT, TQSLGetStationNameDialog::OnDelete)
	EVT_BUTTON(GS_HELPBUT, TQSLGetStationNameDialog::OnHelp)
	EVT_BUTTON(GS_NEWBUT, TQSLGetStationNameDialog::OnNew)
	EVT_BUTTON(GS_MODIFYBUT, TQSLGetStationNameDialog::OnModify)
	EVT_LISTBOX(GS_NAMELIST, TQSLGetStationNameDialog::OnNamelist)
	EVT_LISTBOX_DCLICK(GS_NAMELIST, TQSLGetStationNameDialog::OnDblClick)
	EVT_TEXT(GS_NAMEENTRY, TQSLGetStationNameDialog::OnNameChange)
	EVT_MENU(GS_CMD_PROPERTIES, TQSLGetStationNameDialog::DisplayProperties)
	EVT_SET_FOCUS(TQSLGetStationNameDialog::OnSetFocus)
END_EVENT_TABLE()

void
TQSLGetStationNameDialog::OnSetFocus(wxFocusEvent& event) {
	if (!firstFocused) {
		firstFocused = true;
		if (issave) {
			if (name_entry)
				name_entry->SetFocus();
		} else if (namelist)
			namelist->SetFocus();
	}
}

void
TQSLGetStationNameDialog::OnOk(wxCommandEvent&) {
	wxString s = name_entry->GetValue().Trim();
	if (editonly)
		EndModal(wxID_CANCEL);
	else if (s != "") {
		_selected = s;
		EndModal(wxID_OK);
	}

}

void
TQSLGetStationNameDialog::OnCancel(wxCommandEvent&) {
	EndModal(wxID_CANCEL);
}

void
TQSLGetStationNameDialog::RefreshList() {
//	while(namelist->Number() > 0)
//		namelist->Delete(0);
	namelist->Clear();
	item_data.clear();
	int n;
	tQSL_Location loc;
	check_tqsl_error(tqsl_initStationLocationCapture(&loc));
	check_tqsl_error(tqsl_getNumStationLocations(loc, &n));
	for (int i = 0; i < n && i < 2000; i++) {
		item it;
		char buf[256];
		check_tqsl_error(tqsl_getStationLocationName(loc, i, buf, sizeof buf));
//		item_data.push_back(wxString(buf));
		it.name = buf;
		char cbuf[256];
		check_tqsl_error(tqsl_getStationLocationCallSign(loc, i, cbuf, sizeof cbuf));
		it.call = cbuf;
//		wxString s(buf);
//		it.label = it.name + " (" + it.call + ")";		
		it.label = it.call + " - " + it.name;		
		item_data.push_back(it);
	}
	sort(item_data.begin(), item_data.end(), itemLess);
	for (int i = 0; i < (int)item_data.size(); i++)
		namelist->Append(item_data[i].label, (void *)item_data[i].name.c_str());
//cout << "sel(0): " << namelist->GetSelection() << endl;
//	namelist->Deselect(namelist->GetSelection());
//cout << "sel(1): " << namelist->GetSelection() << endl;
}

TQSLGetStationNameDialog::TQSLGetStationNameDialog(wxWindow *parent, wxHtmlHelpController *help, const wxPoint& pos,
	bool i_issave, const wxString& title, bool i_editonly)
	: wxDialog(parent, -1, "Select Station Data", pos), issave(i_issave), editonly(i_editonly),
	newbut(0), modbut(0), updating(false), firstFocused(false), _help(help) {
	wxBoxSizer *topsizer = new wxBoxSizer(wxHORIZONTAL);
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	wxSize text_size = getTextSize(this);
	int control_width = text_size.GetWidth()*30;

	if (title != "")
		SetTitle(title);
	else if (issave)
		SetTitle("Save Station Data");
	// List
	namelist = new TQSLStationListBox(this, GS_NAMELIST, wxDefaultPosition,
		wxSize(control_width, text_size.GetHeight()*10),
		0, 0, wxLB_MULTIPLE|wxLB_HSCROLL|wxLB_ALWAYS_SB);
	sizer->Add(namelist, 1, wxALL|wxEXPAND, 10);
	RefreshList();
	sizer->Add(new wxStaticText(this, -1, issave ? "Enter a name for this Station Location" :
		"Selected Station Location"), 0, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 10);
	name_entry = new wxTextCtrl(this, GS_NAMEENTRY, "", wxDefaultPosition,
		wxSize(control_width, -1));
	if (!issave)
		name_entry->Enable(false);
	sizer->Add(name_entry, 0, wxALL|wxEXPAND, 10);
	topsizer->Add(sizer, 1, 0, 0);
	wxBoxSizer *button_sizer = new wxBoxSizer(wxVERTICAL);
	if (!issave) {
		newbut = new wxButton(this, GS_NEWBUT, "New...");
		newbut->Enable(TRUE);
		button_sizer->Add(newbut, 0, wxALL|wxALIGN_TOP, 3);
		modbut = new wxButton(this, GS_MODIFYBUT, "Edit...");
		modbut->Enable(FALSE);
		button_sizer->Add(modbut, 0, wxALL|wxALIGN_TOP, 3);
	}
	delbut = new wxButton(this, GS_DELETEBUT, "Delete");
	delbut->Enable(FALSE);
	button_sizer->Add(delbut, 0, wxALL|wxALIGN_TOP, 3);
	button_sizer->Add(new wxStaticText(this, -1, ""), 1, wxEXPAND);
	if (_help)
		button_sizer->Add(new wxButton(this, GS_HELPBUT, "Help" ), 0, wxALL|wxALIGN_BOTTOM, 3);
	if (!editonly)
		button_sizer->Add(new wxButton(this, GS_CANCELBUT, "Cancel" ), 0, wxALL|wxALIGN_BOTTOM, 3);
	okbut = new wxButton(this, GS_OKBUT, "OK" );
	button_sizer->Add(okbut, 0, wxALL|wxALIGN_BOTTOM, 3);
	topsizer->Add(button_sizer, 0, wxTOP|wxBOTTOM|wxRIGHT|wxEXPAND, 7);
	hack = (namelist->Number() > 0) ? true : false;

	UpdateControls();
	SetAutoLayout(TRUE);
	SetSizer(topsizer);

	topsizer->Fit(this);
	topsizer->SetSizeHints(this);
	CentreOnParent();
}

void
TQSLGetStationNameDialog::UpdateButtons() {
	wxArrayInt newsels;
	namelist->GetSelections(newsels);
	delbut->Enable(newsels.GetCount() > 0);
	if (modbut)
		modbut->Enable(newsels.GetCount() > 0);
	wxArrayInt sels;
	namelist->GetSelections(sels);
	if (!editonly)
		okbut->Enable(issave ? (name_entry->GetValue().Trim() != "") : (sels.GetCount() > 0));
}

void
TQSLGetStationNameDialog::UpdateControls() {
	if (updating)	// Sentinel to prevent recursion
		return;
	updating = true;
	wxArrayInt newsels;
	namelist->GetSelections(newsels);
	int newsel = -1;
	for (int i = 0; i < (int)newsels.GetCount(); i++) {
		if (sels.Index(newsels[i]) == wxNOT_FOUND) {
			newsel = newsels[i];
			break;
		}
	}
//cout << "newsel: " << newsel << endl;
	if (newsel > -1) {
		for (int i = 0; i < (int)newsels.GetCount(); i++) {
			if (newsels[i] != newsel)
				namelist->Deselect(newsels[i]);
		}
	}
	namelist->GetSelections(sels);
	int idx = (sels.GetCount() > 0) ? sels[0] : -1;
//cout << "UpdateControls selection: " << idx << endl;
	if (idx >= 0)
		name_entry->SetValue((idx < 0) ? "" : (const char *)(namelist->GetClientData(idx)));
	UpdateButtons();
//cout << "UpdateControls selection(1): " << idx << endl;
	updating = false;
}

void
TQSLGetStationNameDialog::OnDelete(wxCommandEvent&) {
	wxArrayInt newsels;
	namelist->GetSelections(newsels);
	int idx = (newsels.GetCount() > 0) ? newsels[0] : -1;
	if (idx < 0)
		return;
	wxString name = (const char *)(namelist->GetClientData(idx));
	if (name == "")
		return;
	if (wxMessageBox(wxString("Delete \"") + name + "\"?", "TQSL Confirm", wxYES_NO|wxCENTRE, this) == wxYES) {
		check_tqsl_error(tqsl_deleteStationLocation(name.c_str()));
		if (!issave)
			name_entry->Clear();
		RefreshList();
		UpdateControls();
	}
}

void
TQSLGetStationNameDialog::OnNamelist(wxCommandEvent& event) {
/*	if (hack) {		// We seem to get an extraneous start-up event (GTK only?)
cout << "OnNamelist hack" << endl;
		hack = false;
		if (want_selected < 0)
			namelist->Deselect(0);
		else
			namelist->SetSelection(want_selected);
	}
*/
//cout << "OnNamelist" << endl;
	UpdateControls();
}

void
TQSLGetStationNameDialog::OnDblClick(wxCommandEvent& event) {
	OnNamelist(event);
	OnOk(event);
}

void
TQSLGetStationNameDialog::OnNew(wxCommandEvent&) {
	EndModal(wxID_APPLY);
}

void
TQSLGetStationNameDialog::OnModify(wxCommandEvent&) {
	EndModal(wxID_MORE);
}

void
TQSLGetStationNameDialog::OnHelp(wxCommandEvent&) {
	if (_help)
		_help->Display("stnloc.htm");
}

void
TQSLGetStationNameDialog::OnNameChange(wxCommandEvent&) {
	UpdateButtons();
}

void
TQSLGetStationNameDialog::DisplayProperties(wxCommandEvent&) {
	wxArrayInt newsels;
	namelist->GetSelections(newsels);
	int idx = (newsels.GetCount() > 0) ? newsels[0] : -1;
	if (idx < 0)
		return;
	wxString name = (const char *)(namelist->GetClientData(idx));
	if (name == "")
		return;
	tQSL_Location loc;
	try {
		map<wxString,wxString> props;
		check_tqsl_error(tqsl_getStationLocation(&loc, name.c_str()));
		do {
			int nfield;
			check_tqsl_error(tqsl_getNumLocationField(loc, &nfield));
			for (int i = 0; i < nfield; i++) {
				char buf[256];
				check_tqsl_error(tqsl_getLocationFieldDataLabel(loc, i, buf, sizeof buf));
				wxString key = buf;
				int type;
				check_tqsl_error(tqsl_getLocationFieldDataType(loc, i, &type));
				if (type == TQSL_LOCATION_FIELD_DDLIST || type == TQSL_LOCATION_FIELD_LIST) {
					int sel;
					check_tqsl_error(tqsl_getLocationFieldIndex(loc, i, &sel));
					check_tqsl_error(tqsl_getLocationFieldListItem(loc, i, sel, buf, sizeof buf));
				} else
					check_tqsl_error(tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf));
				props[key] = buf;
			}
			int rval;
			if (tqsl_hasNextStationLocationCapture(loc, &rval) || !rval)
				break;
			check_tqsl_error(tqsl_nextStationLocationCapture(loc));
		} while(1);
		check_tqsl_error(tqsl_endStationLocationCapture(&loc));
		PropList plist(this);
		int i = 0;
		for (map<wxString,wxString>::iterator it = props.begin(); it != props.end(); it++) {
			plist.list->InsertItem(i, it->first);
			plist.list->SetItem(i, 1, it->second);
			i++;
//cout << idx << ", " << it->first << " => " << it->second << endl;
		}
		plist.ShowModal();
	}
	catch (TQSLException& x) {
		wxLogError(x.what());
	}
}

void
TQSLGetStationNameDialog::SelectName(const wxString& name) {
	wxArrayInt sels;
	namelist->GetSelections(sels);
	for (int i = 0; i < (int)sels.GetCount(); i++)
		namelist->Deselect(sels[i]);
	for (int i = 0; i < namelist->Number(); i++) {
		if (name == (const char *)(namelist->GetClientData(i))) {
			namelist->SetSelection(i, TRUE);
			
			break;
		}
	}
	namelist->GetSelections(sels);
	UpdateControls();
}

int
TQSLGetStationNameDialog::ShowModal() {
	if (namelist->Number() == 0 && !issave)
		wxMessageBox("You have no Station Locations defined.\n\n"
			"You must define at least one Station Location to use for signing.\n"
			"Use the \"New\" Button of the dialog you're about to see to\ndefine a Station Location.",
			"TQSL Warning", wxOK, this);
	return wxDialog::ShowModal();
}

