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

#include "wx/wizard.h"
#include "wx/radiobox.h"

#include "certtree.h"

#ifndef ADIF_BOOLEAN
	#define ADIF_BOOLEAN // Hack!
#endif
#include "tqsllib.h"

#include <vector>

class CRQ_ProviderPage : public wxWizardPageSimple {
public:
	CRQ_ProviderPage(wxWizard *parent, TQSL_CERT_REQ *crq = 0);
	virtual bool TransferDataFromWindow();
	TQSL_PROVIDER provider;
private:
	void DoUpdateInfo();
	void UpdateInfo(wxCommandEvent&);
	std::vector<TQSL_PROVIDER> providers;
	wxComboBox *tc_provider;
	wxStaticText *tc_provider_info;

	DECLARE_EVENT_TABLE()
};

class CRQ_IntroPage : public wxWizardPageSimple {
public:
	CRQ_IntroPage(wxWizard *parent, TQSL_CERT_REQ *crq = 0);
	virtual bool TransferDataFromWindow();
	wxString callsign;
	tQSL_Date qsonotbefore, qsonotafter;
	int dxcc;
private:
	wxTextCtrl *tc_call;
	wxComboBox *tc_qsobeginy, *tc_qsobeginm, *tc_qsobegind, *tc_dxcc;
	wxComboBox *tc_qsoendy, *tc_qsoendm, *tc_qsoendd;
};

class CRQ_NamePage : public wxWizardPageSimple {
public:
	CRQ_NamePage(wxWizard *parent, TQSL_CERT_REQ *crq = 0);
	virtual bool TransferDataFromWindow();
	wxString name, addr1, addr2, city, state, zip, country;
private:
	wxTextCtrl *tc_name, *tc_addr1, *tc_addr2, *tc_city, *tc_state,
		*tc_zip, *tc_country;
};

class CRQ_EmailPage : public wxWizardPageSimple {
public:
	CRQ_EmailPage(wxWizard *parent, TQSL_CERT_REQ *crq = 0);
	virtual bool TransferDataFromWindow();
	wxString email;
private:
	wxTextCtrl *tc_email;
};

class CRQ_PasswordPage : public wxWizardPageSimple {
public:
	CRQ_PasswordPage(wxWizard *parent);
	virtual bool TransferDataFromWindow();
	wxString password;
private:
	wxTextCtrl *tc_pw1, *tc_pw2;
};

class CRQ_SignPage : public wxWizardPageSimple {
public:
	CRQ_SignPage(wxWizard *parent, wxSize& size);
	virtual bool TransferDataFromWindow();
	void CertSelChanged(wxTreeEvent&);
	tQSL_Cert cert;
private:
	wxRadioBox *choice;
	CertTree *cert_tree;

	DECLARE_EVENT_TABLE()
};

#endif // __crqwiz_h
