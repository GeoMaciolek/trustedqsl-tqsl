//---------------------------------------------------------------------------
#ifndef debugH
#define debugH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "cspin.h"
#include <Buttons.hpp>
#include <Dialogs.hpp>
//---------------------------------------------------------------------------
class TdebugFm : public TForm
{
__published:	// IDE-managed Components
 TLabel *Label1;
 TEdit *dbFileEd;
 TBitBtn *BitBtn1;
 TBitBtn *BitBtn2;
 TLabel *Label2;
 TCSpinEdit *CSpinEdit1;
 TOpenDialog *debugDlg;
 void __fastcall BitBtn1Click(TObject *Sender);
 void __fastcall dbFileEdDblClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TdebugFm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TdebugFm *debugFm;
//---------------------------------------------------------------------------
#endif
