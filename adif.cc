#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "adif.h"

static char cvsID[] = "$Id$";
void AdifBase::init()
{
  address=NULL;
  age=NULL;
  arrlSect=NULL;
  band=NULL;
  call=NULL;
  cnty=NULL;
  comment=NULL;
  cont=NULL;
  contestId=NULL;
  cqz=NULL;
  dxcc=NULL;
  freq=NULL;
  gridSquare=NULL;
  iota=NULL;
  ituz=NULL;
  mode=NULL;
  name=NULL;
  notes=NULL;
  oper=NULL;
  pfx=NULL;
  propMode=NULL;
  qslMsg=NULL;
  qslRDate=NULL;
  qslSDate=NULL;
  qslRcvd=NULL;
  qslSent=NULL;
  qslVia=NULL;
  qsoDate=NULL;
  qth=NULL;
  rstRcvd=NULL;
  rstSent=NULL;
  rxPwr=NULL;
  satMode=NULL;
  satName=NULL;
  srx=NULL;
  stx=NULL;
  tenTen=NULL;
  timeOff=NULL;
  timeOn=NULL;
  txPwr=NULL;
  veProv=NULL;
};
void AdifBase::clearQSO()
{

  // need to free memory first

  init();

  // these must be changed to free if null
  delete address;
  delete age;
  delete arrlSect;
  band=NULL;
  call=NULL;
  cnty=NULL;
  comment=NULL;
  cont=NULL;
  contestId=NULL;
  cqz=NULL;
  dxcc=NULL;
  freq=NULL;
  gridSquare=NULL;
  iota=NULL;
  ituz=NULL;
  mode=NULL;
  name=NULL;
  notes=NULL;
  oper=NULL;
  pfx=NULL;
  propMode=NULL;
  qslMsg=NULL;
  qslRDate=NULL;
  qslSDate=NULL;
  qslRcvd=NULL;
  qslSent=NULL;
  qslVia=NULL;
  qsoDate=NULL;
  qth=NULL;
  rstRcvd=NULL;
  rstSent=NULL;
  rxPwr=NULL;
  satMode=NULL;
  satName=NULL;
  srx=NULL;
  stx=NULL;
  tenTen=NULL;
  timeOff=NULL;
  timeOn=NULL;
  txPwr=NULL;
  veProv=NULL;
};

AdifBase::AdifBase()
{
  cvsID = cvsID;

  clearQSO();
  privKey=NULL;
  myCert=NULL;
  signature=NULL;
}

// return value will point to the beginning of the next line
// and str will be the current line with EOL striped and null termated
//
char *AdifBase::nextLine(char *str)
{

  char		*rcStr;  //return String;
  char 		*a;

  
  a = strstr(str,"\r\n");  // windows format
  if (a == NULL)
    {
      a = strchr(str,'\n');  // Unix format
      if ( a == NULL)
	{
	  a = strchr(str,'\r');  // Mac format
	  if (a == NULL)
	    return(NULL);  // found no EOL marks
	}
      *a = '\0';
      a++;
    }
  else
    {
      *a = '\0';
      a = a + 2;  // should beginning of next line
    }
  //printf("%20.20s\n",str);
  return(a);
  
}
char *AdifBase::AdifParseNext(char *str)
{

  char		*rcStr;  //return String;
  char 		*nextP;
  char		*eor;
  char		buf[2000];
 
  nextP = nextLine(str);
  while (nextP != NULL)
    {
      eor = strstr(str,"<EOR>");
      if (eor == NULL)
	{
	  str = nextP;
	  nextP = nextLine(str);
	  continue;
	}
      // the above drill was to find out if this was a QSO record

      *eor = '\0';
      break;
    }

  char 	*endTag;
  char 	*value;
  char 	*vName;
  char 	*valueLen;
  int 	len;
  char	valueStr[1000];


  while(str != NULL)
    {
      endTag = strchr(str,'>');
      if (endTag == NULL)
	return(nextP);

      *endTag = '\0';  // so endTag looks like "<CALL:4" because we nulled >
      vName = str+1;

      valueLen = strchr(vName,':');
      *valueLen = '\0';
      valueLen++;

      value = endTag + 1;
      len = atol(valueLen);
      memcpy(valueStr,value,len);
      valueStr[len] = '\0';

      str = value + len + 1;

      //printf("%s: %s\n",name, valueStr);

      // set up for next field
      str = strchr(str,'<');

      // pick out the fields

      if (strcastcmp("CALL",vName) == 0)
	{
	  call = strdup(vName);
	  continue;
	}

      if (strcastcmp("AGE",vName) == 0)
	{
	  age = strdup(vName);
	  continue;
	}
	
      if (strcastcmp("ADDRESS",vName) == 0)
	{
	  address = strdup(vName);
	  continue;
	}  

      if (strcastcmp("ARRLSECT",vName) == 0)
	{
	  arrlSect = strdup(vName);
	  continue;
	}


      if (strcastcmp("BAND",vName) == 0)
	{
	  band = strdup(vName);
	  continue;
	}

      if (strcastcmp("CNTY",vName) == 0)
	{
	  cnty = strdup(vName);
	  continue;
	}


      if (strcastcmp("COMMENT",vName) == 0)
	{
	  comment = strdup(vName);
	  continue;
	}

      if (strcastcmp("CONT",vName) == 0)
	{
	  cont = strdup(vName);
	  continue;
	}

      if (strcastcmp("CONTEST_ID",vName) == 0)
	{
	  contestId = strdup(vName);
	  continue;
	}


      if (strcastcmp("CQZ",vName) == 0)
	{
	  cqz = strdup(vName);
	  continue;
	}

      if (strcastcmp("DXCC",vName) == 0)
	{
	  dxcc = strdup(vName);
	  continue;
	}

      if (strcastcmp("FREQ",vName) == 0)
	{
	  freq = strdup(vName);
	  continue;
	}

      if (strcastcmp("GRID_SQUARE",vName) == 0)
	{
	  gridSquare = strdup(vName);
	  continue;
	}

      if (strcastcmp("IOTA",vName) == 0)
	{
	  iota = strdup(vName);
	  continue;
	}

      if (strcastcmp("ITUZ",vName) == 0)
	{
	  ituz = strdup(vName);
	  continue;
	}

      if (strcastcmp("MODE",vName) == 0)
	{
	  mode = strdup(vName);
	  continue;
	}

      if (strcastcmp("NAME",vName) == 0)
	{
	  name = strdup(vName);
	  continue;
	}


      if (strcastcmp("NOTES",vName) == 0)
	{
	  notes = strdup(vName);
	  continue;
	}

      if (strcastcmp("OPERATOR",vName) == 0)
	{
	  oper = strdup(vName);
	  continue;
	}

      if (strcastcmp("PFX",vName) == 0)
	{
	  pfx = strdup(vName);
	  continue;
	}

      if (strcastcmp("PROP_MODE",vName) == 0)
	{
	  propMode = strdup(vName);
	  continue;
	}

      if (strcastcmp("qslMsg",vName) == 0)
	{
	  qslMsg = strdup(vName);
	  continue;
	}

      if (strcastcmp("QSL_R_DATE",vName) == 0)
	{
	  qslRDate = strdup(vName);
	  continue;
	}

      if (strcastcmp("QSL_S_DATE",vName) == 0)
	{
	  qslSDate = strdup(vName);
	  continue;
	}

      if (strcastcmp("QSL_RCVD",vName) == 0)
	{
	  qslRcvd = strdup(vName);
	  continue;
	}

      if (strcastcmp("QSL_SENT",vName) == 0)
	{
	  qslSent = strdup(vName);
	  continue;
	}

      if (strcastcmp("QSL_VIA",vName) == 0)
	{
	  qslVia = strdup(vName);
	  continue;
	}

      if (strcastcmp("QSO_DATE",vName) == 0)
	{
	  qslDate = strdup(vName);
	  continue;
	}


      if (strcastcmp("QTH",vName) == 0)
	{
	  qtz = strdup(vName);
	  continue;
	}


      if (strcastcmp("RST_RCVD",vName) == 0)
	{
	  rstRcvd = strdup(vName);
	  continue;
	}


      if (strcastcmp("RST_SENT",vName) == 0)
	{
	  rstSent = strdup(vName);
	  continue;
	}

      if (strcastcmp("RX_PWR",vName) == 0)
	{
	  rxPwr = strdup(vName);
	  continue;
	}


      if (strcastcmp("SAT_MODE",vName) == 0)
	{
	  satMode = strdup(vName);
	  continue;
	}


      if (strcastcmp("SAT_NAME",vName) == 0)
	{
	  satName = strdup(vName);
	  continue;
	}


      if (strcastcmp("SRX",vName) == 0)
	{
	  srx = strdup(vName);
	  continue;
	}

      if (strcastcmp("STX",vName) == 0)
	{
	  stx = strdup(vName);
	  continue;
	}


      if (strcastcmp("TENTEN",vName) == 0)
	{
	  tenTen = strdup(vName);
	  continue;
	}

      if (strcastcmp("TIME_OFF",vName) == 0)
	{
	  timeOff = strdup(vName);
	  continue;
	}


      if (strcastcmp("TIME_ON",vName) == 0)
	{
	  timeOn = strdup(vName);
	  continue;
	}


      if (strcastcmp("TX_PWR",vName) == 0)
	{
	  txPwr = strdup(vName);
	  continue;
	}


      if (strcastcmp("VE_PROV",vName) == 0)
	{
	  veProv = strdup(vName);
	  continue;
	}
    }


  //  printf("strlen: %d\n",strlen(str));
  return(nextP);
  
  


}
