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
USEFORM("savecert.cpp", expCerts);
USEFORM("impcert.cpp", importCertFm);
USEFORM("savepubk.cpp", expPubKey);
USEFORM("savepk.cpp", expPrivKey);
USEFORM("impPubkey.cpp", impPubKeyFm);
USEFORM("tblbrow.cpp", tblView);
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
   Application->CreateForm(__classid(TexpCerts), &expCerts);
   Application->CreateForm(__classid(TimportCertFm), &importCertFm);
   Application->CreateForm(__classid(TexpPubKey), &expPubKey);
   Application->CreateForm(__classid(TexpPrivKey), &expPrivKey);
   Application->CreateForm(__classid(TimpPubKeyFm), &impPubKeyFm);
   Application->CreateForm(__classid(TtblView), &tblView);
   Application->Run();
 }
 catch (Exception &exception)
 {
   Application->ShowException(&exception);
 }
 return 0;
}
//---------------------------------------------------------------------------
