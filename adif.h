#ifndef ADIF_H
#define ADIF_H

class AdifBase
{
  char		*address=NULL;
  char		*age=NULL;
  char		*arrlSect=NULL;
  char		*band=NULL;
  char		*call=NULL;
  char		*cnty=NULL;
  char		*comment=NULL;
  char		*cont=NULL;
  char		*contestId=NULL;
  char		*cqz=NULL;
  char		*dxcc=NULL;
  char		*freq=NULL;
  char		*gridSquare=NULL;
  char		*iota=NULL;
  char		*ituz=NULL;
  char		*mode=NULL;
  char		*name=NULL;
  char		*notes=NULL;
  char		*operator=NULL;
  char		*pfx=NULL;
  char		*propMode=NULL;
  char		*qslMsg=NULL;
  char		*qslRDate=NULL;
  char		*qslSDate=NULL;
  char		*qslRcvd=NULL;
  char		*qslSent=NULL;
  char		*qslVia=NULL;
  char		*qsoDate=NULL;
  char		*qth=NULL;
  char		*rstRcvd=NULL;
  char		*rstSent=NULL;
  char		*rxPwr=NULL;
  char		*satMode=NULL;
  char		*satName=NULL;
  char		*srx=NULL;
  char		*stx=NULL;
  char		*tenTen=NULL;
  char		*timeOff=NULL;
  char		*timeOn=NULL;
  char		*txPwr=NULL;
  char		*veProv=NULL;

  char		*privKey=NULL;
  char		*myCert=NULL;
  char		*signature=NULL;
  
 public:
  AdifBase();	
  void		init();
  void		clearQSO();
  char		*readNext();
  

};



#endif 
