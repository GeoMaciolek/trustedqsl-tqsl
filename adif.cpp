/* 
    TrustedQSL/LoTW client and PKI Library
    Copyright (C) 2001  Darryl Wagoner WA1GON wa1gon@arrl.net and
                     ARRL

    This library is free software; you can redistribute it and/or
    modify it under the terms of the Open License packaged with
    the source.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

    You should have received a copy of the Open License see 
    http://www.trustedQSL.org/ or contact wa1gon@arrl.net.

*/

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
  state=NULL;
  stx=NULL;
  tenTen=NULL;
  timeOff=NULL;
  timeOn=NULL;
  txPwr=NULL;
  veProv=NULL;

  // tqsl data members

  privKey=NULL;
  myCert=NULL;
  signature=NULL;
};
void AdifBase::clearQSO()
{

  // these must be changed to free if null
  delete address;
  delete age;
  delete arrlSect;
  delete band;
  delete call;
  delete cnty;
  delete comment;
  delete cont;
  delete contestId;
  delete cqz;
  delete dxcc;
  delete freq;
  delete gridSquare;
  delete iota;
  delete ituz;
  delete mode;
  delete name;
  delete notes;
  delete oper;
  delete pfx;
  delete propMode;
  delete qslMsg;
  delete qslRDate;
  delete qslSDate;
  delete qslRcvd;
  delete qslSent;
  delete qslVia;
  delete qsoDate;
  delete qth;
  delete rstRcvd;
  delete rstSent;
  delete rxPwr;
  delete satMode;
  delete satName;
  delete srx;
  delete state;
  delete stx;
  delete tenTen;
  delete timeOff;
  delete timeOn;
  delete txPwr;
  delete veProv;
  // tqsl data members
  delete privKey;
  delete myCert;
  delete signature;
  
  // need to free memory first

  init();
};

AdifBase::AdifBase()
{
  cvsID = cvsID;

  init();

}

void AdifBase::dump()
{
  if (address)
    printf("<ADDRESS:%d>%s\n",strlen(address),address);


  if (age)
    printf("<AGE:%d>%s\n",strlen(age),age);

  if (arrlSect)
    printf("<ARRL_SECT:%d>%s\n",strlen(arrlSect),arrlSect);

  if (band)
    printf("<BAND:%d>%s\n",strlen(band),band);

  if (call)
    printf("<CALL:%d>%s\n",strlen(call),call);

  if (cnty)
    printf("<CNTY:%d>%s\n",strlen(cnty),cnty);

  if (comment)
    printf("<COMMENT:%d>%s\n",strlen(comment),comment);

  if (cont)
    printf("<CONT:%d>%s\n",strlen(cont),cont);

  if (contestId)
    printf("<CONTEST_ID:%d>%s\n",strlen(contestId),contestId);

  if (cqz)
    printf("<CQZ:%d>%s\n",strlen(cqz),cqz);

  if (dxcc)
    printf("<DXCC:%d>%s\n",strlen(dxcc),dxcc);

  if (freq)
    printf("<FREQ:%d>%s\n",strlen(freq),freq);

  if (gridSquare)
    printf("<GRIDSQUARE:%d>%s\n",strlen(gridSquare),gridSquare);

  if (iota)
    printf("<IOTA:%d>%s\n",strlen(iota),iota);

  if (ituz)
    printf("<ITUZ:%d>%s\n",strlen(ituz),ituz);

  if (mode)
    printf("<MODE:%d>%s\n",strlen(mode),mode);

  if (name)
    printf("<NAME:%d>%s\n",strlen(name),name);

  if (notes)
    printf("<NOTES:%d>%s\n",strlen(notes),notes);

  if (oper)
    printf("<OPERATOR:%d>%s\n",strlen(oper),oper);

  if (pfx)
    printf("<PFX:%d>%s\n",strlen(pfx),pfx);

  if (propMode)
    printf("<PROP_MODE:%d>%s\n",strlen(propMode),propMode);

  if (qslMsg)
    printf("<QSLMSG:%d>%s\n",strlen(qslMsg),qslMsg);


  if (qslRDate)
    printf("<QSLRDATE:%d>%s\n",strlen(qslRDate),qslRDate);

  if (qslSDate)
    printf("<QSLSDATE:%d>%s\n",strlen(qslSDate),qslSDate);

  if (qslRcvd)
    printf("<QSL_RCVD:%d>%s\n",strlen(qslRcvd),qslRcvd); 

  if (qslSent)
    printf("<QSL_SENT:%d>%s\n",strlen(qslSent),qslSent);  

  if (qslVia)
    printf("<QSL_VIA:%d>%s\n",strlen(qslVia),qslVia);  

  if (qsoDate)
    printf("<QSO_DATE:%d>%s\n",strlen(qsoDate),qsoDate);  

  if (qth)
    printf("<QTH:%d>%s\n",strlen(qth),qth);  

  if (rstRcvd)
    printf("<RST_RCVD:%d>%s\n",strlen(rstRcvd),rstRcvd); 

  if (rstSent)
    printf("<RST_SENT:%d>%s\n",strlen(rstSent),rstSent); 

  if (rxPwr)
    printf("<RX_PWR:%d>%s\n",strlen(rxPwr),rxPwr); 

  if (satMode)
    printf("<SAT_MODE:%d>%s\n",strlen(satMode),satMode);     

  if (satName)
    printf("<SAT_NAME:%d>%s\n",strlen(satName),satName); 

  if (srx)
    printf("<SRX:%d>%s\n",strlen(srx),srx); 

  if (state)
    printf("<STATE:%d>%s\n",strlen(state),state); 

  if (stx)
    printf("<STX:%d>%s\n",strlen(stx),stx); 

  if (tenTen)
    printf("<TEN_TEN:%d>%s\n",strlen(tenTen),tenTen);

  if (timeOff)
    printf("<TIME_OFF:%d>%s\n",strlen(timeOff),timeOff);

  if (txPwr)
    printf("<TX_PWR:%d>%s\n",strlen(txPwr),txPwr);

  if (veProv)
    printf("<VE_PROV:%d>%s\n",strlen(veProv),veProv);

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

      if (strcasecmp("CALL",vName) == 0)
	{
	  call = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("AGE",vName) == 0)
	{
	  age = strdup(valueStr);
	  continue;
	}
	
      if (strcasecmp("ADDRESS",vName) == 0)
	{
	  address = strdup(valueStr);
	  continue;
	}  

      if (strcasecmp("ARRLSECT",vName) == 0)
	{
	  arrlSect = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("BAND",vName) == 0)
	{
	  band = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("CNTY",vName) == 0)
	{
	  cnty = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("COMMENT",vName) == 0)
	{
	  comment = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("CONT",vName) == 0)
	{
	  cont = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("CONTEST_ID",vName) == 0)
	{
	  contestId = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("CQZ",vName) == 0)
	{
	  cqz = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("DXCC",vName) == 0)
	{
	  dxcc = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("FREQ",vName) == 0)
	{
	  freq = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("GRID_SQUARE",vName) == 0)
	{
	  gridSquare = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("IOTA",vName) == 0)
	{
	  iota = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("ITUZ",vName) == 0)
	{
	  ituz = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("MODE",vName) == 0)
	{
	  mode = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("NAME",vName) == 0)
	{
	  name = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("NOTES",vName) == 0)
	{
	  notes = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("OPERATOR",vName) == 0)
	{
	  oper = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("PFX",vName) == 0)
	{
	  pfx = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("PROP_MODE",vName) == 0)
	{
	  propMode = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("qslMsg",vName) == 0)
	{
	  qslMsg = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("QSL_R_DATE",vName) == 0)
	{
	  qslRDate = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("QSL_S_DATE",vName) == 0)
	{
	  qslSDate = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("QSL_RCVD",vName) == 0)
	{
	  qslRcvd = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("QSL_SENT",vName) == 0)
	{
	  qslSent = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("QSL_VIA",vName) == 0)
	{
	  qslVia = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("QSO_DATE",vName) == 0)
	{
	  qsoDate = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("QTH",vName) == 0)
	{
	  qth = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("RST_RCVD",vName) == 0)
	{
	  rstRcvd = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("RST_SENT",vName) == 0)
	{
	  rstSent = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("RX_PWR",vName) == 0)
	{
	  rxPwr = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("SAT_MODE",vName) == 0)
	{
	  satMode = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("SAT_NAME",vName) == 0)
	{
	  satName = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("SRX",vName) == 0)
	{
	  srx = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("STATE",vName) == 0)
	{
	  state = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("STX",vName) == 0)
	{
	  stx = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("TENTEN",vName) == 0)
	{
	  tenTen = strdup(valueStr);
	  continue;
	}

      if (strcasecmp("TIME_OFF",vName) == 0)
	{
	  timeOff = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("TIME_ON",vName) == 0)
	{
	  timeOn = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("TX_PWR",vName) == 0)
	{
	  txPwr = strdup(valueStr);
	  continue;
	}


      if (strcasecmp("VE_PROV",vName) == 0)
	{
	  veProv = strdup(valueStr);
	  continue;
	}
    }


  //  printf("strlen: %d\n",strlen(str));
  return(nextP);
  
  


}
