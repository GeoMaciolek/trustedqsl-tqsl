

CC = gcc
CXX = g++
CXXFLAGS = -g
LIB = -lcrypto
COMOBJ = fileio.o convert.o

all:	sign verify qgenkey

sign:	sign.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o dsasign sign.o  $(COMOBJ) $(LIB)

qgenkey:	qgenkey.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o qgenkey qgenkey.o  $(COMOBJ) $(LIB)

verify:	verify.o  $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o verify verify.o  $(COMOBJ) $(LIB)

dsatest.o:	dsatest.cc
	$(CXX) -c dsatest.cc

