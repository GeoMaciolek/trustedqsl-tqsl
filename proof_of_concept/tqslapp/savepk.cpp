//---------------------------------------------------------------------------
#include <vcl.h>
#include <stdio.h>
#pragma hdrstop
#include "../tqsl.h"
#include "savepk.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "Wwdatsrc"
#pragma link "Wwdbgrid"
#pragma link "Wwdbigrd"
#pragma link "Wwtable"
#pragma resource "*.dfm"
TexpPrivKey *expPrivKey;
//---------------------------------------------------------------------------
__fastcall TexpPrivKey::TexpPrivKey(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TexpPrivKey::SpeedButton1Click(TObject *Sender)
{
   if (encryptCb->Checked)
     prvDlg->DefaultExt = "epk";
   else
     prvDlg->DefaultExt = "prv";
   if (prvDlg->Execute())
     {
       fnameEd->Text = prvDlg->FileName;
     }
}
//---------------------------------------------------------------------------
void __fastcall TexpPrivKey::BitBtn2Click(TObject *Sender)
{
 Close(); 
}
//---------------------------------------------------------------------------
void __fastcall TexpPrivKey::BitBtn1Click(TObject *Sender)
{
   FILE  *fp;
   char  kStr[100];


   strcpy(kStr,prvTblKey->AsString.c_str());

   int rc;

   // check the password even if we save it encrypted.
   rc = tqslDecryptPriv(kStr,privPassEd->Text.c_str(),
         prvTblChkHash->AsString.c_str());
   if (rc < 1)
     {
       ShowMessage("Incorrect Private Key!");
       return;
     }

   // we are saving the key encrypted so we want to copy back the encrypted
   // version
   if (!encryptCb->Checked)
     {
       strcpy(kStr,prvTblKey->AsString.c_str());
     }

   fp = fopen(fnameEd->Text.c_str(),"w");
   if (fp == NULL)
     {
       fnameEd->Text = "";
       return;
     }
   fprintf(fp,"%s|%s|%s|%s",prvTblCallSign->AsString.c_str(),
          prvTblKeyNum->AsString.c_str(),kStr,
          prvTblChkHash->AsString.c_str());
   fclose(fp);
   ShowMessage("Private Key Saved!");
}
//---------------------------------------------------------------------------
