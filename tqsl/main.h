//---------------------------------------------------------------------------
#ifndef mainH
#define mainH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Menus.hpp>
//---------------------------------------------------------------------------
class TForm1 : public TForm
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
 void __fastcall CreateNewKeyPair1Click(TObject *Sender);
private:	// User declarations
public:		// User declarations
 __fastcall TForm1(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TForm1 *Form1;
//---------------------------------------------------------------------------
#endif
