
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "tqsl.h"

char *readPrivKey(const char *fname);
extern int debugLevel;

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

int main(int argc, char *argv[])
{  

  const int 		maxFileNameSize = 100;
  int 			optCnt=0;
  int 			c,errFlg=0;
  char			msgFile[maxFileNameSize];
  char			secretFile[maxFileNameSize];
  char			outFile[maxFileNameSize];
  char			certFile[maxFileNameSize];
  TqslSignature 	signature;
  char			*privKey;
  unsigned char		*msg;
  TqslCert		cert;

  while ((c = getopt(argc, argv, "f:s:o:c:d")) != EOF)
    switch (c) 
      {
      case 'd':
	debugLevel = 5;
	break;
      case 'f':
	strcpy(msgFile,optarg);
	optCnt++;
	break;
      case 's':
	strcpy(secretFile,optarg);
	optCnt++;
	break;
      case 'o':
	strcpy(outFile,optarg);
	optCnt++;
	break;
      case 'c':
	strcpy(certFile,optarg);
	optCnt++;
	break;
      default:
	printf("invalid option -%c\n",c);
	errFlg++;
	break;
      }

  if (errFlg || optCnt != 4)
    {
      fprintf(stderr,"%s usage %s -c cert file -f msg file -s private key file -o output file\n",
	      argv[0],argv[0]);
      return(1);
    }

  privKey = readPrivKey(secretFile);
  if (privKey == NULL)
    {
      fprintf(stderr,"Problem reading private key file %s\n",secretFile);
      return(2);
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

  int fd;

  fd = open(msgFile,O_RDONLY);
  if (fd < 0)
    {
      fprintf(stderr,"Problem opening private key file %s\n",msgFile);  
      return(5);
    }

  int rc;
  rc = read(fd,msg,fs);
  if (rc != fs)
    {
      fprintf(stderr,"Problem reading private key file %s\n",msgFile);  
      return(6);
    }     
     
  rc = tqslReadCert(certFile,&cert);
  if (rc == 0)
    {
      fprintf(stderr,"Problem reading cert key file %s\n",certFile);  
      return(7);
    }       

  rc = tqslSignData(privKey,msg,&cert,&signature,fs);
  
  FILE 	*fp;

  fp = fopen(outFile,"w");
  if (fp == NULL)
    {
      fprintf(stderr,"Problem writing signature file %s\n",outFile);  
      return(7);
    }

  fwrite(&signature,sizeof(signature),1,fp);
  fclose(fp);


  
}
