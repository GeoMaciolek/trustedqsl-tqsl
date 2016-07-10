/***************************************************************************
                          tqslwiz.cpp  -  description
                             -------------------
    begin                : Tue Nov 5 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: tqslwiz.cpp,v 1.6 2013/03/01 13:12:57 k1mu Exp $
 ***************************************************************************/

#include "tqslwiz.h"
#include <wx/tokenzr.h>
#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "wxutil.h"
#include "tqsltrace.h"
#include "winstrdefs.h"

#define TQSL_LOCATION_FIELD_UPPER	1
#define TQSL_LOCATION_FIELD_MUSTSEL	2
#define TQSL_LOCATION_FIELD_SELNXT	4

BEGIN_EVENT_TABLE(TQSLWizard, wxWizard)
	EVT_WIZARD_PAGE_CHANGED(-1, TQSLWizard::OnPageChanged)
END_EVENT_TABLE()

BEGIN_EVENT_TABLE(TQSLWizCertPage, TQSLWizPage)
	EVT_COMBOBOX(-1, TQSLWizCertPage::OnComboBoxEvent)
	EVT_CHECKBOX(-1, TQSLWizCertPage::OnCheckBoxEvent)
	EVT_WIZARD_PAGE_CHANGING(wxID_ANY, TQSLWizCertPage::OnPageChanging)
#if wxMAJOR_VERSION < 3 && (wxMAJOR_VERSION != 2 && wxMINOR_VERSION != 9)
	EVT_SIZE(TQSLWizCertPage::OnSize)
#endif
END_EVENT_TABLE()

void
TQSLWizard::OnPageChanged(wxWizardEvent& ev) {
	tqslTrace("TQSLWizard::OnPageChanged", NULL);
	(reinterpret_cast<TQSLWizPage *>(GetCurrentPage()))->SetFocus();
	ExtWizard::OnPageChanged(ev);
}

TQSLWizard::TQSLWizard(tQSL_Location locp, wxWindow *parent, wxHtmlHelpController *help,
	const wxString& title, bool expired)
	: ExtWizard(parent, help, title), loc(locp), _curpage(-1) {
	tqslTrace("TQSLWizard::TQSLWizard", "locp=0x%lx, parent=0x%lx, title=%s, expired=%d", reinterpret_cast<void *>(locp), reinterpret_cast<void *>(parent), S(title), expired);

	char buf[256];
	if (!tqsl_getStationLocationCaptureName(locp, buf, sizeof buf)) {
		wxString s = wxString::FromUTF8(buf);
		SetLocationName(s);
	}
	tqsl_setStationLocationCertFlags(locp, expired ? TQSL_SELECT_CERT_WITHKEYS | TQSL_SELECT_CERT_EXPIRED : TQSL_SELECT_CERT_WITHKEYS);
	tqsl_setStationLocationCapturePage(locp, 1);
}

TQSLWizPage *
TQSLWizard::GetPage(bool final) {
	tqslTrace("TQSLWizard::GetPage", "final=%d", final);
	int page_num;
	if (final)
		page_num = 0;
	else if (tqsl_getStationLocationCapturePage(loc, &page_num))
		return 0;
	tqslTrace("TQSLWizard::GetPage", "page_num = %d", page_num);
	if (_pages[page_num]) {
		if (page_num == 0)
			(reinterpret_cast<TQSLWizFinalPage *>(_pages[0]))->prev = GetCurrentTQSLPage();
		return _pages[page_num];
	}
	if (page_num == 0)
		_pages[page_num] = new TQSLWizFinalPage(this, loc, GetCurrentTQSLPage());
	else
		_pages[page_num] = new TQSLWizCertPage(this, loc);
	_curpage = page_num;
	if (page_num == 0)
		(reinterpret_cast<TQSLWizFinalPage *>(_pages[0]))->prev = GetCurrentTQSLPage();
	return _pages[page_num];
}

void TQSLWizCertPage::OnSize(wxSizeEvent& ev) {
#if wxMAJOR_VERSION < 3 && (wxMAJOR_VERSION != 2 && wxMINOR_VERSION != 9)
	TQSLWizPage::OnSize(ev);
#endif
	UpdateFields();
}

TQSLWizPage *
TQSLWizCertPage::GetPrev() const {
	tqslTrace("TQSLWizCertPage::GetPrev", NULL);
	int rval;

	tqsl_setStationLocationCapturePage(loc, loc_page);
	if (tqsl_hasPrevStationLocationCapture(loc, &rval) || !rval) {
		return 0;
	}
	tqsl_prevStationLocationCapture(loc);
	return GetParent()->GetPage();
}

TQSLWizPage *
TQSLWizCertPage::GetNext() const {
	tqslTrace("TQSLWizCertPage::GetNext", NULL);
	TQSLWizPage *newp;
	bool final = false;
	int rval;

	tqsl_setStationLocationCapturePage(loc, loc_page);
	if (!tqsl_hasNextStationLocationCapture(loc, &rval) && rval) {
		tqsl_nextStationLocationCapture(loc);
	} else {
		final = true;
	}
	newp = GetParent()->GetPage(final);
	return newp;
}

void
TQSLWizCertPage::UpdateFields(int noupdate_field) {
	tqslTrace("TQSLWizCertPage::UpdateFields", "noupdate_field=%d", noupdate_field);
	wxSize text_size = getTextSize(this);

	validate();
	if (noupdate_field >= 0)
		tqsl_updateStationLocationCapture(loc);
	for (int i = noupdate_field+1; i < static_cast<int>(controls.size()); i++) {
		int changed;
		int in_type;
		tqsl_getLocationFieldChanged(loc, i, &changed);
		tqsl_getLocationFieldInputType(loc, i, &in_type);

		if (noupdate_field >= 0 && !changed && in_type != TQSL_LOCATION_FIELD_BADZONE)
			continue;
		if (in_type == TQSL_LOCATION_FIELD_DDLIST || in_type == TQSL_LOCATION_FIELD_LIST) {
			// Update this list
			char gabbi_name[40];
			tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
			int selected;
			bool defaulted = false;
			tqsl_getLocationFieldIndex(loc, i, &selected);
			int new_sel = 0;
			wxString old_sel = (reinterpret_cast<wxComboBox *>(controls[i]))->GetStringSelection();
			if (old_sel.IsEmpty() && strcmp(gabbi_name, "CALL") == 0) {
				old_sel = (reinterpret_cast<TQSLWizard*>(GetParent()))->GetDefaultCallsign();
				if (!old_sel.IsEmpty())
					defaulted = true;		// Set from default
			}
			(reinterpret_cast<wxComboBox *>(controls[i]))->Clear();
			int nitems;
			tqsl_getNumLocationFieldListItems(loc, i, &nitems);
			for (int j = 0; j < nitems && j < 2000; j++) {
				char item[200];
				tqsl_getLocationFieldListItem(loc, i, j, item, sizeof(item));
				// Translate the first [None] entry if it exists
#ifdef tqsltranslate
				__("[None]");
#endif
				wxString item_text = wxString::FromUTF8(item);
				if (item_text == old_sel)
					new_sel = j;
				if (j == 0)
					item_text = wxGetTranslation(item_text);
				(reinterpret_cast<wxComboBox *>(controls[i]))->Append(item_text);
			}
			if (noupdate_field < 0 && !defaulted)
				new_sel = selected;
			if (nitems > new_sel)
				(reinterpret_cast<wxComboBox *>(controls[i]))->SetSelection(new_sel);
			tqsl_setLocationFieldIndex(loc, i, new_sel);
			if (nitems > new_sel)
				(reinterpret_cast<wxComboBox *>(controls[i]))->SetSelection(new_sel);
			(reinterpret_cast<wxComboBox *>(controls[i]))->Enable(nitems > 1);
		} else if (in_type == TQSL_LOCATION_FIELD_TEXT) {
			int len;
			tqsl_getLocationFieldDataLength(loc, i, &len);
			int w, h;
			(reinterpret_cast<wxTextCtrl *>(controls[i]))->GetSize(&w, &h);
			(reinterpret_cast<wxTextCtrl *>(controls[i]))->SetSize((len+1)*text_size.GetWidth(), h);
			if (noupdate_field < 0) {
				char buf[256];
				tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
				(reinterpret_cast<wxTextCtrl *>(controls[i]))->SetValue(wxString::FromUTF8(buf));
			}
		} else if (in_type == TQSL_LOCATION_FIELD_BADZONE) {
			int len;
			tqsl_getLocationFieldDataLength(loc, i, &len);
			int w, h;
			(reinterpret_cast<wxStaticText *>(controls[i]))->GetSize(&w, &h);
			(reinterpret_cast<wxStaticText *>(controls[i]))->SetSize((len+1)*text_size.GetWidth(), h);
			char buf[256];
			tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
			if (strlen(buf) > 0)
				valMsg = wxGetTranslation(wxString::FromUTF8(buf));
			else
				valMsg = wxT("");
			(reinterpret_cast<wxStaticText *>(controls[i]))->SetLabel(valMsg);
		}
	}
	if (noupdate_field >= 0)
		tqsl_updateStationLocationCapture(loc);
}

void
TQSLWizCertPage::OnComboBoxEvent(wxCommandEvent& event) {
	tqslTrace("TQSLWizCertPage::OnComboBoxEvent", NULL);
	int control_idx = event.GetId() - TQSL_ID_LOW;
	if (control_idx < 0 || control_idx >= static_cast<int>(controls.size()))
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
	tqslTrace("TQSLWizCertPage::TQSLWizCertPage", "parent=0x%lx, locp=0x%lx", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(locp));
	initialized = false;
	errlbl = NULL;
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	int control_width = getTextSize(this).GetWidth() * 40;

	valMsg = wxT("");
	tqsl_getStationLocationCapturePage(loc, &loc_page);
	wxScreenDC sdc;
	int label_w = 0;
	int numf;
	tqsl_getNumLocationField(loc, &numf);
	for (int i = 0; i < numf; i++) {
		wxCoord w, h;
		char label[256];
		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		wxString lbl = wxGetTranslation(wxString::FromUTF8(label));
		sdc.GetTextExtent(lbl, &w, &h);
		if (w > label_w)
			label_w = w;
	}
	wxCoord max_label_w, max_label_h;
	// Measure width of longest expected label string
	sdc.GetTextExtent(wxT("Longest label expected"), &max_label_w, &max_label_h);
	bool addCheckbox = false;
	wxString cbLabel;
	for (int i = 0; i < numf; i++) {
		char label[256];
		int in_type, flags;
		wxBoxSizer *hsizer;
		tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
		wxString lbl = wxGetTranslation(wxString::FromUTF8(label));
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		if (in_type != TQSL_LOCATION_FIELD_BADZONE) {
			hsizer = new wxBoxSizer(wxHORIZONTAL);
			hsizer->Add(new wxStaticText(this, -1, lbl, wxDefaultPosition,
				wxSize(label_w, -1), wxALIGN_RIGHT/*|wxST_NO_AUTORESIZE*/), 0, wxTOP, 5);
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
					cbLabel = lbl;
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
				sdc.GetTextExtent(wxString::FromUTF8("X"), &w, &h);
				errlbl = new wxStaticText(this, -1, wxT(""), wxDefaultPosition,
					wxSize(w*tsize, -1), wxALIGN_LEFT|wxST_NO_AUTORESIZE);
				control_p = errlbl;
				break;
		}
		controls.push_back(control_p);
		if (in_type != TQSL_LOCATION_FIELD_BADZONE) {
			hsizer->Add(control_p, 0, wxLEFT|wxTOP, 5);
			sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 10);
		} else {
			sizer->Add(control_p, 0, wxEXPAND | wxLEFT| wxRIGHT, 10);
		}
	}

	if (addCheckbox) {
		wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
		okEmptyCB = new wxCheckBox(this, TQSL_ID_LOW+numf, _("Allow 'None' for ") + cbLabel, wxDefaultPosition, wxDefaultSize);

		hsizer->Add(new wxStaticText(this, -1, wxT(""),
			wxDefaultPosition, wxSize(label_w, -1), wxALIGN_RIGHT|wxST_NO_AUTORESIZE), 0, wxTOP, 5);
		hsizer->Add(okEmptyCB, 0, wxLEFT|wxTOP, 5);
		sizer->Add(hsizer, 0, wxLEFT|wxRight, 10);
	} else {
		okEmptyCB = 0;
	}
	initialized = true;
	UpdateFields();
	AdjustPage(sizer, wxT("stnloc1.htm"));
}

TQSLWizCertPage::~TQSLWizCertPage() {
}

const char *
TQSLWizCertPage::validate() {
	tqslTrace("TQSLWizCertPage::validate", NULL);

	if (!initialized) return 0;
	valMsg = wxT("");
	tqsl_setStationLocationCapturePage(loc, loc_page);
	for (int i = 0; i < static_cast<int>(controls.size()); i++) {
		char gabbi_name[40];
		int in_type;
		int flags;
		tqsl_getLocationFieldDataGABBI(loc, i, gabbi_name, sizeof gabbi_name);
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		tqsl_getLocationFieldFlags(loc, i, &flags);
		if (flags == TQSL_LOCATION_FIELD_SELNXT) {
			int index;
			tqsl_getLocationFieldIndex(loc, i, &index);
			if (index <= 0 && (okEmptyCB && !okEmptyCB->IsChecked())) {
				char label[256];
				tqsl_getLocationFieldDataLabel(loc, i, label, sizeof label);
				valMsg = wxString::Format(_("You must select a %hs"), label);
			}
			if (okEmptyCB) {
				if (index <= 0)
					okEmptyCB->Enable(true);
				else
					okEmptyCB->Enable(false);
			}
		} else if (strcmp(gabbi_name, "GRIDSQUARE") == 0) {
			wxString gridVal = (reinterpret_cast<wxTextCtrl *>(controls[i]))->GetValue();
			if (gridVal.size() == 0) {
				continue;
			}
			wxString editedGrids = wxT("");
			wxStringTokenizer grids(gridVal, wxT(","));	// Comma-separated list of squares
			while (grids.HasMoreTokens()) {
				wxString grid = grids.GetNextToken().Trim().Trim(false);
				// Truncate to six character field
				grid = grid.Left(6);
				if (grid[0] <= 'z' && grid[0] >= 'a')
					grid[0] = grid[0] - 'a' + 'A';	// Upper case first two
				if (grid.size() > 1 && (grid[1] <= 'z' && grid[1] >= 'a'))
					grid[1] = grid[1] - 'a' + 'A';
				if (grid[0] < 'A' || grid[0] > 'R') {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square Field"), grid.c_str());
				}
				if (grid.size() > 1 && (grid[1] < 'A' || grid[1] > 'R')) {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square Field"), grid.c_str());
				}
				if (grid.size() > 2 && (grid[2] < '0' || grid[2] > '9')) {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square"), grid.c_str());
				}
				if (grid.size() < 4) {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square"), grid.c_str());
				}
				if (grid[3] < '0' || grid[3] > '9') {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square"), grid.c_str());
				}

				if (grid.size() > 4 && (grid[4] <= 'Z' && grid[4] >= 'A'))
					grid[4] = grid[4] - 'A' + 'a';	// Lower case subsquare
				if (grid.size() > 5 && (grid[5] <= 'Z' && grid[5] >= 'A'))
					grid[5] = grid[5] - 'A' + 'a';

				if (grid.size() > 4 && (grid[4] < 'a' || grid[4] > 'x')) {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Subsquare"), grid.c_str());
				}
				if (grid.size() > 5 && (grid[5] < 'a' || grid[5] > 'x')) {
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Subsquare"), grid.c_str());
				}
				if (grid.size() != 6 && grid.size() != 4) {
					// Not long enough yet or too long.
					if (valMsg.IsEmpty())
						valMsg = wxString::Format(_("%s: Invalid Grid Square"), grid.c_str());
				}
				if (!editedGrids.IsEmpty())
					editedGrids += wxT(",");
				editedGrids += grid;
				(reinterpret_cast<wxTextCtrl *>(controls[i]))->SetValue(editedGrids);
				tqsl_setLocationFieldCharData(loc, i, (reinterpret_cast<wxTextCtrl *>(controls[i]))->GetValue().ToUTF8());
			}
		} else if (in_type == TQSL_LOCATION_FIELD_BADZONE) {
// Possible errors, here for harvesting
#ifdef tqsltranslate
	static const char* verrs[] = {
		__("Invalid zone selections for state"),
		__("Invalid zone selections for province"),
		__("Invalid zone selections for oblast"),
		__("Invalid zone selections for DXCC entity");
};
#endif
			char buf[256];
			tqsl_getLocationFieldCharData(loc, i, buf, sizeof buf);
			if (strlen(buf) > 0)
				valMsg = wxGetTranslation(wxString::FromUTF8(buf));
			(reinterpret_cast<wxStaticText *>(controls[i]))->SetLabel(valMsg);
		}
	}
	if (errlbl) errlbl->SetLabel(valMsg);
	return 0;
}

bool
TQSLWizCertPage::TransferDataFromWindow() {
	tqslTrace("TQSLWizCertPage::TransferDataFromWindow", NULL);

	tqsl_setStationLocationCapturePage(loc, loc_page);
	for (int i = 0; i < static_cast<int>(controls.size()); i++) {
		int in_type;
		tqsl_getLocationFieldInputType(loc, i, &in_type);
		switch(in_type) {
			case TQSL_LOCATION_FIELD_DDLIST:
			case TQSL_LOCATION_FIELD_LIST:
				break;
			case TQSL_LOCATION_FIELD_TEXT:
				tqsl_setLocationFieldCharData(loc, i, (reinterpret_cast<wxTextCtrl *>(controls[i]))->GetValue().ToUTF8());
				break;
		}
	}
	if (errlbl) errlbl->SetLabel(valMsg);
	return true;
}
void
TQSLWizCertPage::OnPageChanging(wxWizardEvent& ev) {
	tqslTrace("TQSLWizCertPage::OnPageChanging", "Direction=", ev.GetDirection());

	validate();
	if (valMsg.Len() > 0 && ev.GetDirection()) {
		ev.Veto();
		wxMessageBox(valMsg, _("Error"), wxOK | wxICON_ERROR, this);
	}
}

BEGIN_EVENT_TABLE(TQSLWizFinalPage, TQSLWizPage)
	EVT_LISTBOX(TQSL_ID_LOW, TQSLWizFinalPage::OnListbox)
	EVT_LISTBOX_DCLICK(TQSL_ID_LOW, TQSLWizFinalPage::OnListbox)
	EVT_TEXT(TQSL_ID_LOW+1, TQSLWizFinalPage::check_valid)
	EVT_WIZARD_PAGE_CHANGING(wxID_ANY, TQSLWizFinalPage::OnPageChanging)
END_EVENT_TABLE()

void
TQSLWizFinalPage::OnListbox(wxCommandEvent &) {
	tqslTrace("TQSLWizFinalPage::OnListbox", NULL);
	if (namelist->GetSelection() >= 0) {
		const char *cp = (const char *)(namelist->GetClientData(namelist->GetSelection()));
		if (cp)
			newname->SetValue(wxString::FromUTF8(cp).Trim(true).Trim(false));
	}
}

TQSLWizFinalPage::TQSLWizFinalPage(TQSLWizard *parent, tQSL_Location locp, TQSLWizPage *i_prev)
	: TQSLWizPage(parent, locp), prev(i_prev) {
	tqslTrace("TQSLWizFinalPage::TQSLWizFinalPage", "parent=0x%lx, locp=0x%lx, i_prev=0x%lx", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(locp), reinterpret_cast<void *>(i_prev));
	initialized = false;
	errlbl = NULL;
	valMsg = wxT("");
	wxSize text_size = getTextSize(this);
	int control_width = text_size.GetWidth()*40;

	int y = text_size.GetHeight();
	sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, _("Station Data input complete"));
	sizer->Add(st, 0, wxALIGN_CENTER|wxTOP, 10);

	// Title
	st = new wxStaticText(this, -1, _("Select or enter name of this station location"));
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
		item_data.push_back(strdup(buf));
		char cbuf[256];
		tqsl_getStationLocationCallSign(loc, i, cbuf, sizeof cbuf);
		wxString s = wxString::FromUTF8(buf);
		s += (wxString(wxT(" (")) + wxString::FromUTF8(cbuf) + wxString(wxT(")")));
		const void *v = item_data.back();
		namelist->Append(s, const_cast<void *>(v));
	}
	if (namelist->GetCount() > 0)
		namelist->SetSelection(0, FALSE);
	// New name
	st = new wxStaticText(this, -1, _("Station Location Name"));
	sizer->Add(st, 0, wxALIGN_LEFT|wxTOP, 10);
	newname = new wxTextCtrl(this, TQSL_ID_LOW+1, wxT(""), wxPoint(0, y), wxSize(control_width, -1));
	sizer->Add(newname, 0, wxEXPAND);
	newname->SetValue(parent->GetLocationName());
	errlbl = new wxStaticText(this, -1, wxT(""), wxDefaultPosition, wxSize(control_width, -1),
				wxALIGN_LEFT/*|wxST_NO_AUTORESIZE*/);
	sizer->Add(errlbl, 0, wxTOP);
	initialized = true;
	AdjustPage(sizer, wxT("stnloc2.htm"));
}

bool
TQSLWizFinalPage::TransferDataFromWindow() {
	tqslTrace("TQSLWizFinalPage::TransferDataFromWindow", NULL);
	validate();
	if (valMsg.Len() > 0) // Must be a "back"
		return true;
	wxString s = newname->GetValue().Trim(true).Trim(false);
	(reinterpret_cast<TQSLWizard *>(GetParent()))->SetLocationName(s);
	return true;
}

void
TQSLWizFinalPage::OnPageChanging(wxWizardEvent& ev) {
	tqslTrace("TQSLWizFinalPage::OnPageChanging", "Direction=", ev.GetDirection());

	validate();
	if (valMsg.Len() > 0 && ev.GetDirection()) {
		ev.Veto();
		wxMessageBox(valMsg, _("Error"), wxOK | wxICON_ERROR, this);
	}
}

const char *
TQSLWizFinalPage::validate() {
	tqslTrace("TQSLWizFinalPage::validate", NULL);
	if (!initialized) return 0;
	wxString val = newname->GetValue().Trim(true).Trim(false);
	valMsg = wxT("");

	if (val == wxT("")) {
		valMsg = wxGetTranslation(_("Station name must be provided"));
	}
	if (errlbl) errlbl->SetLabel(valMsg);
	return 0;
}

TQSLWizFinalPage::~TQSLWizFinalPage() {
	for (int i = 0; i < static_cast<int>(item_data.size()); i++) {
		free(item_data[i]);
	}
}
