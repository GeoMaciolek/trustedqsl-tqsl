/***************************************************************************
                          qsodatadialog.h  -  description
                             -------------------
    begin                : Sat Dec 7 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#ifndef QSODATADIALOG_H
#define QSODATADIALOG_H

using namespace std;

#include "wx/wxprec.h"

#ifdef __BORLANDC__
	#pragma hdrstop
#endif

#ifndef WX_PRECOMP
	#include "wx/wx.h"
#endif

#include "tqsllib.h"
#include <wx/bmpbuttn.h>
#include <wx/textctrl.h>
#include <vector>

/** Display and edit QSO data
  *
  *@author Jon Bloom
  */

class QSORecord {
public:
	QSORecord() { _mode = "CW"; _band = "160M"; }

	wxString _call, _freq;
	wxString _mode, _band;
	tQSL_Date _date;
	tQSL_Time _time;
};

typedef vector<QSORecord> QSORecordList;

class QSODataDialog : public wxDialog  {
public: 
	QSODataDialog(wxWindow *parent, QSORecordList *reclist = 0, wxWindowID id = -1,
		const wxString& title = "QSO Data");
	~QSODataDialog();
	wxString GetMode() const;
	bool SetMode(const wxString&);
	QSORecord rec;
	virtual bool TransferDataToWindow();
	virtual bool TransferDataFromWindow();

private:
	void OnOk(wxCommandEvent&);
	void OnCancel(wxCommandEvent&);
	void OnRecUp(wxCommandEvent&);
	void OnRecDown(wxCommandEvent&);
	void OnRecBottom(wxCommandEvent&);
	void OnRecTop(wxCommandEvent&);
	void OnRecNew(wxCommandEvent&);
	void OnRecDelete(wxCommandEvent&);
	void SetRecno(int);
	void UpdateControls();

	wxTextCtrl *_recno_ctrl;
	wxStaticText *_recno_label_ctrl;
	wxBitmapButton *_recdown_ctrl, *_recup_ctrl, *_recbottom_ctrl, *_rectop_ctrl;
	QSORecordList *_reclist;
	int _mode, _band;
	int _recno;
	DECLARE_EVENT_TABLE()
	bool _isend;
};

#endif
