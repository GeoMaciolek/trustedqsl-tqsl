/***************************************************************************
                          tqslwiz.h  -  description
                             -------------------
    begin                : Tue Nov 5 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __tqslwiz_h
#define __tqslwiz_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

using namespace std;

#include "wx/wxprec.h"

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include "wx/wizard.h"
#include "wx/radiobox.h"

#include "certtree.h"

#ifndef ADIF_BOOLEAN
	#define ADIF_BOOLEAN // Hack!
#endif
#include "tqsllib.h"

#include <vector>
#include <map>

#define TQSL_ID_LOW 6000

class TQSLWizPage;

class TQSLWizard : public wxWizard {
public:
	TQSLWizard(tQSL_Location locp, wxWindow* parent, int id = -1, const wxString& title = wxEmptyString,
		const wxBitmap& bitmap = wxNullBitmap, const wxPoint& pos = wxDefaultPosition);

	TQSLWizPage *GetPage(bool final = false);
	TQSLWizPage *GetCurrentTQSLPage() const { return (TQSLWizPage *)GetCurrentPage(); }
	void SetLocationName(wxString& s) { sl_name = s; }
	wxString GetLocationName() { return sl_name; }
	TQSLWizPage *GetFinalPage() { return (_pages.size() > 0) ? _pages[0] : 0; }

	bool page_changing;
private:
	void OnPageChanged(wxWizardEvent&);
	wxString sl_name;
	tQSL_Location loc;
	int _curpage;
	map<int, TQSLWizPage *> _pages;
	TQSLWizPage *final;

	DECLARE_EVENT_TABLE()
};

class TQSLWizPage : public wxWizardPage {
public:
	TQSLWizPage(TQSLWizard *parent, tQSL_Location locp) : wxWizardPage(parent) { loc = locp; }
	virtual TQSLWizard *GetParent() const { return (TQSLWizard *)wxWindow::GetParent(); }

	tQSL_Location loc;
};

class TQSLWizCertPage : public TQSLWizPage {
public:
	TQSLWizCertPage(TQSLWizard *parent, tQSL_Location locp);
	~TQSLWizCertPage();
	virtual bool TransferDataFromWindow();
	void OnComboBoxEvent(wxCommandEvent&);
	int loc_page;
	void UpdateFields(int noupdate_field = -1);
	virtual TQSLWizPage *GetPrev() const;
	virtual TQSLWizPage *GetNext() const;
private:
	virtual void OnSetFocus(wxFocusEvent& event);
	vector<void *> controls;

	DECLARE_EVENT_TABLE()
};

class TQSLWizFinalPage : public TQSLWizPage {
public:
	TQSLWizFinalPage(TQSLWizard *parent, tQSL_Location locp, TQSLWizPage *i_prev);
	TQSLWizPage *GetPrev() const { return prev; }
	TQSLWizPage *GetNext() const { return 0; }
	TQSLWizPage *prev;
	virtual bool TransferDataFromWindow();
	void OnListbox(wxCommandEvent &);
private:
	wxListBox *namelist;
	wxTextCtrl *newname;
	wxBoxSizer *sizer;
	vector<wxString> item_data;

	DECLARE_EVENT_TABLE()
};

#endif	// __tqslwiz_h
