//---------------------------------------------------------------------------
#ifndef impcertH
#define impcertH
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
class TimportCertFm : public TForm
{
__published:	// IDE-managed Components
 TEdit *fnameEd;
 TLabel *Label1;
 TSpeedButton *SpeedButton1;
 TBitBtn *BitBtn1;
 TBitBtn *BitBtn2;
 TOpenDialog *openDlg;
 TTable *forcertTbl;
 TAutoIncField *forcertTblPid;
 TStringField *forcertTblCallSign;
 TFloatField *forcertTblCertNum;
 TStringField *forcertTblCertCA;
 TStringField *forcertTblCertType;
 TMemoField *forcertTblCert;
 TDateField *forcertTblIssue;
 TDateField *forcertTblExpires;
 TTable *caCertTbl;
 TAutoIncField *caCertTblPid;
 TStringField *caCertTblCallSign;
 TFloatField *caCertTblCertNum;
 TStringField *caCertTblCertCA;
 TStringField *caCertTblCertType;
 TMemoField *caCertTblCert;
 TDateField *caCertTblIssue;
 TDateField *caCertTblExpires;
 TBooleanField *caCertTblTrusted;void __fastcall SpeedButton1Click(TObject *Sender);
 void __fastcall BitBtn2Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TimportCertFm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TimportCertFm *importCertFm;
//---------------------------------------------------------------------------
#endif
