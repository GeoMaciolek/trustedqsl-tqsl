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

static char *error_title = "Certificate Request Error";

static void set_font(wxWindow *w, wxFont& font) {
#ifndef __WIN32__
	w->SetFont(font);
#endif
}

// Page constructors

BEGIN_EVENT_TABLE(CRQ_ProviderPage, wxWizardPageSimple)
	EVT_COMBOBOX(ID_CRQ_PROVIDER, CRQ_ProviderPage::UpdateInfo)
END_EVENT_TABLE()

static bool
prov_cmp(const TQSL_PROVIDER& p1, const TQSL_PROVIDER& p2) {
	return strcasecmp(p1.organizationName, p2.organizationName) < 0;
}

CRQ_ProviderPage::CRQ_ProviderPage(wxWizard *parent, TQSL_CERT_REQ *crq) :  wxWizardPageSimple(parent) {
	wxFont font = GetFont();
	font.SetPointSize(TEXT_POINTS);
	set_font(this, font);
	wxStaticText *st = new wxStaticText(this, -1, "\nThis will create a new certificate request file.\n\n"
		"Once you supply the requested information and the\n"
		"request file has been created, you must send the\n"
		"request file to the certificate issuer."); //, wxPoint(0, 0), wxDefaultSize); //, wxST_NO_AUTORESIZE);
	set_font(st, font);
	wxSize size = st->GetBestSize();
	int y = TEXT_HEIGHT * GEOM1;
	st = new wxStaticText(this, -1, "Certificate Issuer:", wxPoint(0, y),
		wxSize(size.GetWidth(), TEXT_HEIGHT));
	y += LABEL_HEIGHT + VSEP;
	tc_provider = new wxComboBox(this, ID_CRQ_PROVIDER, "", wxPoint(0, y),
		wxSize(TEXT_WIDTH*28, TEXT_HEIGHT), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
	tc_provider->SetFont(font);
	y += TEXT_HEIGHT*2 + VSEP;
	tc_provider_info = new wxStaticText(this, ID_CRQ_PROVIDER_INFO, "", wxPoint(0, y),
		wxSize(size.GetWidth(), TEXT_HEIGHT*5));
	y += tc_provider_info->GetSize().GetHeight();
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
	SetSize(size);
	DoUpdateInfo();
	Show(FALSE);
}

void
CRQ_ProviderPage::DoUpdateInfo() {
	int sel = tc_provider->GetSelection();
	if (sel >= 0) {
		int idx = (int)(tc_provider->GetClientData(sel));
		if (idx >=0 && idx < (int)providers.size()) {
			provider = providers[idx];
			wxString info;
			info = provider.organizationName;
			if (provider.organizationalUnitName[0] != 0)
				info += wxString("\n  ") + provider.organizationalUnitName;
			if (provider.emailAddress[0] != 0)
				info += wxString("\nEmail: ") + provider.emailAddress;
			if (provider.url[0] != 0)
				info += wxString("\nURL: ") + provider.url;
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

CRQ_IntroPage::CRQ_IntroPage(wxWizard *parent, TQSL_CERT_REQ *crq) :  wxWizardPageSimple(parent) {
	wxFont font = GetFont();
	font.SetPointSize(TEXT_POINTS);
	set_font(this, font);
	int y = TEXT_HEIGHT;
	wxStaticText *st = new wxStaticText(this, -1, "Call sign:", wxPoint(0, y),
		wxSize(TEXT_WIDTH*10, TEXT_HEIGHT));
	set_font(st, font);
	tc_call = new wxTextCtrl(this, ID_CRQ_CALL, "", wxPoint(st->GetSize().GetWidth() + TEXT_WIDTH, y),
		wxSize(TEXT_WIDTH*15, TEXT_HEIGHT));
	tc_call->SetFont(font);
	if (crq && crq->callSign) {
		tc_call->SetValue(crq->callSign);
		tc_call->Enable(false);
	}
	y += TEXT_HEIGHT + VSEP;
	st = new wxStaticText(this, -1, "DXCC entity:", wxPoint(0, y),
		wxSize(TEXT_WIDTH*10, TEXT_HEIGHT));
	set_font(st, font);
	tc_dxcc = new wxComboBox(this, ID_CRQ_DXCC, "", wxPoint(st->GetSize().GetWidth() + TEXT_WIDTH, y),
		wxSize(TEXT_WIDTH*28, TEXT_HEIGHT), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
	tc_dxcc->SetFont(font);
	y += LABEL_HEIGHT + VSEP;

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
	int x;
#ifdef __WIN32__
	wxSize label_size = wxDefaultSize;
#else
	wxSize label_size(TEXT_WIDTH*10, TEXT_HEIGHT);
#endif

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
	 	y += TEXT_HEIGHT + VSEP;
		st = new wxStaticText(this, -1, labels[i], wxPoint(0, y), label_size);
		set_font(st, font);
		y += LABEL_HEIGHT;
		x = TEXT_WIDTH*2;
		st = new wxStaticText(this, -1, "Y", wxPoint(x, y), label_size);
		set_font(st, font);
		x += st->GetSize().GetWidth() + TEXT_WIDTH;
		*(boxes[i][0].cb) = new wxComboBox(this, boxes[i][0].id, "", wxPoint(x, y),
			wxSize(TEXT_WIDTH*10, TEXT_HEIGHT), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		(*(boxes[i][0].cb))->SetFont(font);
		x += (*(boxes[i][0].cb))->GetSize().GetWidth() + TEXT_WIDTH;
		st = new wxStaticText(this, -1, "M", wxPoint(x, y), label_size);
		set_font(st, font);
		x += st->GetSize().GetWidth() + TEXT_WIDTH;
		*(boxes[i][1].cb) = new wxComboBox(this, boxes[i][1].id, "", wxPoint(x, y),
			wxSize(TEXT_WIDTH*6, TEXT_HEIGHT), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		(*(boxes[i][1].cb))->SetFont(font);
		x += (*(boxes[i][1].cb))->GetSize().GetWidth() + TEXT_WIDTH;
		st = new wxStaticText(this, -1, "D", wxPoint(x, y), label_size);
		set_font(st, font);
		x += st->GetSize().GetWidth() + TEXT_WIDTH;
		*(boxes[i][2].cb) = new wxComboBox(this, boxes[i][2].id, "", wxPoint(x, y),
			wxSize(TEXT_WIDTH*6, TEXT_HEIGHT), 0, 0, wxCB_DROPDOWN|wxCB_READONLY);
		(*(boxes[i][2].cb))->SetFont(font);
		x += (*(boxes[i][2].cb))->GetSize().GetWidth();
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
		y += VSEP;
	}
	SetSize(wxSize(x, y));
	if (crq) {
//		tc_qsobeginy->SetValue(wxString::Format("%d", crq->qsoNotBefore.year).c_str());
//		tc_qsobeginm->SetValue(wxString::Format("%d", crq->qsoNotBefore.month).c_str());
//		tc_qsobegind->SetValue(wxString::Format("%d", crq->qsoNotBefore.day).c_str());
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
//			tc_qsoendy->SetValue(wxString::Format("%d", crq->qsoNotAfter.year).c_str());
//			tc_qsoendm->SetValue(wxString::Format("%d", crq->qsoNotAfter.month).c_str());
//			tc_qsoendd->SetValue(wxString::Format("%d", crq->qsoNotAfter.day).c_str());
			tc_qsoendy->SetSelection(sels[1][0]);
			tc_qsoendm->SetSelection(sels[1][1]);
			tc_qsoendd->SetSelection(sels[1][2]);
		}
	} else {
		tc_qsobeginy->SetSelection(0);
		tc_qsobeginm->SetSelection(10);
		tc_qsobegind->SetSelection(0);
	}
	Show(FALSE);
}

CRQ_NamePage::CRQ_NamePage(wxWizard *parent, TQSL_CERT_REQ *crq) :  wxWizardPageSimple(parent) {
	wxFont font = GetFont();
	font.SetPointSize(TEXT_POINTS);
	SetFont(font);
	wxStaticText *st = new wxStaticText(this, -1, "Name", wxPoint(0, TEXT_HEIGHT+VSEP), wxSize(TEXT_WIDTH*10, TEXT_HEIGHT), wxALIGN_RIGHT|wxST_NO_AUTORESIZE);
	st->SetFont(font);
	tc_name = new wxTextCtrl(this, ID_CRQ_NAME, "", wxPoint(TEXT_WIDTH*11, TEXT_HEIGHT+VSEP), wxSize(TEXT_WIDTH*30, TEXT_HEIGHT));
	tc_name->SetFont(font);
	wxConfig *config = (wxConfig *)wxConfig::Get();
	wxString val;
	if (crq && crq->name)
		tc_name->SetValue(crq->name);
	else if (config->Read("Name", &val))
		tc_name->SetValue(val);
	st = new wxStaticText(this, -1, "Address", wxPoint(0, (TEXT_HEIGHT+VSEP)*2), wxSize(TEXT_WIDTH*10, TEXT_HEIGHT), wxALIGN_RIGHT|wxST_NO_AUTORESIZE);
	st->SetFont(font);
	tc_addr1 = new wxTextCtrl(this, ID_CRQ_ADDR1, "", wxPoint(TEXT_WIDTH*11, (TEXT_HEIGHT+VSEP)*2), wxSize(TEXT_WIDTH*30, TEXT_HEIGHT));
	tc_addr1->SetFont(font);
	if (crq && crq->address1)
		tc_addr1->SetValue(crq->address1);
	else if (config->Read("Addr1", &val))
		tc_addr1->SetValue(val);
	tc_addr2 = new wxTextCtrl(this, ID_CRQ_ADDR2, "", wxPoint(TEXT_WIDTH*11, (TEXT_HEIGHT+VSEP)*3), wxSize(TEXT_WIDTH*30, TEXT_HEIGHT));
	tc_addr2->SetFont(font);
	if (crq && crq->address2)
		tc_addr2->SetValue(crq->address2);
	else if (config->Read("Addr2", &val))
		tc_addr2->SetValue(val);
	st = new wxStaticText(this, -1, "City", wxPoint(0, (TEXT_HEIGHT+VSEP)*4), wxSize(TEXT_WIDTH*10, TEXT_HEIGHT), wxALIGN_RIGHT|wxST_NO_AUTORESIZE);
	st->SetFont(font);
	tc_city = new wxTextCtrl(this, ID_CRQ_CITY, "", wxPoint(TEXT_WIDTH*11, (TEXT_HEIGHT+VSEP)*4), wxSize(TEXT_WIDTH*15, TEXT_HEIGHT));
	tc_city->SetFont(font);
	if (crq && crq->city)
		tc_city->SetValue(crq->city);
	else if (config->Read("City", &val))
		tc_city->SetValue(val);
	st = new wxStaticText(this, -1, "State", wxPoint(TEXT_WIDTH*27, (TEXT_HEIGHT+VSEP)*4), wxSize(TEXT_WIDTH*6, TEXT_HEIGHT));
	st->SetFont(font);
	tc_state = new wxTextCtrl(this, ID_CRQ_STATE, "", wxPoint(TEXT_WIDTH*35, (TEXT_HEIGHT+VSEP)*4), wxSize(TEXT_WIDTH*6, TEXT_HEIGHT));
	tc_state->SetFont(font);
	if (crq && crq->state)
		tc_state->SetValue(crq->state);
	else if (config->Read("State", &val))
		tc_state->SetValue(val);
	st = new wxStaticText(this, -1, "Zip/Postal", wxPoint(0, (TEXT_HEIGHT+VSEP)*5), wxSize(TEXT_WIDTH*10, TEXT_HEIGHT), wxALIGN_RIGHT|wxST_NO_AUTORESIZE);
	st->SetFont(font);
	tc_zip = new wxTextCtrl(this, ID_CRQ_ZIP, "", wxPoint(TEXT_WIDTH*11, (TEXT_HEIGHT+VSEP)*5), wxSize(TEXT_WIDTH*30, TEXT_HEIGHT));
	tc_zip->SetFont(font);
	if (crq && crq->postalCode)
		tc_zip->SetValue(crq->postalCode);
	else if (config->Read("ZIP", &val))
		tc_zip->SetValue(val);
	st = new wxStaticText(this, -1, "Country", wxPoint(0, (TEXT_HEIGHT+VSEP)*6), wxSize(TEXT_WIDTH*10, TEXT_HEIGHT), wxALIGN_RIGHT|wxST_NO_AUTORESIZE);
	st->SetFont(font);
	tc_country = new wxTextCtrl(this, ID_CRQ_COUNTRY, "", wxPoint(TEXT_WIDTH*11, (TEXT_HEIGHT+VSEP)*6), wxSize(TEXT_WIDTH*30, TEXT_HEIGHT));
	tc_country->SetFont(font);
	if (crq && crq->country)
		tc_country->SetValue(crq->country);
	else if (config->Read("Country", &val))
		tc_country->SetValue(val);
	SetSize(TEXT_WIDTH*42, (TEXT_HEIGHT+VSEP)*7);
	Show(FALSE);
}

CRQ_EmailPage::CRQ_EmailPage(wxWizard *parent, TQSL_CERT_REQ *crq) :  wxWizardPageSimple(parent) {
	wxFont font = GetFont();
	font.SetPointSize(TEXT_POINTS);
	SetFont(font);
	wxStaticText *st = new wxStaticText(this, -1, "Your e-mail address", wxPoint(0, TEXT_HEIGHT));
	st->SetFont(font);
	tc_email = new wxTextCtrl(this, ID_CRQ_NAME, "", wxPoint(0, (TEXT_HEIGHT)*2), wxSize(TEXT_WIDTH*40, TEXT_HEIGHT));
	tc_email->SetFont(font);
	wxConfig *config = (wxConfig *)wxConfig::Get();
	wxString val;
	if (crq && crq->emailAddress)
		tc_email->SetValue(crq->emailAddress);
	else if (config->Read("Email", &val))
		tc_email->SetValue(val);
	st = new wxStaticText(this, -1, "Note: The e-mail address you provide here is the\n"
		"address to which the issued certificate will be sent.\n"
		"Make sure it's the correct address!\n",
		wxPoint(0, TEXT_HEIGHT*3 + VSEP));
	st->SetFont(font);
	Show(FALSE);
}

CRQ_PasswordPage::CRQ_PasswordPage(wxWizard *parent) :  wxWizardPageSimple(parent) {
	wxFont font = GetFont();
	font.SetPointSize(TEXT_POINTS);
	SetFont(font);
	wxStaticText *st = new wxStaticText(this, -1,
		"You may protect your private key for this certificate", wxPoint(0, TEXT_HEIGHT));
	st->SetFont(font);
	st = new wxStaticText(this, -1,
		"using a password. Doing so is recommended.", wxPoint(0, TEXT_HEIGHT*2));
	st->SetFont(font);
	st = new wxStaticText(this, -1, "Password:", wxPoint(0, TEXT_HEIGHT*4));
	st->SetFont(font);
	tc_pw1 = new wxTextCtrl(this, ID_CRQ_PW1, "", wxPoint(0, (TEXT_HEIGHT)*5), wxSize(TEXT_WIDTH*40, TEXT_HEIGHT), wxTE_PASSWORD);
	tc_pw1->SetFont(font);
	st = new wxStaticText(this, -1, "Enter the password again for verification:", wxPoint(0, TEXT_HEIGHT*6));
	st->SetFont(font);
	tc_pw2 = new wxTextCtrl(this, ID_CRQ_PW2, "", wxPoint(0, (TEXT_HEIGHT)*7), wxSize(TEXT_WIDTH*40, TEXT_HEIGHT), wxTE_PASSWORD);
	tc_pw2->SetFont(font);
	st = new wxStaticText(this, -1, "DO NOT lose the password you choose!\n"
		"You will be unable to use the certificate\nwithout this password!",
		wxPoint(0, TEXT_HEIGHT*9));
	Show(FALSE);
}

BEGIN_EVENT_TABLE(CRQ_SignPage, wxWizardPageSimple)
	EVT_TREE_SEL_CHANGED(ID_CRQ_CERT, CRQ_SignPage::CertSelChanged)
END_EVENT_TABLE()


void CRQ_SignPage::CertSelChanged(wxTreeEvent& event) {
	if (cert_tree->GetItemData(event.GetItem()))
		choice->SetSelection(1);
}

CRQ_SignPage::CRQ_SignPage(wxWizard *parent, wxSize& size) :  wxWizardPageSimple(parent) {
	wxFont font = GetFont();
	font.SetPointSize(TEXT_POINTS);
	SetFont(font);
	int y = 10;
//	wxStaticText *st = new wxStaticText(this, -1, "Sign Certificate Request", wxPoint(0, y));
//	st->SetFont(font);
//	y += TEXT_HEIGHT*2;
	wxString choices[] = { "Unsigned", "Signed" };

	choice = new wxRadioBox(this, ID_CRQ_SIGN, "Sign Request", wxPoint(0, y),
		wxSize(size.GetWidth(), -1), 2, choices, 1, wxRA_SPECIFY_COLS);
	y += choice->wxWindow::GetSize().GetHeight() + TEXT_HEIGHT;
	cert_tree = new CertTree(this, ID_CRQ_CERT, wxPoint(0, y),
		wxSize(size.GetWidth(), TEXT_HEIGHT*10), wxTR_HAS_BUTTONS | wxSUNKEN_BORDER);
	cert_tree->SetBackgroundColour(wxColour(255, 255, 255));
	cert_tree->Build(0);
}	

// Page validation

bool
CRQ_ProviderPage::TransferDataFromWindow() {
	return true;
}


bool
CRQ_IntroPage::TransferDataFromWindow() {
	wxString val = tc_call->GetValue();
	bool ok = true;
	const char *errmsg = "You must enter a valid call sign.";

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
	dxcc = (int)(tc_dxcc->GetClientData(tc_dxcc->GetSelection()));
	if (dxcc < 1) {
		errmsg = "You must select a DXCC entity.";
		ok = false;
	}
	if (ok) {
		qsonotbefore.year = atoi(tc_qsobeginy->GetStringSelection());
		qsonotbefore.month = atoi(tc_qsobeginm->GetStringSelection());
		qsonotbefore.day = atoi(tc_qsobegind->GetStringSelection());
		qsonotafter.year = atoi(tc_qsoendy->GetStringSelection());
		qsonotafter.month = atoi(tc_qsoendm->GetStringSelection());
		qsonotafter.day = atoi(tc_qsoendd->GetStringSelection());
		if (!tqsl_isDateValid(&qsonotbefore)) {
			errmsg = "QSO begin date: You must choose proper values for Year, Month and Day.";
			ok = false;
		} else if (!tqsl_isDateNull(&qsonotafter) && !tqsl_isDateValid(&qsonotafter)) {
			errmsg = "QSO end date: You must either choose proper values for Year, Month and Day or leave all three blank.";
			ok = false;
		} else if (tqsl_isDateValid(&qsonotafter)
			&& tqsl_compareDates(&qsonotbefore, &qsonotafter) > 0) {
			errmsg = "QSO end date cannot be before QSO begin date.";
			ok = false;
		}
	}
	if (!ok)
		wxMessageBox(errmsg, error_title);
	else {
		callsign = val;
		callsign.MakeUpper();
		tc_call->SetValue(callsign);
	}
	return ok;
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

bool
CRQ_NamePage::TransferDataFromWindow() {
	name = tc_name->GetValue();
	addr1 = tc_addr1->GetValue();
	addr2 = tc_addr2->GetValue();
	city = tc_city->GetValue();
	state = tc_state->GetValue();
	zip = tc_zip->GetValue();
	country = tc_country->GetValue();

	const char *errmsg = 0;

	if (cleanString(name))
		errmsg = "You must enter your name";
	if (!errmsg && cleanString(addr1))
		errmsg = "You must enter your address";
	cleanString(addr2);
	if (!errmsg && cleanString(city))
		errmsg = "You must enter your city";
	cleanString(state);
	cleanString(zip);
	cleanString(country);
	if (errmsg) {
		wxMessageBox(errmsg, error_title);
		return false;
	}
	tc_name->SetValue(name);
	tc_addr1->SetValue(addr1);
	tc_addr2->SetValue(addr2);
	tc_city->SetValue(city);
	tc_state->SetValue(state);
	tc_zip->SetValue(zip);
	tc_country->SetValue(country);
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->Write("Name", name);
	config->Write("Addr1", addr1);
	config->Write("Addr2", addr2);
	config->Write("City", city);
	config->Write("State", state);
	config->Write("ZIP", zip);
	config->Write("Country", country);
	return true;
}


bool
CRQ_EmailPage::TransferDataFromWindow() {
	email = tc_email->GetValue();
	cleanString(email);
	int i = email.First('@');
	int j = email.Last('.');
	if (i < 0 || j < i) {
		wxMessageBox("You must enter a valid email address", error_title);
		return false;
	}
	wxConfig *config = (wxConfig *)wxConfig::Get();
	config->Write("Email", email);
	return true;
}

bool
CRQ_PasswordPage::TransferDataFromWindow() {
	wxString pw1 = tc_pw1->GetValue();
	wxString pw2 = tc_pw2->GetValue();
	
	if (pw1 != pw2) {
		wxMessageBox("The two copies of the password you entered do not match.", error_title);
		return false;
	}
	password = pw1;
	return true;
}

bool
CRQ_SignPage::TransferDataFromWindow() {
	if (choice->GetSelection() == 0)
		cert = 0;
	else {
		CertTreeItemData *data = (CertTreeItemData *)cert_tree->GetItemData(cert_tree->GetSelection());
		if (!data) {
			wxMessageBox("To sign the request you must select a certificate from the list");
			return false;
		}
		cert = data->getCert();
	}
	return true;
}
