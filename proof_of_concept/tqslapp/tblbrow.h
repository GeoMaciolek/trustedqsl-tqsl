//---------------------------------------------------------------------------
#ifndef tblbrowH
#define tblbrowH
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
#include <DBCtrls.hpp>
#include <DBTables.hpp>
#include <ExtCtrls.hpp>
#include <Grids.hpp>
//---------------------------------------------------------------------------
class TtblView : public TForm
{
__published:	// IDE-managed Components
 TwwDataSource *privDS;
 TwwTable *privTbl;
 TwwDBGrid *wwDBGrid1;
 TDBNavigator *DBNavigator1;
 TLabel *Label1;
 TwwTable *pubKeyTbl;
 TwwDataSource *pubKeyDS;
 TwwDBGrid *wwDBGrid2;
 TLabel *Label2;
 TDBNavigator *DBNavigator2;
 TwwTable *caCertTbl;
 TwwDataSource *caCertDS;
 TwwDBGrid *wwDBGrid3;
 TLabel *Label3;
 TDBNavigator *DBNavigator3;
 TwwTable *certTbl;
 TwwDataSource *certDS;
 TwwDBGrid *wwDBGrid4;
 TLabel *Label4;
 TDBNavigator *DBNavigator4;
 TBitBtn *BitBtn1;
 void __fastcall BitBtn1Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TtblView(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TtblView *tblView;
//---------------------------------------------------------------------------
#endif
