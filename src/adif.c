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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "adif.h"


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
	while( ( 0 != *cs ) && ( 0 == ( result = ( toupper( *cs++ ) - toupper(*ct++) ) ) ) );

	return( result );
}


char *
adifGetError( ADIF_GET_FIELD_ERROR status )
{
	char *result;

	switch( status )
	{
		case ADIF_GET_FIELD_SUCCESS:
			result = "ADIF success";
			break;

		case ADIF_GET_FIELD_NO_NAME_MATCH:
			result = "ADIF field no name match";
			break;

		case ADIF_GET_FIELD_NO_TYPE_MATCH:
			result = "ADIF field no type match";
			break;

		case ADIF_GET_FIELD_NO_RANGE_MATCH:
			result = "ADIF field no range match";
			break;

		case ADIF_GET_FIELD_NO_ENUMERATION_MATCH:
			result = "ADIF field no enumeration match";
			break;

		case ADIF_GET_FIELD_NO_RESULT_ALLOCATION:
			result = "ADIF field no result allocation";
			break;

		case ADIF_GET_FIELD_NAME_LENGTH_OVERFLOW:
			result = "ADIF field name length overflow";
			break;

		case ADIF_GET_FIELD_DATA_LENGTH_OVERFLOW:
			result = "ADIF field data length overflow";
			break;

		case ADIF_GET_FIELD_SIZE_OVERFLOW:
			result = "ADIF field size overflow";
			break;

		case ADIF_GET_FIELD_TYPE_OVERFLOW:
			result = "ADIF field type overflow";
			break;

		case ADIF_GET_FIELD_ERRONEOUS_STATE:
			result = "ADIF erroneously executing default state";
			break;

		case ADIF_GET_FIELD_EOF:
			result = "ADIF reached End of File";
			break;

		default:
			result = "ADIF unknown error";
			break;
	}

	return( result );
};

ADIF_GET_FIELD_ERROR
adifGetField( adifFieldResults *field, FILE *filehandle, const adifFieldDefinitions *adifFields, const char *typesDefined[], unsigned char *(*allocator)(size_t) )
{
	ADIF_GET_FIELD_ERROR status;
	ADIF_STATE adifState;
	int currentCharacter;
	unsigned int iIndex;
	unsigned int dataLength;
	unsigned int dataIndex;
  const char blankString[] = "";
	ADIF_BOOLEAN recordData;
	signed long dataValue;


  /* get the next name value pair */
	status = ADIF_GET_FIELD_SUCCESS;
  adifState = ADIF_STATE_BEGIN;

	/* assume that we do not wish to record this data */
	recordData = FALSE;

  /* clear the field buffers */
  strcpy( field->name, blankString );
  strcpy( field->size, blankString );
  strcpy( field->type, blankString );
	field->data = NULL;
	field->adifNameIndex = 0;
	field->userPointer = NULL;

  while( adifState != ADIF_STATE_DONE )
 	{
 		if( EOF != ( currentCharacter = fgetc( filehandle ) ) )
    {
     	switch( adifState )
     	{
     		case ADIF_STATE_BEGIN:
   	 			/* GET STARTED */
   				/* find the field opening "<", ignoring everything else */
   				if( '<' == currentCharacter )
   				{
   					adifState = ADIF_STATE_GET_NAME;
  	      }
          						
   				break;

    		case ADIF_STATE_GET_NAME:
  	 			/* GET FIELD NAME */
   				/* add field name characters to buffer, until '>' or ':' found */
   				if( ( '>' == currentCharacter ) || ( ':' == currentCharacter ) )
					{
					 	/* find if the name is a match to a LoTW supported field name */
         		status = ADIF_GET_FIELD_NO_NAME_MATCH;
 						adifState = ADIF_STATE_GET_SIZE;

					 	for( iIndex = 0;
								 ( ADIF_GET_FIELD_NO_NAME_MATCH == status ) &&
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
								recordData = TRUE;
								status = ADIF_GET_FIELD_SUCCESS;

					 		}
       				if( '>' == currentCharacter )
       				{
       					adifState = ADIF_STATE_DONE;
      	 			}
						}
					}
   				else if( strlen( field->name ) < ADIF_FIELD_NAME_LENGTH_MAX )
   				{
  					/* add to field match string */
    				strCatChar( field->name, currentCharacter );
    			}
   				else
  	 			{
            status = ADIF_GET_FIELD_NAME_LENGTH_OVERFLOW;
  					adifState = ADIF_STATE_DONE;
   				}

  	 			break;

   			case ADIF_STATE_GET_SIZE:
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

						if( dataLength != 0 )
						{
     					if( ':' == currentCharacter )
     					{
								/* get the type */
     						adifState = ADIF_STATE_GET_TYPE;
     		      }
        			else
         	   	{
								/* no explicit type, set to LoTW default */
	           		strcpy( field->type, adifFields[( field->adifNameIndex )].type );
								/* get the data */
 								adifState = ADIF_STATE_GET_DATA;
     	     		}

							/* only allocate if we care about the data */
							if( recordData )
							{
      					if( dataLength <= adifFields[( field->adifNameIndex )].max_length )
   	  					{
	 	           		/* allocate space for data results, and ASCIIZ */
	    	         	if( NULL == ( field->data = (*allocator)( dataLength + 1 ) ) )
	   	         		{
		   	       			status = ADIF_GET_FIELD_NO_RESULT_ALLOCATION;
      							adifState = ADIF_STATE_DONE;
	   	  	      	}
   							}
    						else
    						{
            	 		status = 	ADIF_GET_FIELD_DATA_LENGTH_OVERFLOW;
      						adifState = ADIF_STATE_DONE;
    	 					}
							}
	          }
						else
						{
							/* no data, done */
  	 					adifState = ADIF_STATE_DONE;
						}
					}
     			else if( strlen( field->size ) < ADIF_FIELD_SIZE_LENGTH_MAX )
    	 		{
   					/* add to field size string */
   					strCatChar( field->size, currentCharacter );
   				}
  	 			else
   				{
            status = ADIF_GET_FIELD_SIZE_OVERFLOW;
 						adifState = ADIF_STATE_DONE;
   				}

   	 			break;

   			case ADIF_STATE_GET_TYPE:
   				/* GET FIELD TYPE */
     	  	/* get the number of characters in the value data */
  	 			if( '>' == currentCharacter )
   				{
         		/* check what type of field this is */
          	/* place default type in, if necessary */
           	if( 0 == strcmp( field->type, "" ) )
           	{
           		strcpy( field->type, adifFields[( field->adifNameIndex )].type );
   						adifState = ADIF_STATE_GET_DATA;
           	}
						else
						{
             	/* find if the type is a match to a LoTW supported data type */
         			status = ADIF_GET_FIELD_NO_TYPE_MATCH;
   						adifState = ADIF_STATE_DONE;
             	for( iIndex = 0;
			         		 ( ADIF_GET_FIELD_NO_TYPE_MATCH == status ) &&
									 ( 0 != strcmp( "", typesDefined[iIndex] ) );
									 iIndex++ )
             	{
								/* case insensitive compare */
               	if( 0 == strCmpNoCase( field->type, typesDefined[iIndex] ) )
               	{
             			status = ADIF_GET_FIELD_SUCCESS;
		   						adifState = ADIF_STATE_GET_DATA;
             		}
	          	}
						}
   				}
  	 			else if( strlen( field->type ) < ADIF_FIELD_TYPE_LENGTH_MAX )
   				{
    				/* add to field type string */
    				strCatChar( field->type, currentCharacter );
   	 			}
   				else
   				{
            status = ADIF_GET_FIELD_TYPE_OVERFLOW;
  					adifState = ADIF_STATE_DONE;
  				}

   	 			break;

   			case ADIF_STATE_GET_DATA:
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
					}
					else
   				{
   					adifState = ADIF_STATE_DONE;
   	 			}
   				break;

   			case ADIF_STATE_DONE:
   				/* DONE, should never get here */

   	 		default:
          status = ADIF_GET_FIELD_ERRONEOUS_STATE;
   				adifState = ADIF_STATE_DONE;
   				break;
   	 	}
   	}
 		else
 		{
 			status = ADIF_GET_FIELD_EOF;
 			adifState = ADIF_STATE_DONE;
 		}
 	}

 	if( ADIF_GET_FIELD_SUCCESS == status )
 	{
 		/* check data for enumeration match and range errors */
  	/* match enumeration */
 		switch( adifFields[( field->adifNameIndex )].rangeType )
  	{
   		case ADIF_RANGE_TYPE_NONE:
   			break;

   		case ADIF_RANGE_TYPE_MINMAX:
   			sscanf( (const char *)field->data, "%lu", &dataValue );

   			if( ( dataValue < adifFields[( field->adifNameIndex )].min_value ) ||
   					( dataValue > adifFields[( field->adifNameIndex )].max_value ) )
   			{
					status = ADIF_GET_FIELD_NO_RANGE_MATCH;
   			}

   			break;

   		case ADIF_RANGE_TYPE_ENUMERATION:
				status = ADIF_GET_FIELD_NO_ENUMERATION_MATCH;
     		for( iIndex = 0;
						 ( status == ADIF_GET_FIELD_NO_ENUMERATION_MATCH ) &&
						 ( 0 != strcmp( blankString, adifFields[( field->adifNameIndex )].enumStrings[iIndex] ) );
						 iIndex++ )
	   		{
					/* case insensitive compare */
     			if( 0 == strCmpNoCase( (const char *)field->data, adifFields[( field->adifNameIndex )].enumStrings[iIndex] ) )
    			{
  					status = ADIF_GET_FIELD_SUCCESS;
     			}
     		}

     		break;
   	}
  }

	return( status );
}
