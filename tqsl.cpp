

/*
    TrustedQSL Digital Signature Library
    Copyright (C) 2001  Darryl Wagoner WA1GON wa1gon@arrl.net

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation
    version 2.1 of the License.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#ifdef __BCPLUSPLUS__
#include <io.h>
#else
#include <unistd.h>
#endif
#include <ctype.h>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/dsa.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#include "tqsl.h"
#include "sign.h"

#if 0
#ifdef __BCPLUSPLUS__
__declspec(dllimport) DSA * _DSA_new(void);
#endif
#endif
//
// All functions return 0 for failed and no zero on success
//
int debugLevel=0;

static char cvsID[] = "$Id$";

static void packCert(TqslCert *cert,char *certStr)
{
 
  char tmpStr[50];

  certStr[0]=cert->data.certType;
  certStr[1] = '\0';
  strcat(certStr,cert->data.issueDate);
  strcat(certStr,cert->data.expireDate);
  strcat(certStr,cert->data.caID);
  strcat(certStr,cert->data.caCall1);
  strcat(certStr,cert->data.caCall2);
  strcat(certStr,cert->data.caCall3);

  strcat(certStr,cert->data.caCertNum);

  sprintf(tmpStr,"%c",cert->data.publicKey.pkType);
  strncat(certStr,tmpStr,1);
  strcat(certStr,cert->data.publicKey.callSign);
  strcat(certStr,cert->data.publicKey.pubkeyNum);

  if (debugLevel > 4)
    fprintf(stderr,"pkey len is %d\n",
	    strlen((char *)cert->data.publicKey.pkey));
  strcat(certStr,(char *)cert->data.publicKey.pkey);
}



static void stripNL(char *buf)
{
  int i,j;
  // strip out any newline or cr
  j=0;
  for(i=0;i< (int)strlen(buf);i++)
    {
      if (buf[i] == '\n' || buf[i] ==  '\r')
	continue;
      buf[j] = buf[i];
      j++;

    }
  buf[j] = '\0';   // tie the ribbon on it
}

static int getFileSize(const char *fname)
{
  struct stat sbuf;
  int rc;

  cvsID = cvsID;
  rc = stat(fname,&sbuf);
  if (rc < 0) 
    return(-1);
  return(sbuf.st_size);

}


char *tqslSigToStr(TqslSignature *)
{
  return(NULL);
}

int  tqslStrToSig(TqslSignature *,const char *)
{
  return(0);
}

char *tqslPubKeyToStr(TqslPublicKey *pubkey)
{
  char pkStr[500];

  sprintf(pkStr,"%c|%s|%s|%s",
	  pubkey->pkType,pubkey->callSign,pubkey->pubkeyNum,pubkey->pkey); 
  return(strdup(pkStr));

}

int tqslStrToPubKey(TqslPublicKey *pubkey,const char *buf)
{
  char	*tok;
  stripNL((char *)buf);  // clean out any NL or CR lurking around

  if (buf[0] == '1')  //  Type 1 Public key
    {

      tok = strtok((char *)buf,"|");
      if (tok == NULL)
	  return(0);
      pubkey->pkType = *tok;

      tok = strtok(NULL,"|");
      if (tok == NULL)
	  return(0);
      strcpy(pubkey->callSign,tok);

      tok = strtok(NULL,"|");
      if (tok == NULL)
	  return(0);

      strcpy(pubkey->pubkeyNum,tok);

      tok = strtok(NULL,"|");
      if (tok == NULL)
	  return(0);

      strcpy((char *)pubkey->pkey,tok);
      return(1);
    }
  return(0);  // unknown type
}

char * 	tqslCertToStr(TqslCert *cert)
{
  char certStr[500];

  // first do cert except for pub key 
  sprintf(certStr,"%c|%s|%s|%s|%s|%s|%s|%s|%c|%s|%s|%s|%s",
	  cert->data.certType,cert->data.issueDate,
	  cert->data.expireDate,cert->data.caID,cert->data.caCall1,
	  cert->data.caCall2,cert->data.caCall3,cert->data.caCertNum,
	  cert->data.publicKey.pkType,cert->data.publicKey.callSign,
	  cert->data.publicKey.pubkeyNum,cert->data.publicKey.pkey,
	  cert->signature);

  return(strdup(certStr));
}

int tqslStrToCert(TqslCert *cert,const char *buf)
{
  char *tok;

  // make sure it is the correct type
  if (buf[0] != '0' && buf[0] != '1')
      return(0);

  // cert type
  tok = strtok((char *)buf,"|");
  if (tok == NULL)
      return(0);
  
  cert->data.certType = tok[0];

  // issue Date
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy(cert->data.issueDate,tok);

  // expire Date
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy(cert->data.expireDate,tok);


  // CA id
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy(cert->data.caID,tok);

  // CA signer 1
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy(cert->data.caCall1,tok);


  // CA signer 2
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy(cert->data.caCall2,tok);


  // CA signer 3
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy(cert->data.caCall3,tok);

  // CA cert #
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy(cert->data.caCertNum,tok);


  // Pk type
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  cert->data.publicKey.pkType = tok[0];


  // call sign
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy(cert->data.publicKey.callSign,tok);


  // call sign
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy(cert->data.publicKey.pubkeyNum,tok);


  // public key
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy((char *)cert->data.publicKey.pkey,tok);

  // signature
  tok = strtok(NULL,"|");
  if (tok == NULL)
      return(0);
  strcpy(cert->signature,tok);
  return(1);

}

int tqslReadCert(const char *fname,TqslCert *cert)
{
  int		rc;
  int		fd;
  char		*buf;
#ifndef __BCPLUSPLUS__
  cvsID = cvsID;  // avoid warnigns
#endif


  int fs;

  fs = getFileSize(fname);
  if (fs < 10)
    {
      return(0);
    }

  buf = (char *)malloc(fs+1);

  if (buf == NULL)
    return(0);

  fd = open(fname,O_RDONLY);
  if (fd < 0)
    {
      free(buf);
      return(0);
    }

  rc = read(fd,buf,fs);
  close(fd);  // we are done with it.
  if (rc != fs)
    {
      free(buf);
      return(0);
    }

  rc = tqslStrToCert(cert,buf);
  free(buf);
  return(rc);
}
int tqslWriteCert(const char *fname,TqslCert *cert)
{


  FILE		*fd;
  char		*certStr;

  certStr = tqslCertToStr(cert);
  if (certStr == NULL)
    return(0);

  fd = fopen(fname,"w");
  if (fd == NULL)
    {
      free(certStr);
      return(0);
    }

  fprintf(fd,"%s\n",certStr);
  free(certStr);

  fclose(fd);  // we are done with it.
  return(1);
}

// tqslGenNewKeys
//
// return 0 on error.  Even if 1 file was successful written.
//

int tqslGenNewKeys(const char *callSign,char **privKey,
	      TqslPublicKey  *pubKey)
{
  DSA    *dsa;
  char	*p;
  int 	i;
  char	newCall[CALL_SIZE+1];
  int 	callLen;
  int   rc;

  //
  // make sure call sign isn't too long
  //
  memset(newCall,0,CALL_SIZE+1);
  if ((int)strlen(callSign) > CALL_SIZE)
    callLen =  CALL_SIZE;
  else
    callLen = strlen(callSign);

  for(i=0;i < callLen;i++)
    newCall[i] = toupper(callSign[i]);

  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(0);
    }

  // read common public values

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  rc = DSA_generate_key(dsa);
  if (rc == 0)
    return(0);

  //  initPublicKey(pubKey);
  
  pubKey->pkType = '1';
  p = BN_bn2hex(dsa->pub_key);


  strcpy(pubKey->callSign,newCall);
  strcpy((char *)pubKey->pkey,p);
  pubKey->pubkeyNum[0] = '1';


  // get private Key
  *privKey = BN_bn2hex(dsa->priv_key);
  return(1);

}


int tqslCheckCert(TqslCert *cert,TqslCert *CACert,int chkCA)
{
  DSA    	*dsa;

  int		rc;
  char		certStr[700];


  if (cert->data.certType == '0' || CACert == NULL)  // then selfsigned
    CACert = cert;

  packCert(cert,certStr);   // pack the cert into standard format

  if (debugLevel > 1)
    fprintf(stderr,"certStr: %s\n",certStr);

  // do we need to check the CA Cert?
  if (chkCA > 0)
    {
      rc = tqslCheckCert(CACert,NULL,0);
      if (rc != 1)  // ca cert not valid
	return(rc);
    }
  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(0);
    }

  // read common public values

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  char pkhex[200];
  strncpy(pkhex,(const char *)CACert->data.publicKey.pkey,pubKeySize);
  pkhex[pubKeySize] = '\0';


  BN_hex2bn(&dsa->pub_key,pkhex);

  unsigned char hash[40];
  SHA1((const unsigned char *)certStr,strlen(certStr),hash);


  if (debugLevel > 1)
    fprintf(stderr,"hash: %s\n",bin2hex(hash,SHA_DIGEST_LENGTH));

  unsigned char 	sigRet[500];
  int			sigLen;

  sigLen = strlen(cert->signature);
  hex2bin(cert->signature,sigRet,sigLen);
  
  sigLen = (sigLen/2);
  rc = DSA_verify(1,hash,SHA_DIGEST_LENGTH,sigRet,sigLen,dsa);
  DSA_free(dsa);
  ERR_remove_state(0);  // clean up error queue

  if (rc >= 0)
    return(rc);
  else
    return(0);
}
int tqslSignCert(TqslCert *cert,const char *caPrivKey,
                 const char *caId,TqslPublicKey *pubKey,
                 const char *certNum,const char *issueDate,
                 char *expireDate,int selfSign,char *call1,
			     char *call2,char *call3)
{

  DSA    	*dsa;
  int		rc;
  char		certStr[500];

  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(0);
    }

  // read common public values

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  rc = BN_hex2bn(&dsa->priv_key,caPrivKey);
  if (rc == 0)
    {
      DSA_free(dsa);
      return(0);
    }

  memset(cert,0,sizeof(TqslCert));
  memset(certStr,0,sizeof(certStr));
  
  if (selfSign == 1)
    cert->data.certType = '0';
  else
    cert->data.certType = '1';

  cert->data.publicKey = *pubKey;
  
  if (debugLevel > 4)
    fprintf(stderr,"pkey len is %d\n",
	    strlen((char *)cert->data.publicKey.pkey));

  strcpy(cert->data.issueDate,issueDate);
  strcpy(cert->data.expireDate,expireDate);
  strcpy(cert->data.caID,caId);
  strcpy(cert->data.caCertNum,certNum);
  strcpy(cert->data.caCall1,call1);
  strcpy(cert->data.caCall2,call2);
  strcpy(cert->data.caCall3,call3);

  packCert(cert,certStr);

  unsigned char hash[40];
  if (debugLevel > 1)
    fprintf(stderr,"certStr: %s\n",certStr);
  SHA1((unsigned char *)certStr,strlen(certStr),hash);

  unsigned char sigRet[500];
  unsigned int sigLen;

  if (debugLevel > 0)
    {
      char *p;
      p = bin2hex(hash,SHA_DIGEST_LENGTH);
      printf("hash: %s\n",p); 
      printf("amcert: %s\n", (char *)cert);
    }

  rc = DSA_sign(1,hash,SHA_DIGEST_LENGTH,sigRet,&sigLen,dsa);
  
  if (debugLevel > 3)
    {
      char *hv;
      printf("siglen = %d\n",sigLen);
      hv = bin2hex(sigRet,sigLen);
      printf("sigret: %s\n",hv);
    }

  char *sigStr;
  sigStr = bin2hex(sigRet,sigLen);
  strcpy(cert->signature,sigStr);
  //  sprintf(tmpStr,"%d",sigLen*2);
  //  memcpy(cert->sigSize,tmpStr,3);
  DSA_free(dsa);

  if (rc >= 0)
    return(rc);
  else
    return(0);

}

int tqslReadPub(const char *fname,TqslPublicKey *pubkey)
{

  int		rc;
  int		fd;
  char		*buf;
  int		fs;
 
  fs = getFileSize(fname);
  if (fs < 10)
    {
      return(0);
    }

  buf = (char *)malloc(fs+1);

  if (buf == NULL)
    return(0);

  buf[fs] = '\0';

  fd = open(fname,O_RDONLY);
  if (fd < 0)
      return(0);

  rc = read(fd,buf,fs);
  close(fd);  // we are done with it.
  if (rc < fs)  // make sure we read the complete file
      return(0);

  rc = tqslStrToPubKey(pubkey,buf);
  free(buf);
  return(rc);

}

int tqslWritePub(const char *fname,TqslPublicKey *pubkey)
{

  FILE		*fout;
  char		*pkStr;

  fout = fopen(fname,"w");
  if (fout == NULL)
    return(0);


  pkStr = tqslPubKeyToStr(pubkey);
      
  if (pkStr == NULL)
    {
      fclose(fout);
      return(0);
    }

  fprintf(fout,"%s\n",pkStr);
  fclose(fout);
  free(pkStr);
  return(1);
}

int tqslSignData(const char *privKey,const unsigned char *data,
		 TqslCert *cert,TqslSignature *signature, int len)
{

  DSA    	*dsa;
  int		rc;

  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(0);
    }

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  rc = BN_hex2bn(&dsa->priv_key,privKey);
  if (rc == 0)
    {
      DSA_free(dsa);
      return(0);
    } 


  memset(signature,0,sizeof(TqslSignature));
  memset(signature,' ',sizeof(TqslSignature));


  unsigned char hash[40];


  SHA1(data,len,hash);

  unsigned char 	sigRet[500];
  unsigned int 		sigLen;
  char 			*hexSign;

  rc = DSA_sign(1,hash,SHA_DIGEST_LENGTH,sigRet,&sigLen,dsa);
  DSA_free(dsa);
  if (rc <= 0)
    return(0);

  hexSign = bin2hex(sigRet,sigLen);
  memcpy(&signature->signature,hexSign,strlen(hexSign));
  signature->sigType = '1';
  signature->cert = *cert;

  return(1);

  
}
int tqslVerifyData(unsigned char *data,TqslSignature *signature,int len)
{
  DSA    	*dsa;
  int		rc;

  dsa = DSA_new();
  if (dsa == NULL)
    {
      return(0);
    }

  BN_hex2bn(&dsa->p,pVal);
  BN_hex2bn(&dsa->g,gVal);
  BN_hex2bn(&dsa->q,qVal);

  char pkhex[200];
  strncpy(pkhex,(const char *)signature->cert.data.publicKey.pkey,pubKeySize);
  pkhex[pubKeySize] = '\0';
  BN_hex2bn(&dsa->pub_key,pkhex);

  unsigned char hash[40];

  SHA1(data,len,hash);

  unsigned char 	sigRet[500];
  unsigned int 		sigLen;

  sigLen = strlen(signature->signature);
  hex2bin(signature->signature,sigRet,sigLen);
  
  sigLen = (sigLen/2);

  rc = DSA_verify(1,hash,SHA_DIGEST_LENGTH,sigRet,sigLen,dsa);
  DSA_free(dsa);
  if (rc <= 0)
    return(0);

  return(1);

}
