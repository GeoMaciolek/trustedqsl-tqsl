/***************************************************************************
                          qsodatadialog.cpp  -  description
                             -------------------
    begin                : Sat Dec 7 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id: qsodatadialog.cpp,v 1.7 2013/03/01 12:59:37 k1mu Exp $
 ***************************************************************************/

#include "qsodatadialog.h"
#include <string>
#include <vector>
#include <algorithm>

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "tqslvalidator.h"
#include "wx/valgen.h"
#include "wx/spinctrl.h"
#include "wx/statline.h"
#include "tqsllib.h"
#include "tqslexcept.h"
#include "tqsltrace.h"

using std::vector;

#define TQSL_ID_LOW 6000

#ifdef __WIN32__
	#define TEXT_HEIGHT 24
	#define LABEL_HEIGHT 18
	#define TEXT_WIDTH 8
	#define TEXT_POINTS 10
	#define VSEP 3
	#define GEOM1 4
#elif defined(__APPLE__)
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

#undef TEXT_HEIGHT
#define TEXT_HEIGHT -1

#define SKIP_HEIGHT (TEXT_HEIGHT+VSEP)

#define QD_CALL	TQSL_ID_LOW
#define QD_DATE	TQSL_ID_LOW+1
#define QD_TIME	TQSL_ID_LOW+2
#define QD_MODE	TQSL_ID_LOW+3
#define QD_BAND	TQSL_ID_LOW+4
#define QD_FREQ	TQSL_ID_LOW+5
#define QD_OK TQSL_ID_LOW+6
#define QD_CANCEL TQSL_ID_LOW+7
#define QD_RECNO TQSL_ID_LOW+8
#define QD_RECDOWN TQSL_ID_LOW+9
#define QD_RECUP TQSL_ID_LOW+10
#define QD_RECBOTTOM TQSL_ID_LOW+11
#define QD_RECTOP TQSL_ID_LOW+12
#define QD_RECNEW TQSL_ID_LOW+13
#define QD_RECDELETE TQSL_ID_LOW+14
#define QD_RECNOLABEL TQSL_ID_LOW+15
#define QD_HELP TQSL_ID_LOW+16
#define QD_PROPMODE TQSL_ID_LOW+17
#define QD_SATELLITE TQSL_ID_LOW+18
#define QD_RXBAND TQSL_ID_LOW+19
#define QD_RXFREQ TQSL_ID_LOW+20


static void set_font(wxWindow *w, wxFont& font) {
#ifndef __WIN32__
	w->SetFont(font);
#endif
}

// Images for buttons.

#include "left.xpm"
#include "right.xpm"
#include "bottom.xpm"
#include "top.xpm"

class choice {
 public:
	choice(const wxString& _value, const wxString& _display = wxT(""), int _low = 0, int _high = 0) {
		value = _value;
		display = (_display == wxT("")) ? value : _display;
		low = _low;
		high = _high;
	}
	wxString value, display;
	int low, high;
	bool operator ==(const choice& other) { return other.value == value; }
};

class valid_list : public vector<choice> {
 public:
	valid_list() {}
	valid_list(const char **values, int nvalues);
	wxString *GetChoices() const;
};

valid_list::valid_list(const char **values, int nvalues) {
	while(nvalues--)
		push_back(choice(wxString::FromUTF8(*(values++))));
}

wxString *
valid_list::GetChoices() const {
	wxString *ary = new wxString[size()];
	wxString *sit = ary;
	const_iterator it;
	for (it = begin(); it != end(); it++)
		*sit++ = (*it).display;
	return ary;
}

static valid_list valid_modes;
static valid_list valid_bands;
static valid_list valid_rxbands;
static valid_list valid_propmodes;
static valid_list valid_satellites;

static int
init_valid_lists() {
	tqslTrace("init_valid_lists");
	if (valid_bands.size() > 0)
		return 0;
	if (tqsl_init())
		return 1;
	int count;
	if (tqsl_getNumMode(&count))
		return 1;
	const char *cp, *cp1;
	for (int i = 0; i < count; i++) {
		if (tqsl_getMode(i, &cp, 0))
			return 1;
		valid_modes.push_back(choice(wxString::FromUTF8(cp)));
	}
	valid_rxbands.push_back(choice(wxT(""), wxT("NONE")));
	if (tqsl_getNumBand(&count))
		return 1;
	for (int i = 0; i < count; i++) {
		int low, high, scale;
		if (tqsl_getBand(i, &cp, &cp1, &low, &high))
			return 1;
		wxString low_s = wxString::Format(wxT("%d"), low);
		wxString high_s = wxString::Format(wxT("%d"), high);
		const char *hz;
		if (!strcmp(cp1, "HF")) {
			hz = "kHz";
			scale = 1;		// Config file freqs are in KHz
		} else {
			hz = "mHz";
			scale = 1000;		// Freqs are in MHz for VHF/UHF.
		}
		if (low >= 1000) {
			low_s = wxString::Format(wxT("%g"), low / 1000.0);
			high_s = wxString::Format(wxT("%g"), high / 1000.0);
			if (!strcmp(cp1, "HF")) {
				hz = "MHz";
			} else {
				hz = "GHz";
			}
			if (high == 0)
				high_s = wxT("UP");
		}
		wxString display = wxString::Format(wxT("%hs (%s-%s %hs)"), cp,
                        low_s.c_str(), high_s.c_str(), hz);
		valid_bands.push_back(choice(wxString::FromUTF8(cp), display, low*scale, high*scale));
		valid_rxbands.push_back(choice(wxString::FromUTF8(cp), display, low*scale, high*scale));
	}
	valid_propmodes.push_back(choice(wxT(""), wxT("NONE")));
	if (tqsl_getNumPropagationMode(&count))
		return 1;
	for (int i = 0; i < count; i++) {
		if (tqsl_getPropagationMode(i, &cp, &cp1))
			return 1;
		valid_propmodes.push_back(choice(wxString::FromUTF8(cp), wxString::FromUTF8(cp1)));
	}
	valid_satellites.push_back(choice(wxT(""), wxT("NONE")));
	if (tqsl_getNumSatellite(&count))
		return 1;
	for (int i = 0; i < count; i++) {
		if (tqsl_getSatellite(i, &cp, &cp1, 0, 0))
			return 1;
		valid_satellites.push_back(choice(wxString::FromUTF8(cp), wxString::FromUTF8(cp1)));
	}
	return 0;
}

#define LABEL_WIDTH (22*TEXT_WIDTH)

BEGIN_EVENT_TABLE(QSODataDialog, wxDialog)
	EVT_BUTTON(QD_OK, QSODataDialog::OnOk)
	EVT_BUTTON(QD_CANCEL, QSODataDialog::OnCancel)
	EVT_BUTTON(QD_HELP, QSODataDialog::OnHelp)
	EVT_BUTTON(QD_RECDOWN, QSODataDialog::OnRecDown)
	EVT_BUTTON(QD_RECUP, QSODataDialog::OnRecUp)
	EVT_BUTTON(QD_RECBOTTOM, QSODataDialog::OnRecBottom)
	EVT_BUTTON(QD_RECTOP, QSODataDialog::OnRecTop)
	EVT_BUTTON(QD_RECNEW, QSODataDialog::OnRecNew)
	EVT_BUTTON(QD_RECDELETE, QSODataDialog::OnRecDelete)
END_EVENT_TABLE()

QSODataDialog::QSODataDialog(wxWindow *parent, wxHtmlHelpController *help, QSORecordList *reclist, wxWindowID id, const wxString& title)
	: wxDialog(parent, id, title), _reclist(reclist), _isend(false), _help(help) {
	tqslTrace("QSODataDialog::QSODataDialog", "parent=0x%lx, reclist=0x%lx, id=0x%lx, %s", reinterpret_cast<void *>(parent), reinterpret_cast<void *>(reclist), reinterpret_cast<void *>(id), S(title));
	wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);
	wxFont font = GetFont();
//	font.SetPointSize(TEXT_POINTS);
	set_font(this, font);

#define QD_MARGIN 3

	if (init_valid_lists())
		throw TQSLException(tqsl_getErrorString());
	// Call sign
	wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, wxT("Call Sign:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	_call_ctrl = new wxTextCtrl(this, QD_CALL, wxT(""), wxDefaultPosition, wxSize(14*TEXT_WIDTH, TEXT_HEIGHT),
		0, wxTextValidator(wxFILTER_NONE, &rec._call));
	sizer->Add(_call_ctrl, 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Date
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, wxT("UTC Date (YYYY-MM-DD):"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxTextCtrl(this, QD_DATE, wxT(""), wxDefaultPosition, wxSize(14*TEXT_WIDTH, TEXT_HEIGHT),
		0, TQSLDateValidator(&rec._date)), 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Time
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, wxT("UTC Time (HHMM):"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxTextCtrl(this, QD_TIME, wxT(""), wxDefaultPosition, wxSize(14*TEXT_WIDTH, TEXT_HEIGHT),
		0, TQSLTimeValidator(&rec._time)), 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Mode
	sizer = new wxBoxSizer(wxHORIZONTAL);
	wxString *choices = valid_modes.GetChoices();
	sizer->Add(new wxStaticText(this, -1, wxT("Mode:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxChoice(this, QD_MODE, wxDefaultPosition, wxDefaultSize,
		valid_modes.size(), choices, 0, wxGenericValidator(&_mode)), 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);
	// Band
	sizer = new wxBoxSizer(wxHORIZONTAL);
	choices = valid_bands.GetChoices();
	sizer->Add(new wxStaticText(this, -1, wxT("Band:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxChoice(this, QD_BAND, wxDefaultPosition, wxDefaultSize,
		valid_bands.size(), choices, 0, wxGenericValidator(&_band)), 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);
	// RX Band
	sizer = new wxBoxSizer(wxHORIZONTAL);
	choices = valid_rxbands.GetChoices();
	sizer->Add(new wxStaticText(this, -1, wxT("RX Band:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxChoice(this, QD_BAND, wxDefaultPosition, wxDefaultSize,
		valid_rxbands.size(), choices, 0, wxGenericValidator(&_rxband)), 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);
	// Frequency
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, wxT("Frequency (MHz):"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxTextCtrl(this, QD_FREQ, wxT(""), wxDefaultPosition, wxSize(14*TEXT_WIDTH, TEXT_HEIGHT),
		0, wxTextValidator(wxFILTER_NONE, &rec._freq)), 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// RX Frequency
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxStaticText(this, -1, wxT("RX Frequency (MHz):"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxTextCtrl(this, QD_RXFREQ, wxT(""), wxDefaultPosition, wxSize(14*TEXT_WIDTH, TEXT_HEIGHT),
		0, wxTextValidator(wxFILTER_NONE, &rec._rxfreq)), 0, wxALL, QD_MARGIN);
	topsizer->Add(sizer, 0);
	// Propagation Mode
	sizer = new wxBoxSizer(wxHORIZONTAL);
	choices = valid_propmodes.GetChoices();
	sizer->Add(new wxStaticText(this, -1, wxT("Propagation Mode:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxChoice(this, QD_PROPMODE, wxDefaultPosition, wxDefaultSize,
		valid_propmodes.size(), choices, 0, wxGenericValidator(&_propmode)), 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);
	// Satellite
	sizer = new wxBoxSizer(wxHORIZONTAL);
	choices = valid_satellites.GetChoices();
	sizer->Add(new wxStaticText(this, -1, wxT("Satellite:"), wxDefaultPosition,
		wxSize(LABEL_WIDTH, TEXT_HEIGHT), wxALIGN_RIGHT), 0, wxALL, QD_MARGIN);
	sizer->Add(new wxChoice(this, QD_SATELLITE, wxDefaultPosition, wxDefaultSize,
		valid_satellites.size(), choices, 0, wxGenericValidator(&_satellite)), 0, wxALL, QD_MARGIN);
	delete[] choices;
	topsizer->Add(sizer, 0);

	if (_reclist != 0) {
		if (_reclist->empty())
			_reclist->push_back(QSORecord());
		topsizer->Add(new wxStaticLine(this, -1), 0, wxEXPAND|wxLEFT|wxRIGHT, 10);
		_recno_label_ctrl = new wxStaticText(this, QD_RECNOLABEL, wxT(""), wxDefaultPosition,
			wxSize(20*TEXT_WIDTH, TEXT_HEIGHT), wxST_NO_AUTORESIZE|wxALIGN_CENTER);
		topsizer->Add(_recno_label_ctrl, 0, wxALIGN_CENTER|wxALL, 5);
		_recno = 1;
		sizer = new wxBoxSizer(wxHORIZONTAL);
		_recbottom_ctrl = new wxBitmapButton(this, QD_RECBOTTOM, wxBitmap(bottom_xpm), wxDefaultPosition, wxSize(18, TEXT_HEIGHT)),
		sizer->Add(_recbottom_ctrl, 0, wxTOP|wxBOTTOM, 5);
		_recdown_ctrl = new wxBitmapButton(this, QD_RECDOWN, wxBitmap(left_xpm), wxDefaultPosition, wxSize(18, TEXT_HEIGHT));
		sizer->Add(_recdown_ctrl, 0, wxTOP|wxBOTTOM, 5);
		_recno_ctrl = new wxTextCtrl(this, QD_RECNO, wxT("1"), wxDefaultPosition,
			wxSize(4*TEXT_WIDTH, TEXT_HEIGHT));
		_recno_ctrl->Enable(FALSE);
		sizer->Add(_recno_ctrl, 0, wxALL, 5);
		_recup_ctrl = new wxBitmapButton(this, QD_RECUP, wxBitmap(right_xpm), wxDefaultPosition, wxSize(18, TEXT_HEIGHT));
		sizer->Add(_recup_ctrl, 0, wxTOP|wxBOTTOM, 5);
		_rectop_ctrl = new wxBitmapButton(this, QD_RECTOP, wxBitmap(top_xpm), wxDefaultPosition, wxSize(18, TEXT_HEIGHT)),
		sizer->Add(_rectop_ctrl, 0, wxTOP|wxBOTTOM, 5);
		if (_reclist->size() > 0)
			rec = *(_reclist->begin());
		topsizer->Add(sizer, 0, wxALIGN_CENTER);
		sizer = new wxBoxSizer(wxHORIZONTAL);
		sizer->Add(new wxButton(this, QD_RECNEW, wxT("Add QSO")), 0, wxALL, 5);
		sizer->Add(new wxButton(this, QD_RECDELETE, wxT("Delete")), 0, wxALL, 5);
		topsizer->Add(sizer, 0, wxALIGN_CENTER);
	}

	topsizer->Add(new wxStaticLine(this, -1), 0, wxEXPAND|wxLEFT|wxRIGHT, 10);
	sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->Add(new wxButton(this, QD_HELP, wxT("Help")), 0, wxALL, 10);
	sizer->Add(new wxButton(this, QD_CANCEL, wxT("Cancel")), 0, wxALL, 10);
	sizer->Add(new wxButton(this, QD_OK, wxT("OK")), 0, wxALL, 10);
	topsizer->Add(sizer, 0, wxALIGN_CENTER);

	UpdateControls();

	SetAutoLayout(TRUE);
	SetSizer(topsizer);
	topsizer->Fit(this);
	topsizer->SetSizeHints(this);

	CentreOnParent();
}

QSODataDialog::~QSODataDialog() {
}

bool
QSODataDialog::TransferDataFromWindow() {
	tqslTrace("QSODataDialog::TransferDataFromWindow");
	rec._call.Trim(FALSE).Trim(TRUE);
	if (!wxDialog::TransferDataFromWindow())
		return false;
	if (_mode < 0 || _mode >= static_cast<int>(valid_modes.size()))
		return false;
	rec._mode = valid_modes[_mode].value;
	if (_band < 0 || _band >= static_cast<int>(valid_bands.size()))
		return false;
	rec._band = valid_bands[_band].value;
	rec._rxband = valid_rxbands[_rxband].value;
	rec._freq.Trim(FALSE).Trim(TRUE);
	rec._rxfreq.Trim(FALSE).Trim(TRUE);
	rec._propmode = valid_propmodes[_propmode].value;
	rec._satellite = valid_satellites[_satellite].value;

	double freq;

	if (!rec._freq.IsEmpty()) {
		if (!rec._freq.ToDouble(&freq)) {
			wxMessageBox(wxT("QSO Frequency is invalid"), wxT("QSO Data Error"),
				wxOK | wxICON_EXCLAMATION, this);
			return false;
		}
		freq = freq * 1000.0;		// Freq is is MHz but the limits are in KHz
		if (freq < valid_bands[_band].low || (valid_bands[_band].high > 0 && freq > valid_bands[_band].high)) {
			wxMessageBox(wxT("QSO Frequency is out of range for the selected band"), wxT("QSO Data Error"),
				wxOK | wxICON_EXCLAMATION, this);
			return false;
		}
	}

	if (!rec._rxfreq.IsEmpty()) {
		if (!rec._rxfreq.ToDouble(&freq)) {
			wxMessageBox(wxT("QSO RX Frequency is invalid"), wxT("QSO Data Error"),
				wxOK | wxICON_EXCLAMATION, this);
			return false;
		}
		freq = freq * 1000.0;		// Freq is is MHz but the limits are in KHz
		if (freq < valid_rxbands[_rxband].low || (valid_rxbands[_rxband].high > 0 && freq > valid_rxbands[_rxband].high)) {
			wxMessageBox(wxT("QSO RX Frequency is out of range for the selected band"), wxT("QSO Data Error"),
				wxOK | wxICON_EXCLAMATION, this);
			return false;
		}
	}
	if (!_isend && rec._call == wxT("")) {
		wxMessageBox(wxT("Call Sign cannot be empty"), wxT("QSO Data Error"),
			wxOK | wxICON_EXCLAMATION, this);
		return false;
	}
	if (rec._propmode == wxT("SAT") && rec._satellite == wxT("")) {
		wxMessageBox(wxT("'Satellite' propagation mode selected, so\na a Satellite must be chosen"), wxT("QSO Data Error"),
			wxOK | wxICON_EXCLAMATION, this);
		return false;
	}
	if (rec._propmode != wxT("SAT") && rec._satellite != wxT("")) {
		wxMessageBox(wxT("Satellite choice requires that\nPropagation Mode be 'Satellite'"), wxT("QSO Data Error"),
			wxOK | wxICON_EXCLAMATION, this);
		return false;
	}
	if (_reclist != 0)
			(*_reclist)[_recno-1] = rec;
	return true;
}

bool
QSODataDialog::TransferDataToWindow() {
	tqslTrace("QSODataDialog::TransferDataToWindow");
	valid_list::iterator it;
	if ((it = find(valid_modes.begin(), valid_modes.end(), rec._mode.Upper())) != valid_modes.end())
		_mode = distance(valid_modes.begin(), it);
	else
		wxLogWarning(wxT("QSO Data: Invalid Mode ignored - %s"), (const char*) rec._mode.Upper().ToUTF8());
	if ((it = find(valid_bands.begin(), valid_bands.end(), rec._band.Upper())) != valid_bands.end())
		_band = distance(valid_bands.begin(), it);
	if ((it = find(valid_rxbands.begin(), valid_rxbands.end(), rec._rxband.Upper())) != valid_rxbands.end())
		_rxband = distance(valid_rxbands.begin(), it);
	if ((it = find(valid_propmodes.begin(), valid_propmodes.end(), rec._propmode.Upper())) != valid_propmodes.end())
		_propmode = distance(valid_propmodes.begin(), it);
	if ((it = find(valid_satellites.begin(), valid_satellites.end(), rec._satellite.Upper())) != valid_satellites.end())
		_satellite = distance(valid_satellites.begin(), it);
	return wxDialog::TransferDataToWindow();
}

void
QSODataDialog::OnOk(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnOk");
	_isend = true;
	TransferDataFromWindow();
	_isend = false;
	if (rec._call == wxT("") && _recno == static_cast<int>(_reclist->size())) {
		_reclist->erase(_reclist->begin() + _recno - 1);
		EndModal(wxID_OK);
	} else if (Validate() && TransferDataFromWindow()) {
		EndModal(wxID_OK);
	}
}

void
QSODataDialog::OnCancel(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnCancel");
	EndModal(wxID_CANCEL);
}

void
QSODataDialog::OnHelp(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnHelp");
	if (_help)
		_help->Display(wxT("qsodata.htm"));
}

void
QSODataDialog::SetRecno(int new_recno) {
	tqslTrace("QSODataDialog::SetRecno", "new_recno=%d", new_recno);
	if (_reclist == NULL || new_recno < 1)
		return;
	if (TransferDataFromWindow()) {
//   		(*_reclist)[_recno-1] = rec;
		if (_reclist && new_recno > static_cast<int>(_reclist->size())) {
			new_recno = _reclist->size() + 1;
			QSORecord newrec;
			// Copy QSO fields from current record
			if (_recno > 0) {
				newrec = (*_reclist)[_recno-1];
				newrec._call = wxT("");
			}
			_reclist->push_back(newrec);
		}
		_recno = new_recno;
		if (_reclist) rec = (*_reclist)[_recno-1];
		TransferDataToWindow();
		UpdateControls();
		_call_ctrl->SetFocus();
	}
}

void
QSODataDialog::OnRecDown(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecDown");
	SetRecno(_recno - 1);
}

void
QSODataDialog::OnRecUp(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecUp");
	SetRecno(_recno + 1);
}

void
QSODataDialog::OnRecBottom(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecBottom");
	SetRecno(1);
}

void
QSODataDialog::OnRecTop(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecTop");
	if (_reclist == 0)
		return;
	SetRecno(_reclist->size());
}

void
QSODataDialog::OnRecNew(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecNew");
	if (_reclist == 0)
		return;
	SetRecno(_reclist->size()+1);
}

void
QSODataDialog::OnRecDelete(wxCommandEvent&) {
	tqslTrace("QSODataDialog::OnRecDelete");
	if (_reclist == 0)
		return;
	_reclist->erase(_reclist->begin() + _recno - 1);
	if (_reclist->empty())
		_reclist->push_back(QSORecord());
	if (_recno > static_cast<int>(_reclist->size()))
		_recno = _reclist->size();
	rec = (*_reclist)[_recno-1];
	TransferDataToWindow();
	UpdateControls();
}

void
QSODataDialog::UpdateControls() {
	tqslTrace("QSODataDialog::UpdateControls");
	if (_reclist == 0)
		return;
	_recdown_ctrl->Enable(_recno > 1);
	_recbottom_ctrl->Enable(_recno > 1);
	_recup_ctrl->Enable(_recno < static_cast<int>(_reclist->size()));
	_rectop_ctrl->Enable(_recno < static_cast<int>(_reclist->size()));
	_recno_ctrl->SetValue(wxString::Format(wxT("%d"), _recno));
	_recno_label_ctrl->SetLabel(wxString::Format(wxT("%d QSO Record%hs"), static_cast<int>(_reclist->size()),
		(_reclist->size() == 1) ? "" : "s"));
}
