/***************************************************************************
                          tqslcertctrls.h  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __tqslcertctrls_h
#define __tqslcertctrls_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

enum {		// Menu items
	tc_Quit,
	tc_CRQWizard,
	tc_Load,
	tc_Preferences,
	tc_c_Properties,
	tc_c_Sign,
	tc_c_Renew,
	tc_c_Import,
	tc_c_Export,
	tc_c_Delete,
	tc_h_Contents,
	tc_h_About,
};

enum {		// Window IDs
	tc_CertTree = (wxID_HIGHEST+1),
	tc_CertPropDial,
	tc_CertPropDialButton,
	ID_CRQ_PROVIDER,
	ID_CRQ_PROVIDER_INFO,
	ID_CRQ_COUNTRY,
	ID_CRQ_ZIP,
	ID_CRQ_NAME,
	ID_CRQ_CITY,
	ID_CRQ_ADDR1,
	ID_CRQ_ADDR2,
	ID_CRQ_EMAIL,
	ID_CRQ_STATE,
	ID_CRQ_CALL,
	ID_CRQ_QBYEAR,
	ID_CRQ_QBMONTH,
	ID_CRQ_QBDAY,
	ID_CRQ_QEYEAR,
	ID_CRQ_QEMONTH,
	ID_CRQ_QEDAY,
	ID_CRQ_DXCC,
	ID_CRQ_SIGN,
	ID_CRQ_CERT,
	ID_CRQ_PW1,
	ID_CRQ_PW2,
	ID_LCW_P12,
	ID_LCW_TQ6,
	ID_PREF_ROOT_CB,
	ID_PREF_CA_CB,
	ID_PREF_USER_CB,
	ID_PREF_ALLCERT_CB,
	ID_OK_BUT,
	ID_CAN_BUT,
	ID_HELP_BUT
};


#endif	// __tqslcertctrls_h
