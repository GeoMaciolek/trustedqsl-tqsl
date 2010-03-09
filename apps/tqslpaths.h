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

#ifdef MAC
#include "Carbon.h"
#endif

#include <wx/filefn.h>
#if defined(__WIN32__)
	#include <windows.h>
#endif

class DocPaths : public wxPathList {
public:
	DocPaths(wxString subdir) : wxPathList() {
		Add(wxGetHomeDir() + wxT("/help/") + subdir);
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
		Add(wxT("help/") + subdir);
#elif defined(MAC)
                CFURLRef mainBundleURL;
                FSRef bundleFSRef;
                char npath[1024];

                mainBundleURL = CFBundleCopyBundleURL(CFBundleGetMainBundle());
                CFURLGetFSRef(mainBundleURL, &bundleFSRef);
                FSRefMakePath(&bundleFSRef, (unsigned char*)npath, sizeof(npath)
 - 1);
                // if last char is not a /, append one
                if ((strlen(npath) > 0) && (npath[strlen(npath)-1] != '/'))
                {
                        strcat(npath,"/");
                }

                CFRelease(mainBundleURL);

/* The wxWindows converters don't have a c_str to wxSting converter
 * so it crashes on attempts to wxString the path.
 * This quick and dirty conversion to wchar fixes that.
 */
		size_t l = strlen(npath) * 5;
		wchar_t *np = (wchar_t *)malloc(l);
		char *src = npath;
#if (SIZEOF_WCHAR_T == 2)
		wxInt16 *dst = (wxInt16 *np);
#elif (SIZEOF_WCHAR_T == 4)
		wxInt32 *dst = (wxInt32 *) np;
#else
#error "unrecognized wchar_t size"
#endif
		while (*src) {
			*dst++ = *src++;
		}
		*dst++ = 0;
		*dst++ = 0;
		wxString foo(np);
		Add(foo + wxT("Contents/Resources/Help/"));
		Add(wxT("/Applications/") + subdir + wxT(".app/Contents/Resources/Help/"));
#else
		Add(wxT("/usr/share/TrustedQSL/help/") + subdir);
		Add(wxT("/usr/local/share/TrustedQSL/help/") + subdir);
		Add(subdir);
#endif
	}
};
