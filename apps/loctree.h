/***************************************************************************
                          loctree.h  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __loctree_h
#define __loctree_h

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

#include "tqsllib.h"

#include "wx/treectrl.h"

class LocTreeItemData : public wxTreeItemData {
public:
	LocTreeItemData(wxString locname) : _locname(locname) {}
	wxString getLocname() { return _locname; }

private:
	wxString _locname;
};

class LocTree : public wxTreeCtrl {
public:
	LocTree(wxWindow *parent, const wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style);
	virtual ~LocTree();
	int Build(int flags = TQSL_SELECT_CERT_WITHKEYS, const TQSL_PROVIDER *provider = 0);
	void OnItemActivated(wxTreeEvent& event);
	void OnRightDown(wxMouseEvent& event);
	bool useContextMenu;
	LocTreeItemData *GetItemData(wxTreeItemId id) { return (LocTreeItemData *)wxTreeCtrl::GetItemData(id); }
	int GetNumLocations() const { return _nloc; }

private:
	int _nloc;
	DECLARE_EVENT_TABLE()

};

#endif	// __loctree_h
