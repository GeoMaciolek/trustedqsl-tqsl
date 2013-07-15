/***************************************************************************
                          tqslvalidator.cpp  -  description
                             -------------------
    begin                : Sun Dec 8 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "tqslvalidator.h"
#include "tqsltrace.h"

TQSLValidator::TQSLValidator(void *objp) {
	_objp = objp;
}

bool
TQSLValidator::Copy(const TQSLValidator& val) {
	tqslTrace("TQSLValidator::Copy");
	wxValidator::Copy(val);
	_objp = val._objp;
	_type = val._type;
	return TRUE;
}

bool
TQSLValidator::TransferFromWindow() {
	tqslTrace("TQSLValidator::TransferFromWindow");
	if (!m_validatorWindow)
		return FALSE;
	if (!m_validatorWindow->IsKindOf(CLASSINFO(wxTextCtrl)))
		return FALSE;
	if (_objp == 0)
		return FALSE;
	wxTextCtrl *ctl = (wxTextCtrl *) m_validatorWindow;
	wxString str = ctl->GetValue();
	FromString(str);
	return TRUE;
}

bool
TQSLValidator::TransferToWindow() {
	tqslTrace("TQSLValidator::TransferToWindow");
	if (!m_validatorWindow)
		return FALSE;
	if (!m_validatorWindow->IsKindOf(CLASSINFO(wxTextCtrl)))
		return FALSE;
	if (_objp == 0)
		return FALSE;
	wxString str = this->ToString();
	wxTextCtrl *ctl = (wxTextCtrl *) m_validatorWindow;
	ctl->SetValue(str);
	return TRUE;
}

bool
TQSLValidator::Validate(wxWindow* parent) {
	tqslTrace("TQSLValidator::Validate", "parent=%lx", (void *)parent);
	if (!m_validatorWindow)
		return FALSE;
	if (!m_validatorWindow->IsKindOf(CLASSINFO(wxTextCtrl)))
		return FALSE;
	if (_objp == 0)
		return FALSE;
	wxTextCtrl *ctl = (wxTextCtrl *) m_validatorWindow;
	wxString str = ctl->GetValue();
	if (!IsValid(str)) {
		m_validatorWindow->SetFocus();
		wxString buf;
		buf.Printf(wxT("Invalid %s: \"%s\""), (const char *)_type.mb_str(), (const char *)str.mb_str());
		wxMessageBox(buf, wxT("QSO Data Error"), wxOK | wxICON_EXCLAMATION, parent);
		return FALSE;
	}
	return TRUE;
}

void
TQSLDateValidator::FromString(const wxString& str) {
	if (_objp != 0)
		tqsl_initDate((tQSL_Date *)_objp, str.mb_str());
}

wxString
TQSLDateValidator::ToString() {
	if (_objp == 0)
		return wxT("");
	tQSL_Date *_datep = (tQSL_Date *)_objp;
	if (!tqsl_isDateValid(_datep))
		return wxT("");
	char buf[20];
	tqsl_convertDateToText(_datep, buf, sizeof buf);
	return wxString(buf, wxConvLocal);
}

bool
TQSLDateValidator::IsValid(const wxString& str) {
	tqslTrace("TQSLDateValidator::IsValid", "str=%s", _S(str));
	tQSL_Date d;
	return (!tqsl_initDate(&d, str.mb_str()) && tqsl_isDateValid(&d));
}

void
TQSLTimeValidator::FromString(const wxString& str) {
	tqslTrace("TQSLTimeValidator::FromString", "str=%s", _S( str));
	if (_objp != 0)
		tqsl_initTime((tQSL_Time *)_objp, str.mb_str());
}

wxString
TQSLTimeValidator::ToString() {
	if (_objp == 0)
		return wxT("");
	tQSL_Time *_timep = (tQSL_Time *)_objp;
	if (!tqsl_isTimeValid(_timep))
		return wxT("");
	char buf[20];
	tqsl_convertTimeToText(_timep, buf, sizeof buf);
	return wxString(buf, wxConvLocal);
}

bool
TQSLTimeValidator::IsValid(const wxString& str) {
	tqslTrace("TQSLTimeValidator::IsValid", "str=%s", _S(str));
	tQSL_Time t;
	return (!tqsl_initTime(&t, str.mb_str()) && tqsl_isTimeValid(&t));
}
