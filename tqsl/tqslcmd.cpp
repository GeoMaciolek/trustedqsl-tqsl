//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop
USERES("tqslcmd.res");
USEFORM("main.cpp", tqslFm);
USEFORM("genkeys.cpp", genKeyFm);
USELIB("..\tqsllib.lib");
USELIB("..\libeay32.lib");
USEFORM("CaSign.cpp", genCertFm);
USEFORM("about.cpp", aboutFm);
USEFORM("caValid.cpp", caValidateFm);
USEFORM("debug.cpp", debugFm);
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
 try
 {
   Application->Initialize();
   Application->CreateForm(__classid(TtqslFm), &tqslFm);
   Application->CreateForm(__classid(TgenKeyFm), &genKeyFm);
   Application->CreateForm(__classid(TgenCertFm), &genCertFm);
   Application->CreateForm(__classid(TaboutFm), &aboutFm);
   Application->CreateForm(__classid(TcaValidateFm), &caValidateFm);
   Application->CreateForm(__classid(TdebugFm), &debugFm);
   Application->Run();
 }
 catch (Exception &exception)
 {
   Application->ShowException(&exception);
 }
 return 0;
}
//---------------------------------------------------------------------------
