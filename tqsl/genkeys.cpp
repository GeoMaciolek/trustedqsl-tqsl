//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "genkeys.h"
#include <tqsl.h>
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
 const char *secretFile = "c:\\src\\tpriv.prv";
 const char *pubFile = "c:\\src\\tpriv.pub";

 int rc;
  if (callSignEd->Text.Length() < 3)
   return;
  rc = tqslGenNewKeys(callSignEd->Text.c_str(),secretFile,pubFile);
  if (rc <= 0)
    {
     Label2->Caption = rc;
      //fprintf(stderr,"Creation of public/private key pair failed! %d\n",rc);
      //return(rc);
    } 
}
//---------------------------------------------------------------------------
