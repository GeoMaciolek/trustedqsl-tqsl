//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
USERES("tqsl.res");
USEFORM("main.cpp", tqslFm);
USEFORM("genkeys.cpp", genKeyFm);
USELIB("..\tqsllib.lib");
USELIB("..\libeay32.lib");
USEFORM("CaSign.cpp", genCertFm);
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
 try
 {
   Application->Initialize();
   Application->CreateForm(__classid(TtqslFm), &tqslFm);
   Application->CreateForm(__classid(TgenKeyFm), &genKeyFm);
   Application->CreateForm(__classid(TgenCertFm), &genCertFm);
   Application->Run();
 }
 catch (Exception &exception)
 {
   Application->ShowException(&exception);
 }
 return 0;
}
//---------------------------------------------------------------------------
