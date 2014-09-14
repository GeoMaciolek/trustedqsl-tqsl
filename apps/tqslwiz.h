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

#include "wx/wxprec.h"

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include "extwizard.h"
#include "wx/radiobox.h"

#include "certtree.h"

#ifndef ADIF_BOOLEAN
	#define ADIF_BOOLEAN // Hack!
#endif
#include "tqsllib.h"

#include <vector>
#include <map>

using std::map;
using std::vector;

#define TQSL_ID_LOW 6000

class TQSLWizPage;

class TQSLWizard : public ExtWizard {
 public:
	TQSLWizard(tQSL_Location locp, wxWindow *parent, wxHtmlHelpController *help = 0, const wxString& title = wxEmptyString, bool expired = false);

//	TQSLWizard(tQSL_Location locp, wxWindow* parent, int id = -1, const wxString& title = wxEmptyString,
//		const wxBitmap& bitmap = wxNullBitmap, const wxPoint& pos = wxDefaultPosition);

	TQSLWizPage *GetPage(bool final = false);
	TQSLWizPage *GetCurrentTQSLPage() { return reinterpret_cast<TQSLWizPage *>(GetCurrentPage()); }
	void SetLocationName(wxString& s) { sl_name = s; }
	void SetDefaultCallsign(wxString& c) {sl_call = c; }
	wxString GetLocationName() { return sl_name; }
	wxString GetDefaultCallsign() {return sl_call; }
	TQSLWizPage *GetFinalPage() { return (_pages.size() > 0) ? _pages[0] : 0; }
	bool expired;

 private:
	void OnPageChanged(wxWizardEvent&);
	wxString sl_name;
	wxString sl_call;
	tQSL_Location loc;
	int _curpage;
	map<int, TQSLWizPage *> _pages;
	TQSLWizPage *final;

	DECLARE_EVENT_TABLE()
};

class TQSLWizPage : public ExtWizard_Page {
 public:
	TQSLWizPage(TQSLWizard *parent, tQSL_Location locp) : ExtWizard_Page(parent) { loc = locp; }
	virtual TQSLWizard *GetParent() const { return reinterpret_cast<TQSLWizard *>(wxWindow::GetParent()); }

	tQSL_Location loc;
	bool initialized;
	wxString valMsg;
};

class TQSLWizCertPage : public TQSLWizPage {
 public:
	TQSLWizCertPage(TQSLWizard *parent, tQSL_Location locp);
	~TQSLWizCertPage();
	virtual bool TransferDataFromWindow();
	void OnComboBoxEvent(wxCommandEvent&);
	void OnCheckBoxEvent(wxCommandEvent&);
	int loc_page;
	void UpdateFields(int noupdate_field = -1);
	virtual const char *validate();
	virtual TQSLWizPage *GetPrev() const;
	virtual TQSLWizPage *GetNext() const;
	void OnSize(wxSizeEvent&);
	void OnPageChanging(wxWizardEvent &);
 private:
	vector<void *> controls;
	wxStaticText *errlbl;
	wxCheckBox *okEmptyCB;
	DECLARE_EVENT_TABLE()
};

class TQSLWizFinalPage : public TQSLWizPage {
 public:
	TQSLWizFinalPage(TQSLWizard *parent, tQSL_Location locp, TQSLWizPage *i_prev);
	~TQSLWizFinalPage();
	TQSLWizPage *GetPrev() const { return prev; }
	TQSLWizPage *GetNext() const { return 0; }
	void OnPageChanged(wxWizardEvent &);
	TQSLWizPage *prev;
	virtual bool TransferDataFromWindow();
	void OnListbox(wxCommandEvent &);
	virtual const char *validate();
	void OnPageChanging(wxWizardEvent &);
 private:
	wxListBox *namelist;
	wxTextCtrl *newname;
	wxStaticText *errlbl;
	wxBoxSizer *sizer;
	vector<char *> item_data;

	DECLARE_EVENT_TABLE()
};

#endif	// __tqslwiz_h
