//---------------------------------------------------------------------------
#include <vcl.h>
#include <stdio.h>
#pragma hdrstop

#include "../tqsl.h"
#include "impcert.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TimportCertFm *importCertFm;
//---------------------------------------------------------------------------
__fastcall TimportCertFm::TimportCertFm(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------


void __fastcall TimportCertFm::SpeedButton1Click(TObject *Sender)
{
   if (openDlg->Execute())
  {
    fnameEd->Text = openDlg->FileName;
  }
}
//---------------------------------------------------------------------------
void __fastcall TimportCertFm::BitBtn2Click(TObject *Sender)
{
 TqslCert cert;
 char      tStr[1000];
 char      certStr[1000];
 char      *p;
 extern int errno;
 FILE      *fp;



  int rc;

  ShortDateFormat = "dd/mm/yyyy";
  fp = fopen(fnameEd->Text.c_str(),"r");
  if (fp == NULL)
   return;

  while ((p=fgets(tStr,999,fp)) != NULL)
   {
     p = strchr(tStr,'\r');
     if (p)
       *p = '\0';
     p = strchr(tStr,'\n');
     if (p)
       *p = '\0';
     strcpy(certStr,tStr);
     rc = tqslStrToCert(&cert,tStr);
     if (rc > 0)
       {
         if ( cert.data.certType == '1')
           {
             forcertTbl->Insert();
             forcertTblCert->AsString = certStr;
             forcertTblCallSign->AsString = cert.data.publicKey.callSign;
             forcertTblIssue->AsString = cert.data.issueDate;
             forcertTblExpires->AsString = cert.data.expireDate;
             forcertTblCertCA->AsString = cert.data.caID;
             forcertTblCertNum->AsString = cert.data.publicKey.pubkeyNum;
             forcertTblCertType->AsString = cert.data.certType;
             forcertTbl->Post();
           }
         else  if ( cert.data.certType == '0')
           {
             caCertTbl->Insert();
             caCertTblCert->AsString = certStr;
             caCertTblCallSign->AsString = cert.data.publicKey.callSign;
             caCertTblIssue->AsString = cert.data.issueDate;
             caCertTblExpires->AsString = cert.data.expireDate;
             caCertTblCertCA->AsString = cert.data.caID;
             caCertTblCertNum->AsString = cert.data.publicKey.pubkeyNum;
             caCertTblCertType->AsString = cert.data.certType;
             caCertTblTrusted->AsBoolean = FALSE;
             caCertTbl->Post();

           }
       }

   }
  ShowMessage("Certificates have been imported!");
 
}
//---------------------------------------------------------------------------
