/***************************************************************************
                                  tqslupdater.cpp
                             -------------------
    begin                : Fri 28 Apr 2017
    copyright            : (C) 2017 by the TrustedQSL Developers
    author               : Rick Murphy
    email                : k1mu@arrl.net
 ***************************************************************************/

#include <curl/curl.h> // has to be before something else in this list
#include <stdlib.h>
#include <errno.h>
#ifndef _WIN32
#include <unistd.h>
#include <fcntl.h>
#endif
#include <expat.h>
#include <sys/stat.h>

#ifndef _WIN32
    #include <unistd.h>
    #include <dirent.h>
#else
    #include <direct.h>
    #include "windirent.h"
#endif
#include "tqsl_prefs.h"

#include <wx/wxprec.h>
#include <wx/object.h>
#include <wx/wxchar.h>
#include <wx/config.h>
#include <wx/regex.h>
#include <wx/tokenzr.h>
#include <wx/hashmap.h>
#include <wx/hyperlink.h>
#include <wx/notebook.h>
#include <wx/statline.h>
#include <wx/app.h>
#include <wx/stdpaths.h>
#include <wx/intl.h>
#include <wx/cshelp.h>

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif

#include <wx/wxhtml.h>
#include <wx/wfstream.h>

#ifdef _MSC_VER //could probably do a more generic check here...
// stdint exists on vs2012 and (I think) 2010, but not 2008 or its platform
  #define uint8_t unsigned char
#else
#include <stdint.h> //for uint8_t; should be cstdint but this is C++11 and not universally supported
#endif

#ifdef _WIN32
	#include <io.h>
#endif

#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include <map>

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "tqslhelp.h"

#include "util.h"
#include "wxutil.h"

#include "tqslpaths.h"
#include "tqslapp.h"

#include "winstrdefs.h"

using std::ifstream;
using std::ios;
using std::ofstream;
using std::cerr;
using std::endl;
using std::vector;
using std::string;

/// GEOMETRY

#define LABEL_HEIGHT 20

static MyFrame *frame = 0;

static bool quiet = false;

/////////// Application //////////////

class QSLApp : public wxApp {
 public:
	QSLApp();
	virtual ~QSLApp();
	class MyFrame *GUIinit(bool checkUpdates, bool quiet = false);
	virtual bool OnInit();
};

QSLApp::~QSLApp() {
	wxConfigBase *c = wxConfigBase::Set(0);
	if (c)
		delete c;
}

IMPLEMENT_APP(QSLApp)

BEGIN_EVENT_TABLE(MyFrame, wxFrame)
	EVT_CLOSE(MyFrame::OnExit)
END_EVENT_TABLE()

void
MyFrame::OnExit(TQ_WXCLOSEEVENT& WXUNUSED(event)) {
	Destroy();		// close the window
}

void
MyFrame::DoExit(wxCommandEvent& WXUNUSED(event)) {
	Close();
	Destroy();
}

static wxSemaphore *updateLocker;

class UpdateThread : public wxThread {
 public:
	UpdateThread(MyFrame *frame, bool silent, bool noGUI) : wxThread(wxTHREAD_DETACHED) {
		_frame = frame;
		_silent = silent;
		_noGUI = noGUI;
	}
	virtual void *Entry() {
		_frame->DoCheckForUpdates(_silent, _noGUI);
		updateLocker->Post();
		return NULL;
	}
 private:
	MyFrame *_frame;
	bool _silent;
	bool _noGUI;
};

void
MyFrame::DoUpdateCheck(bool silent, bool noGUI) {
	//check for updates
	if (!noGUI) {
		wxBeginBusyCursor();
		wxTheApp->Yield(true);
		wxLogMessage(_("Checking for TQSL updates..."));
		wxTheApp->Yield(true);
	}
	updateLocker = new wxSemaphore(0, 1);		// One waiter
	UpdateThread* thread = new UpdateThread(this, silent, noGUI);
	thread->Create();
	wxTheApp->Yield(true);
	thread->Run();
	wxTheApp->Yield(true);
	while (updateLocker->TryWait() == wxSEMA_BUSY) {
		wxTheApp->Yield(true);
#if (wxMAJOR_VERSION == 2 && wxMINOR_VERSION == 9)
		wxGetApp().DoIdle();
#endif
		wxMilliSleep(300);
	}
	delete updateLocker;
	if (!noGUI) {
		wxString val = logwin->GetValue();
		wxString chk(_("Checking for TQSL updates..."));
		val.Replace(chk + wxT("\n"), wxT(""));
		val.Replace(chk, wxT(""));
		logwin->SetValue(val);		// Clear the checking message
		notebook->SetSelection(0);
		wxEndBusyCursor();
	}
}

MyFrame::MyFrame(const wxString& title, int x, int y, int w, int h, bool checkUpdates, bool quiet, wxLocale* loca)
	: wxFrame(0, -1, title, wxPoint(x, y), wxSize(w, h)), locale(loca) {
	_quiet = quiet;

	// File menu
	file_menu = new wxMenu;
	file_menu->Append(tm_f_exit, _("E&xit TQSL\tAlt-X"));

	// Main menu
	wxMenuBar *menu_bar = new wxMenuBar;
	menu_bar->Append(file_menu, _("&File"));

	SetMenuBar(menu_bar);

	wxPanel* topPanel = new wxPanel(this);
	wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
	topPanel->SetSizer(topSizer);

}

static CURL*
tqsl_curl_init(const char *logTitle, const char *url, bool newFile) {
	CURL* curlReq = curl_easy_init();
	if (!curlReq)
		return NULL;
        DocPaths docpaths(wxT("tqslapp"));

	wxString filename;
	//set up options
	curl_easy_setopt(curlReq, CURLOPT_URL, url);

	wxStandardPaths sp;
	wxString exePath;
	wxFileName::SplitPath(sp.GetExecutablePath(), &exePath, 0, 0);
	docpaths.Add(exePath);
	wxString caBundlePath = docpaths.FindAbsoluteValidPath(wxT("ca-bundle.crt"));
	if (!caBundlePath.IsEmpty()) {
		char caBundle[256];
		strncpy(caBundle, caBundlePath.ToUTF8(), sizeof caBundle);
		curl_easy_setopt(curlReq, CURLOPT_CAINFO, caBundle);
	}
	return curlReq;
}

void MyFrame::CheckForUpdates(wxCommandEvent&) {
	DoUpdateCheck(false, false);
}

wxString GetUpdatePlatformString() {
	wxString ret;
#if defined _WIN32
	#if defined(_WIN64)
		//this is 64 bit code already; if we are running we support 64
		ret = wxT("win64 win32");

	#else // this is not 64 bit code, but we are on windows
		// are we 64-bit compatible? if so prefer it
		BOOL val = false;
		typedef BOOL (WINAPI *LPFN_ISWOW64PROCESS) (HANDLE, PBOOL);
		LPFN_ISWOW64PROCESS fnIsWow64Process =
			(LPFN_ISWOW64PROCESS) GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");
		if (fnIsWow64Process != NULL) {
			fnIsWow64Process(GetCurrentProcess(), &val);
		}
		if(val) //32 bit running on 64 bit
			ret = wxT("win64 win32");
		else //just 32-bit only
			ret = wxT("win32");
	#endif

#endif
	return ret;
}

// Class for encapsulating version information
class revLevel {
 public:
	explicit revLevel(long _major = 0, long _minor = 0, long _patch = 0) {
		major = _major;
		minor = _minor;
		patch = _patch;
	}
	explicit revLevel(const wxString& _value) {
		wxString str = _value;
		str.Trim(true);
		str.Trim(false);
		wxStringTokenizer vers(str, wxT("."));
		wxString majorVer = vers.GetNextToken();
		wxString minorVer = vers.GetNextToken();
		wxString patchVer = vers.GetNextToken();
		if (majorVer.IsNumber()) {
			majorVer.ToLong(&major);
		} else {
			major = -1;
		}
		if (minorVer.IsNumber()) {
			minorVer.ToLong(&minor);
		} else {
			minor = -1;
		}
		if (patchVer != wxT("") && patchVer.IsNumber()) {
			patchVer.ToLong(&patch);
		} else {
			patch = 0;
		}
	}
	wxString Value(void) {
		if (patch > 0)
			return wxString::Format(wxT("%ld.%ld.%ld"), major, minor, patch);
		else
			return wxString::Format(wxT("%ld.%ld"), major, minor);
	}
	long major;
	long minor;
	long patch;
	bool operator >(const revLevel& other) {
		if (major > other.major) return true;
		if (major == other.major) {
			if (minor > other.minor) return true;
			if (minor == other.minor) {
				if (patch > other.patch) return true;
			}
		}
		return false;
	}
	bool operator  >=(const revLevel& other) {
		if (major > other.major) return true;
		if (major == other.major) {
			if (minor > other.minor) return true;
			if (minor == other.minor) {
				if (patch >= other.patch) return true;
			}
		}
		return false;
	}
};

class revInfo {
 public:
	explicit revInfo(bool _noGUI = false, bool _silent = false) {
		noGUI = _noGUI;
		silent = _silent;
		error = false;
		message = false;
		mutex = new wxMutex;
		condition = new wxCondition(*mutex);
		mutex->Lock();
	}
	~revInfo() {
		if (condition)
			delete condition;
		if (mutex)
			delete mutex;
	}
	bool error;
	bool message;
	bool noGUI;
	bool silent;
	wxString errorText;
	wxString homepage;
	wxString url;
	wxMutex* mutex;
	wxCondition* condition;
};


static size_t file_recv(void *ptr, size_t size, size_t nmemb, void *stream) {
	size_t left = nmemb * size;
	size_t written;

	while (left > 0) {
  		written = fwrite(ptr, size, nmemb, reinterpret_cast <FILE *>(stream));
		if (written == 0)
			return 0;
		left -= (written * size);
	}
	return nmemb * size;
}


#ifdef _WIN32
void
MyFrame::UpdateTQSL(wxString& url) {
 retry:
	curlReq = tqsl_curl_init("TQSL Update Download Log", (const char *)url.ToUTF8(), false);

	wxString filename;
	filename.Printf(wxT("%hs\\tqslupdate.msi"), wxString::FromUTF8("%TEMP%"));
	wchar_t* lfn = utf8_to_wchar(filename.ToUTF8());
	FILE *updateFile = _wfopen(lfn, L"wb");
	free_wchar(lfn);
	if (!updateFile) {
		wxMessageBox(wxString::Format(_("Can't open TQSL update file %s: %hs"), filename.c_str(), strerror(errno)), _("Error"), wxOK | wxICON_ERROR, this);
		return;
	}

	curl_easy_setopt(curlReq, CURLOPT_WRITEFUNCTION, &file_recv);
	curl_easy_setopt(curlReq, CURLOPT_WRITEDATA, updateFile);

	curl_easy_setopt(curlReq, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	curl_easy_setopt(curlReq, CURLOPT_ERRORBUFFER, errorbuf);
	int retval = curl_easy_perform(curlReq);
	if (retval == CURLE_OK) {
		if (fclose(updateFile)) {
			wxMessageBox(wxString::Format(_("Error writing new configuration file %s: %hs"), filename.c_str(), strerror(errno)), _("Error"), wxOK | wxICON_ERROR, this);
			return;
		}
		wxExecute(wxT("msiexec /i ") + filename, wxEXEC_ASYNC);
		wxExit();
	} else {
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			wxLogMessage(_("Unable to update - either your Internet connection is down or LoTW is unreachable."));
			wxLogMessage(_("Please try again later."));
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			wxLogMessage(_("Unable to update. The nework is down or the LoTW site is too busy."));
			wxLogMessage(_("Please try again later."));
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			wxLogMessage(_("Unable to connect to the update site."));
			wxLogMessage(_("Please try again later."));
		} else { // some other error
			wxString fmt = _("Error downloading new file:");
			fmt += wxT("\n%hs");
			wxMessageBox(wxString::Format(fmt, errorbuf), _("Update"), wxOK | wxICON_EXCLAMATION, this);
		}
	}
}
#endif

class FileUploadHandler {
 public:
	string s;
	FileUploadHandler(): s() { s.reserve(2000); }

	size_t internal_recv(char *ptr, size_t size, size_t nmemb) {
		s.append(ptr, size*nmemb);
		return size*nmemb;
	}

	static size_t recv(char *ptr, size_t size, size_t nmemb, void *userdata) {
		return (reinterpret_cast<FileUploadHandler*>(userdata))->internal_recv(ptr, size, nmemb);
	}
};

void
MyFrame::DoCheckForUpdates(bool silent, bool noGUI) {

	revInfo* ri = new revInfo;
	ri->noGUI = noGUI;
	ri->silent = silent;

	wxString updateURL = DEFAULT_UPD_URL;

	curlReq = tqsl_curl_init("Version Check Log", (const char*)updateURL.ToUTF8(), NULL, true);

	//the following allow us to analyze our file

	FileUploadHandler handler;

	curl_easy_setopt(curlReq, CURLOPT_WRITEFUNCTION, &FileUploadHandler::recv);
	curl_easy_setopt(curlReq, CURLOPT_WRITEDATA, &handler);

	if (silent) { // if there's a problem, we don't want the program to hang while we're starting it
		curl_easy_setopt(curlReq, CURLOPT_CONNECTTIMEOUT, 120);
	}

	curl_easy_setopt(curlReq, CURLOPT_FAILONERROR, 1); //let us find out about a server issue

	char errorbuf[CURL_ERROR_SIZE];
	curl_easy_setopt(curlReq, CURLOPT_ERRORBUFFER, errorbuf);

	int retval = curl_easy_perform(curlReq);
	if (retval == CURLE_OK) {
		// Add the config.xml text to the result
		wxString configURL = DEFAULT_UPD_CONFIG_URL;
		curl_easy_setopt(curlReq, CURLOPT_URL, (const char*)configURL.ToUTF8());

		retval = curl_easy_perform(curlReq);
		if (retval == CURLE_OK) {
			wxString result = wxString::FromAscii(handler.s.c_str());
			wxString url;
			WX_DECLARE_STRING_HASH_MAP(wxString, URLHashMap);
			URLHashMap map;

			wxStringTokenizer urls(result, wxT("\n"));
			wxString onlinever;
			while(urls.HasMoreTokens()) {
				wxString header = urls.GetNextToken().Trim();
				if (header.StartsWith(wxT("TQSLVERSION;"), &onlinever)) {
					continue;	
				} else if (header.IsEmpty()) {
					continue; //blank line
				} else if (header[0] == '#') {
					continue; //comments
				} else if (header.StartsWith(wxT("config.xml"), &onlinever)) {
					continue;
				} else {
					int sep = header.Find(';'); //; is invalid in URLs
					if (sep == wxNOT_FOUND) continue; //malformed string
					wxString plat = header.Left(sep);
					wxString url = header.Right(header.size()-sep-1);
					map[plat] = url;
				}
			}
			// if (ri->newProgram) {
				wxString ourPlatURL; //empty by default (we check against this later)

				wxStringTokenizer plats(GetUpdatePlatformString(), wxT(" "));
				while(plats.HasMoreTokens()) {
					wxString tok = plats.GetNextToken();
					//see if this token is here
					if (map.count(tok)) { ourPlatURL=map[tok]; break; }
				}
				ri->homepage = map[wxT("homepage")];
				ri->url = ourPlatURL;
			//}
		} else {
			if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
				ri->error = true;
				ri->errorText = wxString(_("Unable to check for updates - either your Internet connection is down or LoTW is unreachable."));
				ri->errorText += wxT("\n");
				ri->errorText += _("Please try again later.");
				ri->errorText = wxString::FromUTF8("Unable to check for updates - either your Internet connection is down or LoTW is unreachable.\nPlease try again later.");
			} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
				ri->error = true;
				ri->errorText = wxString(_("Unable to check for updates. The nework is down or the LoTW site is too busy."));
				ri->errorText += wxT("\n");
				ri->errorText += _("Please try again later.");
			} else if (retval == CURLE_SSL_CONNECT_ERROR) {
				ri->error = true;
				ri->errorText = wxString(_("Unable to connect to the update site."));
				ri->errorText += wxT("\n");
				ri->errorText += _("Please try again later.");
			} else { // some other error
				ri->message = true;
				wxString fmt = _("Error downloading new version information:");
				fmt += wxT("\n%hs");
				ri->errorText = wxString::Format(fmt, errorbuf);
			}
		}
	} else {
		if (retval == CURLE_COULDNT_RESOLVE_HOST || retval == CURLE_COULDNT_CONNECT) {
			ri->error = true;
			ri->errorText = wxString(_("Unable to check for updates - either your Internet connection is down or LoTW is unreachable."));
			ri->errorText += wxT("\n");
			ri->errorText += _("Please try again later.");
		} else if (retval == CURLE_WRITE_ERROR || retval == CURLE_SEND_ERROR || retval == CURLE_RECV_ERROR) {
			ri->error = true;
			ri->errorText = wxString(_("Unable to check for updates. The nework is down or the LoTW site is too busy."));
			ri->errorText += wxT("\n");
			ri->errorText += _("Please try again later.");
		} else if (retval == CURLE_SSL_CONNECT_ERROR) {
			ri->error = true;
			ri->errorText = wxString(_("Unable to connect to the update site."));
			ri->errorText += wxT("\n");
			ri->errorText += _("Please try again later.");
		} else { // some other error
			ri->message = true;
			wxString fmt = _("Error downloading update version information:");
			fmt += wxT("\n%hs");
			ri->errorText = wxString::Format(fmt, errorbuf);
		}
	}

	// Send the result back to the main thread
	wxCommandEvent* event = new wxCommandEvent(wxEVT_COMMAND_MENU_SELECTED, bg_updateCheck);
	event->SetClientData(ri);
	wxPostEvent(frame, *event);
	ri->condition->Wait();
	ri->mutex->Unlock();
	delete ri;
	delete event;
	if (curlReq) curl_easy_cleanup(curlReq);
	curlReq = NULL;

	return;
}
#if !defined(__APPLE__) && !defined(_WIN32) && !defined(__clang__)
	#pragma GCC diagnostic warning "-Wunused-local-typedefs"
#endif

QSLApp::QSLApp() : wxApp() {
}

MyFrame *
QSLApp::GUIinit(bool checkUpdates, bool quiet) {

	frame = new MyFrame(wxT("TQSL"), 0, 0, 0, 0, checkUpdates, quiet, NULL);
	frame->Show(!quiet);
	frame->SetFocus();
	SetTopWindow(frame);

	return frame;
}

bool
QSLApp::OnInit() {
	frame = 0;


	GUIinit(true, quiet);
	return true;
}

