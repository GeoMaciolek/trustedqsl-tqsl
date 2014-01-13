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
