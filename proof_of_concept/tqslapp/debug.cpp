//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "../tqsl.h"
#include "debug.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "cspin"
#pragma resource "*.dfm"
extern int debugLevel;
TdebugFm *debugFm;
//---------------------------------------------------------------------------
__fastcall TdebugFm::TdebugFm(TComponent* Owner)
 : TForm(Owner)
{
}
//---------------------------------------------------------------------------

void __fastcall TdebugFm::BitBtn1Click(TObject *Sender)
{
   debugLevel = CSpinEdit1->Value;

   if (debugLevel > 0)
     {
     if (dbFile != NULL)
       {
         fclose(dbFile);
         dbFile = NULL;
       }
     dbFile = fopen(dbFileEd->Text.c_str(),"a+");
     if (dbFile == NULL)
       {
         ShowMessage("Could not open file: " + dbFileEd->Text);
         return;
       }
     }
   else
     {
       if (dbFile != NULL)
         {
           fclose(dbFile);
           dbFile = NULL;
         }
     }
   Close();

}
//---------------------------------------------------------------------------

void __fastcall TdebugFm::dbFileEdDblClick(TObject *Sender)
{
   if (debugDlg->Execute())
     {
       dbFileEd->Text = debugDlg->FileName;
     }
}
//---------------------------------------------------------------------------
