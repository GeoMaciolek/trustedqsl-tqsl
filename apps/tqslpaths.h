/***************************************************************************
                          tqslpaths.h  -  description
                             -------------------
    begin                : Mon Dec 9 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include <wx/filefn.h>
#if defined(__WIN32__)
	#include <windows.h>
#endif

class DocPaths : public wxPathList {
public:
	DocPaths(wxString subdir) : wxPathList() {
		Add(wxGetHomeDir() + "/help/" + subdir);
#if defined(__WIN32__)
		HKEY hkey;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\TrustedQSL",
			0, KEY_READ, &hkey) == ERROR_SUCCESS) {

			DWORD dtype;
			char path[256];
			DWORD bsize = sizeof path;
			if (RegQueryValueEx(hkey, "HelpDir", 0, &dtype, (LPBYTE)path, &bsize)
				== ERROR_SUCCESS) {
				Add(wxString(path) + "/" + subdir);
			}
		}
		Add("help/" + subdir);
#else
		Add("/usr/share/TrustedQSL/help/" + subdir);
		Add(subdir);
#endif
	}
};
