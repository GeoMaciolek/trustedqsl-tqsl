/***************************************************************************
                          extwizard.cpp  -  description
                             -------------------
    begin                : Thu Aug 7 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "extwizard.h"
#include "tqsltrace.h"

#define EW_HELP_BUT wxID_HIGHEST+300

BEGIN_EVENT_TABLE(ExtWizard, wxWizard)
	EVT_WIZARD_PAGE_CHANGED(-1, ExtWizard::OnPageChanged)
END_EVENT_TABLE()

void
ExtWizard::OnPageChanged(wxWizardEvent& ev) {
	tqslTrace("ExtWizard::OnPageChanged", "Direction=%d", ev.GetDirection());
	GetCurrentPage()->refresh();
	GetCurrentPage()->SetFocus();
	GetCurrentPage()->validate();
}

void
ExtWizard::ReportSize(const wxSize& size) {
	tqslTrace("ExtWizard::ReportSize", "size=%d %d", size.GetWidth(), size.GetHeight());
	if (size.GetWidth() > _minsize.GetWidth())
		_minsize.SetWidth(size.GetWidth());
	if (size.GetHeight() > _minsize.GetHeight())
		_minsize.SetHeight(size.GetHeight());
}

ExtWizard::ExtWizard(wxWindow *parent, wxHtmlHelpController *help, const wxString& title)
	: wxWizard(parent, -1, title), _help(help) {
	tqslTrace("ExtWizard::ExtWizard", "parent=%lx, title=%s", reinterpret_cast<void *>(parent), S(title));

	CenterOnParent();
}

BEGIN_EVENT_TABLE(ExtWizard_Page, wxWizardPageSimple)
	EVT_BUTTON(EW_HELP_BUT, ExtWizard_Page::OnHelp)
END_EVENT_TABLE()

void
ExtWizard_Page::check_valid(TQ_WXTEXTEVENT&) {
	tqslTrace("ExtWizard_Page::check_valid", NULL);
	validate();
}

void
ExtWizard_Page::AdjustPage(wxBoxSizer *sizer, const wxString& helpfile) {
	tqslTrace("ExtWizard_Page::AdjustPage", NULL);
	_helpfile = helpfile;

	if (_helpfile != wxT("") && _parent->HaveHelp()) {
		// Space filler
		sizer->Add(new wxStaticText(this, -1, wxT("")), 1, 0, 10);
		sizer->Add(new wxButton(this, EW_HELP_BUT, _("Help")), 0, wxALL, 10);
	}

	SetAutoLayout(TRUE);
	SetSizer(sizer);
	sizer->Fit(this);
	sizer->SetSizeHints(this);
	_parent->ReportSize(sizer->CalcMin());
}

