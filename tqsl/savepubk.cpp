//---------------------------------------------------------------------------
#include <vcl.h>
#include <stdio.h>
#pragma hdrstop

#include "savepubk.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "Wwdatsrc"
#pragma link "Wwdbgrid"
#pragma link "Wwdbigrd"
#pragma link "Wwtable"
#pragma resource "*.dfm"
TexpPubKey *expPubKey;
//---------------------------------------------------------------------------
__fastcall TexpPubKey::TexpPubKey(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------

void __fastcall TexpPubKey::SpeedButton1Click(TObject *Sender)
{
   if (pkDlg->Execute())
     {
       fnameEd->Text = pkDlg->FileName;
     }
   
}
//---------------------------------------------------------------------------

void __fastcall TexpPubKey::BitBtn2Click(TObject *Sender)
{
 Close(); 
}
//---------------------------------------------------------------------------
void __fastcall TexpPubKey::BitBtn1Click(TObject *Sender)
{
     FILE  *fp;

     fp = fopen(fnameEd->Text.c_str(),"w");
     if (fp == NULL)
       {
       char tStr[100];
       sprintf(tStr,"Opening of file %s failed",fnameEd->Text.c_str());
       ShowMessage(tStr);
       }

     fprintf(fp,"%s\n",pubKeyTblPublicKey->AsString.c_str());
     fclose(fp);
     ShowMessage("Public Key has been saved!");
     Close();
}
//---------------------------------------------------------------------------
