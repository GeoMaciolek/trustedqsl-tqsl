/***************************************************************************
                          adif.h  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by ARRL
    email                : MSimcik@localhost.localdomain
    revision             : $Id$
 ***************************************************************************/

#ifndef __ADIF_H
#define __ADIF_H

#include "tqsllib.h"

#include <stdio.h>
#include <stdlib.h>

/** \defgroup ADIF ADIF API
  *
  * These functions and data structures provide a means of parsing an ADIF
  * file into its component fields, along with range-checking the field contents.
  */
/** @{ */


#define TQSL_ADIF_FIELD_NAME_LENGTH_MAX 64
#define TQSL_ADIF_FIELD_SIZE_LENGTH_MAX 10
#define TQSL_ADIF_FIELD_TYPE_LENGTH_MAX 1

#ifndef TQSL_ADIF_BOOLEAN
typedef enum { TQSL_FALSE, TQSL_TRUE } TQSL_ADIF_BOOLEAN;
#endif

typedef void * tQSL_ADIF;

/// Specifies the type of range limits to apply to a field
typedef enum
{
	TQSL_ADIF_RANGE_TYPE_NONE,
	TQSL_ADIF_RANGE_TYPE_MINMAX,
	TQSL_ADIF_RANGE_TYPE_ENUMERATION
} TQSL_ADIF_RANGE_TYPE;

/// Response values returned from tqsl_getADIFField()
typedef enum
{
	TQSL_ADIF_GET_FIELD_SUCCESS,
	TQSL_ADIF_GET_FIELD_NO_NAME_MATCH,
	TQSL_ADIF_GET_FIELD_NO_TYPE_MATCH,
	TQSL_ADIF_GET_FIELD_NO_RANGE_MATCH,
	TQSL_ADIF_GET_FIELD_NO_ENUMERATION_MATCH,
	TQSL_ADIF_GET_FIELD_NO_RESULT_ALLOCATION,
	TQSL_ADIF_GET_FIELD_NAME_LENGTH_OVERFLOW,
	TQSL_ADIF_GET_FIELD_DATA_LENGTH_OVERFLOW,
	TQSL_ADIF_GET_FIELD_SIZE_OVERFLOW,
	TQSL_ADIF_GET_FIELD_TYPE_OVERFLOW,
	TQSL_ADIF_GET_FIELD_ERRONEOUS_STATE,
	TQSL_ADIF_GET_FIELD_EOF
} TQSL_ADIF_GET_FIELD_ERROR;

/** An ADIF field definition */
typedef struct
{
	char name[TQSL_ADIF_FIELD_NAME_LENGTH_MAX + 1];
	char type[TQSL_ADIF_FIELD_TYPE_LENGTH_MAX + 1];
	TQSL_ADIF_RANGE_TYPE rangeType;
	unsigned int max_length;
	long signed min_value;
	long signed max_value;
	const char **enumStrings;
	void *userPointer;
} tqsl_adifFieldDefinitions;

/** Field returned from parsing */
typedef struct
{
	char name[TQSL_ADIF_FIELD_NAME_LENGTH_MAX + 1];
	char size[TQSL_ADIF_FIELD_SIZE_LENGTH_MAX + 1];
	char type[TQSL_ADIF_FIELD_TYPE_LENGTH_MAX + 1];
	unsigned char *data;
	unsigned int adifNameIndex;
	void *userPointer;
} tqsl_adifFieldResults;


/* function prototypes */

#ifdef __cplusplus
extern "C" {
#endif

/** Get the ADIF error message that corresponds to a particular error value */
DLLEXPORT char *tqsl_adifGetError( TQSL_ADIF_GET_FIELD_ERROR status );

/** Initialize an ADIF file for reading */
DLLEXPORT int tqsl_beginADIF(tQSL_ADIF *adifp, const char *filename);

/** Get the next field from an ADIF file
  *
  * \li \c adif - ADIF handle returned from tqsl_beginADIF()
  * \li \c field - pointer to struct that contains the field data and description
  * \li \c status - pointer to returned status variable
  * \li \c adifFields - pointer to an array of field-definition structures. The last
  *    item in the array should have an empty string as its \c name member.
  * \li \c typesDefined - pointer to an array of char pointers that define the
  *    allowed field-type strings. The last item in the array should point to
  *    an empty string.
  * \li \c allocator - pointer to a function that returns a pointer to a memory
  *    block of the specified size. This function will be called at most one
  *    time during a call to tqsl_getADIFField. The returned pointer will then
  *    be used to populate the \c data member of \c field. The caller is
  *    responsible for freeing this memory, if needed.
  */
DLLEXPORT int tqsl_getADIFField(tQSL_ADIF adif, tqsl_adifFieldResults *field, TQSL_ADIF_GET_FIELD_ERROR *status,
	const tqsl_adifFieldDefinitions *adifFields, const char * const *typesDefined,
	unsigned char *(*allocator)(size_t) );

/** Get the current line number (starting from 1) of the input file */
DLLEXPORT int tqsl_getADIFLine(tQSL_ADIF adif, int *lineno);

/** End and release an ADIF file */
DLLEXPORT int tqsl_endADIF(tQSL_ADIF *adifp);

/** Form an ADIF field string.
  *
  * N.B. On systems that distinguish text-mode files from binary-mode files,
  * notably Windows, the text should be written in binary mode.
  */
DLLEXPORT int tqsl_adifMakeField(const char *fieldname, char type, const unsigned char *value, int len,
	unsigned char *buf, int buflen);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* __ADIF_H */
