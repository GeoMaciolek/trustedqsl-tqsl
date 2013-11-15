/***************************************************************************
                          wxutil.h  -  description
                             -------------------
    begin                : Thu Aug 14 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __wxutil_h
#define __wxutil_h


#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "wx/wxprec.h"

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#if wxCHECK_VERSION(2, 5, 0)
	#define TQ_WXCLOSEEVENT wxCloseEvent
	#define TQ_WXTEXTEVENT wxCommandEvent
	#define TQ_WXCOOKIE wxTreeItemIdValue
#else
	#define TQ_WXCLOSEEVENT wxCommandEvent
	#define TQ_WXTEXTEVENT wxEvent
	#define TQ_WXCOOKIE long
#endif

wxSize getTextSize(wxWindow *win);
wxString urlEncode(wxString& str);

#endif	// __wxutil_h
