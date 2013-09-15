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
#include "wx/notebook.h"
#include "wx/checkbox.h"
#include "wx/grid.h"
#include "wx/wxhtml.h"
#include "wx/filepicker.h"

#include "tqslctrls.h"

#include <map>

#define DEFAULT_CABRILLO_FILES wxT("log cbr")
#define DEFAULT_ADIF_FILES wxT("adi")
#define DEFAULT_AUTO_BACKUP true
#define DEFAULT_CERT_WARNING 60
//online
//#define ENABLE_ONLINE_PREFS
#define DEFAULT_UPL_URL wxT("https://lotw.arrl.org/lotw/upload")
#define DEFAULT_UPL_FIELD wxT("upfile")
#define DEFAULT_UPL_STATUSRE wxT("<!-- .UPL.\\s*([^-]+)\\s*-->")
#define DEFAULT_UPL_STATUSOK wxT("accepted")
#define DEFAULT_UPL_MESSAGERE wxT("<!-- .UPLMESSAGE.\\s*(.+)\\s*-->")
#define DEFAULT_UPL_VERIFYCA true

#define DEFAULT_UPD_URL wxT("https://lotw.arrl.org/lotw/tqslupdate")
#define DEFAULT_UPD_CONFIG_URL wxT("https://lotw.arrl.org/lotw/config_xml_version")
#define DEFAULT_CONFIG_FILE_URL wxT("https://lotw.arrl.org/lotw/config.tq6")

#define DEFAULT_LOTW_LOGIN_URL wxT("https://lotw.arrl.org/lotwuser/default")

enum {		// Window IDs
	ID_OK_BUT,
	ID_CAN_BUT,
	ID_HELP_BUT,
	ID_PREF_FILE_CABRILLO = (wxID_HIGHEST+1),
	ID_PREF_FILE_ADIF,
	ID_PREF_FILE_AUTO_BACKUP,
	ID_PREF_FILE_BACKUP,
	ID_PREF_FILE_BADCALLS,
	ID_PREF_FILE_DATERANGE,
	ID_PREF_MODE_MAP,
	ID_PREF_MODE_ADIF,
	ID_PREF_MODE_DELETE,
	ID_PREF_MODE_ADD,
	ID_PREF_ADD_ADIF,
	ID_PREF_ADD_MODES,
	ID_PREF_CAB_DELETE,
	ID_PREF_CAB_ADD,
	ID_PREF_CAB_EDIT,
	ID_PREF_ONLINE_DEFAULT,
	ID_PREF_ONLINE_URL,
	ID_PREF_ONLINE_FIELD,
	ID_PREF_ONLINE_STATUSRE,
	ID_PREF_ONLINE_STATUSOK,
	ID_PREF_ONLINE_MESSAGERE,
	ID_PREF_ONLINE_VERIFYCA,
	ID_PREF_ONLINE_UPD_CONFIGURL, 
        ID_PREF_ONLINE_UPD_CONFIGFILE,
	ID_PREF_PROXY_ENABLED,
	ID_PREF_PROXY_HOST,
	ID_PREF_PROXY_PORT,
	ID_PREF_PROXY_TYPE
};

class PrefsPanel : public wxPanel {
public:
	PrefsPanel(wxWindow *parent, const wxString& helpfile = wxT("prefs.htm")) :
		wxPanel(parent), _helpfile(helpfile) {}
	wxString HelpFile() { return _helpfile; }
private:
	wxString _helpfile;
};
	
class FilePrefs : public PrefsPanel {
public:
	FilePrefs(wxWindow *parent);
	virtual bool TransferDataFromWindow();
	void OnShowHide(wxCommandEvent&) { ShowHide(); }
	void ShowHide();
private:
	wxTextCtrl *cabrillo, *adif;
	wxCheckBox *autobackup, *badcalls, *daterange;
	wxDirPickerCtrl *dirPick;
	DECLARE_EVENT_TABLE()
};

#if defined(ENABLE_ONLINE_PREFS)
class OnlinePrefs : public PrefsPanel {
public:
	OnlinePrefs(wxWindow *parent);
	virtual bool TransferDataFromWindow();
	void ShowHide();
	void OnShowHide(wxCommandEvent&) { ShowHide(); }
	DECLARE_EVENT_TABLE()
private:
	wxTextCtrl *uploadURL, *postField, *statusRegex, *statusSuccess, *messageRegex;
	wxTextCtrl *updConfigURL, *configFileURL;
	wxCheckBox *verifyCA, *useDefaults;
	bool defaults;
};
#endif

typedef std::map <wxString, wxString> ModeSet;

class ModeMap : public PrefsPanel {
public:
	ModeMap(wxWindow *parent);
	virtual bool TransferDataFromWindow();
private:
	void SetModeList();
	void OnDelete(wxCommandEvent &);
	void OnAdd(wxCommandEvent &);
	wxButton *delete_but;
	wxListBox *map;
	ModeSet modemap;
	DECLARE_EVENT_TABLE()
};

typedef std::map <wxString, std::pair <int, int> > ContestSet;

class ContestMap : public PrefsPanel {
public:
	ContestMap(wxWindow *parent);
	virtual bool TransferDataFromWindow();
private:
	void SetContestList();
	void OnDelete(wxCommandEvent &);
	void OnAdd(wxCommandEvent &);
	void OnEdit(wxCommandEvent &);
	void Buttons();

	wxButton *delete_but, *edit_but;
	wxGrid *grid;
	ContestSet contestmap;
	DECLARE_EVENT_TABLE()
};

class ProxyPrefs : public PrefsPanel {
public:
	ProxyPrefs(wxWindow *parent);
	virtual bool TransferDataFromWindow();
	void ShowHide();
	void OnShowHide(wxCommandEvent&) { ShowHide(); }
	DECLARE_EVENT_TABLE()
private:
	wxCheckBox *proxyEnabled;
	wxTextCtrl *proxyHost, *proxyPort;
	wxChoice *proxyType;
	bool enabled;
};

typedef std::map <wxString, wxString> ModeSet;
class Preferences : public wxFrame {
public:
	Preferences(wxWindow *parent, wxHtmlHelpController *help = 0);
	void OnOK(wxCommandEvent &);
	void OnCancel(wxCommandEvent &);
	void OnHelp(wxCommandEvent &);
	void OnClose(wxCloseEvent&);
	DECLARE_EVENT_TABLE()
private:
	wxNotebook *notebook;
	FilePrefs *fileprefs;
	ModeMap *modemap;
	ContestMap *contestmap;
	ProxyPrefs *proxyPrefs;
#if defined(ENABLE_ONLINE_PREFS)
	OnlinePrefs *onlinePrefs;
#endif
	wxHtmlHelpController *_help;
};

class AddMode : public wxDialog {
public:
	AddMode(wxWindow *parent);
	virtual bool TransferDataFromWindow();
	void OnOK(wxCommandEvent &);
	void OnCancel(wxCommandEvent &) { Close(true); }
	wxString key, value;
	DECLARE_EVENT_TABLE()
private:
	wxTextCtrl *adif;
	wxListBox *modelist;
};

class EditContest : public wxDialog {
public:
	EditContest(wxWindow *parent, wxString ctype = wxT("Edit"), wxString _contest = wxT(""),
		int _contest_type = 0, int _callsign_field = 5);
	void OnOK(wxCommandEvent&);
	void OnCancel(wxCommandEvent &) { Close(true); }
	virtual bool TransferDataFromWindow();
	wxString contest;
	int contest_type, callsign_field;
private:
	wxTextCtrl *name;
	wxRadioBox *type;
	wxTextCtrl *fieldnum;
	DECLARE_EVENT_TABLE()
};
	
#endif	// __TQSL_PREFS_H
