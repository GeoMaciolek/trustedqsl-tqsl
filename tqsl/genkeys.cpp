//---------------------------------------------------------------------------
#include <vcl.h>
#include <fcntl.h>
#include<io.h>

#pragma hdrstop

#include "genkeys.h"
#include "../tqsl.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TgenKeyFm *genKeyFm;
//---------------------------------------------------------------------------
__fastcall TgenKeyFm::TgenKeyFm(TComponent* Owner)
 : TForm(Owner)
{

}
//---------------------------------------------------------------------------
void __fastcall TgenKeyFm::Button1Click(TObject *Sender)
{
 char  callsign[100];
 //const char *secretFile = "c:\\src\\tpriv.prv";
 //const char *pubFile = "c:\\src\\tpriv.pub";
 const char *secretFile = "tpriv.prv";
 const char *pubFile = "tpriv.pub";
 TqslPublicKey pubKey;
 char		*secretKey;

 int rc;
  if (callSignEd->Text.Length() < 3)
   return;
  rc = tqslGenNewKeys(callSignEd->Text.c_str(),&secretKey,&pubKey);
  if (rc > 0)
    {
     Label2->Caption = rc;
     int fd;

     fd = open(pubFile,O_WRONLY|O_CREAT|O_TRUNC,0644);
     if (fd >= 0)
       {
         rc = write(fd,&pubKey,sizeof(pubKey));
         close(fd);
         if (rc != sizeof(pubKey))
	          {
	            //fprintf(stderr,"writing public file failed\n");
	            return;
	          }
       }
     else
       {
         //fprintf(stderr,"creating public file failed\n");
         return;
       }

  fd = open(secretFile,O_WRONLY|O_CREAT|O_TRUNC,0600);
  if (fd >= 0)
    {
      rc = write(fd,secretKey,strlen(secretKey));
      close(fd);
      if (rc != (int)strlen(secretKey))
	      {
	      //fprintf(stderr,"writing private file failed\n");
	      return;
	      }
    }
  else
    {
      //fprintf(stderr,"creating private file failed\n");
      return;
    }	 

  } 
}
//---------------------------------------------------------------------------
