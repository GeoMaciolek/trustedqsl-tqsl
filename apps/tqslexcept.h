/***************************************************************************
                          tqslexcept.h  -  description
                             -------------------
    begin                : Fri Nov 15 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __tqslexcept_h
#define __tqslexcept_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include <exception>
#include <string>

class TQSLException : public exception {
public:
	TQSLException(const char *msg);
	virtual const char *what() { return _msg.c_str(); }
private:
	string _msg;
};

inline TQSLException::TQSLException(const char *msg) : exception() {
	_msg = msg;
}

#endif	// __tqslexcept_h
