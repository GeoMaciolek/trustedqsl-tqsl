//---------------------------------------------------------------------------
#include <vcl.h>
#include <stdio.h>
#pragma hdrstop

#include "caValid.h"
#include "../tqsl.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "Wwdatsrc"
#pragma link "Wwdbgrid"
#pragma link "Wwdbigrd"
#pragma link "Wwquery"
#pragma link "Wwtable"
#pragma resource "*.dfm"
TcaValidateFm *caValidateFm;
//---------------------------------------------------------------------------
__fastcall TcaValidateFm::TcaValidateFm(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------


// not this does not yet validate all of the signers to the cert.
// This will done later in the project.

void __fastcall TcaValidateFm::BitBtn2Click(TObject *Sender)
{
   char certStr[1000];
   char sqlStr[200];
   TqslCert amCert;
   TqslCert caCert;
   int rc;

   strcpy(certStr,certTblCert->AsString.c_str());
   rc = tqslStrToCert(&amCert,certStr);
   if (rc == 0)
     {
     ShowMessage("Failed to convert certificate string.");
     return;
     }
   if (amCert.data.certType == '0')  // check to see if self signed cert
     {
      rc = tqslCheckCert(&amCert, &amCert,0);
      if (rc >= 0)
       {
         sprintf(sqlStr,"Self Signed Certificate for %s is Valid",
           amCert.data.publicKey.callSign);
         ShowMessage(sqlStr);
       }
      else
       {
         sprintf(sqlStr,"*** Self Signed Certificate for %s Failed Validation ***",
           amCert.data.publicKey.callSign);
         ShowMessage(sqlStr);
       }
     }
   else
     {
       tQry->Close();
       sprintf(sqlStr,"select * from certs where CallSign = '%s' and CertNum = '%s'",
         amCert.data.caID,amCert.data.caCertNum);
       tQry->SQL->Clear();
       tQry->SQL->Add(sqlStr);
       tQry->Open();
       if (tQry->RecordCount <= 0)
         {
             sprintf(sqlStr,"CA Certificate for %s number %s isn't on file",
               amCert.data.caID,amCert.data.caCertNum);
             ShowMessage(sqlStr);
             return;
         }
       strcpy(certStr,tQryCert->AsString.c_str());
       rc = tqslStrToCert(&caCert,certStr);
       if (rc == 0)
         {
             sprintf(sqlStr,"Couldn't convert CA Cert of %s number %s",
               amCert.data.caID,amCert.data.caCertNum);
             ShowMessage(sqlStr);
             return;
         }
         
      rc = tqslCheckCert(&amCert, &caCert,1);
      if (rc >= 0)
       {
         sprintf(sqlStr,"Certificate for %s signed by %s is Valid",
           amCert.data.publicKey.callSign,caCert.data.publicKey.callSign);
         ShowMessage(sqlStr);
       }
      else
       {
         sprintf(sqlStr,"*** Certificate for %s Failed Validation ***",
           amCert.data.publicKey.callSign);
         ShowMessage(sqlStr);
       }
    }
}
//---------------------------------------------------------------------------

void __fastcall TcaValidateFm::BitBtn1Click(TObject *Sender)
{
 Close();
}
//---------------------------------------------------------------------------

