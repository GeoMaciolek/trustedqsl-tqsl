/***************************************************************************
                          crqwiz.cpp  -  description
                             -------------------
    begin                : Sat Jun 15 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include <ctype.h>
#include "crqwiz.h"
#include "dxcc.h"
#include "util.h"
#include "tqslcertctrls.h"

#include "wx/validate.h"
#include "wx/datetime.h"
#include "wx/config.h"

#include <algorithm>

#include <iostream>

using namespace std;

CRQWiz::CRQWiz(TQSL_CERT_REQ *crq, tQSL_Cert xcert, wxWindow *parent, wxHtmlHelpController *help,
	const wxString& title) :
	ExtWizard(parent, help, title), cert(xcert), _crq(crq)  {

	CRQ_ProviderPage *provider = new CRQ_ProviderPage(this, _crq);
	CRQ_IntroPage *intro = new CRQ_IntroPage(this, _crq);
	CRQ_NamePage *name = new CRQ_NamePage(this, _crq);
	CRQ_EmailPage *email = new CRQ_EmailPage(this, _crq);
	CRQ_PasswordPage *pw = new CRQ_PasswordPage(this);
	CRQ_SignPage *sign = new CRQ_SignPage(this);
	wxWizardPageSimple::Chain(provider, intro);
	wxWizardPageSimple::Chain(intro, name);
	wxWizardPageSimple::Chain(name, email);
	wxWizardPageSimple::Chain(email, pw);
	if (!cert)
		wxWizardPageSimple::Chain(pw, sign);
	_first = provider;
	AdjustSize();
	CenterOnParent();
}


// Page constructors

BEGIN_EVENT_TABLE(CRQ_ProviderPage, CRQ_Page)
	EVT_COMBOBOX(ID_CRQ_PROVIDER, CRQ_ProviderPage::UpdateInfo)
END_EVENT_TABLE()

static bool
prov_cmp(const TQSL_PROVIDER& p1, const TQSL_PROVIDER& p2) {
	return strcasecmp(p1.organizationName, p2.organizationName) < 0;
}

CRQ_ProviderPage::CRQ_ProviderPage(CRQWiz *parent, TQSL_CERT_REQ *crq) :  CRQ_Page(parent) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("M"));
	int em_h = st->GetSize().GetHeight();
	int em_w = st->GetSize().GetWidth();
	st->SetLabel(wxT("This will create a new certificate request file.\n\n"
		"Once you supply the requested information and the\n"
		"request file has been created, you must send the\n"
		"request file to the certificate issuer."));
	st->SetSize(em_w * 30, em_h * 5);

	sizer->Add(st, 0, wxALL, 10);
	
	sizer->Add(new wxStaticText(this, -1, wxT("Certificate Issuer:")), 0, wxLEFT|wxRIGHT, 10);
	tc_provider = new wxComboBox(this, ID_CRQ_PROVIDER, wxT(""), wxDefaultPosition,
		wxDefaultSize, 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
	sizer->Add(tc_provider, 0, wxLEFT|wxRIGHT|wxEXPAND, 10);
	tc_provider_info = new wxStaticText(this, ID_CRQ_PROVIDER_INFO, wxT(""), wxDefaultPosition,
		wxSize(0, em_h*5));
	sizer->Add(tc_provider_info, 0, wxALL|wxEXPAND, 10);
	int nprov = 0;
	if (tqsl_getNumProviders(&nprov))
		wxMessageBox(wxString(tqsl_getErrorString(), wxConvLocal), wxT("Error"));
	for (int i = 0; i < nprov; i++) {
		TQSL_PROVIDER prov;
		if (!tqsl_getProvider(i, &prov))
			providers.push_back(prov);
	}
	sort(providers.begin(), providers.end(), prov_cmp);
	int selected = -1;
	for (int i = 0; i < (int)providers.size(); i++) {
		tc_provider->Append(wxString(providers[i].organizationName, wxConvLocal), (void *)i);
		if (crq && !strcmp(providers[i].organizationName, crq->providerName)
			&& !strcmp(providers[i].organizationalUnitName, crq->providerUnit)) {
			selected = i;
		}
	}
	tc_provider->SetSelection((selected < 0) ? 0 : selected);
	if (providers.size() < 2 || selected >= 0)
		tc_provider->Enable(false);
	DoUpdateInfo();
	AdjustPage(sizer, wxT("crq.htm"));
}

void
CRQ_ProviderPage::DoUpdateInfo() {
	int sel = tc_provider->GetSelection();
	if (sel >= 0) {
		long idx = (long)(tc_provider->GetClientData(sel));
		if (idx >=0 && idx < (int)providers.size()) {
			Parent()->provider = providers[idx];
			wxString info;
			info = wxString(Parent()->provider.organizationName, wxConvLocal);
			if (Parent()->provider.organizationalUnitName[0] != 0)
				info += wxString(wxT("\n  ")) + wxString(Parent()->provider.organizationalUnitName, wxConvLocal);
			if (Parent()->provider.emailAddress[0] != 0)
				info += wxString(wxT("\nEmail: ")) + wxString(Parent()->provider.emailAddress, wxConvLocal);
			if (Parent()->provider.url[0] != 0)
				info += wxString(wxT("\nURL: ")) + wxString(Parent()->provider.url, wxConvLocal);
			tc_provider_info->SetLabel(info);
		}
	}
}

void
CRQ_ProviderPage::UpdateInfo(wxCommandEvent&) {
	DoUpdateInfo();
}


static wxDateTime::Month mons[] = {
	wxDateTime::Inv_Month, wxDateTime::Jan, wxDateTime::Feb, wxDateTime::Mar,
	wxDateTime::Apr, wxDateTime::May, wxDateTime::Jun, wxDateTime::Jul,
	wxDateTime::Aug, wxDateTime::Sep, wxDateTime::Oct, wxDateTime::Nov,
	wxDateTime::Dec };

BEGIN_EVENT_TABLE(CRQ_IntroPage, CRQ_Page)
	EVT_TEXT(ID_CRQ_CALL, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_DXCC, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QBYEAR, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QBMONTH, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QBDAY, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QEYEAR, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QEMONTH, CRQ_Page::check_valid)
	EVT_COMBOBOX(ID_CRQ_QEDAY, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_IntroPage::CRQ_IntroPage(CRQWiz *parent, TQSL_CERT_REQ *crq) :  CRQ_Page(parent) {
	initialized = false;
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *dst = new wxStaticText(this, -1, wxT("DXCC entity:"));
	wxStaticText *st = new wxStaticText(this, -1, wxT("M"), wxDefaultPosition, wxDefaultSize,
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT);
	int em_h = st->GetSize().GetHeight();
	int em_w = st->GetSize().GetWidth();
	st->SetLabel(wxT("Call sign:"));
	st->SetSize(dst->GetSize());

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(st, 0, wxRIGHT, 5);
	wxString cs;
	if (crq && crq->callSign)
		cs = wxString(crq->callSign, wxConvLocal);
	tc_call = new wxTextCtrl(this, ID_CRQ_CALL, cs, wxDefaultPosition, wxSize(em_w*15, -1));
	hsizer->Add(tc_call, 0, wxEXPAND, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 10);
	if (crq && crq->callSign)
		tc_call->Enable(false);

	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(dst, 0, wxRIGHT, 5);
	tc_dxcc = new wxComboBox(this, ID_CRQ_DXCC, wxT(""), wxDefaultPosition,
		wxSize(em_w*25, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
	hsizer->Add(tc_dxcc, 1, 0, 0);
	sizer->Add(hsizer, 0, wxALL, 10);

	DXCC dx;
	bool ok = dx.getFirst();
	while (ok) {
		tc_dxcc->Append(wxString(dx.name(), wxConvLocal), (void *)dx.number());
		ok = dx.getNext();
	}
	const char *ent = "NONE";
	if (crq) {
		if (dx.getByEntity(crq->dxccEntity)) {
			ent = dx.name();
			tc_dxcc->Enable(false);
		}
	}
	int i = tc_dxcc->FindString(wxString(ent, wxConvLocal));
	if (i >= 0)
		tc_dxcc->SetSelection(i);
	struct {
		wxComboBox **cb;
		int id;
	} boxes[][3] = {
		{ {&tc_qsobeginy, ID_CRQ_QBYEAR}, {&tc_qsobeginm, ID_CRQ_QBMONTH}, {&tc_qsobegind,ID_CRQ_QBDAY} },
		{ {&tc_qsoendy, ID_CRQ_QEYEAR}, {&tc_qsoendm, ID_CRQ_QEMONTH}, {&tc_qsoendd,ID_CRQ_QEDAY} }
	};
	const char *labels[] = { "QSO begin date:", "QSO end date:" };

	int year = wxDateTime::GetCurrentYear();

	int sels[2][3];
	int dates[2][3];
	if (crq) {
		dates[0][0] = crq->qsoNotBefore.year;
		dates[0][1] = crq->qsoNotBefore.month;
		dates[0][2] = crq->qsoNotBefore.day;
		dates[1][0] = crq->qsoNotAfter.year;
		dates[1][1] = crq->qsoNotAfter.month;
		dates[1][2] = crq->qsoNotAfter.day;
	}
	for (int i = 0; i < int(sizeof labels / sizeof labels[0]); i++) {
		sels[i][0] = sels[i][1] = sels[i][2] = 0;
		sizer->Add(new wxStaticText(this, -1, wxString(labels[i], wxConvLocal)), 0, wxBOTTOM, 5);
		hsizer = new wxBoxSizer(wxHORIZONTAL);
		hsizer->Add(new wxStaticText(this, -1, wxT("Y")), 0, wxLEFT, 20);
		*(boxes[i][0].cb) = new wxComboBox(this, boxes[i][0].id, wxT(""), wxDefaultPosition,
			wxSize(em_w*8, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		hsizer->Add(*(boxes[i][0].cb), 0, wxLEFT, 5);
		hsizer->Add(new wxStaticText(this, -1, wxT("M")), 0, wxLEFT, 10);
		*(boxes[i][1].cb) = new wxComboBox(this, boxes[i][1].id, wxT(""), wxDefaultPosition,
			wxSize(em_w*5, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		hsizer->Add(*(boxes[i][1].cb), 0, wxLEFT, 5);
		hsizer->Add(new wxStaticText(this, -1, wxT("D")), 0, wxLEFT, 10);
		*(boxes[i][2].cb) = new wxComboBox(this, boxes[i][2].id, wxT(""), wxDefaultPosition,
			wxSize(em_w*5, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		hsizer->Add(*(boxes[i][2].cb), 0, wxLEFT, 5);
		int iofst = 0;
		if (i > 0) {
			iofst++;
			for (int j = 0; j < 3; j++)
				(*(boxes[i][j].cb))->Append(wxT(""));
		}
		for (int j = 1945; j <= year; j++) {
			wxString s;
			s.Printf(wxT("%d"), j);
			if (crq && dates[i][0] == j)
				sels[i][0] = j - 1945 + iofst;
			(*(boxes[i][0].cb))->Append(s);
		}
		year++;
		for (int j = 1; j <= 12; j++) {
			wxString s;
			s.Printf(wxT("%d"), j);
			if (crq && dates[i][1] == j)
				sels[i][1] = j - 1 + iofst;
			(*(boxes[i][1].cb))->Append(s);
		}
		for (int j = 1; j <= 31; j++) {
			wxString s;
			s.Printf(wxT("%d"), j);
			if (crq && dates[i][2] == j)
				sels[i][2] = j - 1 + iofst;
			(*(boxes[i][2].cb))->Append(s);
		}
		sizer->Add(hsizer, 0, wxLEFT|wxRIGHT, 10);
	}
	if (crq) {
		tc_qsobeginy->SetSelection(sels[0][0]);
		tc_qsobeginm->SetSelection(sels[0][1]);
		tc_qsobegind->SetSelection(sels[0][2]);
		wxDateTime now = wxDateTime::Now();
		wxDateTime qsoEnd(crq->qsoNotAfter.day, mons[crq->qsoNotAfter.month],
			crq->qsoNotAfter.year, 23, 59, 59);
		if (qsoEnd < now) {
			// Looks like this is a cert for an expired call sign,
			// so keep the QSO end date as-is. Otherwise, leave it
			// blank so CA can fill it in.
			tc_qsoendy->SetSelection(sels[1][0]);
			tc_qsoendm->SetSelection(sels[1][1]);
			tc_qsoendd->SetSelection(sels[1][2]);
		}
	} else {
		tc_qsobeginy->SetSelection(0);
		tc_qsobeginm->SetSelection(10);
		tc_qsobegind->SetSelection(0);
	}
	tc_status = new wxStaticText(this, -1, wxT(""), wxDefaultPosition, wxSize(0, em_h*2));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, wxT("crq0.htm"));
	initialized = true;
}

BEGIN_EVENT_TABLE(CRQ_NamePage, CRQ_Page)
	EVT_TEXT(ID_CRQ_NAME, CRQ_Page::check_valid)
	EVT_TEXT(ID_CRQ_ADDR1, CRQ_Page::check_valid)
	EVT_TEXT(ID_CRQ_CITY, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_NamePage::CRQ_NamePage(CRQWiz *parent, TQSL_CERT_REQ *crq) :  CRQ_Page(parent) {
	initialized = false;
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *zst = new wxStaticText(this, -1, wxT("Zip/Postal"));

	wxStaticText *st = new wxStaticText(this, -1, wxT("M"), wxDefaultPosition, wxDefaultSize,
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT);
	int em_w = st->GetSize().GetWidth();
	int def_w = em_w * 20;
	st->SetLabel(wxT("Name"));
	st->SetSize(zst->GetSize());

	wxConfig *config = (wxConfig *)wxConfig::Get();
	wxString val;
	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(st, 0, wxRIGHT, 5);
	wxString s;
	if (crq && crq->name)
		s = wxString(crq->name, wxConvLocal);
	else if (config->Read(wxT("Name"), &val))
		s = val;
	tc_name = new wxTextCtrl(this, ID_CRQ_NAME, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_name, 1, 0, 0);
	sizer->Add(hsizer, 0, wxALL, 10);

	s = wxT("");
	if (crq && crq->address1)
		s = wxString(crq->address1, wxConvLocal);
	else if (config->Read(wxT("Addr1"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, wxT("Address"), wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_addr1 = new wxTextCtrl(this, ID_CRQ_ADDR1, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_addr1, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);

	s = wxT("");
	if (crq && crq->address2)
		s = wxString(crq->address2, wxConvLocal);
	else if (config->Read(wxT("Addr2"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, wxT(""), wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_addr2 = new wxTextCtrl(this, ID_CRQ_ADDR2, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_addr2, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);

	s = wxT("");
	if (crq && crq->city)
		s = wxString(crq->city, wxConvLocal);
	else if (config->Read(wxT("City"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, wxT("City"), wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_city = new wxTextCtrl(this, ID_CRQ_CITY, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_city, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);

	s = wxT("");
	if (crq && crq->state)
		s = wxString(crq->state, wxConvLocal);
	else if (config->Read(wxT("State"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, wxT("State"), wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_state = new wxTextCtrl(this, ID_CRQ_STATE, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_state, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);

	s = wxT("");
	if (crq && crq->postalCode)
		s = wxString(crq->postalCode, wxConvLocal);
	else if (config->Read(wxT("ZIP"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(zst, 0, wxRIGHT, 5);
	tc_zip = new wxTextCtrl(this, ID_CRQ_ZIP, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_zip, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);

	s = wxT("");
	if (crq && crq->country)
		s = wxString(crq->country, wxConvLocal);
	else if (config->Read(wxT("Country"), &val))
		s = val;
	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, wxT("Country"), wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_country = new wxTextCtrl(this, ID_CRQ_COUNTRY, s, wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_country, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	tc_status = new wxStaticText(this, -1, wxT(""));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, wxT("crq1.htm"));
	initialized = true;
}

BEGIN_EVENT_TABLE(CRQ_EmailPage, CRQ_Page)
	EVT_TEXT(ID_CRQ_EMAIL, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_EmailPage::CRQ_EmailPage(CRQWiz *parent, TQSL_CERT_REQ *crq) :  CRQ_Page(parent) {
	initialized = false;
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("M"));
	int em_w = st->GetSize().GetWidth();
	st->SetLabel(wxT("Your e-mail address"));

	wxConfig *config = (wxConfig *)wxConfig::Get();
	wxString val;
	wxString s;
	if (crq && crq->emailAddress)
		s = wxString(crq->emailAddress, wxConvLocal);
	else if (config->Read(wxT("Email"), &val))
		s = val;
	sizer->Add(st, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	tc_email = new wxTextCtrl(this, ID_CRQ_EMAIL, s, wxDefaultPosition, wxSize(em_w*30, -1));
	sizer->Add(tc_email, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	sizer->Add(new wxStaticText(this, -1, wxT("Note: The e-mail address you provide here is the\n"
		"address to which the issued certificate will be sent.\n"
		"Make sure it's the correct address!\n")), 0, wxALL, 10);
	tc_status = new wxStaticText(this, -1, wxT(""));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, wxT("crq2.htm"));
	initialized = true;
}

BEGIN_EVENT_TABLE(CRQ_PasswordPage, CRQ_Page)
	EVT_TEXT(ID_CRQ_PW1, CRQ_Page::check_valid)
	EVT_TEXT(ID_CRQ_PW2, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_PasswordPage::CRQ_PasswordPage(CRQWiz *parent) :  CRQ_Page(parent) {
	initialized = false;

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, wxT("M"));
	int em_w = st->GetSize().GetWidth();
	int em_h = st->GetSize().GetHeight();
	st->SetLabel(
wxT("You may protect your private key for this certificate\n"
"using a password. Doing so is recommended.\n\n"
"Password:")
	);
	st->SetSize(em_w * 30, em_h * 5);

	sizer->Add(st, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	tc_pw1 = new wxTextCtrl(this, ID_CRQ_PW1, wxT(""), wxDefaultPosition, wxSize(em_w*20, -1), wxTE_PASSWORD);
	sizer->Add(tc_pw1, 0, wxLEFT|wxRIGHT, 10);
	sizer->Add(new wxStaticText(this, -1, wxT("Enter the password again for verification:")),
		0, wxLEFT|wxRIGHT|wxTOP, 10);
	tc_pw2 = new wxTextCtrl(this, ID_CRQ_PW2, wxT(""), wxDefaultPosition, wxSize(em_w*20, -1), wxTE_PASSWORD);
	sizer->Add(tc_pw2, 0, wxLEFT|wxRIGHT, 10);
	sizer->Add(new wxStaticText(this, -1, wxT("DO NOT lose the password you choose!\n"
		"You will be unable to use the certificate\nwithout this password!")),
		0, wxALL, 10);
	tc_status = new wxStaticText(this, -1, wxT(""));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, wxT("crq3.htm"));
	initialized = true;
}

BEGIN_EVENT_TABLE(CRQ_SignPage, CRQ_Page)
	EVT_TREE_SEL_CHANGED(ID_CRQ_CERT, CRQ_SignPage::CertSelChanged)
	EVT_RADIOBOX(ID_CRQ_SIGN, CRQ_Page::check_valid)
END_EVENT_TABLE()


void CRQ_SignPage::CertSelChanged(wxTreeEvent& event) {
	if (cert_tree->GetItemData(event.GetItem()))
		choice->SetSelection(1);
	wxCommandEvent dummy;
	check_valid(dummy);
}

CRQ_SignPage::CRQ_SignPage(CRQWiz *parent)
	:  CRQ_Page(parent) {

	initialized = false;
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	tc_status = new wxStaticText(this, -1, wxT("M"));
	int em_h = tc_status->GetSize().GetHeight();
	int em_w = tc_status->GetSize().GetWidth();

	wxString choices[] = { wxT("Unsigned"), wxT("Signed") };

	choice = new wxRadioBox(this, ID_CRQ_SIGN, wxT("Sign Request"), wxDefaultPosition,
		wxSize(em_w*30, -1), 2, choices, 1, wxRA_SPECIFY_COLS);
	sizer->Add(choice, 0, wxALL|wxEXPAND, 10);
	cert_tree = new CertTree(this, ID_CRQ_CERT, wxDefaultPosition,
		wxSize(em_w*30, em_h*10), wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);
	sizer->Add(cert_tree, 0, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND);
	cert_tree->SetBackgroundColour(wxColour(255, 255, 255));
	tc_status->SetLabel(wxT(""));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, wxT("crq4.htm"));
	initialized = true;
}

void
CRQ_SignPage::refresh() {
	if (cert_tree->Build(0, &(Parent()->provider)) > 0)
		choice->SetSelection(1);
	else
		choice->SetSelection(0);
}

// Page validation

bool
CRQ_ProviderPage::TransferDataFromWindow() {
	// Nothing to validate
	return true;
}

const char *
CRQ_IntroPage::validate() {
	if (!initialized)
		return 0;
	wxString val = tc_call->GetValue();
	bool ok = true;
	wxString msg(wxT("You must enter a valid call sign."));
	static wxCharBuffer msg_buf(1);

	if (val.Len() < 3)
		ok = false;
	if (ok) {
		bool havealpha = false, havenumeric = false;
		const wxWX2MBbuf tmp_buf = val.mb_str();
		const char *cp = (const char *)tmp_buf;
		while (ok && *cp) {
			char c = toupper(*cp++);
			if (isalpha(c))
				havealpha = true;
			else if (isdigit(c))
				havenumeric = true;
			else if (c != '/')
				ok = false;
		}
		ok = (ok && havealpha && havenumeric);
	}
	Parent()->dxcc = (long)(tc_dxcc->GetClientData(tc_dxcc->GetSelection()));
	if (Parent()->dxcc < 0) {
		msg = wxT("You must select a DXCC entity.");
		ok = false;
	}
	if (ok) {
		Parent()->qsonotbefore.year = atoi(tc_qsobeginy->GetStringSelection().mb_str());
		Parent()->qsonotbefore.month = atoi(tc_qsobeginm->GetStringSelection().mb_str());
		Parent()->qsonotbefore.day = atoi(tc_qsobegind->GetStringSelection().mb_str());
		Parent()->qsonotafter.year = atoi(tc_qsoendy->GetStringSelection().mb_str());
		Parent()->qsonotafter.month = atoi(tc_qsoendm->GetStringSelection().mb_str());
		Parent()->qsonotafter.day = atoi(tc_qsoendd->GetStringSelection().mb_str());
		if (!tqsl_isDateValid(&Parent()->qsonotbefore)) {
			msg = wxT("QSO begin date: You must choose proper values for\nYear, Month and Day.");
			ok = false;
		} else if (!tqsl_isDateNull(&Parent()->qsonotafter) && !tqsl_isDateValid(&Parent()->qsonotafter)) {
			msg = wxT("QSO end date: You must either choose proper values\nfor Year, Month and Day or leave all three blank.");
			ok = false;
		} else if (tqsl_isDateValid(&Parent()->qsonotafter)
			&& tqsl_compareDates(&Parent()->qsonotbefore, &Parent()->qsonotafter) > 0) {
			msg = wxT("QSO end date cannot be before QSO begin date.");
			ok = false;
		}
	}

	// Data looks okay, now let's make sure this isn't a duplicate request
	// (unless it's a renewal).

	if (ok && !Parent()->_crq) {
		val.MakeUpper();
		tQSL_Cert *certlist = 0;
		int ncert = 0;
		tqsl_selectCertificates(&certlist, &ncert, val.mb_str(), Parent()->dxcc, 0,
			&(Parent()->provider), TQSL_SELECT_CERT_WITHKEYS);
//cerr << "ncert: " << ncert << endl;
		if (ncert > 0) {
			char cert_before_buf[40], cert_after_buf[40];
			for (int i = 0; i < ncert; i++) {
				// See if this cert overlaps the user-specified date range
				tQSL_Date cert_not_before, cert_not_after;
				int cert_dxcc = 0;
				tqsl_getCertificateQSONotBeforeDate(certlist[i], &cert_not_before);
				tqsl_getCertificateQSONotAfterDate(certlist[i], &cert_not_after);
				tqsl_getCertificateDXCCEntity(certlist[i], &cert_dxcc);
//cerr << "dxcc: " << cert_dxcc << endl;
				if (cert_dxcc == Parent()->dxcc
					&& ((tqsl_isDateValid(&Parent()->qsonotafter)
						&& !(tqsl_compareDates(&Parent()->qsonotbefore, &cert_not_after) == 1
						|| tqsl_compareDates(&Parent()->qsonotafter, &cert_not_before) == -1))
					|| (!tqsl_isDateValid(&Parent()->qsonotafter)
						&& !(tqsl_compareDates(&Parent()->qsonotbefore, &cert_not_after) == 1))
				)) {
					ok = false;	// Overlap!
					tqsl_convertDateToText(&cert_not_before, cert_before_buf, sizeof cert_before_buf);
					tqsl_convertDateToText(&cert_not_after, cert_after_buf, sizeof cert_after_buf);
				}
				tqsl_freeCertificate(certlist[i]);
			}
			if (ok == false) {
				msg = wxString::Format(wxT("You have an overlapping certificate for %s\n(DXCC=%d)\nhaving QSO dates: "), val.c_str(), Parent()->dxcc);
				msg += wxString(cert_before_buf, wxConvLocal) + wxT(" to ") + wxString(cert_after_buf, wxConvLocal);
			}
		}
	}

	if (!ok) {
		tc_status->SetLabel(msg);
		msg_buf = msg.mb_str();
		return (const char *)msg_buf;
	}
	tc_status->SetLabel(wxT(""));
	return 0;
}

bool
CRQ_IntroPage::TransferDataFromWindow() {
	if (validate())		// Should only happen when going Back
		return true;
	if (Parent()->dxcc == 0)
		wxMessageBox(
			wxT("You have selected DXCC Entity NONE\n\n"
			"QSO records signed using the certificate will not\n"
			"be valid for DXCC award credit (but will be valid \n"
			"for other applicable awards). If the certificate is\n"
			"to be used for signing QSOs from maritime/marine\n"
			"mobile, shipboard, or air mobile operations, that is\n"
			"the correct selection. Otherwise, you probably\n"
			"should use the \"Back\" button to return to the DXCC\n"
			"page after clicking \"OK\""),
			wxT("TQSLCert Warning"));
	Parent()->callsign = tc_call->GetValue();
	Parent()->callsign.MakeUpper();
	tc_call->SetValue(Parent()->callsign);
	return true;
}

static bool
cleanString(wxString &str) {
	str.Trim();
	str.Trim(FALSE);
	int idx;
	while ((idx = str.Find(wxT("  "))) > 0) {
		str.Remove(idx, 1);
	}
	return str.IsEmpty();
}

const char *
CRQ_NamePage::validate() {
	if (!initialized)
		return 0;
	Parent()->name = tc_name->GetValue();
	Parent()->addr1 = tc_addr1->GetValue();
	Parent()->city = tc_city->GetValue();

	const char *errmsg = 0;

	if (cleanString(Parent()->name))
		errmsg = "You must enter your name";
	if (!errmsg && cleanString(Parent()->addr1))
		errmsg = "You must enter your address";
	if (!errmsg && cleanString(Parent()->city))
		errmsg = "You must enter your city";
	tc_status->SetLabel(errmsg ? wxString(errmsg, wxConvLocal) : wxT(""));
	return errmsg;
}

bool
CRQ_NamePage::TransferDataFromWindow() {
	Parent()->name = tc_name->GetValue();
	Parent()->addr1 = tc_addr1->GetValue();
	Parent()->addr2 = tc_addr2->GetValue();
	Parent()->city = tc_city->GetValue();
	Parent()->state = tc_state->GetValue();
	Parent()->zip = tc_zip->GetValue();
	Parent()->country = tc_country->GetValue();

	if (validate())
		return true;

	cleanString(Parent()->name);
	cleanString(Parent()->addr1);
	cleanString(Parent()->addr2);
	cleanString(Parent()->city);
	cleanString(Parent()->state);
	cleanString(Parent()->zip);
	cleanString(Parent()->country);
	tc_name->SetValue(Parent()->name);
	tc_addr1->SetValue(Parent()->addr1);
	tc_addr2->SetValue(Parent()->addr2);
	tc_city->SetValue(Parent()->city);
	tc_state->SetValue(Parent()->state);
	tc_zip->SetValue(Parent()->zip);
	tc_country->SetValue(Parent()->country);
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->Write(wxT("Name"), Parent()->name);
	config->Write(wxT("Addr1"), Parent()->addr1);
	config->Write(wxT("Addr2"), Parent()->addr2);
	config->Write(wxT("City"), Parent()->city);
	config->Write(wxT("State"), Parent()->state);
	config->Write(wxT("ZIP"), Parent()->zip);
	config->Write(wxT("Country"), Parent()->country);
	return true;
}

const char *
CRQ_EmailPage::validate() {
	const char *errmsg = 0;
	if (!initialized)
		return 0;
	Parent()->email = tc_email->GetValue();
	cleanString(Parent()->email);
	int i = Parent()->email.First('@');
	int j = Parent()->email.Last('.');
	if (i < 1 || j < i+2 || j == (int)Parent()->email.length()-1)
		errmsg = "You must enter a valid email address";
	tc_status->SetLabel(errmsg ? wxString(errmsg, wxConvLocal) : wxT(""));
	return errmsg;
}

bool
CRQ_EmailPage::TransferDataFromWindow() {
	if (validate())
		return true;
	Parent()->email = tc_email->GetValue();
	cleanString(Parent()->email);
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->Write(wxT("Email"), Parent()->email);
	return true;
}

const char *
CRQ_PasswordPage::validate() {
	const char *errmsg = 0;
	if (!initialized)
		return 0;
	wxString pw1 = tc_pw1->GetValue();
	wxString pw2 = tc_pw2->GetValue();

	if (pw1 != pw2)
		errmsg = "The two copies of the password do not match.";
	tc_status->SetLabel(errmsg ? wxString(errmsg, wxConvLocal) : wxT(""));
	return errmsg;
}

bool
CRQ_PasswordPage::TransferDataFromWindow() {
	if (validate())
		return true;
	Parent()->password = tc_pw1->GetValue();
	return true;
}

const char *
CRQ_SignPage::validate() {
	const char *errmsg = 0;

	if (!initialized)
		return 0;
	if (choice->GetSelection() == 1) {
		CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
		if (!data)
			errmsg = "You must select a signing certificate from the list";
	} else if (Parent()->dxcc == 0)
		errmsg = "Request must be signed (DXCC Entity == NONE)";
	tc_status->SetLabel(errmsg ? wxString(errmsg, wxConvLocal) : wxT(""));
	return errmsg;
}

bool
CRQ_SignPage::TransferDataFromWindow() {
	if (validate())
		return true;
	Parent()->cert = 0;
	if (choice->GetSelection() == 0) {
		if (cert_tree->GetNumCerts() > 0) {
			int rval = wxMessageBox(
wxT("You have one or more certificates that could be used\n"
"to sign this certificate request.\n\n"
"It is strongly recommended that you sign the request\n"
"unless the certificates shown are not actually yours.\n"
"Certificate issuers may choose not to permit unsigned\n"
"certificate requests from existing certificate holders.\n\n"
"Do you want to sign the certificate request?\n"
"Select \"Yes\" to sign the request, \"No\" to continue without signing."),
				wxT("Warning"), wxYES_NO, this);
			if (rval == wxYES)
				return false;
		}
	} else {
		CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
		if (data)
		Parent()->cert = data->getCert();
	}
	return true;
}
