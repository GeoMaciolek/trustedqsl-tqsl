//---------------------------------------------------------------------------
#include <vcl.h>
#include <stdio.h>
#pragma hdrstop

#include "savecert.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TexpCerts *expCerts;
//---------------------------------------------------------------------------
__fastcall TexpCerts::TexpCerts(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TexpCerts::SpeedButton1Click(TObject *Sender)
{
  if (saveDlg->Execute())
  {
    saveCertEd->Text = saveDlg->FileName;

  }
}
//---------------------------------------------------------------------------
void __fastcall TexpCerts::BitBtn1Click(TObject *Sender)
{

    FILE *fp;
    fp = fopen(saveCertEd->Text.c_str(),"w");
    if (fp == NULL)
     return;
    certTbl->First();
    while (!certTbl->Eof)
    {
     fprintf(fp,"%s\n",certTblCert->AsString.c_str());

     certTbl->Next();
    }
    fclose(fp);
}
//---------------------------------------------------------------------------
