//---------------------------------------------------------------------------
#include <vcl.h>
#include <fcntl.h>
#include <stdio.h>
#include<io.h>

#pragma hdrstop

#include "genkeys.h"
#include "../tqsl.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TgenKeyFm *genKeyFm;
//---------------------------------------------------------------------------
__fastcall TgenKeyFm::TgenKeyFm(TComponent* Owner)
 : TForm(Owner)
{

}
//---------------------------------------------------------------------------
void __fastcall TgenKeyFm::Button1Click(TObject *Sender)
{
 char  callsign[100];
 //const char *secretFile = "c:\\src\\tpriv.prv";
 //const char *pubFile = "c:\\src\\tpriv.pub";
 const char *secretFile = "tpriv.prv";
 const char *pubFile = "tpriv.pub";
 TqslPublicKey pubKey;
 char		*secretKey;

 int rc;
  if (callSignEd->Text.Length() < 3)
   return;
  rc = tqslGenNewKeys(callSignEd->Text.c_str(),&secretKey,&pubKey);
  if (rc < 1)
   return;

  Label2->Caption = rc;
  FILE *fd;

  fd = fopen(pubFile,"w");
  if (fd != NULL)
   {
     char *pkStr;
     pkStr = tqslPubKeyToStr(&pubKey);
     if (pkStr != NULL)
       {
         rc = fprintf(fd,"%s\n",pkStr);
         fclose(fd);
       }
     else
       {
         //fprintf(stderr,"creating public file failed\n");
         return;
       }
   }
  fd = fopen(secretFile,"w");
  if (fd >= 0)
    {
      fprintf(fd,"%s\n",secretKey);
      fclose(fd);
    }
  else
    {
      //fprintf(stderr,"creating private file failed\n");
      return;
    }	 

}
//---------------------------------------------------------------------------
