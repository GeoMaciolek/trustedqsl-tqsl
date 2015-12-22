/***************************************************************************
                          crqwiz.h  -  description
                             -------------------
    begin                : Sat Jun 15 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __crqwiz_h
#define __crqwiz_h

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

#include "wx/wxhtml.h"

#include "certtree.h"

#ifndef ADIF_BOOLEAN
	#define ADIF_BOOLEAN // Hack!
#endif
#include "tqsllib.h"

#include <vector>

using std::vector;

class CRQ_Page;

class CRQWiz : public ExtWizard {
 public:
	CRQWiz(TQSL_CERT_REQ *crq, 	tQSL_Cert cert, wxWindow* parent, wxHtmlHelpController *help = 0,
		const wxString& title = _("Request a new Callsign Certificate"));
	CRQ_Page *GetCurrentPage() { return reinterpret_cast<CRQ_Page *>(wxWizard::GetCurrentPage()); }
	bool RunWizard();
	// ProviderPage data
	TQSL_PROVIDER provider;
	// IntroPage data
	wxString callsign;
	tQSL_Date qsonotbefore, qsonotafter;
	int dxcc;
	// NamePage data
	wxString name, addr1, addr2, city, state, zip, country;
	// EmailPage data
	wxString email;
	// PasswordPage data
	wxString password;
	// SignPage data
	tQSL_Cert cert;
	TQSL_CERT_REQ *_crq;
 private:
	CRQ_Page *_first;
};

class CRQ_Page : public ExtWizard_Page {
 public:
	explicit CRQ_Page(CRQWiz* parent = NULL) : ExtWizard_Page(parent) {valMsg = wxT("");}
	CRQWiz *Parent() { return reinterpret_cast<CRQWiz *>(_parent); }
	wxString valMsg;
};

class CRQ_ProviderPage : public CRQ_Page {
 public:
	explicit CRQ_ProviderPage(CRQWiz *parent, TQSL_CERT_REQ *crq = 0);
	virtual bool TransferDataFromWindow();
 private:
	void DoUpdateInfo();
	void UpdateInfo(wxCommandEvent&);
	vector<TQSL_PROVIDER> providers;
	wxComboBox *tc_provider;
	wxStaticText *tc_provider_info;

	DECLARE_EVENT_TABLE()
};

class CRQ_IntroPage : public CRQ_Page {
 public:
	explicit CRQ_IntroPage(CRQWiz *parent, TQSL_CERT_REQ *crq = 0);
	virtual bool TransferDataFromWindow();
	virtual const char *validate();
 private:
	wxTextCtrl *tc_call;
	wxComboBox *tc_qsobeginy, *tc_qsobeginm, *tc_qsobegind, *tc_dxcc;
	wxComboBox *tc_qsoendy, *tc_qsoendm, *tc_qsoendd;
	wxStaticText *tc_status;
	bool initialized;		// Set true when validating makes sense
	int em_w;

	DECLARE_EVENT_TABLE()
};

class CRQ_NamePage : public CRQ_Page {
 public:
	explicit CRQ_NamePage(CRQWiz *parent, TQSL_CERT_REQ *crq = 0);
	virtual bool TransferDataFromWindow();
	virtual const char *validate();
 private:
	wxTextCtrl *tc_name, *tc_addr1, *tc_addr2, *tc_city, *tc_state,
		*tc_zip, *tc_country;
	wxStaticText *tc_status;
	bool initialized;

	DECLARE_EVENT_TABLE()
};

class CRQ_EmailPage : public CRQ_Page {
 public:
	explicit CRQ_EmailPage(CRQWiz *parent, TQSL_CERT_REQ *crq = 0);
	virtual bool TransferDataFromWindow();
	virtual const char *validate();
 private:
	wxTextCtrl *tc_email;
	wxStaticText *tc_status;
	bool initialized;

	DECLARE_EVENT_TABLE()
};

class CRQ_PasswordPage : public CRQ_Page {
 public:
	explicit CRQ_PasswordPage(CRQWiz *parent);
	virtual bool TransferDataFromWindow();
	virtual const char *validate();
 private:
	wxTextCtrl *tc_pw1, *tc_pw2;
	wxStaticText *tc_status;
	bool initialized;

	DECLARE_EVENT_TABLE()
};

class CRQ_SignPage : public CRQ_Page {
 public:
	explicit CRQ_SignPage(CRQWiz *parent);
	virtual bool TransferDataFromWindow();
	void CertSelChanged(wxTreeEvent&);
	virtual const char *validate();
	virtual void refresh();
 private:
	wxRadioBox *choice;
	CertTree *cert_tree;
	wxStaticText *tc_status;
	bool initialized;
	int em_w;

	DECLARE_EVENT_TABLE()
};

inline bool
CRQWiz::RunWizard() {
	return wxWizard::RunWizard(_first);
}

#endif // __crqwiz_h
