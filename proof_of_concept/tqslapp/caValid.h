//---------------------------------------------------------------------------
#ifndef caValidH
#define caValidH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "Wwdatsrc.hpp"
#include "Wwdbgrid.hpp"
#include "Wwdbigrd.hpp"
#include "Wwquery.hpp"
#include "Wwtable.hpp"
#include <Buttons.hpp>
#include <Db.hpp>
#include <DBCtrls.hpp>
#include <DBTables.hpp>
#include <ExtCtrls.hpp>
#include <Grids.hpp>
//---------------------------------------------------------------------------
class TcaValidateFm : public TForm
{
__published:	// IDE-managed Components
 TwwDataSource *certDs;
 TwwTable *certTbl;
 TwwQuery *tQry;
 TwwDBGrid *wwDBGrid1;
 TDBNavigator *DBNavigator1;
 TBitBtn *BitBtn1;
 TBitBtn *BitBtn2;
 TAutoIncField *certTblPid;
 TStringField *certTblCallSign;
 TFloatField *certTblCertNum;
 TMemoField *certTblCert;
 TStringField *certTblVCAR1_Call;
 TStringField *certTblVCAR1_Sig;
 TStringField *certTblVCAR1_PKNum;
 TStringField *certTblVCAR2_Call;
 TStringField *certTblVCAR2_Sig;
 TStringField *certTblVCAR2_PKNum;
 TStringField *certTblVCAR3_Call;
 TStringField *certTblVCAR3_Sig;
 TStringField *certTblVCAR3_PKNum;
 TIntegerField *tQryPid;
 TStringField *tQryCallSign;
 TFloatField *tQryCertNum;
 TMemoField *tQryCert;
 TStringField *tQryVCAR1_Call;
 TStringField *tQryVCAR1_Sig;
 TStringField *tQryVCAR1_PKNum;
 TStringField *tQryVCAR2_Call;
 TStringField *tQryVCAR2_Sig;
 TStringField *tQryVCAR2_PKNum;
 TStringField *tQryVCAR3_Call;
 TStringField *tQryVCAR3_Sig;
 TStringField *tQryVCAR3_PKNum;
 void __fastcall BitBtn2Click(TObject *Sender);
 
 void __fastcall BitBtn1Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TcaValidateFm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TcaValidateFm *caValidateFm;
//---------------------------------------------------------------------------
#endif
