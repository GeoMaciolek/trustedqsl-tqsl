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

	wxStaticText *st = new wxStaticText(this, -1, "M");
	int em_h = st->GetSize().GetHeight();
	st->SetLabel("This will create a new certificate request file.\n\n"
		"Once you supply the requested information and the\n"
		"request file has been created, you must send the\n"
		"request file to the certificate issuer.");

	sizer->Add(st, 0, wxALL, 10);
	
	sizer->Add(new wxStaticText(this, -1, "Certificate Issuer:"), 0, wxLEFT|wxRIGHT, 10);
	tc_provider = new wxComboBox(this, ID_CRQ_PROVIDER, "", wxDefaultPosition,
		wxDefaultSize, 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
	sizer->Add(tc_provider, 0, wxLEFT|wxRIGHT|wxEXPAND, 10);
	tc_provider_info = new wxStaticText(this, ID_CRQ_PROVIDER_INFO, "", wxDefaultPosition,
		wxSize(0, em_h*5));
	sizer->Add(tc_provider_info, 0, wxALL|wxEXPAND, 10);
	int nprov = 0;
	if (tqsl_getNumProviders(&nprov))
		wxMessageBox(tqsl_getErrorString(), "Error");
	for (int i = 0; i < nprov; i++) {
		TQSL_PROVIDER prov;
		if (!tqsl_getProvider(i, &prov))
			providers.push_back(prov);
	}
	sort(providers.begin(), providers.end(), prov_cmp);
	int selected = -1;
	for (int i = 0; i < (int)providers.size(); i++) {
		tc_provider->Append(providers[i].organizationName, (void *)i);
		if (crq && !strcmp(providers[i].organizationName, crq->providerName)
			&& !strcmp(providers[i].organizationalUnitName, crq->providerUnit)) {
			selected = i;
		}
	}
	tc_provider->SetSelection((selected < 0) ? 0 : selected);
	if (providers.size() < 2 || selected >= 0)
		tc_provider->Enable(false);
	DoUpdateInfo();
	AdjustPage(sizer, "crq.htm");
}

void
CRQ_ProviderPage::DoUpdateInfo() {
	int sel = tc_provider->GetSelection();
	if (sel >= 0) {
		int idx = (int)(tc_provider->GetClientData(sel));
		if (idx >=0 && idx < (int)providers.size()) {
			Parent()->provider = providers[idx];
			wxString info;
			info = Parent()->provider.organizationName;
			if (Parent()->provider.organizationalUnitName[0] != 0)
				info += wxString("\n  ") + Parent()->provider.organizationalUnitName;
			if (Parent()->provider.emailAddress[0] != 0)
				info += wxString("\nEmail: ") + Parent()->provider.emailAddress;
			if (Parent()->provider.url[0] != 0)
				info += wxString("\nURL: ") + Parent()->provider.url;
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
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *dst = new wxStaticText(this, -1, "DXCC entity:");
	wxStaticText *st = new wxStaticText(this, -1, "M", wxDefaultPosition, wxDefaultSize,
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT);
	int em_h = st->GetSize().GetHeight();
	int em_w = st->GetSize().GetWidth();
	st->SetLabel("Call sign:");
	st->SetSize(dst->GetSize());

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(st, 0, wxRIGHT, 5);
	tc_call = new wxTextCtrl(this, ID_CRQ_CALL, "", wxDefaultPosition, wxSize(em_w*15, -1));
	hsizer->Add(tc_call, 0, wxEXPAND, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxTOP|wxEXPAND, 10);
	if (crq && crq->callSign) {
		tc_call->SetValue(crq->callSign);
		tc_call->Enable(false);
	}

	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(dst, 0, wxRIGHT, 5);
	tc_dxcc = new wxComboBox(this, ID_CRQ_DXCC, "", wxDefaultPosition,
		wxSize(em_w*25, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
	hsizer->Add(tc_dxcc, 1, 0, 0);
	sizer->Add(hsizer, 0, wxALL, 10);

	DXCC dx;
	bool ok = dx.getFirst();
	while (ok) {
		tc_dxcc->Append(dx.name(), (void *)dx.number());
		ok = dx.getNext();
	}
	const char *ent = "UNITED STATES";
	if (crq) {
		if (dx.getByEntity(crq->dxccEntity)) {
			ent = dx.name();
			tc_dxcc->Enable(false);
		}
	}
	int i = tc_dxcc->FindString(ent);
	if (i >= 0)
		tc_dxcc->SetSelection(i);
	struct {
		wxComboBox **cb;
		int id;
	} boxes[][3] = {
		{ {&tc_qsobeginy, ID_CRQ_QBYEAR}, {&tc_qsobeginm, ID_CRQ_QBMONTH}, {&tc_qsobegind,ID_CRQ_QBDAY} },
		{ {&tc_qsoendy, ID_CRQ_QEYEAR}, {&tc_qsoendm, ID_CRQ_QEMONTH}, {&tc_qsoendd,ID_CRQ_QEDAY} }
	};
	char *labels[] = { "QSO begin date:", "QSO end date:" };

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
		sizer->Add(new wxStaticText(this, -1, labels[i]), 0, wxBOTTOM, 5);
		hsizer = new wxBoxSizer(wxHORIZONTAL);
		hsizer->Add(new wxStaticText(this, -1, "Y"), 0, wxLEFT, 20);
		*(boxes[i][0].cb) = new wxComboBox(this, boxes[i][0].id, "", wxDefaultPosition,
			wxSize(em_w*8, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		hsizer->Add(*(boxes[i][0].cb), 0, wxLEFT, 5);
		hsizer->Add(new wxStaticText(this, -1, "M"), 0, wxLEFT, 10);
		*(boxes[i][1].cb) = new wxComboBox(this, boxes[i][1].id, "", wxDefaultPosition,
			wxSize(em_w*5, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		hsizer->Add(*(boxes[i][1].cb), 0, wxLEFT, 5);
		hsizer->Add(new wxStaticText(this, -1, "D"), 0, wxLEFT, 10);
		*(boxes[i][2].cb) = new wxComboBox(this, boxes[i][2].id, "", wxDefaultPosition,
			wxSize(em_w*5, -1), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		hsizer->Add(*(boxes[i][2].cb), 0, wxLEFT, 5);
		int iofst = 0;
		if (i > 0) {
			iofst++;
			for (int j = 0; j < 3; j++)
				(*(boxes[i][j].cb))->Append("");
		}
		for (int j = 1945; j <= year; j++) {
			wxString s;
			s.Printf("%d", j);
			if (crq && dates[i][0] == j)
				sels[i][0] = j - 1945 + iofst;
			(*(boxes[i][0].cb))->Append(s);
		}
		year++;
		for (int j = 1; j <= 12; j++) {
			wxString s;
			s.Printf("%d", j);
			if (crq && dates[i][1] == j)
				sels[i][1] = j - 1 + iofst;
			(*(boxes[i][1].cb))->Append(s);
		}
		for (int j = 1; j <= 31; j++) {
			wxString s;
			s.Printf("%d", j);
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
	tc_status = new wxStaticText(this, -1, "", wxDefaultPosition, wxSize(0, em_h*2));
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, "crq0.htm");
}

BEGIN_EVENT_TABLE(CRQ_NamePage, CRQ_Page)
	EVT_TEXT(ID_CRQ_NAME, CRQ_Page::check_valid)
	EVT_TEXT(ID_CRQ_ADDR1, CRQ_Page::check_valid)
	EVT_TEXT(ID_CRQ_CITY, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_NamePage::CRQ_NamePage(CRQWiz *parent, TQSL_CERT_REQ *crq) :  CRQ_Page(parent) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *zst = new wxStaticText(this, -1, "Zip/Postal");

	wxStaticText *st = new wxStaticText(this, -1, "M", wxDefaultPosition, wxDefaultSize,
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT);
	int em_w = st->GetSize().GetWidth();
	int def_w = em_w * 20;
	st->SetLabel("Name");
	st->SetSize(zst->GetSize());

	wxBoxSizer *hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(st, 0, wxRIGHT, 5);
	tc_name = new wxTextCtrl(this, ID_CRQ_NAME, "", wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_name, 1, 0, 0);
	sizer->Add(hsizer, 0, wxALL, 10);
	wxConfig *config = (wxConfig *)wxConfig::Get();
	wxString val;
	if (crq && crq->name)
		tc_name->SetValue(crq->name);
	else if (config->Read("Name", &val))
		tc_name->SetValue(val);

	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, "Address", wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_addr1 = new wxTextCtrl(this, ID_CRQ_ADDR1, "", wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_addr1, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	if (crq && crq->address1)
		tc_addr1->SetValue(crq->address1);
	else if (config->Read("Addr1", &val))
		tc_addr1->SetValue(val);

	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, "", wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_addr2 = new wxTextCtrl(this, ID_CRQ_ADDR2, "", wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_addr2, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	if (crq && crq->address2)
		tc_addr2->SetValue(crq->address2);
	else if (config->Read("Addr2", &val))
		tc_addr2->SetValue(val);

	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, "City", wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_city = new wxTextCtrl(this, ID_CRQ_CITY, "", wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_city, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	if (crq && crq->city)
		tc_city->SetValue(crq->city);
	else if (config->Read("City", &val))
		tc_city->SetValue(val);

	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, "State", wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_state = new wxTextCtrl(this, ID_CRQ_STATE, "", wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_state, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	if (crq && crq->state)
		tc_state->SetValue(crq->state);
	else if (config->Read("State", &val))
		tc_state->SetValue(val);

	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(zst, 0, wxRIGHT, 5);
	tc_zip = new wxTextCtrl(this, ID_CRQ_ZIP, "", wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_zip, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	if (crq && crq->postalCode)
		tc_zip->SetValue(crq->postalCode);
	else if (config->Read("ZIP", &val))
		tc_zip->SetValue(val);

	hsizer = new wxBoxSizer(wxHORIZONTAL);
	hsizer->Add(new wxStaticText(this, -1, "Country", wxDefaultPosition, zst->GetSize(),
		wxST_NO_AUTORESIZE|wxALIGN_RIGHT), 0, wxRIGHT, 5);
	tc_country = new wxTextCtrl(this, ID_CRQ_COUNTRY, "", wxDefaultPosition, wxSize(def_w, -1));
	hsizer->Add(tc_country, 1, 0, 0);
	sizer->Add(hsizer, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	if (crq && crq->country)
		tc_country->SetValue(crq->country);
	else if (config->Read("Country", &val))
		tc_country->SetValue(val);
	tc_status = new wxStaticText(this, -1, "");
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, "crq1.htm");
}

BEGIN_EVENT_TABLE(CRQ_EmailPage, CRQ_Page)
	EVT_TEXT(ID_CRQ_EMAIL, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_EmailPage::CRQ_EmailPage(CRQWiz *parent, TQSL_CERT_REQ *crq) :  CRQ_Page(parent) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, "M");
	int em_w = st->GetSize().GetWidth();
	st->SetLabel("Your e-mail address");

	sizer->Add(st, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	tc_email = new wxTextCtrl(this, ID_CRQ_EMAIL, "", wxDefaultPosition, wxSize(em_w*30, -1));
	sizer->Add(tc_email, 0, wxLEFT|wxRIGHT|wxBOTTOM, 10);
	wxConfig *config = (wxConfig *)wxConfig::Get();
	wxString val;
	if (crq && crq->emailAddress)
		tc_email->SetValue(crq->emailAddress);
	else if (config->Read("Email", &val))
		tc_email->SetValue(val);
	sizer->Add(new wxStaticText(this, -1, "Note: The e-mail address you provide here is the\n"
		"address to which the issued certificate will be sent.\n"
		"Make sure it's the correct address!\n"), 0, wxALL, 10);
	tc_status = new wxStaticText(this, -1, "");
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, "crq2.htm");
}

BEGIN_EVENT_TABLE(CRQ_PasswordPage, CRQ_Page)
	EVT_TEXT(ID_CRQ_PW1, CRQ_Page::check_valid)
	EVT_TEXT(ID_CRQ_PW2, CRQ_Page::check_valid)
END_EVENT_TABLE()

CRQ_PasswordPage::CRQ_PasswordPage(CRQWiz *parent) :  CRQ_Page(parent) {
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText *st = new wxStaticText(this, -1, "M");
	int em_w = st->GetSize().GetWidth();
	st->SetLabel(
"You may protect your private key for this certificate\n"
"using a password. Doing so is recommended.\n\n"
"Password:"
	);

	sizer->Add(st, 0, wxLEFT|wxRIGHT|wxTOP, 10);
	tc_pw1 = new wxTextCtrl(this, ID_CRQ_PW1, "", wxDefaultPosition, wxSize(em_w*20, -1), wxTE_PASSWORD);
	sizer->Add(tc_pw1, 0, wxLEFT|wxRIGHT, 10);
	sizer->Add(new wxStaticText(this, -1, "Enter the password again for verification:"),
		0, wxLEFT|wxRIGHT|wxTOP, 10);
	tc_pw2 = new wxTextCtrl(this, ID_CRQ_PW2, "", wxDefaultPosition, wxSize(em_w*20, -1), wxTE_PASSWORD);
	sizer->Add(tc_pw2, 0, wxLEFT|wxRIGHT, 10);
	sizer->Add(new wxStaticText(this, -1, "DO NOT lose the password you choose!\n"
		"You will be unable to use the certificate\nwithout this password!"),
		0, wxALL, 10);
	tc_status = new wxStaticText(this, -1, "");
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, "crq3.htm");
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
	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	tc_status = new wxStaticText(this, -1, "M");
	int em_h = tc_status->GetSize().GetHeight();
	int em_w = tc_status->GetSize().GetWidth();

	wxString choices[] = { "Unsigned", "Signed" };

	choice = new wxRadioBox(this, ID_CRQ_SIGN, "Sign Request", wxDefaultPosition,
		wxSize(em_w*30, -1), 2, choices, 1, wxRA_SPECIFY_COLS);
	sizer->Add(choice, 0, wxALL|wxEXPAND, 10);
	cert_tree = new CertTree(this, ID_CRQ_CERT, wxDefaultPosition,
		wxSize(em_w*30, em_h*10), wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);
	sizer->Add(cert_tree, 0, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND);
	cert_tree->SetBackgroundColour(wxColour(255, 255, 255));
	tc_status->SetLabel("");
	sizer->Add(tc_status, 0, wxALL|wxEXPAND, 10);
	AdjustPage(sizer, "crq4.htm");
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
	wxString val = tc_call->GetValue();
	bool ok = true;
	const char *errmsg = "You must enter a valid call sign.";
	static wxString msg;

	if (val.Len() < 3)
		ok = false;
	if (ok) {
		bool havealpha = false, havenumeric = false;
		const char *cp = val.c_str();
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
	Parent()->dxcc = (int)(tc_dxcc->GetClientData(tc_dxcc->GetSelection()));
	if (Parent()->dxcc < 1) {
		errmsg = "You must select a DXCC entity.";
		ok = false;
	}
	if (ok) {
		Parent()->qsonotbefore.year = atoi(tc_qsobeginy->GetStringSelection());
		Parent()->qsonotbefore.month = atoi(tc_qsobeginm->GetStringSelection());
		Parent()->qsonotbefore.day = atoi(tc_qsobegind->GetStringSelection());
		Parent()->qsonotafter.year = atoi(tc_qsoendy->GetStringSelection());
		Parent()->qsonotafter.month = atoi(tc_qsoendm->GetStringSelection());
		Parent()->qsonotafter.day = atoi(tc_qsoendd->GetStringSelection());
		if (!tqsl_isDateValid(&Parent()->qsonotbefore)) {
			errmsg = "QSO begin date: You must choose proper values for\nYear, Month and Day.";
			ok = false;
		} else if (!tqsl_isDateNull(&Parent()->qsonotafter) && !tqsl_isDateValid(&Parent()->qsonotafter)) {
			errmsg = "QSO end date: You must either choose proper values\nfor Year, Month and Day or leave all three blank.";
			ok = false;
		} else if (tqsl_isDateValid(&Parent()->qsonotafter)
			&& tqsl_compareDates(&Parent()->qsonotbefore, &Parent()->qsonotafter) > 0) {
			errmsg = "QSO end date cannot be before QSO begin date.";
			ok = false;
		}
	}

	// Data looks okay, now let's make sure this isn't a duplicate request
	// (unless it's a renewal).

	if (ok && !Parent()->_crq) {
		val.MakeUpper();
		tQSL_Cert *certlist = 0;
		int ncert = 0;
		tqsl_selectCertificates(&certlist, &ncert, val.c_str(), Parent()->dxcc, 0,
			&(Parent()->provider), TQSL_SELECT_CERT_WITHKEYS);
		if (ncert > 0) {
			for (int i = 0; i < ncert; i++)
				tqsl_freeCertificate(certlist[i]);
			wxString msg = wxString::Format("There is already a certificate for %s (DXCC=%d)",
				val.c_str(), Parent()->dxcc);
			errmsg = msg.c_str();
			ok = false;
		}
	}

	if (!ok) {
		tc_status->SetLabel(errmsg);
		return errmsg;
	}
	tc_status->SetLabel("");
	return 0;
}

bool
CRQ_IntroPage::TransferDataFromWindow() {
	if (validate())		// Should only happen when going Back
		return true;
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
	while ((idx = str.Find("  ")) > 0) {
		str.Remove(idx, 1);
	}
	return str.IsEmpty();
}

const char *
CRQ_NamePage::validate() {
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
	tc_status->SetLabel(errmsg ? errmsg : "");
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
	config->Write("Name", Parent()->name);
	config->Write("Addr1", Parent()->addr1);
	config->Write("Addr2", Parent()->addr2);
	config->Write("City", Parent()->city);
	config->Write("State", Parent()->state);
	config->Write("ZIP", Parent()->zip);
	config->Write("Country", Parent()->country);
	return true;
}

const char *
CRQ_EmailPage::validate() {
	const char *errmsg = 0;
	Parent()->email = tc_email->GetValue();
	cleanString(Parent()->email);
	int i = Parent()->email.First('@');
	int j = Parent()->email.Last('.');
	if (i < 1 || j < i+2 || j == (int)Parent()->email.length()-1)
		errmsg = "You must enter a valid email address";
	tc_status->SetLabel(errmsg ? errmsg : 0);
	return errmsg;
}

bool
CRQ_EmailPage::TransferDataFromWindow() {
	if (validate())
		return true;
	Parent()->email = tc_email->GetValue();
	cleanString(Parent()->email);
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->Write("Email", Parent()->email);
	return true;
}

const char *
CRQ_PasswordPage::validate() {
	const char *errmsg = 0;
	wxString pw1 = tc_pw1->GetValue();
	wxString pw2 = tc_pw2->GetValue();
	
	if (pw1 != pw2)
		errmsg = "The two copies of the password do not match.";
	tc_status->SetLabel(errmsg ? errmsg : "");
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

	if (choice->GetSelection() == 1) {
		CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
		if (!data)
			errmsg = "You must select a signing certificate from the list";
	}
	tc_status->SetLabel(errmsg ? errmsg : "");
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
"You have one or more certificates that could be used\n"
"to sign this certificate request.\n\n"
"It is strongly recommended that you sign the request\n"
"unless the certificates shown are not actually yours.\n"
"Certificate issuers may choose not to permit unsigned\n"
"certificate requests from existing certificate holders.\n\n"
"Do you want to sign the certificate request?\n"
"Select \"Yes\" to sign the request, \"No\" to continue without signing.",
				"Warning", wxYES_NO, this);
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
