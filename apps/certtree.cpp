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

#include "tqslcertctrls.h"
#include "util.h"
#include "dxcc.h"
#include "tqslerrno.h"
#include <errno.h>

///////////// Certificate Tree Control ////////////////


BEGIN_EVENT_TABLE(CertTree, wxTreeCtrl)
	EVT_TREE_ITEM_ACTIVATED(-1, CertTree::OnItemActivated)
	EVT_RIGHT_DOWN(CertTree::OnRightDown)
END_EVENT_TABLE()

CertTree::CertTree(wxWindow *parent, const wxWindowID id, const wxPoint& pos,
		const wxSize& size, long style) :
		wxTreeCtrl(parent, id, pos, size, style) {
	useContextMenu = true;
}

CertTree::~CertTree() {
}

CertTreeItemData::~CertTreeItemData() {
	if (_cert)
		tqsl_freeCertificate(_cert);
}

int
CertTree::Build() {
	typedef map<wxString,wxTreeItemId> idmap;

	idmap issuers;

	DeleteAllItems();
	wxTreeItemId rootId = AddRoot("tQSL Certificates");
	tQSL_Cert *certs;
	int ncerts = 0;
	if (tqsl_selectCertificates(&certs, &ncerts, 0, 0, 0, 0)) {
		if (tQSL_Error != TQSL_SYSTEM_ERROR || errno != ENOENT)
			displayTQSLError("Error while accessing certificate store");
		return ncerts;
	}
	for (int i = 0; i < ncerts; i++) {
		char issname[129];
		if (tqsl_getCertificateIssuerOrganization(certs[i], issname, sizeof issname)) {
			displayTQSLError("Error parsing certificate for issuer");
			return ncerts;
		}
		wxTreeItemId id = issuers[issname];
		if (!id.IsOk()) {
			id = rootId;
			id = AppendItem(id, issname);
			Expand(id);
			issuers[issname] = id;
		}
		char callsign[129];
		if (tqsl_getCertificateCallSign(certs[i], callsign, sizeof callsign - 4)) {
			displayTQSLError("Error parsing certificate for call sign");
			return ncerts;
		}
		strcat(callsign, " - ");
		DXCC dxcc;
		int dxccEntity;
		if (tqsl_getCertificateDXCCEntity(certs[i], &dxccEntity)) {
			displayTQSLError("Error parsing certificate for DXCC entity");
			return ncerts;
		}
		const char *entityName;
		if (dxcc.getByEntity(dxccEntity))
			entityName = dxcc.name();
		else
			entityName = "<UNKNOWN ENTITY>";
		strncat(callsign, dxcc.name(), sizeof callsign - strlen(callsign));
		callsign[sizeof callsign-1] = 0;
		CertTreeItemData *cert = new CertTreeItemData(certs[i]);
		AppendItem(id, callsign, -1, -1, cert);
		Expand(id);
	}
	Expand(rootId);
	return ncerts;
}

void
CertTree::OnItemActivated(wxTreeEvent& event) {
	wxTreeItemId id = event.GetItem();
	displayCertProperties((CertTreeItemData *)GetItemData(id));
}

void
CertTree::OnRightDown(wxMouseEvent& event) {
	if (!useContextMenu)
		return;
	wxTreeItemId id = HitTest(event.GetPosition());
	if (id && GetItemData(id)) {
		SelectItem(id);
		wxMenu *cm = makeCertificateMenu(true);
		PopupMenu(cm, event.GetPosition());
		delete cm;
	}
}

