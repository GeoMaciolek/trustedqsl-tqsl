/***************************************************************************
                          tqslcert.h  -  description
                             -------------------
    begin                : Thu Jun 13 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __tqslcert_h
#define __tqslcert_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "wx/wxprec.h"
#include "wx/object.h"
#include "wx/config.h"

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include "wx/wxhtml.h"

#include "certtree.h"
#include "tqslcertctrls.h"

#ifndef ADIF_BOOLEAN
	#define ADIF_BOOLEAN // Hack!
#endif
#include "tqsllib.h"

class MyFrame;

class CertApp : public wxApp {
public:
	CertApp();
	virtual ~CertApp();
	bool OnInit();
};

class MyFrame : public wxFrame {
public:
	MyFrame(const wxString& title, int x, int y, int w, int h);
	virtual ~MyFrame();
	void OnQuit(wxCommandEvent& event);
	void CRQWizard(wxCommandEvent& event);
	void CRQWizardRenew(wxCommandEvent& event);
	void OnTreeSel(wxTreeEvent& event);
	void OnCertProperties(wxCommandEvent& event);
	void OnCertExport(wxCommandEvent& event);
	void OnCertDelete(wxCommandEvent& event);
//	void OnCertImport(wxCommandEvent& event);
	void OnSign(wxCommandEvent& event);
	void OnLoadCertificateFile(wxCommandEvent& event);
	void OnPreferences(wxCommandEvent& event);
	void OnHelpContents(wxCommandEvent& event);
	void OnHelpAbout(wxCommandEvent& event);
	void DisplayHelp(const char *file = "main.htm") { help.Display(wxString(file, wxConvLocal)); }

	CertTree *cert_tree;
private:
	wxMenu *cert_menu;
	int renew;
	TQSL_CERT_REQ *req;
	wxHtmlHelpController help;

	DECLARE_EVENT_TABLE()
};

#endif // __tqslcert_h
