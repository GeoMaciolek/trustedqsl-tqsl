/***************************************************************************
                          tqslwiz.cpp  -  description
                             -------------------
    begin                : Tue Nov 5 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

using namespace std;

#include "tqslwiz.h"
//#include "dxcc.h"

//#include "wx/textctrl.h"

#ifdef __WIN32__
	#define TEXT_HEIGHT 24
	#define LABEL_HEIGHT 18
	#define TEXT_WIDTH 8
	#define TEXT_POINTS 10
	#define VSEP 3
	#define GEOM1 4
#else
	#define TEXT_HEIGHT 18
	#define LABEL_HEIGHT TEXT_HEIGHT
	#define TEXT_WIDTH 8
	#define TEXT_POINTS 12
	#define VSEP 4
	#define GEOM1 6
#endif

#define CONTROL_WIDTH (TEXT_WIDTH*30)

static void set_font(wxWindow *w, wxFont& font) {
#ifndef __WIN32__
	w->SetFont(font);
#endif
}

BEGIN_EVENT_TABLE(TQSLWizard, wxWizard)
	EVT_WIZARD_PAGE_CHANGED(-1, TQSLWizard::OnPageChanged)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(TQSLWizCertPage, TQSLWizPage)
	EVT_COMBOBOX(-1, TQSLWizCertPage::OnComboBoxEvent)
	EVT_SET_FOCUS(TQSLWizCertPage::OnSetFocus)
END_EVENT_TABLE()

void
TQSLWizard::OnPageChanged(wxWizardEvent&) {
	((TQSLWizPage *)GetCurrentPage())->SetFocus();
}

TQSLWizard::TQSLWizard(tQSL_Location locp, wxWindow* parent, int id,
	const wxString& title, const wxBitmap& bitmap, const wxPoint& pos)
		: wxWizard(parent, id, title, bitmap, pos), page_changing(false),
		loc(locp), _curpage(-1) {

	char buf[256];
	if (!tqsl_getStationLocationCaptureName(locp, buf, sizeof buf)) {
		wxString s(buf);
		SetLocationName(s);
	}
	tqsl_setStationLocationCapturePage(locp, 1);
}

TQSLWizPage *
TQSLWizard::GetPage(bool final) {
	int page_num;
	page_changing = false;
	if (final)
		page_num = 0;
	else if (tqsl_getStationLocationCapturePage(loc, &page_num))
		return 0;
	if (_pages[page_num]) {
		if (page_num == 0)
			((TQSLWizFinalPage *)_pages[0])->prev = GetCurrentTQSLPage();
		return _pages[page_num];
	}
	if (page_num == 0)
		_pages[page_num] = new TQSLWizFinalPage(this, loc, GetCurrentTQSLPage());
	else
		_pages[page_num] = new TQSLWizCertPage(this, loc);
	if (page_num == 0)
		((TQSLWizFinalPage *)_pages[0])->prev = GetCurrentTQSLPage();
	return _pages[page_num];
}

void
TQSLWizCertPage::OnSetFocus(wxFocusEvent& event) {
	if (controls.size() > 0)
		((wxControl *)controls[0])->SetFocus();
}

TQSLWizPage *
TQSLWizCertPage::GetPrev() const {
	int rval;
	if (tqsl_hasPrevStationLocationCapture(loc, &rval) || !rval) {
		GetParent()->page_changing = false;
		return 0;
	}
	if ((TQSLWizard *)GetParent()->page_changing) {
		tqsl_prevStationLocationCapture(loc);
		return GetParent()->GetPage();
	}
	return GetParent()->GetPage();
}

TQSLWizPage *
TQSLWizCertPage::GetNext() const {
	TQSLWizPage *newp;
	bool final = false;
	if (GetParent()->page_changing) {
		int rval;
		if (!tqsl_hasNextStationLocationCapture(loc, &rval) && rval) {
			tqsl_nextStationLocationCapture(loc);
		} else
			final = true;
	}
	newp = GetParent()->GetPage(final);
	return newp;
}

void
TQSLWizCertPage::UpdateFields(int noupdate_field) {
	if (noupdate_field >= 0)
		tqsl_updateStationLocationCapture(loc);
	for (int i = noupdate_field+1; i < (int)controls.size(); i++) {
		int changed;
		tqsl_getLocationFieldChanged(loc, i, &changed);
		if (noupdate_field >= 0 && !changed)
			continue;
		int in_type;
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		if (in_type == TQSL_LOCATION_FIELD_DDLIST || in_type == TQSL_LOCATION_FIELD_LIST) {
			// Update this list
			int text_width = 0;
			char gabbi_name[40];
			tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
			int selected;
			tqsl_getLocationFieldIndex(loc, i, &selected);
			int new_sel = 0;
			wxString old_sel = ((wxComboBox *)controls[i])->GetStringSelection();
			((wxComboBox *)controls[i])->Clear();
			int nitems;
			tqsl_getNumLocationFieldListItems(loc, i, &nitems);
			for (int j = 0; j < nitems && j < 2000; j++) {
				char item[80];
				tqsl_getLocationFieldListItem(loc, i, j, item, sizeof(item));
				wxString item_text(item);
				((wxComboBox *)controls[i])->Append(item_text);
				if (!strcmp(item_text, old_sel.c_str()))
					new_sel = j;
				wxCoord w, h;
				((wxComboBox *)controls[i])->GetTextExtent(item_text, &w, &h);
				if (w > text_width)
					text_width = w;
			}
			if (text_width > 0) {
				int w, h;
				((wxComboBox *)controls[i])->GetSize(&w, &h);
				((wxComboBox *)controls[i])->SetSize(text_width + TEXT_WIDTH*4, h);
			}
			if (noupdate_field < 0)
				new_sel = selected;
			((wxComboBox *)controls[i])->SetSelection(new_sel);
			tqsl_setLocationFieldIndex(loc, i, new_sel);
			if (noupdate_field >= 0)
				tqsl_updateStationLocationCapture(loc);
			((wxComboBox *)controls[i])->Enable(nitems > 1);
		} else if (in_type == TQSL_LOCATION_FIELD_TEXT) {
			int len;
			tqsl_getLocationFieldDataLength(loc, i, &len);
			int w, h;
			((wxTextCtrl *)controls[i])->GetSize(&w, &h);
			((wxTextCtrl *)controls[i])->SetSize((len+1)*TEXT_WIDTH, h);
			if (noupdate_field < 0) {
				char buf[256];
				tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
				((wxTextCtrl *)controls[i])->SetValue(buf);
			}
		}
	}
}

void
TQSLWizCertPage::OnComboBoxEvent(wxCommandEvent& event) {
	int control_idx = event.GetId() - TQSL_ID_LOW;
	if (control_idx < 0 || control_idx >= (int)controls.size())
		return;
	int in_type;
	tqsl_getLocationFieldInputType(loc, control_idx, &in_type);
	switch (in_type) {
		case TQSL_LOCATION_FIELD_DDLIST:
		case TQSL_LOCATION_FIELD_LIST:
			tqsl_setLocationFieldIndex(loc, control_idx, event.m_commandInt);
			UpdateFields(control_idx);
			break;
	}
}

TQSLWizCertPage::TQSLWizCertPage(TQSLWizard *parent, tQSL_Location locp) : TQSLWizPage(parent, locp) {
	tqsl_getStationLocationCapturePage(loc, &loc_page);
	wxFont font = GetFont();
	font.SetPointSize(TEXT_POINTS);
	set_font(this, font);
	int y = TEXT_HEIGHT;
	wxScreenDC sdc;
	int label_w = 0;
	int numf;
	tqsl_getNumLocationField(loc, &numf);
	for (int i = 0; i < numf; i++) {
		wxCoord w, h;
		char label[256];
		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		sdc.GetTextExtent(label, &w, &h);
		if (w > label_w)
			label_w = w;
	}
	wxCoord max_label_w, max_label_h;
	// Measure width of longest expected label string
	sdc.GetTextExtent("Longest label expected", &max_label_w, &max_label_h);
	int control_x = label_w + TEXT_WIDTH;
	for (int i = 0; i < numf; i++) {
		char label[256];
		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		wxStaticText *st = new wxStaticText(this, -1, label, wxPoint(0, y),
			wxSize(label_w, TEXT_HEIGHT), wxALIGN_RIGHT|wxST_NO_AUTORESIZE);
		set_font(st, font);
		void *control_p = 0;
		char gabbi_name[256];
		tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
		int in_type;
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		switch(in_type) {
			case TQSL_LOCATION_FIELD_DDLIST:
			case TQSL_LOCATION_FIELD_LIST:
				control_p = new wxComboBox(this, TQSL_ID_LOW+i, "", wxPoint(control_x, y), wxSize(CONTROL_WIDTH, TEXT_HEIGHT),
					0, 0, wxCB_DROPDOWN|wxCB_READONLY);
				break;
			case TQSL_LOCATION_FIELD_TEXT:
				control_p = new wxTextCtrl(this, TQSL_ID_LOW+i, "", wxPoint(control_x, y), wxSize(CONTROL_WIDTH, TEXT_HEIGHT));
				break;
		}
		controls.push_back(control_p);
		y += TEXT_HEIGHT+VSEP;
	}
	SetSize(max_label_w + TEXT_WIDTH + CONTROL_WIDTH, (TEXT_HEIGHT+VSEP)*10);
	Show(FALSE);
	UpdateFields();
}

TQSLWizCertPage::~TQSLWizCertPage() {
}

bool
TQSLWizCertPage::TransferDataFromWindow() {
	GetParent()->page_changing = true;
	tqsl_setStationLocationCapturePage(loc, loc_page);
	for (int i = 0; i < (int)controls.size(); i++) {
		int in_type;
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		switch(in_type) {
			case TQSL_LOCATION_FIELD_DDLIST:
			case TQSL_LOCATION_FIELD_LIST:
				break;
			case TQSL_LOCATION_FIELD_TEXT:
				tqsl_setLocationFieldCharData(loc, i, ((wxTextCtrl *)controls[i])->GetValue().c_str());
				break;
		}
	}
	return true;
}

BEGIN_EVENT_TABLE(TQSLWizFinalPage, TQSLWizPage)
	EVT_LISTBOX(TQSL_ID_LOW, TQSLWizFinalPage::OnListbox)
	EVT_LISTBOX_DCLICK(TQSL_ID_LOW, TQSLWizFinalPage::OnListbox)
END_EVENT_TABLE()

void
TQSLWizFinalPage::OnListbox(wxCommandEvent &) {
	if (namelist->GetSelection() >= 0) {
		const char *cp = (const char *)(namelist->GetClientData(namelist->GetSelection()));
		if (cp)
			newname->SetValue(cp);
	}
}

TQSLWizFinalPage::TQSLWizFinalPage(TQSLWizard *parent, tQSL_Location locp, TQSLWizPage *i_prev) :
	TQSLWizPage(parent, locp), prev(i_prev) {
	wxFont font = GetFont();
	font.SetPointSize(TEXT_POINTS);
	set_font(this, font);
	int y = TEXT_HEIGHT;
	sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, "Station Data input complete");
//	set_font(st, font);
	sizer->Add(st, 0, wxALIGN_CENTER|wxTOP, 10);
	st = new wxStaticText(this, -1, "Select or enter name of this station location");
//	set_font(st, font);
	sizer->Add(st, 0, wxALIGN_CENTER|wxBOTTOM, 10);
//SetAutoLayout(TRUE);
//	SetSizer(sizer);
//	sizer->Fit(this);
//	sizer->SetSizeHints(this);

//	return;

	// Title
	set_font(st, font);
	sizer->Add(st, 0, wxALIGN_CENTER|wxBOTTOM, 10);
	// List of existing location names
	namelist = new wxListBox(this, TQSL_ID_LOW, wxDefaultPosition, wxSize(CONTROL_WIDTH, TEXT_HEIGHT*10),
		0, 0, wxLB_SINGLE|wxLB_HSCROLL|wxLB_NEEDED_SB);
	sizer->Add(namelist, 1, wxEXPAND);
	int n;
	tqsl_getNumStationLocations(locp, &n);
	for (int i = 0; i < n; i++) {
		char buf[256];
		tqsl_getStationLocationName(loc, i, buf, sizeof buf);
		item_data.push_back(wxString(buf));
		char cbuf[256];
		tqsl_getStationLocationCallSign(loc, i, cbuf, sizeof cbuf);
		wxString s(buf);
		s += (wxString(" (") + wxString(cbuf) + wxString(")"));
		namelist->Append(s, (void *)item_data.back().c_str());
	}
	if (namelist->Number() > 0)
		namelist->SetSelection(0, FALSE);
	// New name
	st = new wxStaticText(this, -1, "Station Location Name");
	set_font(st, font);
	sizer->Add(st, 0, wxALIGN_LEFT|wxTOP, 10);
	newname = new wxTextCtrl(this, TQSL_ID_LOW+1, "", wxPoint(0, y), wxSize(CONTROL_WIDTH, TEXT_HEIGHT));
	sizer->Add(newname, 0, wxEXPAND);
	newname->SetValue(parent->GetLocationName());
	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
}

bool
TQSLWizFinalPage::TransferDataFromWindow() {
//	wxMessageBox(wxString::Format("this %d", this->GetClientSize().GetWidth()));

//return true;

	if (newname->GetValue().Trim() == "") {
		wxMessageBox("You must enter a name for this station data", "TQSL");
		return false;
	}
	wxString s = newname->GetValue();
	((TQSLWizard *)GetParent())->SetLocationName(s);
	return true;
}
