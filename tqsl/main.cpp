//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "main.h"
#include "genkeys.h"
#include "CaSign.h"
#include "../tqsl.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TtqslFm *tqslFm;
//---------------------------------------------------------------------------
__fastcall TtqslFm::TtqslFm(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TtqslFm::CreateNewKeyPair1Click(TObject *Sender)
{
 genKeyFm->Show(); 
}
//---------------------------------------------------------------------------

void __fastcall TtqslFm::ImportCertificate1Click(TObject *Sender)
{
 TqslCert cert;
 char      tStr[300];
 char      *p;
 extern int errno;
 openDlg->DefaultExt = ".cert";
 openDlg->InitialDir = "c:\\tqsl";
  if (!openDlg->Execute())
     return;

  int rc;
  strcpy(tStr,openDlg->FileName.c_str());
  rc = tqslReadCert(tStr,&cert);
  if (rc < 1)
   return;

  forCertTbl->Insert();

  char *p1,*p2,*p3;
  int len;
    len = sizeof(cert);
  memcpy(tStr,&cert,sizeof(cert));
  tStr[sizeof(cert)] = 0;
  forCertTblCert->AsString = tStr;

  p1 = &cert.data.publicKey.callSign[0];
  p2 = &cert.data.issueDate[0];
  p3 = &cert.data.expireDate[0];
  
  memset(tStr,0,sizeof(tStr));

  // get call sign
  len = sizeof(cert.data.publicKey.callSign);
  strncpy(tStr,cert.data.publicKey.callSign,len);

  // find first space because that will be the end of the string
  p = strchr(tStr,' ');
  if (p)
   *p = '\0';
  forCertTblCallSign->AsString = tStr;

  // get CA id
  memset(tStr,0,sizeof(tStr));
  len = sizeof(cert.data.caID);
  strncpy(tStr,cert.data.caID,0);
  tStr[sizeof(cert.data.caID)-1] = '\0';

  // find first space because that will be the end of the string
  p = strchr(tStr,' ');
  if (p)
   *p = '\0';
  forCertTblCertCA->AsString = tStr;

  // get CA issue Date
  memset(tStr,0,sizeof(tStr));
  len = sizeof(cert.data.issueDate);
  strncpy(tStr,cert.data.issueDate,len);
  tStr[sizeof(cert.data.issueDate)-1] = '\0';

  // find first space because that will be the end of the string
  p = strchr(tStr,' ');
  if (p)
   *p = '\0';
  forCertTblIssue->AsString = tStr;
  
   // get CA issue Date
  memset(tStr,0,sizeof(tStr));
  len = sizeof(cert.data.expireDate);
  strncpy(tStr,cert.data.expireDate,len);
  tStr[sizeof(cert.data.expireDate)-1] = '\0';

  // find first space because that will be the end of the string
  p = strchr(tStr,' ');
  if (p)
   *p = '\0';
  forCertTblExpires->AsString = tStr;

    // get CA issue Date
  memset(tStr,0,sizeof(tStr));
  len = sizeof(cert.data.caCertNum);
  strncpy(tStr,cert.data.caCertNum,len);
  tStr[sizeof(cert.data.caCertNum)-1] = '\0';

  // find first space because that will be the end of the string
  p = strchr(tStr,' ');
  if (p)
   *p = '\0';
  forCertTblCertNum->AsString = tStr;

  tStr[0] = cert.data.certType;
  tStr[1] = '\0';

  forCertTblCertType->AsString = tStr;
  forCertTbl->Post();
}
//---------------------------------------------------------------------------


void __fastcall TtqslFm::Signacertificate1Click(TObject *Sender)
{
 genCertFm->Show(); 
}
//---------------------------------------------------------------------------

