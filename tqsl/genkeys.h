//---------------------------------------------------------------------------
#ifndef genkeysH
#define genkeysH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <Dialogs.hpp>
#include "Wwtable.hpp"
#include <Db.hpp>
#include <DBTables.hpp>
//---------------------------------------------------------------------------
class TgenKeyFm : public TForm
{
__published:	// IDE-managed Components
 TEdit *callSignEd;
 TLabel *Label1;
 TButton *Button1;
 TSaveDialog *savePrvDlg;
 TwwTable *prvTbl;
 TAutoIncField *prvTblPid;
 TIntegerField *prvTblKeyNum;
 TStringField *prvTblKey;
 TButton *Button2;
 void __fastcall Button1Click(TObject *Sender);


 void __fastcall Button2Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TgenKeyFm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TgenKeyFm *genKeyFm;
//---------------------------------------------------------------------------
#endif
