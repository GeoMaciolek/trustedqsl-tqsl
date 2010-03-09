/***************************************************************************
                          certtree.h  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __certtree_h
#define __certtree_h

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

class CertTreeItemData : public wxTreeItemData {
public:
	CertTreeItemData(tQSL_Cert cert) : _cert(cert) {}
	~CertTreeItemData();
	tQSL_Cert getCert() { return _cert; }

private:
	tQSL_Cert _cert;
};

class CertTree : public wxTreeCtrl {
public:
	CertTree(wxWindow *parent, const wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style);
	virtual ~CertTree();
	int Build(int flags = TQSL_SELECT_CERT_WITHKEYS, const TQSL_PROVIDER *provider = 0);
	void OnItemActivated(wxTreeEvent& event);
	void OnRightDown(wxMouseEvent& event);
	bool useContextMenu;
	CertTreeItemData *GetItemData(wxTreeItemId id) { return (CertTreeItemData *)wxTreeCtrl::GetItemData(id); }
	int GetNumCerts() const { return _ncerts; }

private:
	int _ncerts;
	DECLARE_EVENT_TABLE()

};

#endif	// __certtree_h
