//---------------------------------------------------------------------------
#include <vcl.h>
#include <stdio.h>
#pragma hdrstop

#include "../tqsl.h"
#include "impPubkey.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TimpPubKeyFm *impPubKeyFm;
//---------------------------------------------------------------------------
__fastcall TimpPubKeyFm::TimpPubKeyFm(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TimpPubKeyFm::SpeedButton1Click(TObject *Sender)
{
   if (!pubKDlg->Execute())
     return;

   pkFname->Text = pubKDlg->FileName;

}
//---------------------------------------------------------------------------
void __fastcall TimpPubKeyFm::BitBtn1Click(TObject *Sender)
{
   Close(); 
}
//---------------------------------------------------------------------------

// adds a public key into the database.
//
// things to do, is check to see if the cert for this public key is
// already there, if so mark it as signed.
// 
void __fastcall TimpPubKeyFm::Button1Click(TObject *Sender)
{
   TqslPublicKey pubkey;
   char  sqlCmd[300];
   int rc;
   
   rc = tqslReadPub(pkFname->Text.c_str(),&pubkey);
   if (rc < 1)
     {
       ShowMessage("Could not open file " + pkFname->Text);
       return;
     }

   sprintf(sqlCmd,"select * from pubkeys where CallSign = '%s' and "
     "PubkeyNum = '%s'",pubkey.callSign,pubkey.pubkeyNum);

   pkQry->Close();
   pkQry->SQL->Clear();
   pkQry->SQL->Add(sqlCmd);
   pkQry->Open();

   if (pkQry->RecordCount > 0)
     {
       ShowMessage("This Public Key already exist in the database!");
       pkQry->Close();
       return;
     }

   pkQry->Close();

   char *keyStr;
   keyStr = tqslPubKeyToStr(&pubkey);

   pubkeyTbl->Insert();
   pubkeyTblPublicKey->AsString = keyStr;
   pubkeyTblSigned->AsBoolean = false;
   pubkeyTblPubkeyNum->AsString = pubkey.pubkeyNum;
   pubkeyTblCallSign->AsString = pubkey.callSign;
   pubkeyTbl->Post();
}
//---------------------------------------------------------------------------
