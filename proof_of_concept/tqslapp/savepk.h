//---------------------------------------------------------------------------
#ifndef savepkH
#define savepkH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "Wwdatsrc.hpp"
#include "Wwdbgrid.hpp"
#include "Wwdbigrd.hpp"
#include "Wwtable.hpp"
#include <Buttons.hpp>
#include <Db.hpp>
#include <DBTables.hpp>
#include <Dialogs.hpp>
#include <Grids.hpp>
//---------------------------------------------------------------------------
class TexpPrivKey : public TForm
{
__published:	// IDE-managed Components
 TwwDataSource *prvDs;
 TwwTable *prvTbl;
 TwwDBGrid *wwDBGrid1;
 TEdit *fnameEd;
 TLabel *Label1;
 TSpeedButton *SpeedButton1;
 TSaveDialog *prvDlg;
 TCheckBox *encryptCb;
 TEdit *privPassEd;
 TLabel *Label2;
 TBitBtn *BitBtn1;
 TBitBtn *BitBtn2;
 TAutoIncField *prvTblPid;
 TStringField *prvTblCallSign;
 TIntegerField *prvTblKeyNum;
 TStringField *prvTblKey;
 TStringField *prvTblChkHash;
 void __fastcall SpeedButton1Click(TObject *Sender);
 void __fastcall BitBtn2Click(TObject *Sender);
 void __fastcall BitBtn1Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TexpPrivKey(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TexpPrivKey *expPrivKey;
//---------------------------------------------------------------------------
#endif
