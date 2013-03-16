/***************************************************************************
                          certtree.cpp  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include "certtree.h"
#include <map>
#include <vector>
#include <algorithm>

#include "tqslcertctrls.h"
#include "util.h"
#include "dxcc.h"
#include "tqslerrno.h"
#include <errno.h>
#include <wx/imaglist.h>

#include <iostream>

using namespace std;

#include "cert.xpm"
#include "nocert.xpm"
#include "broken-cert.xpm"
#include "folder.xpm"

///////////// Certificate Tree Control ////////////////


BEGIN_EVENT_TABLE(CertTree, wxTreeCtrl)
	EVT_TREE_ITEM_ACTIVATED(-1, CertTree::OnItemActivated)
	EVT_RIGHT_DOWN(CertTree::OnRightDown)
END_EVENT_TABLE()

CertTree::CertTree(wxWindow *parent, const wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style) :
		wxTreeCtrl(parent, id, pos, size, style), _ncerts(0) {
	useContextMenu = true;
	wxBitmap certbm(cert_xpm);
	wxBitmap no_certbm(nocert_xpm);
	wxBitmap broken_certbm(broken_cert_xpm);
	wxBitmap folderbm(folder_xpm);
	wxImageList *il = new wxImageList(16, 16, false, 3);
	il->Add(certbm);
	il->Add(no_certbm);
	il->Add(broken_certbm);
	il->Add(folderbm);
	SetImageList(il);
}


CertTree::~CertTree() {
}

CertTreeItemData::~CertTreeItemData() {
	if (_cert)
		tqsl_freeCertificate(_cert);
}

typedef pair<wxString,int> certitem;
typedef vector<certitem> certlist;

static bool
cl_cmp(const certitem& i1, const certitem& i2) {
	return i1.first < i2.first;
}

int
CertTree::Build(int flags, const TQSL_PROVIDER *provider) {
	typedef map<wxString,certlist> issmap;
	issmap issuers;

	DeleteAllItems();
	wxTreeItemId rootId = AddRoot(wxT("tQSL Certificates"), 3);
	tqsl_init();
	tQSL_Cert *certs;
	if (tqsl_selectCertificates(&certs, &_ncerts, 0, 0, 0, provider, flags)) {
		if (tQSL_Error != TQSL_SYSTEM_ERROR || errno != ENOENT)
			displayTQSLError("Error while accessing certificate store");
		return _ncerts;
	}
	// Separate certs into lists by issuer
	for (int i = 0; i < _ncerts; i++) {	
		char issname[129];
		if (tqsl_getCertificateIssuerOrganization(certs[i], issname, sizeof issname)) {
			displayTQSLError("Error parsing certificate for issuer");
			return _ncerts;
		}
		char callsign[129] = "";
		if (tqsl_getCertificateCallSign(certs[i], callsign, sizeof callsign - 4)) {
			displayTQSLError("Error parsing certificate for call sign");
			return _ncerts;
		}
		strcat(callsign, " - ");
		DXCC dxcc;
		int dxccEntity;
		if (tqsl_getCertificateDXCCEntity(certs[i], &dxccEntity)) {
			displayTQSLError("Error parsing certificate for DXCC entity");
			return _ncerts;
		}
		const char *entityName;
		if (dxcc.getByEntity(dxccEntity))
			entityName = dxcc.name();
		else
			entityName = "<UNKNOWN ENTITY>";
		strncat(callsign, entityName, sizeof callsign - strlen(callsign)-1);
		callsign[sizeof callsign-1] = 0;
		issuers[wxString(issname, wxConvLocal)].push_back(make_pair(wxString(callsign, wxConvLocal),i));
	}
	// Sort each issuer's list and add items to tree
	issmap::iterator iss_it;
	for (iss_it = issuers.begin(); iss_it != issuers.end(); iss_it++) {
		wxTreeItemId id = AppendItem(rootId, iss_it->first, 3);
		certlist& list = iss_it->second;
		sort(list.begin(), list.end(), cl_cmp);
		for (int i = 0; i < (int)list.size(); i++) {
			CertTreeItemData *cert = new CertTreeItemData(certs[list[i].second]);
			int keyonly = 1;
			int keytype = tqsl_getCertificatePrivateKeyType(certs[list[i].second]);
			tqsl_getCertificateKeyOnly(certs[list[i].second], &keyonly);
			AppendItem(id, list[i].first,
				((keytype == TQSL_PK_TYPE_ERR) ? 2 : (keyonly ? 1 : 0)), -1, cert);
		}
		Expand(id);
	}
	Expand(rootId);
	return _ncerts;
}

void
CertTree::OnItemActivated(wxTreeEvent& event) {
	wxTreeItemId id = event.GetItem();
	displayCertProperties((CertTreeItemData *)GetItemData(id), this);
}

void
CertTree::OnRightDown(wxMouseEvent& event) {
	if (!useContextMenu)
		return;
	wxTreeItemId id = HitTest(event.GetPosition());
	if (id && GetItemData(id)) {
		SelectItem(id);
		tQSL_Cert cert = GetItemData(id)->getCert();
		int keyonly = 1;
		if (cert)
			tqsl_getCertificateKeyOnly(cert, &keyonly);
		wxMenu *cm = makeCertificateMenu(true, keyonly);
		PopupMenu(cm, event.GetPosition());
		delete cm;
	}
}

