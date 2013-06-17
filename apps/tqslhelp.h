// Derived from wxWidgets fs_inet.h

#ifndef _TQSL_INET_H_
#define _TQSL_INET_H_

#include "wx/defs.h"

#include "wx/filesys.h"

// ----------------------------------------------------------------------------
// tqslInternetFSHandler
// ----------------------------------------------------------------------------

class WXDLLIMPEXP_NET tqslInternetFSHandler : public wxFileSystemHandler {
public:
        virtual bool CanOpen(const wxString& location);
        virtual wxFSFile* OpenFile(wxFileSystem& fs, const wxString& location);
};

#endif // _TQSL_INET_H_

