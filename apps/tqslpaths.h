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

class DocPaths : public wxPathList {
public:
	DocPaths(wxString subdir) : wxPathList() {
#if defined(__WIN32__)
		Add(wxGetHomeDir() + "/help/" + subdir);
		Add("help/" + subdir);
#else
		Add(wxGetHomeDir() + "/" + subdir);
		Add("/usr/share/doc/" + subdir);
		Add("/usr/doc/" + subdir);
		Add("/usr/lib/" + subdir);
		Add(subdir);
#endif
	}
};
