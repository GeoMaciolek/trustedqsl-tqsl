/***************************************************************************
                          convert.h  -  description
                             -------------------
    begin                : Sun Nov 17 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#ifndef __tqslconvert_h
#define __tqslconvert_h

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#include "tqsllib.h"

/** \file
  * tQSL library converter functions.
  */

/** \defgroup Convert Converter API
  *
  * The Converter API provides the capability of converting Cabrillo
  * and ADIF files to GABBI output.
  */
/** @{ */

typedef void * tQSL_Converter;

#ifdef __cplusplus
extern "C" {
#endif

/** Initiates the conversion process for an ADIF file.
  *
  * \c certs and \c ncerts define a set of certificates that are available to the
  * converter for signing records. Typically, this list will be obtained by
  * calling tqsl_selectCertificates().
  *
  * tqsl_endConverter() should be called to free the resources when the conversion
  * is finished.
  */
int tqsl_beginADIFConverter(tQSL_Converter *conv, const char *filename, tQSL_Cert *certs, int ncerts, tQSL_Location loc);

/** Initiates the conversion process for a Cabrillo file.
  *
  * \c certs and \c ncerts define a set of certificates that are available to the
  * converter for signing records. Typically, this list will be obtained by
  * calling tqsl_selectCertificates().
  *
  * tqsl_endConverter() should be called to free the resources when the conversion
  * is finished.
  */
int tqsl_beginCabrilloConverter(tQSL_Converter *conv, const char *filename, tQSL_Cert *certs, int ncerts, tQSL_Location loc);

/** End the conversion process by freeing the used resources. */
int tqsl_endConverter(tQSL_Converter *conv);

/** This is the main converter function. It returns a single GABBI
  * record.
  *
  * Returns the NULL pointer of error or EOF. (Test tQSL_Error to determine which.)
  *
  * N.B. On systems that distinguish text-mode files from binary-mode files,
  * notably Windows, the GABBI records should be written in binary mode.
  *
  * N.B. If the selected certificate has not been initialized for signing via
  * tqsl_beginSigning(), this function will return a TQSL_SIGNINIT_ERROR.
  * The cert that caused the error can be obtained via tqsl_getConverterCert(),
  * initialized for signing, and then this function can be called again. No
  * data records will be lost in this process.
  */
const char *tqsl_getConverterGABBI(tQSL_Converter conv);

/** Get the certificate used to sign the most recent QSO record. */
int tqsl_getConverterCert(tQSL_Converter conv, tQSL_Cert *certp);

/** Get the input-file line number last read by the converter, starting
  * at line 1. */
int tqsl_getConverterLine(tQSL_Converter conv, int *lineno);

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* __tqslconvert_h */

