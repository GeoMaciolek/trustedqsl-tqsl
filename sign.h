
/*
    TrustedQSL Digital Signature Library
    Copyright (C) 2001  Darryl Wagoner WA1GON wa1gon@arrl.net

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef SIGN_H
#define SIGN_H
#define ERROR -1
int 	sha1File(char *fname,unsigned char *);
int 	writeSign(char *fname,unsigned char *sig,int len);
int 	readSign(char *fname,unsigned char *sig,int len);
int	readBig(const char *fname,BIGNUM **bn);
char 	*bin2hex(const unsigned char *binStr,int len);
int 	writeSignAsc(char *fname, unsigned char *sig,int len);
void 	hex2bin(char *hexStr,unsigned char *binStr,int len);
int 	readSignAsc(char *fname,unsigned char *sig,int len);
#endif SIGN_H

