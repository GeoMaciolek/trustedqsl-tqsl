//---------------------------------------------------------------------------
#ifndef aboutH
#define aboutH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
//---------------------------------------------------------------------------
class TaboutFm : public TForm
{
__published:	// IDE-managed Components
 TLabel *Label1;
 TLabel *Label2;
 TMemo *Memo1;
 TBitBtn *BitBtn1;
 TLabel *Label3;
private:	// User declarations
public:		// User declarations
 __fastcall TaboutFm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TaboutFm *aboutFm;
//---------------------------------------------------------------------------
#endif
