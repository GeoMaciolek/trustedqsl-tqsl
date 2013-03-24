/***************************************************************************
                          tqslerrno.h  -  description
                             -------------------
    begin                : Tue May 28 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __TQSLERRNO_H
#define __TQSLERRNO_H

/** \file
  *  #tQSL_Error values
*/

#define TQSL_NO_ERROR 0
#define TQSL_SYSTEM_ERROR 1
#define TQSL_OPENSSL_ERROR 2
#define TQSL_ADIF_ERROR 3
#define TQSL_CUSTOM_ERROR 4
#define TQSL_CABRILLO_ERROR 5
#define TQSL_OPENSSL_VERSION_ERROR 6
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
#define TQSL_EXPECTED_NAME 25
#define TQSL_NAME_EXISTS 26
#define TQSL_NAME_NOT_FOUND 27
#define TQSL_INVALID_TIME 28
#define TQSL_CERT_DATE_MISMATCH 29
#define TQSL_PROVIDER_NOT_FOUND 30
#define TQSL_CERT_KEY_ONLY 31
#define TQSL_CONFIG_ERROR 32
#define TQSL_CERT_NOT_FOUND 33
#define TQSL_PKCS12_ERROR 34
#define TQSL_CERT_TYPE_ERROR 35
#define TQSL_DATE_OUT_OF_RANGE 36
#define TQSL_DUPLICATE_QSO 37
#define TQSL_DB_ERROR 38
#define TQSL_LOCATION_NOT_FOUND 39
#define TQSL_CALL_NOT_FOUND 40
#define TQSL_CONFIG_SYNTAX_ERROR 41
#define TQSL_FILE_SYSTEM_ERROR 42
#define TQSL_FILE_SYNTAX_ERROR 43

#endif /* __TQSLERRNO_H */
