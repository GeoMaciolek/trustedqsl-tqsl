//---------------------------------------------------------------------------
#ifndef impPubkeyH
#define impPubkeyH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <Db.hpp>
#include <DBTables.hpp>
#include <Dialogs.hpp>
//---------------------------------------------------------------------------
class TimpPubKeyFm : public TForm
{
__published:	// IDE-managed Components
 TEdit *pkFname;
 TSpeedButton *SpeedButton1;
 TLabel *Label1;
 TBitBtn *BitBtn1;
 TButton *Button1;
 TOpenDialog *pubKDlg;
 TQuery *pkQry;
 TTable *pubkeyTbl;
 TAutoIncField *pubkeyTblKid;
 TStringField *pubkeyTblPublicKey;
 TStringField *pubkeyTblCallSign;
 TStringField *pubkeyTblPubkeyNum;
 TBooleanField *pubkeyTblSigned;
 TIntegerField *pkQryKid;
 TStringField *pkQryPublicKey;
 TStringField *pkQryCallSign;
 TStringField *pkQryPubkeyNum;
 TBooleanField *pkQrySigned;
 void __fastcall SpeedButton1Click(TObject *Sender);
 void __fastcall BitBtn1Click(TObject *Sender);
 void __fastcall Button1Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TimpPubKeyFm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TimpPubKeyFm *impPubKeyFm;
//---------------------------------------------------------------------------
#endif
