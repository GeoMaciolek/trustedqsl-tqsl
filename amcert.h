
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
#ifndef AMCERT_H
#define AMCERT_H

const char pVal[] = "F14CF22587FDE3F8BC0AEEF105B8981A724B70E4EA82AA1CE0A"
    "ED02030E10FA6362E6418D41C7BDAF374A818006A7A690E95C44EF60B6B329602"
    "1E205D3D50A7EC8B7633B362399623D09047264EB86643AE47DA22F9DC61";

const char gVal[] = "1A337E37A6C7B32C0DDAE04CF93BADC79BD4D781C39E5E9D141"
     "14C1E8ACA0B8ABAB81F5685305597A23EB5F4CD55508729FCF32DF488FC05FE6BD"
     "AF5AAD3E1308BDFC2FE80E95FB5595B66CAC4C42A5A8A3F62449EE28392";

const char qVal[] = "CEAAC08334A9071F79DF95789C938A81BFB7F24D";

const int signSize=40;
const int CALL_SIZE=10;

struct PubicKey
{
  char			pkType;
  char			callSign[12];
  char			pubkeyNum[5];
  unsigned char		pkey[176];
};


struct AmCertExtern
{
  char			certType;
  char			issueDate[10];
  char			expireDate[10];
  char			caID[10];
  char			caPK[4];
  char			caCertNum[6];
  PubicKey		publicKey;
  unsigned char		signature[signSize];
};

struct qslSignature
{
  char			sigType;
  char			signature[signSize];
  AmCertExtern		amCert;
};

struct signedQSL
{
  char			qslType;
  char			yourCall[CALL_SIZE];
  char			theirCall[CALL_SIZE];
  char			qDate[8];
  char			qTime[6];
  char			myRst[3];
  char			theirRst[3];
  char			county[25];
  char			city[25];
  char			state[2];
  char			grid[6];
  char			cqZone[2];
  char			ituZone[2];
  char			freq[10];
  char			band[7];
  char			mode[10];
  char			itoa[10];
  qslSignature		signature;
};


#endif AMCERT_H


