# $Id$

CC = gcc
CXX = g++
CXXFLAGS = -g -Wall
LIB = -lcrypto
COMOBJ = fileio.o convert.o tqsllib.o 

all:	sign verify qgenkey gencert chkcert dumpcert dumppk

sign:	sign.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o dsasign sign.o  $(COMOBJ) $(LIB)

dumpcert:	dumpcert.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o dumpcert dumpcert.o  $(COMOBJ) $(LIB)

dumppk:	dumppk.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o dumppk dumppk.o  $(COMOBJ) $(LIB)

qgenkey:	qgenkey.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o qgenkey qgenkey.o  $(COMOBJ) $(LIB)

chkcert:	chkcert.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o chkcert chkcert.o  $(COMOBJ) $(LIB)

gencert:	gencert.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o gencert gencert.o  $(COMOBJ) $(LIB)

verify:	verify.o  $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o verify verify.o  $(COMOBJ) $(LIB)

dsatest.o:	dsatest.cc
	$(CXX) -c dsatest.cc


# DO NOT DELETE


