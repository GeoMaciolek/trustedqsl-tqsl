//---------------------------------------------------------------------------
#ifndef CaSignH
#define CaSignH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <Db.hpp>
#include <DBCtrls.hpp>
#include <DBTables.hpp>
#include <DBGrids.hpp>
#include <Grids.hpp>
#include "Wwdatsrc.hpp"
#include "wwdblook.hpp"
#include "Wwtable.hpp"
#include <Mask.hpp>
#include <ComCtrls.hpp>
#include "Wwquery.hpp"
//---------------------------------------------------------------------------
class TgenCertFm : public TForm
{
__published:	// IDE-managed Components
 TEdit *caRepPass1;
 TEdit *caRepPass2;
 TEdit *caRepPass3;
 TLabel *Label1;
 TLabel *Label2;
 TLabel *Label4;
 TCheckBox *selfCB;
 TBitBtn *Okbtn;
 TBitBtn *closeBtn;
 TLabel *Label5;
 TLabel *Label6;
 TwwTable *privTbl;
 TwwDataSource *privDs;
 TwwDataSource *pubKeyDs;
 TwwTable *pubKeyTbl;
 TwwTable *privLk;
 TwwDBLookupCombo *privKeyCB;
 TwwDBLookupCombo *pubKeyCB;
 TBitBtn *signBtn;
 TLabel *Label7;
 TLabel *Label8;
 TAutoIncField *pubKeyTblKid;
 TStringField *pubKeyTblPublicKey;
 TStringField *pubKeyTblCallSign;
 TStringField *pubKeyTblPubkeyNum;
 TBooleanField *pubKeyTblSigned;
 TAutoIncField *privTblPid;
 TStringField *privTblCallSign;
 TIntegerField *privTblKeyNum;
 TDateTimePicker *IssueDt;
 TDateTimePicker *expireDt;
 TwwTable *pubKeyLk;
 TAutoIncField *pubKeyLkKid;
 TStringField *pubKeyLkPublicKey;
 TStringField *pubKeyLkCallSign;
 TStringField *pubKeyLkPubkeyNum;
 TBooleanField *pubKeyLkSigned;
 TMemo *certMm;
 TwwTable *CertTbl;
 TAutoIncField *CertTblPid;
 TStringField *CertTblCallSign;
 TFloatField *CertTblCertNum;
 TMemoField *CertTblCert;
 TwwQuery *certNumQry;
 TFloatField *certNumQrycertMax;
 TStringField *privTblKey;
 TStringField *privTblChkHash;
 TwwTable *amTbl1;
 TwwTable *amTbl2;
 TwwTable *amTbl3;
 TwwDBLookupCombo *am1CallCB;
 TwwDBLookupCombo *am2CallCB;
 TwwDBLookupCombo *am3CallCB;
 TLabel *Label3;
 TLabel *Label10;
 TLabel *Label11;
 TAutoIncField *amTbl1Pid;
 TStringField *amTbl1CallSign;
 TIntegerField *amTbl1KeyNum;
 TStringField *amTbl1Key;
 TStringField *amTbl1ChkHash;
 TAutoIncField *amTbl2Pid;
 TStringField *amTbl2CallSign;
 TIntegerField *amTbl2KeyNum;
 TStringField *amTbl2Key;
 TStringField *amTbl2ChkHash;
 TAutoIncField *amTbl3Pid;
 TStringField *amTbl3CallSign;
 TIntegerField *amTbl3KeyNum;
 TStringField *amTbl3Key;
 TStringField *amTbl3ChkHash;
 TStringField *CertTblVCAR1_Sig;
 TStringField *CertTblVCAR1_PKNum;
 TStringField *CertTblVCAR2_Sig;
 TStringField *CertTblVCAR2_PKNum;
 TStringField *CertTblVCAR3_Sig;
 TStringField *CertTblVCAR3_PKNum;
 TStringField *CertTblVCAR1_Call;
 TStringField *CertTblVCAR2_Call;
 TStringField *CertTblVCAR3_Call;
 TEdit *caPassEd;
 TLabel *Label12;
 void __fastcall signBtnClick(TObject *Sender);
 void __fastcall expireDtEnter(TObject *Sender);
 void __fastcall FormShow(TObject *Sender);
private:	// User declarations

public:		// User declarations
 __fastcall TgenCertFm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TgenCertFm *genCertFm;
//---------------------------------------------------------------------------
#endif
