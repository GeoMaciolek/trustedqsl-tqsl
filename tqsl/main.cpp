//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "main.h"
#include "genkeys.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TForm1 *Form1;
//---------------------------------------------------------------------------
__fastcall TForm1::TForm1(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TForm1::CreateNewKeyPair1Click(TObject *Sender)
{
 genKeyFm->Show(); 
}
//---------------------------------------------------------------------------

