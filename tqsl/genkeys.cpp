//---------------------------------------------------------------------------
#include <vcl.h>
#include <fcntl.h>
#include <stdio.h>
#include<io.h>

#pragma hdrstop
#include "main.h"
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
 char  callsign[20];

 char  clrPass[200];
 // unsigned char

 TqslPublicKey pubKey;
 char		*secretKey;

 int rc;

  if (pass2Ed->Text != pass1Ed->Text)
   {
      ShowMessage("Password Mismatch");
      return;
   }
  if (callSignEd->Text.Length() < 3)
   {
     ShowMessage("Call Sign invalid");
     return;
   }

  strcpy(clrPass,pass2Ed->Text.c_str());
  rc = tqslGenNewKeys(callSignEd->Text.c_str(),&secretKey,&pubKey);

  if (rc < 1)
   {
     ShowMessage("Key pair creation failed");
     return;
   }

  char privHashStr[60];
  char dummyStr[60];


  rc = tqslEncryptPriv(secretKey,clrPass,privHashStr);
  if (rc < 1)
   {
     ShowMessage("Failed encrypting private key");
     return;
   }
  strcpy(dummyStr,secretKey);
  rc = tqslDecryptPriv(dummyStr,clrPass,privHashStr);
  if (rc < 1)
   {
     ShowMessage("Failed decrypting private key");
     return;
   }


  char *pkStr;
  char tmpStr[10];

  try
   {
     pkStr = tqslPubKeyToStr(&pubKey);
     tqslFm->pubTbl->Insert();
     tqslFm->pubTbl->FieldByName("PublicKey")->AsString = pkStr;
     tqslFm->pubTbl->FieldByName("CallSign")->AsString = callSignEd->Text;
     tqslFm->pubTbl->Post();
     tqslFm->pubTbl->Refresh();
     tqslFm->pubTbl->Edit();
     sprintf(tmpStr,"%s",
       tqslFm->pubTbl->FieldByName("Kid")->AsString);
     strncpy(pubKey.pubkeyNum,tmpStr,sizeof(pubKey.pubkeyNum)-1);
     pubKey.pubkeyNum[sizeof(pubKey.pubkeyNum)-1] = '\0';
     pkStr = tqslPubKeyToStr(&pubKey);
     
     // recreate the public key string so it will have the pubkey # in it.
     tqslFm->pubTbl->FieldByName("PublicKey")->AsString = pkStr;
     tqslFm->pubTbl->FieldByName("PubkeyNum")->AsString =
       tqslFm->pubTbl->FieldByName("Kid")->AsString;

     tqslFm->pubTbl->Post();
   }
   catch(EDatabaseError& e)
   {
        String msgStr =  e.Message  + "\n\n";
        ShowMessage(msgStr);
   }
  try
   {
     tqslFm->PrivTbl->Insert();
     tqslFm->PrivTbl->FieldByName("KeyNum")->AsInteger =
       tqslFm->pubTbl->FieldByName("Kid")->AsInteger;
     tqslFm->PrivTbl->FieldByName("ChkHash")->AsString = privHashStr;
     tqslFm->PrivTbl->FieldByName("Key")->AsString = secretKey;
     tqslFm->PrivTbl->FieldByName("CallSign")->AsString = callSignEd->Text;
     tqslFm->PrivTbl->Post();

   }

   // public
   catch(EDatabaseError& e)
   {
        String msgStr =  e.Message  + "\n\n";
        ShowMessage(msgStr);
   }

   //ShowMessage(buf);


}

void __fastcall TgenKeyFm::Button2Click(TObject *Sender)
{
 this->Close(); 
}
//---------------------------------------------------------------------------

