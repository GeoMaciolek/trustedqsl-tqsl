#ifndef ADIF_H
#define ADIF_H

class AdifBase
{
  char		*address;
  char		*age;
  char		*arrlSect;
  char		*band;
  char		*call;
  char		*cnty;
  char		*comment;
  char		*cont;
  char		*contestId;
  char		*cqz;
  char		*dxcc;
  char		*freq;
  char		*gridSquare;
  char		*iota;
  char		*ituz;
  char		*mode;
  char		*name;
  char		*notes;
  char		*oper;
  char		*pfx;
  char		*propMode;
  char		*qslMsg;
  char		*qslRDate;
  char		*qslSDate;
  char		*qslRcvd;
  char		*qslSent;
  char		*qslVia;
  char		*qsoDate;
  char		*qth;
  char		*rstRcvd;
  char		*rstSent;
  char		*rxPwr;
  char		*satMode;
  char		*satName;
  char		*srx;
  char		*state;
  char		*stx;
  char		*tenTen;
  char		*timeOff;
  char		*timeOn;
  char		*txPwr;
  char		*veProv;

  char		*privKey;
  char		*myCert;
  char		*signature;

  // private functions
  char		*nextLine(char *);

 public:
  AdifBase();	
  void		init();
  void		clearQSO();
  char		*readNext();
  void		dump();
  char 		*AdifParseNext(char *);

};



#endif 
