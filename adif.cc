#include "adif.h"


void AdifBase::clearQSO()
{
  char		*address=NULL;
  char		*age=NULL;
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
  operator=NULL;
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

void AdifBase()
{
  clearQSO();
  privKey=NULL;
  myCert=NULL;
  signature=NULL;
}
