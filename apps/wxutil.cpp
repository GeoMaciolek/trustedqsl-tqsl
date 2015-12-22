/***************************************************************************
                          wxutil.cpp  -  description
                             -------------------
    begin                : Thu Aug 14 2003
    copyright            : (C) 2003 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "wxutil.h"
#include <wx/dir.h>
#include <wx/config.h>
#include <wx/filename.h>
#include "tqsllib.h"
#include "tqslerrno.h"

wxSize
getTextSize(wxWindow *win) {
	wxClientDC dc(win);
	wxCoord char_width, char_height;
	dc.GetTextExtent(wxString(wxT("M")), &char_width, &char_height);
	return wxSize(char_width, char_height);
}

// Strip special characters from a string prior to writing to XML
wxString
urlEncode(wxString& str) {
	str.Replace(wxT("&"), wxT("&amp;"), true);
	str.Replace(wxT("\""), wxT("&quot;"), true);
	str.Replace(wxT("'"), wxT("&apos;"), true);
	str.Replace(wxT("<"), wxT("&lt;"), true);
	str.Replace(wxT(">"), wxT("&gt;"), true);
	return str;
}

// Convert UTF-8 string to UCS-2 (MS Unicode default)
int
utf8_to_ucs2(const char *in, char *out, size_t buflen) {
	size_t len = 0;

	while (len < buflen) {
		if ((unsigned char)*in < 0x80) {		// ASCII range
			*out++ = *in;
			if (*in++ == '\0')			// End of string
				break;
			len++;
		} else if (((unsigned char)*in & 0xc0) == 0xc0) {  // Two-byte
			*out++ = ((in[0] & 0x1f) << 6) | (in[1] & 0x3f);
			in += 2;
			len++;
		} else if (((unsigned char)*in & 0xe0) == 0xe0) {  // Three-byte
			unsigned short three =	((in[0] & 0x0f) << 12) |
					        ((in[1] & 0x3f) << 6) |
						 (in[2] & 0x3f);
			*out++ = (three & 0xff00) >> 8;
			len++;
			if (len < buflen) {
				*out++ = (three & 0xff);
				len++;
			}
			in += 3;
		} else {
			in++;		// Unknown. Skip input.
		}
	}
	out[len-1] = '\0';
	return len;
}
int
getPasswordFromUser(wxString& result, const wxString& message, const wxString& caption, const wxString& defaultValue, wxWindow *parent) {
	long style = wxTextEntryDialogStyle;

	wxPasswordEntryDialog dialog(parent, message, caption, defaultValue, style);

	int ret = dialog.ShowModal();
	if (ret == wxID_OK)
		result = dialog.GetValue();

	return ret;
}

static const char *error_strings[] = {
	__("Memory allocation failure"),			/* TQSL_ALLOC_ERROR */
	__("Unable to initialize random number generator"),	/* TQSL_RANDOM_ERROR */
	__("Invalid argument"),					/* TQSL_ARGUMENT_ERROR */
	__("Operator aborted operation"),			/* TQSL_OPERATOR_ABORT */
	__("No Certificate Request matches the selected Callsign Certificate"),/* TQSL_NOKEY_ERROR */
	__("Buffer too small"),					/* TQSL_BUFFER_ERROR */
	__("Invalid date format"),				/* TQSL_INVALID_DATE */
	__("Certificate not initialized for signing"),		/* TQSL_SIGNINIT_ERROR */
	__("Password not correct"),				/* TQSL_PASSWORD_ERROR */
	__("Expected name"),					/* TQSL_EXPECTED_NAME */
	__("Name exists"),					/* TQSL_NAME_EXISTS */
	__("Data for this DXCC entity could not be found"),	/* TQSL_NAME_NOT_FOUND */
	__("Invalid time format"),				/* TQSL_INVALID_TIME */
	__("QSO date is not within the date range specified on your Callsign Certificate"),	/* TQSL_CERT_DATE_MISMATCH */
	__("Certificate provider not found"),			/* TQSL_PROVIDER_NOT_FOUND */
	__("No callsign certificate for key"),			/* TQSL_CERT_KEY_ONLY */
	__("Configuration file cannot be opened"),		/* TQSL_CONFIG_ERROR */
	__("Callsign Certificate or Certificate Request not found"),/* TQSL_CERT_NOT_FOUND */
	__("PKCS#12 file not TQSL compatible"),			/* TQSL_PKCS12_ERROR */
	__("Callsign Certificate not TQSL compatible"),		/* TQSL_CERT_TYPE_ERROR */
	__("Date out of range"),				/* TQSL_DATE_OUT_OF_RANGE */
	__("Duplicate QSO suppressed"),				/* TQSL_DUPLICATE_QSO */
	__("Database error"),					/* TQSL_DB_ERROR */
	__("The selected station location could not be found"),	/* TQSL_LOCATION_NOT_FOUND */
	__("The selected callsign could not be found"),		/* TQSL_CALL_NOT_FOUND */
	__("The TQSL configuration file cannot be parsed"),	/* TQSL_CONFIG_SYNTAX_ERROR */
	__("This file can not be processed due to a system error"),	/* TQSL_FILE_SYSTEM_ERROR */
	__("The format of this file is incorrect."),		/* TQSL_FILE_SYNTAX_ERROR */
};

static wxString
getLocalizedErrorString_v(int err) {
	int adjusted_err;
	wxString buf;

	if (err == 0)
		return _("NO ERROR");
	if (err == TQSL_CUSTOM_ERROR) {
		if (tQSL_CustomError[0] == 0) {
			return _("Unknown custom error");
		} else {
			return wxString::FromUTF8(tQSL_CustomError);
		}
	}
	if (err == TQSL_DB_ERROR && tQSL_CustomError[0] != 0) {
		return wxString::Format(_("Database Error: %hs"), tQSL_CustomError);
	}

	if (err == TQSL_SYSTEM_ERROR || err == TQSL_FILE_SYSTEM_ERROR) {
		if (strlen(tQSL_ErrorFile) > 0) {
			buf = wxString::Format(_("System error: %hs : %hs"),
				tQSL_ErrorFile, strerror(tQSL_Errno));
			tQSL_ErrorFile[0] = '\0';
		} else {
			buf = wxString::Format(_("System error: %hs"),
				strerror(tQSL_Errno));
		}
		return buf;
	}
	if (err == TQSL_FILE_SYNTAX_ERROR) {
		if (strlen(tQSL_ErrorFile) > 0) {
			buf = wxString::Format(_("File syntax error: %hs"),
				tQSL_ErrorFile);
			tQSL_ErrorFile[0] = '\0';
		} else {
			buf = _("File syntax error");
		}
		return buf;
	}
	if (err == TQSL_OPENSSL_ERROR) {
		// Get error details from tqsllib as we have
		// no visibility into the tqsllib openssl context
		const char *msg = tqsl_getErrorString();
		return wxString::FromUTF8(msg);
	}
	if (err == TQSL_ADIF_ERROR) {
		if (strlen(tQSL_ErrorFile) > 0) {
			buf = wxString::Format(wxT("%hs: %hs"), tQSL_ErrorFile, tqsl_adifGetError(tQSL_ADIF_Error));
			tQSL_ErrorFile[0] = '\0';
		} else {
			buf = wxString::FromUTF8(tqsl_adifGetError(tQSL_ADIF_Error));
		}
		return buf;
	}
	if (err == TQSL_CABRILLO_ERROR) {
		if (strlen(tQSL_ErrorFile) > 0) {
			buf = wxString::Format(wxT("%hs: %hs"),
				tQSL_ErrorFile, tqsl_cabrilloGetError(tQSL_Cabrillo_Error));
			tQSL_ErrorFile[0] = '\0';
		} else {
			buf = wxString::FromUTF8(tqsl_cabrilloGetError(tQSL_Cabrillo_Error));
		}
		return buf;
	}
	if (err == TQSL_OPENSSL_VERSION_ERROR) {
		// No visibility into the tqsllib openssl context
		const char *msg = tqsl_getErrorString();
		return wxString::FromUTF8(msg);
	}
	if (err == TQSL_CERT_NOT_FOUND && tQSL_ImportCall[0] != '\0') {
		return wxString::Format(_("Callsign Certificate or Certificate Request not found for callsign %hs serial %ld"),
			tQSL_ImportCall, tQSL_ImportSerial);
	}
	adjusted_err = err - TQSL_ERROR_ENUM_BASE;
	if (adjusted_err < 0 ||
	    adjusted_err >=
		static_cast<int>(sizeof error_strings / sizeof error_strings[0])) {
		return wxString::Format(_("Invalid error code: %d"), err);
	}
	return wxGetTranslation(wxString::FromUTF8(error_strings[adjusted_err]));
}

wxString
getLocalizedErrorString() {
	wxString cp = getLocalizedErrorString_v(tQSL_Error);
	tQSL_Error = TQSL_NO_ERROR;
	tQSL_Errno = 0;
	tQSL_ErrorFile[0] = 0;
	tQSL_CustomError[0] = 0;
	return cp;
}
