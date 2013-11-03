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

extern FILE *diagFile;

void tqslTrace(const char *name, const char *format = NULL, ...);
#define _S(x) ((const char *) x.mb_str())

#endif	// __tqslltrace_h
