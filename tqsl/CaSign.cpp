//---------------------------------------------------------------------------
#include <vcl.h>
#include <stdlib.h>
#include <commctrl.h>
#pragma hdrstop
#include "../tqsl.h"
#include "CaSign.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "Wwdatsrc"
#pragma link "wwdblook"
#pragma link "Wwtable"
#pragma link "Wwquery"
#pragma resource "*.dfm"
TgenCertFm *genCertFm;
//---------------------------------------------------------------------------
__fastcall TgenCertFm::TgenCertFm(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------



void __fastcall TgenCertFm::signBtnClick(TObject *Sender)
{
 // now that we have all the information we can sign the cert

  String           privKeyStr;
  String           pubKeyStr;
  TqslPublicKey    pubKey;
  TqslCert	        amCert;
  int              rc;
  int              self;

  pubKeyStr = pubKeyTblPublicKey->AsString;
  privKeyStr =  privTblKey->AsString;

  if  (selfCB->Checked)
   self = 1;
  else
   self = 0;

  rc = tqslStrToPubKey(&pubKey,pubKeyStr.c_str());
  if (rc == 0)
   {
     // put up error message
     return;
   }

  TDateTime expire;
  TDateTime issue;

  ShortDateFormat = "dd/mm/yyyy";
  String issueStr;
  String expireStr;
  expire = expireDt->Date;
  issue = IssueDt->Date;
  issueStr = issue;
  expireStr = expire;

  char eStr[50];
  char iStr[50];
  // int certNo;

  // Ok we need a unique cert number so we find the highest one
  // and bump it.

  strcpy(eStr,expireStr.c_str());
  strcpy(iStr,issueStr.c_str());
  ShortDateFormat = "dd/mm/yy";
#if 0
  certNumQry->Open();
  if (certNumQry->RecordCount > 0 )
   {
     certNo = certNumQrycertMax->AsInteger + 1;
   }
  else
   certNo = 1;
  certNumQry->Close();
#endif

  if ((amTbl1CallSign->AsString == amTbl2CallSign->AsString) ||
    (amTbl1CallSign->AsString == amTbl3CallSign->AsString) ||
    (amTbl3CallSign->AsString == amTbl2CallSign->AsString))
    {  // trying to use the same call sign for more than one
       // CA.  That is a NO NO!
       ShowMessage("3 different amatuers must sign a certificate.");
       return;
    }
  eStr[10] = 0;
  iStr[10] = 0;
  //char numStr[20];

  // cert number is the same as the public key number.
  // This should be ok because a cert is unique based upon
  // Call sign and Cert Number.
  //
  // There is no requirement to do it this away.  If the CA
  // wants to have a different cert# that should be OK.
  //
  // This just allows me to predict what the cert number is
  // going to be.  So I can have all the reps sign the cert
  // before they have certs themselves
  //

  char  privKey[50];
  strcpy(privKey, privKeyStr.c_str());
  rc = tqslDecryptPriv(privKey,caPassEd->Text.c_str(),
       privTblChkHash->AsString.c_str());
  if (rc < 1)
     {
         ShowMessage("CA's Password Failed");
         return;
     }

  rc = tqslSignCert(&amCert,privKey,privTblCallSign->AsString.c_str(),
   &pubKey, privTblKeyNum->AsString.c_str(),iStr, eStr, self,
   amTbl1CallSign->AsString.c_str(), amTbl2CallSign->AsString.c_str(),
   amTbl3CallSign->AsString.c_str());


   if (rc > 0)
     {
     char *certStr;
     certStr = tqslCertToStr(&amCert);
     if (certStr == NULL)
       return;
     TqslSignature signature;
     // now let have the signers

     CertTbl->Append();

     // sign the certStr and save all the info

     // do Amateur 1 first
     strcpy(privKey, amTbl1Key->AsString.c_str());
     rc = tqslDecryptPriv(privKey,caRepPass1->Text.c_str(),
       amTbl1ChkHash->AsString.c_str());
     if (rc < 1)
       {
         CertTbl->Cancel();
         ShowMessage("Amateur 1 Password Failed");
         return;
     }
     rc = tqslSignData(privKey,certStr, NULL,&signature, strlen(certStr));
     if (rc == 0)
       {
         CertTbl->Cancel();
         return;
       }
     CertTblVCAR1_Sig->AsString = signature.signature;
     CertTblVCAR1_PKNum->AsString = amTbl1KeyNum->AsString;
     CertTblVCAR1_Call->AsString = amTbl1CallSign->AsString;


     // do Amateur 2
     strcpy(privKey, amTbl2Key->AsString.c_str());
     rc = tqslDecryptPriv(privKey,caRepPass2->Text.c_str(),
       amTbl2ChkHash->AsString.c_str());
     if (rc < 1)
       {
         CertTbl->Cancel();
         ShowMessage("Amateur 2 Password Failed");
         return;
     }
     rc = tqslSignData(privKey,certStr, NULL,&signature, strlen(certStr));
     if (rc == 0)
       {
         CertTbl->Cancel();
         return;
       }
     CertTblVCAR2_Sig->AsString = signature.signature;
     CertTblVCAR2_PKNum->AsString = amTbl2KeyNum->AsString;
     CertTblVCAR2_Call->AsString = amTbl2CallSign->AsString;

     // do Amateur 3
     strcpy(privKey, amTbl3Key->AsString.c_str());
     rc = tqslDecryptPriv(privKey,caRepPass3->Text.c_str(),
       amTbl3ChkHash->AsString.c_str());
     if (rc < 1)
       {
         CertTbl->Cancel();
         ShowMessage("Amateur 3 Password Failed");
         return;
     }
     rc = tqslSignData(privKey, certStr, NULL,&signature, strlen(certStr));
     if (rc == 0)
       {
         CertTbl->Cancel();
         return;
       }

     CertTblVCAR3_Sig->AsString = signature.signature;
     CertTblVCAR3_PKNum->AsString = amTbl3KeyNum->AsString;
     CertTblVCAR3_Call->AsString = amTbl3CallSign->AsString;

     CertTblCallSign->AsString = pubKey.callSign;
     CertTblCertNum->AsString = pubKey.pubkeyNum;
     CertTblCert->AsString = certStr;
     // CertTblVCAR_Sig1->AsString = caRep1->Text.c_str();
     // CertTblVCAR_Sig2->AsString = caRep2->Text.c_str();
     // CertTblVCAR_Sig3->AsString = caRep3->Text.c_str();
     CertTbl->Post();

     // mark the pub key as signed.
     pubKeyTbl->Edit();
     pubKeyTblSigned->AsBoolean = TRUE;
     pubKeyTbl->Post();
     
     certMm->Lines->Clear();
     certMm->Lines->Add(certStr);
     free(certStr);
     return;
     }
   else

    ShowMessage("Signed failed");
}
//---------------------------------------------------------------------------


void __fastcall TgenCertFm::expireDtEnter(TObject *Sender)
{

  TDateTime issue;

  ShortDateFormat = "dd/mm/yyyy";

  expireDt->Date = IssueDt->Date + 3652; // add ten years for expiration date
  expireDt->Update();

}
//---------------------------------------------------------------------------

void __fastcall TgenCertFm::FormShow(TObject *Sender)
{

  TDateTime issue;
  pubKeyLk->Refresh();
  ShortDateFormat = "dd/mm/yyyy";

  expireDt->Date = IssueDt->Date + 3652; // add ten years for expiration date
  expireDt->Update();
}
//---------------------------------------------------------------------------

