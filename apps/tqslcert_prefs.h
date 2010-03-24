/***************************************************************************
                          tqslcert_prefs.h  -  description
                             -------------------
    begin                : Mon Jul 1 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __TQSLCERT_PREFS_H
#define __TQSLCERT_PREFS_H

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

#include "wx/dialog.h"
#include "wx/checkbox.h"
#include "tqslcertctrls.h"

#define DEFAULT_CABRILLO_FILES "log cbr"
#define DEFAULT_ADIF_FILES "adi"

class KeyPrefs : public wxPanel {
public:
	KeyPrefs(wxWindow *parent);
	virtual bool TransferDataFromWindow();
private:
	wxCheckBox *root_cb, *ca_cb, *user_cb;
};

class CertPrefs : public wxPanel {
public:
	CertPrefs(wxWindow *parent);
	virtual bool TransferDataFromWindow();
private:
	wxCheckBox *showSuperceded_cb;
	wxCheckBox *showExpired_cb;
};

class Preferences : public wxDialog {
public:
	Preferences(wxWindow *parent);
	void OnOK(wxCommandEvent &);
	void OnCancel(wxCommandEvent &) { Close(TRUE); }
	DECLARE_EVENT_TABLE()
private:
	KeyPrefs *keyprefs;
	CertPrefs *certprefs;
};

#endif	// __TQSLCERT_PREFS_H
