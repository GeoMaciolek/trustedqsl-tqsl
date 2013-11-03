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
#include <wx/wxhtml.h>

#include <vector>

using std::vector;

/** Display and edit QSO data
  *
  *@author Jon Bloom
  */

class QSORecord {
 public:
	QSORecord() { _mode = wxT("CW"); _band = wxT("160M"); _rxband = wxT(""); }

	wxString _call, _freq, _rxfreq;
	wxString _mode, _band, _rxband;
	wxString _propmode, _satellite;
	tQSL_Date _date;
	tQSL_Time _time;
};

typedef vector<QSORecord> QSORecordList;

class QSODataDialog : public wxDialog  {
 public:
	QSODataDialog(wxWindow *parent, wxHtmlHelpController *help, QSORecordList *reclist = 0,
		wxWindowID id = -1, const wxString& title = wxT("QSO Data"));
	~QSODataDialog();
	wxString GetMode() const;
	bool SetMode(const wxString&);
	QSORecord rec;
	virtual bool TransferDataToWindow();
	virtual bool TransferDataFromWindow();

 private:
	void OnOk(wxCommandEvent&);
	void OnCancel(wxCommandEvent&);
	void OnHelp(wxCommandEvent&);
	void OnRecUp(wxCommandEvent&);
	void OnRecDown(wxCommandEvent&);
	void OnRecBottom(wxCommandEvent&);
	void OnRecTop(wxCommandEvent&);
	void OnRecNew(wxCommandEvent&);
	void OnRecDelete(wxCommandEvent&);
	void SetRecno(int recno);
	void UpdateControls();

	wxTextCtrl *_recno_ctrl;
	wxTextCtrl *_call_ctrl;
	wxStaticText *_recno_label_ctrl;
	wxBitmapButton *_recdown_ctrl, *_recup_ctrl, *_recbottom_ctrl, *_rectop_ctrl;
	QSORecordList *_reclist;
	int _mode, _band, _rxband, _propmode, _satellite;
	int _recno;
	DECLARE_EVENT_TABLE()
	bool _isend;
	wxHtmlHelpController *_help;
};

#endif
