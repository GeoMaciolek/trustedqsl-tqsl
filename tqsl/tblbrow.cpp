//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "tblbrow.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "Wwdatsrc"
#pragma link "Wwdbgrid"
#pragma link "Wwdbigrd"
#pragma link "Wwtable"
#pragma resource "*.dfm"
TtblView *tblView;
//---------------------------------------------------------------------------
__fastcall TtblView::TtblView(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------

void __fastcall TtblView::BitBtn1Click(TObject *Sender)
{
 Close(); 
}
//---------------------------------------------------------------------------
