/***************************************************************************
                          tqsl_prefs.h  -  description
                             -------------------
    begin                : Sun Jan 05 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __TQSL_PREFS_H
#define __TQSL_PREFS_H

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

#include <map>

#define DEFAULT_CABRILLO_FILES "log cbr"
#define DEFAULT_ADIF_FILES "adi"

enum {		// Window IDs
	ID_OK_BUT,
	ID_CAN_BUT,
	ID_PREF_FILE_CABRILLO = (wxID_HIGHEST+1),
	ID_PREF_FILE_ADIF,
	ID_PREF_MODE_MAP,
	ID_PREF_MODE_ADIF,
	ID_PREF_MODE_DELETE,
	ID_PREF_MODE_ADD,
	ID_PREF_ADD_ADIF,
	ID_PREF_ADD_MODES,
};
	
class FilePrefs : public wxPanel {
public:
	FilePrefs(wxWindow *parent);
	virtual bool TransferDataFromWindow();
private:
	wxTextCtrl *cabrillo, *adif;
};

typedef std::map <wxString, wxString> ModeSet;

class ModeMap : public wxPanel {
public:
	ModeMap(wxWindow *parent);
	virtual bool TransferDataFromWindow();
private:
	void SetModeList();
	void OnDelete(wxCommandEvent &);
	void OnAdd(wxCommandEvent &);
	wxListBox *map;
	ModeSet modemap;
	DECLARE_EVENT_TABLE()
};

class Preferences : public wxDialog {
public:
	Preferences(wxWindow *parent);
	void OnOK(wxCommandEvent &);
	void OnCancel(wxCommandEvent &) { Close(TRUE); }
	DECLARE_EVENT_TABLE()
private:
	FilePrefs *fileprefs;
	ModeMap *modemap;
};

class AddMode : public wxDialog {
public:
	AddMode(wxWindow *parent);
	void OnOK(wxCommandEvent &);
	void OnCancel(wxCommandEvent &) { Close(TRUE); }
	wxString key, value;
	DECLARE_EVENT_TABLE()
private:
	wxTextCtrl *adif;
	wxListBox *modelist;
};

#endif	// __TQSL_PREFS_H
