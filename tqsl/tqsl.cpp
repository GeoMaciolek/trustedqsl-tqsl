//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
USERES("tqsl.res");
USEFORM("main.cpp", Form1);
USEFORM("genkeys.cpp", genKeyFm);
USELIB("..\tqsllib.lib");
USELIB("..\libeay32.lib");
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
 try
 {
   Application->Initialize();
   Application->CreateForm(__classid(TForm1), &Form1);
   Application->CreateForm(__classid(TgenKeyFm), &genKeyFm);
   Application->Run();
 }
 catch (Exception &exception)
 {
   Application->ShowException(&exception);
 }
 return 0;
}
//---------------------------------------------------------------------------
