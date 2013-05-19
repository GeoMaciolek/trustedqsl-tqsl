/***************************************************************************
                          tqslwiz.cpp  -  description
                             -------------------
    begin                : Tue Nov 5 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: tqslwiz.cpp,v 1.6 2013/03/01 13:12:57 k1mu Exp $
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif


#include "tqslwiz.h"
#include "wxutil.h"


using namespace std;

#define TQSL_LOCATION_FIELD_UPPER	1
#define TQSL_LOCATION_FIELD_MUSTSEL	2
#define TQSL_LOCATION_FIELD_SELNXT	4

BEGIN_EVENT_TABLE(TQSLWizard, wxWizard)
	EVT_WIZARD_PAGE_CHANGED(-1, TQSLWizard::OnPageChanged)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(TQSLWizCertPage, TQSLWizPage)
	EVT_COMBOBOX(-1, TQSLWizCertPage::OnComboBoxEvent)
	EVT_CHECKBOX(-1, TQSLWizCertPage::OnCheckBoxEvent)
	EVT_SIZE(TQSLWizCertPage::OnSize)
END_EVENT_TABLE()

void
TQSLWizard::OnPageChanged(wxWizardEvent& ev) {
	((TQSLWizPage *)GetCurrentPage())->SetFocus();
	ExtWizard::OnPageChanged(ev);
}

TQSLWizard::TQSLWizard(tQSL_Location locp, wxWindow *parent, wxHtmlHelpController *help,
	const wxString& title, bool expired) :
	ExtWizard(parent, help, title), loc(locp), _curpage(-1) {

	char buf[256];
	if (!tqsl_getStationLocationCaptureName(locp, buf, sizeof buf)) {
		wxString s(buf, wxConvLocal);
		SetLocationName(s);
	}
	tqsl_setStationLocationCertFlags(locp, expired ? TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED : TQSL_SELECT_CERT_WITHKEYS);
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
	_curpage = page_num;
	if (page_num == 0)
		((TQSLWizFinalPage *)_pages[0])->prev = GetCurrentTQSLPage();
	return _pages[page_num];
}

void TQSLWizCertPage::OnSize(wxSizeEvent& ev) {
	TQSLWizPage::OnSize(ev); //fix this
	UpdateFields();
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
	wxSize text_size = getTextSize(this);

	if (noupdate_field >= 0)
		tqsl_updateStationLocationCapture(loc);
	if (noupdate_field <= 0) {
		int flags;
		tqsl_getLocationFieldFlags(loc, 0, &flags);
		if (flags == TQSL_LOCATION_FIELD_SELNXT) {
			int index;
			bool fwdok = false;
			tqsl_getLocationFieldIndex(loc, 0, &index);
			if (index > 0)
				fwdok = true;
			if (okEmptyCB && okEmptyCB->IsChecked())
				fwdok = true;
			wxWindow *but = GetParent()->FindWindow(wxID_FORWARD);
		        if (but) {
        			but->Enable(fwdok);
			} 
			if (okEmptyCB) {
				if (index > 0) 
					okEmptyCB->Enable(false);
				else
					okEmptyCB->Enable(true);
			}
		}
	}
	for (int i = noupdate_field+1; i < (int)controls.size(); i++) {
		int changed;
		int in_type;
		tqsl_getLocationFieldChanged(loc, i, &changed);
		tqsl_getLocationFieldInputType(loc, i, &in_type);

		if (noupdate_field >= 0 && !changed && in_type != TQSL_LOCATION_FIELD_BADZONE)
			continue;
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
				/* For Alaska counties, it's pseudo-county name
				 * followed by a '|' then the real county name.
			 	 */
				char *p = strchr(item, '|');
				if (p)
					*p = '\0';
				wxString item_text(item, wxConvLocal);
				((wxComboBox *)controls[i])->Append(item_text);
				if (item_text == old_sel)
					new_sel = j;
				wxCoord w, h;
				((wxComboBox *)controls[i])->GetTextExtent(item_text, &w, &h);
				if (w > text_width)
					text_width = w;
			}
			if (text_width > 0) {
				int w, h;
				((wxComboBox *)controls[i])->GetSize(&w, &h);
				((wxComboBox *)controls[i])->SetSize(text_width + text_size.GetWidth()*4, h);
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
			((wxTextCtrl *)controls[i])->SetSize((len+1)*text_size.GetWidth(), h);
			if (noupdate_field < 0) {
				char buf[256];
				tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
				((wxTextCtrl *)controls[i])->SetValue(wxString(buf, wxConvLocal));
			}
		} else if (in_type == TQSL_LOCATION_FIELD_BADZONE) {
			int len;
			tqsl_getLocationFieldDataLength(loc, i, &len);
			len /= 2;
			int w, h;
			((wxStaticText *)controls[i])->GetSize(&w, &h);
			((wxStaticText *)controls[i])->SetSize((len+1)*text_size.GetWidth(), h);
			char buf[256];
			tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
			((wxStaticText *)controls[i])->SetLabel(wxString(buf, wxConvLocal));
			if (strlen(buf) == 0) {
				this->GetParent()->FindWindow(wxID_FORWARD)->Enable(true);
			} else {
				this->GetParent()->FindWindow(wxID_FORWARD)->Enable(false);
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
			tqsl_setLocationFieldIndex(loc, control_idx, event.GetInt());
			UpdateFields(control_idx);
			break;
	}
}

void
TQSLWizCertPage::OnCheckBoxEvent(wxCommandEvent& event) {
	UpdateFields(-1);
}

TQSLWizCertPage::TQSLWizCertPage(TQSLWizard *parent, tQSL_Location locp)
	: TQSLWizPage(parent, locp) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	int control_width = getTextSize(this).GetWidth() * 20;
	
	tqsl_getStationLocationCapturePage(loc, &loc_page);
	wxScreenDC sdc;
	int label_w = 0;
	int numf;
	tqsl_getNumLocationField(loc, &numf);
	for (int i = 0; i < numf; i++) {
		wxCoord w, h;
		char label[256];
		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		sdc.GetTextExtent(wxString(label, wxConvLocal), &w, &h);
		if (w > label_w)
			label_w = w;
	}
	wxCoord max_label_w, max_label_h;
	// Measure width of longest expected label string
	sdc.GetTextExtent(wxT("Longest label expected"), &max_label_w, &max_label_h);
	bool addCheckbox = false;
	wxString cbLabel;
	for (int i = 0; i < numf; i++) {
		wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
		char label[256];
		int in_type, flags;
		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		if (in_type != TQSL_LOCATION_FIELD_BADZONE) {
			hsizer->Add(new wxStaticText(this, -1, wxString(label, wxConvLocal), wxDefaultPosition,
				wxSize(label_w, -1), wxALIGN_RIGHT|wxST_NO_AUTORESIZE), 0, wxTOP, 5);
		}
		wxWindow *control_p = 0;
		char gabbi_name[256];
		tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
		tqsl_getLocationFieldFlags(loc, i, &flags);
		switch(in_type) {
			case TQSL_LOCATION_FIELD_DDLIST:
			case TQSL_LOCATION_FIELD_LIST:
				control_p = new wxComboBox(this, TQSL_ID_LOW+i, wxT(""), wxDefaultPosition, wxSize(control_width, -1),
					0, 0, wxCB_DROPDOWN|wxCB_READONLY);
				if (flags & TQSL_LOCATION_FIELD_SELNXT) {
					addCheckbox = true;
					cbLabel = wxString(label, wxConvLocal);
				}
				break;
			case TQSL_LOCATION_FIELD_TEXT:
				control_p = new wxTextCtrl(this, TQSL_ID_LOW+i, wxT(""), wxDefaultPosition, wxSize(control_width, -1));
				break;
			case TQSL_LOCATION_FIELD_BADZONE:
				wxCoord w, h;
				int tsize;
				tqsl_getLocationFieldDataLength(loc, i, &tsize);
				tsize /= 2;
				sdc.GetTextExtent(wxString("X", wxConvLocal), &w, &h);
				control_p = new wxStaticText(this, -1, wxT(""), wxDefaultPosition,
					wxSize(w*tsize, -1), wxALIGN_LEFT|wxST_NO_AUTORESIZE);
				break;
		}
		controls.push_back(control_p);
		hsizer->Add(control_p, 0, wxLEFT|wxTOP, 5);
		sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 10);
	}

	if (addCheckbox) {
		wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
		okEmptyCB = new wxCheckBox(this, TQSL_ID_LOW+numf, wxT("Allow 'None' for ") + cbLabel, wxDefaultPosition, wxDefaultSize);
		
		hsizer->Add(new wxStaticText(this, -1, wxT(""),
			wxDefaultPosition, wxSize(label_w, -1), wxALIGN_RIGHT|wxST_NO_AUTORESIZE), 0, wxTOP, 5);
		hsizer->Add(okEmptyCB, 0, wxLEFT|wxTOP, 5);
		sizer->Add(hsizer, 0, wxLEFT|wxRight, 10);
	} else
		okEmptyCB = 0;

	UpdateFields();
	AdjustPage(sizer, wxT("stnloc1.htm"));
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
				tqsl_setLocationFieldCharData(loc, i, ((wxTextCtrl *)controls[i])->GetValue().mb_str());
				break;
		}
	}
	return true;
}

BEGIN_EVENT_TABLE(TQSLWizFinalPage, TQSLWizPage)
	EVT_LISTBOX(TQSL_ID_LOW, TQSLWizFinalPage::OnListbox)
	EVT_LISTBOX_DCLICK(TQSL_ID_LOW, TQSLWizFinalPage::OnListbox)
	EVT_TEXT(TQSL_ID_LOW+1, TQSLWizFinalPage::check_valid)
END_EVENT_TABLE()

void
TQSLWizFinalPage::OnListbox(wxCommandEvent &) {
	if (namelist->GetSelection() >= 0) {
		const char *cp = (const char *)(namelist->GetClientData(namelist->GetSelection()));
		if (cp)
			newname->SetValue(wxString(cp, wxConvLocal));
	}
}

TQSLWizFinalPage::TQSLWizFinalPage(TQSLWizard *parent, tQSL_Location locp, TQSLWizPage *i_prev) :
	TQSLWizPage(parent, locp), prev(i_prev) {
	wxSize text_size = getTextSize(this);
	int control_width = text_size.GetWidth()*30;
	
	int y = text_size.GetHeight();
	sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("Station Data input complete"));
	sizer->Add(st, 0, wxALIGN_CENTER|wxTOP, 10);
	
	// Title
	st = new wxStaticText(this, -1, wxT("Select or enter name of this station location"));
	sizer->Add(st, 0, wxALIGN_CENTER|wxBOTTOM, 10);

	// List of existing location names
	namelist = new wxListBox(this, TQSL_ID_LOW, wxDefaultPosition, wxSize(control_width, text_size.GetHeight()*10),
		0, 0, wxLB_SINGLE|wxLB_HSCROLL|wxLB_NEEDED_SB);
	sizer->Add(namelist, 0, wxEXPAND);
	int n;
	tqsl_getNumStationLocations(locp, &n);
	for (int i = 0; i < n; i++) {
		char buf[256];
		tqsl_getStationLocationName(loc, i, buf, sizeof buf);
		item_data.push_back(wxString(buf, wxConvLocal));
		char cbuf[256];
		tqsl_getStationLocationCallSign(loc, i, cbuf, sizeof cbuf);
		wxString s(buf, wxConvLocal);
		s += (wxString(wxT(" (")) + wxString(cbuf, wxConvLocal) + wxString(wxT(")")));
		const void *v = item_data.back().mb_str();
		namelist->Append(s, (void *)v);
	}
	if (namelist->GetCount() > 0)
		namelist->SetSelection(0, FALSE);
	// New name
	st = new wxStaticText(this, -1, wxT("Station Location Name"));
	sizer->Add(st, 0, wxALIGN_LEFT|wxTOP, 10);
	newname = new wxTextCtrl(this, TQSL_ID_LOW+1, wxT(""), wxPoint(0, y), wxSize(control_width, -1));
	sizer->Add(newname, 0, wxEXPAND);
	newname->SetValue(parent->GetLocationName());
	AdjustPage(sizer, wxT("stnloc2.htm"));
}

bool
TQSLWizFinalPage::TransferDataFromWindow() {
	if (validate())	// Must be a "back"
		return true;
	wxString s = newname->GetValue();
	((TQSLWizard *)GetParent())->SetLocationName(s);
	return true;
}

const char *
TQSLWizFinalPage::validate() {
	wxString val = newname->GetValue();
	const char *errmsg = 0;
	val.Trim();
	if (val == wxT(""))
		errmsg = "Station name must be provided";
	return errmsg;
}
