//---------------------------------------------------------------------------
#ifndef savecertH
#define savecertH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <Dialogs.hpp>
#include <Db.hpp>
#include <DBTables.hpp>
//---------------------------------------------------------------------------
class TexpCerts : public TForm
{
__published:	// IDE-managed Components
 TEdit *saveCertEd;
 TSpeedButton *SpeedButton1;
 TSaveDialog *saveDlg;
 TBitBtn *BitBtn1;
 TBitBtn *BitBtn2;
 TTable *certTbl;
 TAutoIncField *certTblPid;
 TStringField *certTblCallSign;
 TFloatField *certTblCertNum;
 TMemoField *certTblCert;
 void __fastcall SpeedButton1Click(TObject *Sender);
 
 void __fastcall BitBtn1Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TexpCerts(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TexpCerts *expCerts;
//---------------------------------------------------------------------------
#endif
