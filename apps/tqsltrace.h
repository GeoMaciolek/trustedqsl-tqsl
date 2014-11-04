/***************************************************************************
                          tqsltrace.h  -  description
                             -------------------
          copyright (C) 2013 by ARRL and the TrustedQSL Developers
 ***************************************************************************/

#ifndef __tqslltrace_h
#define __tqslltrace_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#ifndef __TQSLERRNO_H
void tqslTrace(const char *name, const char *format = NULL, ...);
#endif
#define S(x) ((const char *) x.ToUTF8())

#endif	// __tqslltrace_h
