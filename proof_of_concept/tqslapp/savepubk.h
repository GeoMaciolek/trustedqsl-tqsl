//---------------------------------------------------------------------------
#ifndef savepubkH
#define savepubkH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <Dialogs.hpp>
#include "Wwdatsrc.hpp"
#include "Wwdbgrid.hpp"
#include "Wwdbigrd.hpp"
#include "Wwtable.hpp"
#include <Db.hpp>
#include <DBTables.hpp>
#include <Grids.hpp>
//---------------------------------------------------------------------------
class TexpPubKey : public TForm
{
__published:	// IDE-managed Components
 TEdit *fnameEd;
 TSpeedButton *SpeedButton1;
 TSaveDialog *pkDlg;
 TLabel *Label1;
 TBitBtn *BitBtn1;
 TBitBtn *BitBtn2;
 TwwDataSource *pubKeyDs;
 TwwTable *pubKeyTbl;
 TwwDBGrid *wwDBGrid1;
 TAutoIncField *pubKeyTblKid;
 TStringField *pubKeyTblPublicKey;
 TStringField *pubKeyTblCallSign;
 TStringField *pubKeyTblPubkeyNum;
 TBooleanField *pubKeyTblSigned;
 void __fastcall SpeedButton1Click(TObject *Sender);
 void __fastcall BitBtn2Click(TObject *Sender);
 void __fastcall BitBtn1Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TexpPubKey(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TexpPubKey *expPubKey;
//---------------------------------------------------------------------------
#endif
