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

TQSLValidator::TQSLValidator(void *objp) {
	_objp = objp;
}

bool
TQSLValidator::Copy(const TQSLValidator& val) {
	wxValidator::Copy(val);
	_objp = val._objp;
	_type = val._type;
	return TRUE;
}

bool
TQSLValidator::TransferFromWindow() {
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
		buf.Printf("Invalid %s: \"%s\"", _type.c_str(), str.c_str());
		wxMessageBox(buf, "QSO Data Error", wxOK | wxICON_EXCLAMATION, parent);
		return FALSE;
	}
	return TRUE;
}

void
TQSLDateValidator::FromString(const wxString& str) {
	if (_objp != 0)
		tqsl_initDate((tQSL_Date *)_objp, str.c_str());
}

wxString
TQSLDateValidator::ToString() {
	if (_objp == 0)
		return "";
	tQSL_Date *_datep = (tQSL_Date *)_objp;
	if (!tqsl_isDateValid(_datep))
		return "";
	char buf[20];
	tqsl_convertDateToText(_datep, buf, sizeof buf);
	return buf;
}

bool
TQSLDateValidator::IsValid(const wxString& str) {
	tQSL_Date d;
	return (!tqsl_initDate(&d, str.c_str()) && tqsl_isDateValid(&d));
}

void
TQSLTimeValidator::FromString(const wxString& str) {
	if (_objp != 0)
		tqsl_initTime((tQSL_Time *)_objp, str.c_str());
}

wxString
TQSLTimeValidator::ToString() {
	if (_objp == 0)
		return "";
	tQSL_Time *_timep = (tQSL_Time *)_objp;
	if (!tqsl_isTimeValid(_timep))
		return "";
	char buf[20];
	tqsl_convertTimeToText(_timep, buf, sizeof buf);
	return buf;
}

bool
TQSLTimeValidator::IsValid(const wxString& str) {
	tQSL_Time t;
	return (!tqsl_initTime(&t, str.c_str()) && tqsl_isTimeValid(&t));
}
