//---------------------------------------------------------------------------
#ifndef genkeysH
#define genkeysH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
//---------------------------------------------------------------------------
class TgenKeyFm : public TForm
{
__published:	// IDE-managed Components
 TEdit *callSignEd;
 TLabel *Label1;
 TLabel *Label2;
 TButton *Button1;
 void __fastcall Button1Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TgenKeyFm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TgenKeyFm *genKeyFm;
//---------------------------------------------------------------------------
#endif
