/***************************************************************************
                          util.h  -  description
                             -------------------
    begin                : Sun Jun 23 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __util_h
#define __util_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

void displayCertProperties(CertTreeItemData *item);
int getPassword(char *buf, int bufsiz);
void displayTQSLError(const char *pre);
wxMenu *makeCertificateMenu(bool enable);

#endif	// __util_h
