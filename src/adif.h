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

#include <stdio.h>
#include <stdlib.h>

#define ADIF_FIELD_NAME_LENGTH_MAX 64
#define ADIF_FIELD_SIZE_LENGTH_MAX 10
#define ADIF_FIELD_TYPE_LENGTH_MAX 1

#ifndef ADIF_BOOLEAN
typedef enum { FALSE, TRUE } ADIF_BOOLEAN;
#endif


typedef enum
{
	ADIF_STATE_BEGIN,
	ADIF_STATE_GET_NAME,
	ADIF_STATE_GET_SIZE,
	ADIF_STATE_GET_TYPE,
	ADIF_STATE_GET_DATA,
	ADIF_STATE_DONE
}  ADIF_STATE;

typedef enum
{
	ADIF_RANGE_TYPE_NONE,
	ADIF_RANGE_TYPE_MINMAX,
	ADIF_RANGE_TYPE_ENUMERATION
} ADIF_RANGE_TYPE;

typedef enum
{
	ADIF_GET_FIELD_SUCCESS,
	ADIF_GET_FIELD_NO_NAME_MATCH,
	ADIF_GET_FIELD_NO_TYPE_MATCH,
	ADIF_GET_FIELD_NO_RANGE_MATCH,
	ADIF_GET_FIELD_NO_ENUMERATION_MATCH,
	ADIF_GET_FIELD_NO_RESULT_ALLOCATION,
	ADIF_GET_FIELD_NAME_LENGTH_OVERFLOW,
	ADIF_GET_FIELD_DATA_LENGTH_OVERFLOW,
	ADIF_GET_FIELD_SIZE_OVERFLOW,
	ADIF_GET_FIELD_TYPE_OVERFLOW,
	ADIF_GET_FIELD_ERRONEOUS_STATE,
	ADIF_GET_FIELD_EOF
} ADIF_GET_FIELD_ERROR;

typedef struct
{
	char name[ADIF_FIELD_NAME_LENGTH_MAX + 1];
	char type[ADIF_FIELD_TYPE_LENGTH_MAX + 1];
	ADIF_RANGE_TYPE rangeType;
	unsigned int max_length;
	long signed min_value;
	long signed max_value;
	char **enumStrings;
	void *userPointer;
} adifFieldDefinitions;

typedef struct
{
	char name[ADIF_FIELD_NAME_LENGTH_MAX + 1];
	char size[ADIF_FIELD_SIZE_LENGTH_MAX + 1];
	char type[ADIF_FIELD_TYPE_LENGTH_MAX + 1];
	unsigned char *data;
	unsigned int adifNameIndex;
	void *userPointer;
} adifFieldResults;


/* function prototypes */

#ifdef __cplusplus
extern "C" {
#endif

char *
adifGetError( ADIF_GET_FIELD_ERROR status );

ADIF_GET_FIELD_ERROR
adifGetField( adifFieldResults *field, FILE *filehandle, const adifFieldDefinitions *adifFields, const char *typesDefined[], unsigned char *(*allocator)(size_t) );

#ifdef __cplusplus
}
#endif

#endif /* __ADIF_H */
