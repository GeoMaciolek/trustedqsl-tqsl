//---------------------------------------------------------------------------
#ifndef mainH
#define mainH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Menus.hpp>
#include <Dialogs.hpp>
#include "Wwtable.hpp"
#include <Db.hpp>
#include <DBTables.hpp>
#include <Buttons.hpp>
//---------------------------------------------------------------------------
class TtqslFm : public TForm
{
__published:	// IDE-managed Components
 TMainMenu *MainMenu1;
 TMenuItem *File1;
 TMenuItem *Exit1;
 TMenuItem *ImportPrivateKey1;
 TMenuItem *ImportCertificate1;
 TMenuItem *ImportPublicKey1;
 TMenuItem *ImportPublicKey2;
 TMenuItem *KeyCreate1;
 TMenuItem *CreateNewKeyPair1;
 TMenuItem *ListKeys1;
 TOpenDialog *openDlg;
 TTable *forCertTbl;
 TAutoIncField *forCertTblPid;
 TStringField *forCertTblCallSign;
 TFloatField *forCertTblCertNum;
 TStringField *forCertTblCertCA;
 TStringField *forCertTblCertType;
 TMemoField *forCertTblCert;
 TDateField *forCertTblIssue;
 TDateField *forCertTblExpires;
 TTable *certTbl;
 TTable *pubTbl;
 TTable *PrivTbl;
 TMenuItem *Sign1;
 TMenuItem *CAFunctions1;
 TMenuItem *Signacertificate1;
 TAutoIncField *pubTblKid;
 TStringField *pubTblPublicKey;
 TStringField *pubTblCallSign;
 TStringField *pubTblPubkeyNum;
 TDataSource *privDS;
 TDataSource *pubDS;
 TDataSource *certDs;
 TBitBtn *BitBtn1;
 TAutoIncField *PrivTblPid;
 TStringField *PrivTblCallSign;
 TIntegerField *PrivTblKeyNum;
 TStringField *PrivTblKey;
 TStringField *PrivTblChkHash;
 void __fastcall CreateNewKeyPair1Click(TObject *Sender);
 void __fastcall ImportCertificate1Click(TObject *Sender);
 
 void __fastcall Signacertificate1Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TtqslFm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TtqslFm *tqslFm;
//---------------------------------------------------------------------------
#endif
