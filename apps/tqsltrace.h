#ifndef __tqslllog_h
#define __tqslllog_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

extern FILE *diagFile;

void tqslTrace(const char *name, const char *format = NULL, ...);
#define _S(x) ((const char *) x.mb_str())

#endif	// __tqslllog_h
