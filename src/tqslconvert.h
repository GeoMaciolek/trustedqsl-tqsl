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
int tqsl_beginADIFConverter(tQSL_Converter *conv, const char *filename, tQSL_Cert *certs,
	int ncerts, tQSL_Location loc);

/** Initiates the conversion process for a Cabrillo file.
  *
  * \c certs and \c ncerts define a set of certificates that are available to the
  * converter for signing records. Typically, this list will be obtained by
  * calling tqsl_selectCertificates().
  *
  * tqsl_endConverter() should be called to free the resources when the conversion
  * is finished.
  */
int tqsl_beginCabrilloConverter(tQSL_Converter *conv, const char *filename, tQSL_Cert *certs,
	int ncerts, tQSL_Location loc);

/** End the conversion process by freeing the used resources. */
int tqsl_endConverter(tQSL_Converter *conv);

/** Configure the converter to allow (allow != 0) or disallow (allow == 0)
  * nonamateur call signs in the CALL field. (Note: the test for
  * validity is fairly trivial and will allow some nonamateur calls to
  * get through, but it does catch most common errors.)
  *
  * \c allow defaults to 0 when tqsl_beginADIFConverter or
  * tqsl_beginCabrilloConverter is called.
  */
int tqsl_setConverterAllowBadCall(tQSL_Converter conv, int allow);

/** Set QSO date filtering in the converter.
  *
  * If \c start points to a valid date, QSOs prior to that date will be ignored
  * by the converter. Similarly, if \c end points to a valid date, QSOs after
  * that date will be ignored. Either or both may be NULL (or point to an
  * invalid date) to disable date filtering for the respective range.
  */
int tqsl_setADIFConverterDateFilter(tQSL_Converter conv, tQSL_Date *start, tQSL_Date *end);

/** This is the main converter function. It returns a single GABBI
  * record.
  *
  * Returns the NULL pointer on error or EOF. (Test tQSL_Error to determine which.)
  *
  * tQSL_Error is set to TQSL_DATE_OUT_OF_RANGE if QSO date range checking
  * is active (see ::tqsl_useADIFConverterDateFilter) and the QSO date is
  * outside the specified range. This is a non-fatal error.
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

/** Get the text of the last record read by the converter.
  *
  * Returns NULL on error.
  */
const char *tqsl_getConverterRecordText(tQSL_Converter conv);

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* __tqslconvert_h */

