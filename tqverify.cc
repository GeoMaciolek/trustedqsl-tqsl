
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tqsl.h"

extern int debugLevel;
char *readPrivKey(const char *fname);


static char cvsID[] = "$Id$";

int getFileSz(char *fname)
{
  
  struct stat sbuf;
  int rc;

  cvsID = cvsID;
  rc = stat(fname,&sbuf);
  if (rc < 0) 
    return(-1);
  return(sbuf.st_size);
}

int readFile(char *fName, unsigned char *buf,int len)
{
  int fd;
  fd = open(fName,O_RDONLY);
  if (fd < 0)
    {
      return(0);
    }

  int rc;
  rc = read(fd,buf,len);
  close(fd);
  if (rc == len)
    {
      return(0);
    }     
  return(1);
     
}

int main(int argc, char *argv[])
{  

  const int 		maxFileNameSize = 100;
  int 			optCnt=0;
  int 			c,errFlg=0;
  char			msgFile[maxFileNameSize];
  char			signFile[maxFileNameSize];
  TqslSignature 	signature;
  unsigned char		*msg;


  while ((c = getopt(argc, argv, "f:s:d")) != EOF)
    switch (c) 
      {
	case 'd':
	  debugLevel =5;
	  break;
      case 'f':
	strcpy(msgFile,optarg);
	optCnt++;
	break;
      case 's':
	strcpy(signFile,optarg);
	optCnt++;
	break;
      default:
	printf("invalid option -%c\n",c);
	errFlg++;
	break;
      }

  if (errFlg || optCnt != 2)
    {
      fprintf(stderr,"%s usage %s -s signature file -f message file\n",
	      argv[0],argv[0]);
      return(1);
    }


  int fs;

  fs = getFileSz(msgFile);
  if (fs <= 0)
    {
      fprintf(stderr,"Can't get file size of %s\n",msgFile);
      return(3);
    }

  msg = (unsigned char *)malloc(fs);
  if (msg == NULL)
    {
      fprintf(stderr,"Problem allocating memory \n");
      return(4);
    }

  int rc;
  rc = readFile(msgFile,msg,fs);
  if (rc != 0)
    {
      fprintf(stderr,"Problem reading message file %s\n",msgFile);  
      return(6);
    }     
     
  rc = readFile(signFile,(unsigned char *)&signature,sizeof(signature));
  if (rc != 0)
    {
      fprintf(stderr,"Problem reading signature file %s\n",signFile);  
      return(7);
    }       

  // we could and should also validate the cert this was signed with

  rc = tqslVerifyData(msg,&signature,fs);

  if (rc >0)
    printf("File %s has been validated as signed by %12.12s\n",msgFile,
	   signature.cert.data.publicKey.callSign);
  else
    printf("File %s could not be validated\n",msgFile);
  return(0);
  
}
