//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "main.h"
#include "tblbrow.h"
#include "genkeys.h"
#include "about.h"
#include "CaSign.h"
#include "caValid.h"
#include "debug.h"
#include "savecert.h"
#include "savepubk.h"
#include "savepk.h"
#include "impPubkey.h"
#include "impcert.h"
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



void __fastcall TtqslFm::Signacertificate1Click(TObject *Sender)
{
 genCertFm->Show(); 
}
//---------------------------------------------------------------------------

void __fastcall TtqslFm::About1Click(TObject *Sender)
{
 aboutFm->Show();
}
//---------------------------------------------------------------------------

void __fastcall TtqslFm::ValidateCertificates1Click(TObject *Sender)
{
 caValidateFm->Show();
}
//---------------------------------------------------------------------------

void __fastcall TtqslFm::DebugLevel1Click(TObject *Sender)
{
 debugFm->Show();
}
//---------------------------------------------------------------------------


void __fastcall TtqslFm::ExportOwnCertificates1Click(TObject *Sender)
{
 expCerts->Show(); 
}
//---------------------------------------------------------------------------

void __fastcall TtqslFm::ImportCertificate1Click(TObject *Sender)
{
 importCertFm->Show(); 
}
//---------------------------------------------------------------------------


void __fastcall TtqslFm::ExportPublicKeys1Click(TObject *Sender)
{
 expPubKey->Show();
}
//---------------------------------------------------------------------------

void __fastcall TtqslFm::ExportPrivateKey1Click(TObject *Sender)
{
 expPrivKey->Show(); 
}
//---------------------------------------------------------------------------

void __fastcall TtqslFm::ImportPublicKey1Click(TObject *Sender)
{
 impPubKeyFm->Show(); 
}
//---------------------------------------------------------------------------

void __fastcall TtqslFm::ImportPrivateKey1Click(TObject *Sender)
{
 ShowMessage("Feature isn't working yet."); 
}
//---------------------------------------------------------------------------

void __fastcall TtqslFm::MaintainCertsandKeys1Click(TObject *Sender)
{
 tblView->Show(); 
}
//---------------------------------------------------------------------------

