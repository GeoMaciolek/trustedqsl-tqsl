/***************************************************************************
                          adif.c  -  description
                             -------------------
    begin                : Wed May 15 2002
    copyright            : (C) 2002 by ARRL
    email                : MSimcik@localhost.localdomain
    revision             : $Id$
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "sysconfig.h"
#endif

#define TQSLLIB_DEF

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "tqsllib.h"
#include "tqslerrno.h"

typedef enum
{
	TQSL_ADIF_STATE_BEGIN,
	TQSL_ADIF_STATE_GET_NAME,
	TQSL_ADIF_STATE_GET_SIZE,
	TQSL_ADIF_STATE_GET_TYPE,
	TQSL_ADIF_STATE_GET_DATA,
	TQSL_ADIF_STATE_DONE
}  TQSL_ADIF_STATE;

struct TQSL_ADIF {
	int sentinel;
	FILE *fp;
	char *filename;
	int line_no;
};

#define CAST_TQSL_ADIF(p) ((struct TQSL_ADIF *)p)

static TQSL_ADIF *
check_adif(tQSL_ADIF adif) {
	if (tqsl_init())
		return 0;
	if (adif == 0)
		return 0;
	if (CAST_TQSL_ADIF(adif)->sentinel != 0x3345)
		return 0;
	return CAST_TQSL_ADIF(adif);
}

static void
free_adif(TQSL_ADIF *adif) {
	if (adif && adif->sentinel == 0x3345) {
		adif->sentinel = 0;
		if (adif->filename)
			free(adif->filename);
		if (adif->fp)
			fclose(adif->fp);
		free(adif);
	}
}
	
DLLEXPORT int
tqsl_beginADIF(tQSL_ADIF *adifp, const char *filename) {
	if (filename == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	struct TQSL_ADIF *adif;
	adif = (struct TQSL_ADIF *)calloc(1, sizeof (struct TQSL_ADIF));
	if (adif == NULL) {
		tQSL_Error = TQSL_ALLOC_ERROR;
		goto err;
	}
	adif->sentinel = 0x3345;
	if ((adif->fp = fopen(filename, "rb")) == NULL) {
		tQSL_Error = TQSL_SYSTEM_ERROR;
		tQSL_Errno = errno;
		strncpy(tQSL_ErrorFile, filename, sizeof tQSL_ErrorFile);
		tQSL_ErrorFile[sizeof tQSL_ErrorFile-1] = 0;
		goto err;
	}
	if ((adif->filename = (char *)malloc(strlen(filename)+1)) == NULL) {
		tQSL_Error = TQSL_ALLOC_ERROR;
		goto err;
	}
	strcpy(adif->filename, filename);
	*((struct TQSL_ADIF **)adifp) = adif;
	return 0;
err:
	free_adif(adif);
	return 1;

}

DLLEXPORT int
tqsl_endADIF(tQSL_ADIF *adifp) {
	TQSL_ADIF *adif;
	if (adifp == 0)
		return 0;
	adif = CAST_TQSL_ADIF(*adifp);
	free_adif(adif);
	*adifp = 0;
	return 0;
}

DLLEXPORT int
tqsl_getADIFLine(tQSL_ADIF adifp, int *lineno) {
	TQSL_ADIF *adif;
	if (!(adif = check_adif(adifp)))
		return 1;
	if (lineno == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*lineno = adif->line_no;
	return 0;
}

static
void
strCatChar( char *string, int character );

static
void
strCatChar( char *string, int character )
{
	char charBuf[1];

	/* convert integer to char */
	charBuf[0] = (char)character;

	/* concatenate the character */
 	strncat( string, charBuf, 1 );
}


static
int
strCmpNoCase( const char *cs, const char *ct );

static
int
strCmpNoCase( const char *cs, const char *ct )
{
	int result;

	result = 0;

	/* until ASCIIZ terminator, compares as if cs & ct were all UPPER CASE */
	while( ( 0 == ( result = ( toupper( *cs ) - toupper( *ct ) ) ) ) && ( 0 != *cs ) )
	{
		cs++;
		ct++;
	}

	return( result );
}


DLLEXPORT const char *
tqsl_adifGetError( TQSL_ADIF_GET_FIELD_ERROR status )
{
	const char *result;

	switch( status )
	{
		case TQSL_ADIF_GET_FIELD_SUCCESS:
			result = (char *) "ADIF success";
			break;

		case TQSL_ADIF_GET_FIELD_NO_NAME_MATCH:
			result = (char *) "ADIF field no name match";
			break;

		case TQSL_ADIF_GET_FIELD_NO_TYPE_MATCH:
			result = (char *) "ADIF field no type match";
			break;

		case TQSL_ADIF_GET_FIELD_NO_RANGE_MATCH:
			result = (char *) "ADIF field no range match";
			break;

		case TQSL_ADIF_GET_FIELD_NO_ENUMERATION_MATCH:
			result = (char *) "ADIF field no enumeration match";
			break;

		case TQSL_ADIF_GET_FIELD_NO_RESULT_ALLOCATION:
			result = (char *) "ADIF field no result allocation";
			break;

		case TQSL_ADIF_GET_FIELD_NAME_LENGTH_OVERFLOW:
			result = (char *) "ADIF field name length overflow";
			break;

		case TQSL_ADIF_GET_FIELD_DATA_LENGTH_OVERFLOW:
			result = (char *) "ADIF field data length overflow";
			break;

		case TQSL_ADIF_GET_FIELD_SIZE_OVERFLOW:
			result = (char *) "ADIF field size overflow";
			break;

		case TQSL_ADIF_GET_FIELD_TYPE_OVERFLOW:
			result = (char *) "ADIF field type overflow";
			break;

		case TQSL_ADIF_GET_FIELD_ERRONEOUS_STATE:
			result = (char *) "ADIF erroneously executing default state";
			break;

		case TQSL_ADIF_GET_FIELD_EOF:
			result = (char *) "ADIF reached End of File";
			break;

		default:
			result = (char *) "ADIF unknown error";
			break;
	}

	return( result );
};

static TQSL_ADIF_GET_FIELD_ERROR
tqsl_adifGetField( tqsl_adifFieldResults *field, FILE *filehandle,
	const tqsl_adifFieldDefinitions *adifFields,
	const char * const *typesDefined, unsigned char *(*allocator)(size_t), int *line_no )
{
	TQSL_ADIF_GET_FIELD_ERROR status;
	TQSL_ADIF_STATE adifState;
	int currentCharacter;
	unsigned int iIndex;
	unsigned int dataLength;
	unsigned int dataIndex = 0;
  const char blankString[] = "";
	TQSL_ADIF_BOOLEAN recordData;
	signed long dataValue;


  /* get the next name value pair */
	status = TQSL_ADIF_GET_FIELD_SUCCESS;
  adifState = TQSL_ADIF_STATE_BEGIN;

	/* assume that we do not wish to record this data */
	recordData = TQSL_FALSE;

  /* clear the field buffers */
  strcpy( field->name, blankString );
  strcpy( field->size, blankString );
  strcpy( field->type, blankString );
	field->data = NULL;
	field->adifNameIndex = 0;
	field->userPointer = NULL;

  while( adifState != TQSL_ADIF_STATE_DONE )
 	{
 		if( EOF != ( currentCharacter = fgetc( filehandle ) ) )
    {
			if (*line_no == 0)
				*line_no = 1;
			if (currentCharacter == '\n')
				(*line_no)++;
     	switch( adifState )
     	{
     		case TQSL_ADIF_STATE_BEGIN:
   	 			/* GET STARTED */
   				/* find the field opening "<", ignoring everything else */
   				if( '<' == currentCharacter )
   				{
   					adifState = TQSL_ADIF_STATE_GET_NAME;
  	      }
          						
   				break;

    		case TQSL_ADIF_STATE_GET_NAME:
  	 			/* GET FIELD NAME */
   				/* add field name characters to buffer, until '>' or ':' found */
   				if( ( '>' == currentCharacter ) || ( ':' == currentCharacter ) )
					{
					 	/* find if the name is a match to a LoTW supported field name */
         		status = TQSL_ADIF_GET_FIELD_NO_NAME_MATCH;
 						adifState = TQSL_ADIF_STATE_GET_SIZE;

					 	for( iIndex = 0;
								 ( TQSL_ADIF_GET_FIELD_NO_NAME_MATCH == status ) &&
								 ( 0 != strcmp( blankString, adifFields[iIndex].name ) );
								 iIndex++ )
					 	{
							/* case insensitive compare */
					   	if( 0 == strCmpNoCase( field->name, adifFields[iIndex].name ) )
					   	{
								/* set name index */
								field->adifNameIndex = iIndex;

								/* copy user pointer */
								field->userPointer = adifFields[iIndex].userPointer;

								/* since we know the name, record the data */
								recordData = TQSL_TRUE;
								status = TQSL_ADIF_GET_FIELD_SUCCESS;

					 		}
       				if( '>' == currentCharacter )
       				{
       					adifState = TQSL_ADIF_STATE_DONE;
      	 			}
						}
					}
   				else if( strlen( field->name ) < TQSL_ADIF_FIELD_NAME_LENGTH_MAX )
   				{
  					/* add to field match string */
    				strCatChar( field->name, currentCharacter );
    			}
   				else
  	 			{
            status = TQSL_ADIF_GET_FIELD_NAME_LENGTH_OVERFLOW;
  					adifState = TQSL_ADIF_STATE_DONE;
   				}

  	 			break;

   			case TQSL_ADIF_STATE_GET_SIZE:
   				/* GET FIELD SIZE */
  	 			/* adding field size characters to buffer, until ':' or '>' found */
   				if( ( ':' == currentCharacter ) || ( '>' == currentCharacter ) )
					{
						/* reset data copy offset */
						dataIndex = 0;

						/* see if any size was read in */
						if( 0 != strcmp( blankString, field->size ) )
						{
     					/* convert data size to integer */
    	 				sscanf( field->size, "%i", &dataLength );
						}
						else
						{
							dataLength = 0;
						}

   					if( ':' == currentCharacter )
   					{
							/* get the type */
   						adifState = TQSL_ADIF_STATE_GET_TYPE;
   		      }
      			else
       	   	{
							/* no explicit type, set to LoTW default */
           		strcpy( field->type, adifFields[( field->adifNameIndex )].type );
							/* get the data */
 							adifState = dataLength == 0 ? TQSL_ADIF_STATE_DONE : TQSL_ADIF_STATE_GET_DATA;
   	     		}

						/* only allocate if we care about the data */
						if( recordData )
						{
    					if( dataLength <= adifFields[( field->adifNameIndex )].max_length )
 	  					{
 	           		/* allocate space for data results, and ASCIIZ */
    	         	if( NULL != ( field->data = (*allocator)( dataLength + 1 ) ) )
								{
									/* ASCIIZ terminator */
			  	   	 		field->data[dataIndex] = 0;
								}
								else
   	         		{
	   	       			status = TQSL_ADIF_GET_FIELD_NO_RESULT_ALLOCATION;
    							adifState = TQSL_ADIF_STATE_DONE;
   	  	      	}
 							}
  						else
  						{
          	 		status = 	TQSL_ADIF_GET_FIELD_DATA_LENGTH_OVERFLOW;
    						adifState = TQSL_ADIF_STATE_DONE;
  	 					}
						}
					}
     			else if( strlen( field->size ) < TQSL_ADIF_FIELD_SIZE_LENGTH_MAX )
    	 		{
   					/* add to field size string */
   					strCatChar( field->size, currentCharacter );
   				}
  	 			else
   				{
            status = TQSL_ADIF_GET_FIELD_SIZE_OVERFLOW;
 						adifState = TQSL_ADIF_STATE_DONE;
   				}

   	 			break;

   			case TQSL_ADIF_STATE_GET_TYPE:
   				/* GET FIELD TYPE */
     	  	/* get the number of characters in the value data */
  	 			if( '>' == currentCharacter )
   				{
         		/* check what type of field this is */
          	/* place default type in, if necessary */
           	if( 0 == strcmp( field->type, "" ) )
           	{
           		strcpy( field->type, adifFields[( field->adifNameIndex )].type );
 							adifState = dataLength == 0 ? TQSL_ADIF_STATE_DONE : TQSL_ADIF_STATE_GET_DATA;
           	}
						else
						{
             	/* find if the type is a match to a LoTW supported data type */
         			status = TQSL_ADIF_GET_FIELD_NO_TYPE_MATCH;
   						adifState = TQSL_ADIF_STATE_DONE;
             	for( iIndex = 0;
			         		 ( TQSL_ADIF_GET_FIELD_NO_TYPE_MATCH == status ) &&
									 ( 0 != strcmp( "", typesDefined[iIndex] ) );
									 iIndex++ )
             	{
								/* case insensitive compare */
               	if( 0 == strCmpNoCase( field->type, typesDefined[iIndex] ) )
               	{
             			status = TQSL_ADIF_GET_FIELD_SUCCESS;
		 							adifState = dataLength == 0 ? TQSL_ADIF_STATE_DONE : TQSL_ADIF_STATE_GET_DATA;
             		}
	          	}
						}
   				}
  	 			else if( strlen( field->type ) < TQSL_ADIF_FIELD_TYPE_LENGTH_MAX )
   				{
    				/* add to field type string */
    				strCatChar( field->type, currentCharacter );
   	 			}
   				else
   				{
            status = TQSL_ADIF_GET_FIELD_TYPE_OVERFLOW;
  					adifState = TQSL_ADIF_STATE_DONE;
  				}

   	 			break;

   			case TQSL_ADIF_STATE_GET_DATA:
   				/* GET DATA */
     			/* read in the prescribed number of characters to form the value */
   				if( 0 != dataLength-- )
					{
						/* only record if we care about the data */
						if( recordData )
						{
							/* ASCIIZ copy that is tolerant of binary data too */
		     	 		field->data[dataIndex++] = (unsigned char)currentCharacter;
	  	   	 		field->data[dataIndex] = 0;
						}
						if (0 == dataLength)
	   					adifState = TQSL_ADIF_STATE_DONE;
					}
					else
   				{
   					adifState = TQSL_ADIF_STATE_DONE;
   	 			}
   				break;

   			case TQSL_ADIF_STATE_DONE:
   				/* DONE, should never get here */

   	 		default:
          status = TQSL_ADIF_GET_FIELD_ERRONEOUS_STATE;
   				adifState = TQSL_ADIF_STATE_DONE;
   				break;
   	 	}
   	}
 		else
 		{
 			status = TQSL_ADIF_GET_FIELD_EOF;
 			adifState = TQSL_ADIF_STATE_DONE;
 		}
 	}

 	if( TQSL_ADIF_GET_FIELD_SUCCESS == status )
 	{
 		/* check data for enumeration match and range errors */
  	/* match enumeration */
 		switch( adifFields[( field->adifNameIndex )].rangeType )
  	{
   		case TQSL_ADIF_RANGE_TYPE_NONE:
   			break;

   		case TQSL_ADIF_RANGE_TYPE_MINMAX:
   			sscanf( (const char *)field->data, "%lu", &dataValue );

   			if( ( dataValue < adifFields[( field->adifNameIndex )].min_value ) ||
   					( dataValue > adifFields[( field->adifNameIndex )].max_value ) )
   			{
					status = TQSL_ADIF_GET_FIELD_NO_RANGE_MATCH;
   			}

   			break;

   		case TQSL_ADIF_RANGE_TYPE_ENUMERATION:
				status = TQSL_ADIF_GET_FIELD_NO_ENUMERATION_MATCH;
     		for( iIndex = 0;
						 ( status == TQSL_ADIF_GET_FIELD_NO_ENUMERATION_MATCH ) &&
						 ( 0 != strcmp( blankString, adifFields[( field->adifNameIndex )].enumStrings[iIndex] ) );
						 iIndex++ )
	   		{
					/* case insensitive compare */
     			if( 0 == strCmpNoCase( (const char *)field->data, adifFields[( field->adifNameIndex )].enumStrings[iIndex] ) )
    			{
  					status = TQSL_ADIF_GET_FIELD_SUCCESS;
     			}
     		}

     		break;
   	}
  }

	return( status );
}

DLLEXPORT int
tqsl_getADIFField(tQSL_ADIF adifp, tqsl_adifFieldResults *field, TQSL_ADIF_GET_FIELD_ERROR *status,
	const tqsl_adifFieldDefinitions *adifFields, const char * const *typesDefined,
	unsigned char *(*allocator)(size_t) ) {
	TQSL_ADIF *adif;
	if (!(adif = check_adif(adifp)))
		return 1;
	if (field == NULL || status == NULL || adifFields == NULL || typesDefined == NULL
		|| allocator == NULL) {
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	*status = tqsl_adifGetField(field, adif->fp, adifFields, typesDefined, allocator, &(adif->line_no));
	return 0;
}

static unsigned char *
tqsl_condx_copy(const unsigned char *src, int slen, unsigned char *dest, int *len) {
	if (slen == 0)
		return dest;
	if (slen < 0)
		slen = strlen((const char *)src);
	if (*len < slen) {
		tQSL_Error = TQSL_BUFFER_ERROR;
		return NULL;
	}
	memcpy(dest, src, slen);
	*len -= slen;
	return dest+slen;
}

/* Output an ADIF field to a file descriptor.
 */
DLLEXPORT int
tqsl_adifMakeField(const char *fieldname, char type, const unsigned char *value, int len,
	unsigned char *buf, int buflen) {
	if (fieldname == NULL || buf == NULL || buflen <= 0) {	/* Silly caller */
		tQSL_Error = TQSL_ARGUMENT_ERROR;
		return 1;
	}
	unsigned char *cp;
	if ((cp = tqsl_condx_copy((const unsigned char *)"<", 1, buf, &buflen)) == NULL)
		return 1;
	if ((cp = tqsl_condx_copy((const unsigned char *)fieldname, -1, cp, &buflen)) == NULL)
		return 1;
	if (value != NULL && len < 0)
		len = strlen((const char *)value);
	if (value != NULL && len != 0) {
		char nbuf[20];
		if ((cp = tqsl_condx_copy((const unsigned char *)":", 1, cp, &buflen)) == NULL)
			return 1;
		sprintf(nbuf, "%d", len);
		if ((cp = tqsl_condx_copy((const unsigned char *)nbuf, -1, cp, &buflen)) == NULL)
			return 1;
		if (type && type != ' ' && type != '\0') {
			if ((cp = tqsl_condx_copy((const unsigned char *)":", 1, cp, &buflen)) == NULL)
				return 1;
			if ((cp = tqsl_condx_copy((const unsigned char *)&type, 1, cp, &buflen)) == NULL)
				return 1;
		}
		if ((cp = tqsl_condx_copy((const unsigned char *)">", 1, cp, &buflen)) == NULL)
			return 1;
		if ((cp = tqsl_condx_copy(value, len, cp, &buflen)) == NULL)
			return 1;
	} else if ((cp = tqsl_condx_copy((const unsigned char *)">", 1, cp, &buflen)) == NULL)
		return 1;
	if ((cp = tqsl_condx_copy((const unsigned char *)"", 1, cp, &buflen)) == NULL)
		return 1;
	return 0;
}
