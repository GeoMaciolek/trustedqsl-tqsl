# $Id$

CC = gcc
CXX = g++
CXXFLAGS = -g -Wall
LIB = -lcrypto
COMOBJ = fileio.o convert.o tqsl.o readpriv.o

#all:	tqgenkey tqgencert tqchkcert tqdumpcert tqdumppk chkdate tqsignfile \
#	tqverify adifsign
all:	tqgenkey tqgencert tqchkcert tqdumpcert tqdumppk chkdate tqsignfile \
	tqverify 

adifsign: adifsign.o adif.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o adifsign adifsign.o  adif.o $(COMOBJ) $(LIB)

tqsignfile: tqsignfile.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o tqsignfile tqsignfile.o  $(COMOBJ) $(LIB)

tqverify: tqverify.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o tqverify tqverify.o  $(COMOBJ) $(LIB)

chkdate: chkdate.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o chkdate chkdate.o  $(COMOBJ) $(LIB)

tqdumpcert:	dumpcert.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o tqdumpcert dumpcert.o  $(COMOBJ) $(LIB)

tqdumppk:	dumppk.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o tqdumppk dumppk.o  $(COMOBJ) $(LIB)

tqgenkey:	qgenkey.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o tqgenkey qgenkey.o  $(COMOBJ) $(LIB)

tqchkcert:	chkcert.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o tqchkcert chkcert.o  $(COMOBJ) $(LIB)

tqgencert:	gencert.o $(COMOBJ)
	$(CXX) $(CXXFLAGS) -o tqgencert gencert.o  $(COMOBJ) $(LIB)


clean:
	rm -f sign verify tqgenkey tqgencert tqchkcert tqdumpcert tqdumppk *.o *~ core *.exe

# DO NOT DELETE


