#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "tqsl.h"
#include "adif.h"

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


int main(int argc,char *argv[])
{

  if (argc != 2)
    {
      printf("usage: %s adif-file\n",argv[0]);
      return(1);
    }

  AdifBase	adif;

  int 		fd;
  char		*adifStr;
  int		fs;


  fs = getFileSz(argv[1]);
  if (fs <= 0)
    {
      return(1);
    }

  adifStr = new char[fs+1];

  fd = open(argv[1],O_RDONLY);
  if (fd < 0)
    {
      fprintf(stderr,"Problem opening ADIF %s\n",argv[1]);  
      return(5);
    }

  int rc;
  rc = read(fd,adifStr,fs);
  if (rc != fs)
    {
      fprintf(stderr,"Problem reading ADIF file %s\n",argv[1]);  
      return(6);
    }     
  adifStr[fs] = '\0';

  char *rcStr;
  char *adifAnchor;

  adifAnchor = adifStr;   // a pointer to the beginning so we can destory
                          // once we are done

  while((rcStr = adif.AdifParseNext(adifStr)) != NULL)
    {

      adifStr = rcStr;
    }

  delete adifAnchor;
  
  return(0);


}
