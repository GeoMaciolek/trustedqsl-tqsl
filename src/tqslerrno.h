/***************************************************************************
                          tqslerrno.h  -  description
                             -------------------
    begin                : Tue May 28 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#ifndef __TQSLERRNO_H
#define __TQSLERRNO_H

#define TQSL_NO_ERROR 0
#define TQSL_SYSTEM_ERROR 1
#define TQSL_OPENSSL_ERROR 2
#define TQSL_ADIF_ERROR 3
#define TQSL_CUSTOM_ERROR 4
#define TQSL_ERROR_ENUM_BASE 16
#define TQSL_ALLOC_ERROR 16
#define TQSL_RANDOM_ERROR 17
#define TQSL_ARGUMENT_ERROR 18
#define TQSL_OPERATOR_ABORT 19
#define TQSL_NOKEY_ERROR 20
#define TQSL_BUFFER_ERROR 21
#define TQSL_INVALID_DATE 22
#define TQSL_SIGNINIT_ERROR 23
#define TQSL_PASSWORD_ERROR 24

#endif /* __TQSLERRNO_H */
