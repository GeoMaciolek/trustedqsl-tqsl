# J. Bloom, 11/4/2003
# TrustedQSL applications

LIBS = @LIBS@ $(openssl_static)
TQSLINC=@TQSLINC@
TQSLLIB=@TQSLLIB@
ZLIB=@ZLIB@
EXPAT=@EXPAT@
OPENSSL=@OPENSSL@
WXDIR=@WXDIR@
WXWIN_STATIC=@wxwin_static@
BUILD=@BUILD@
TOUCH=@TOUCH@
CP=@CP@
PERL=@PERL@
MKDIR=@MKDIR@
STRIP=@STRIP@
SED=@SED@
ECHO=@ECHO@

WXCONFIG="$(WXDIR)/bin/wx-config"
WXRSRC=$(WXDIR)/lib/lib`$(WXCONFIG) --basename`-`$(WXCONFIG) --version`.rsrc

datadir = @datadir@/TrustedQSL
helpdir = $(datadir)

AM_CXXFLAGS = -Wall `$(WXCONFIG) --cxxflags` -I$(TQSLINC) -I$(ZLIB)/include -I$(EXPAT)/include
AM_LDFLAGS = -L$(TQSLLIB) -L$(ZLIB)/lib -L$(EXPAT)/lib -L$(OPENSSL)/lib

BUILT_SOURCES = tqslbuild.h tqslcertbuild.h helpdir

EXTRA_PROGRAMS = tqsl.app tqslcert.app
bin_PROGRAMS = tqsl tqslcert @add_targets@

tqsl_app_SOURCES =

tqslcert_app_SOURCES =

tqsl_LDADD = @tqslcertresources@ @wxwin_libs@
tqsl_DEPENDENCIES = @tqslcertresources@

tqslcert_LDADD = @tqslcertresources@ @wxwin_libs@
tqslcert_DEPENDENCIES = @tqslcertresources@

tqslcert_SOURCES = tqslcert.cpp crqwiz.cpp dxcc.cpp certtree.cpp tqslcert_prefs.cpp \
	getpassword.cpp extwizard.cpp loadcertwiz.cpp wxutil.cpp \
	certtree.h tqslcertctrls.h util.h dxcc.h crqwiz.h getpassword.h extwizard.h \
	tqslcert_prefs.h \
	getpassword.h loadcertwiz.h wxutil.h \
	cert.xpm nocert.xpm broken-cert.xpm folder.xpm

tqsl_SOURCES = tqsl.cpp extwizard.cpp tqslwiz.cpp dxcc.cpp stationdial.cpp qsodatadialog.cpp \
	tqslvalidator.cpp tqsl_prefs.cpp wxutil.cpp tqslcert.h tqslapp.h \
	tqslwiz.h qsodatadialog.h tqslexcept.h tqslpaths.h stationdial.h dxcc.h tqsl_prefs.h \
	extwizard.h certtree.h tqslvalidator.h \
	left.xpm right.xpm bottom.xpm top.xpm

noinst_SCRIPTS =

CLEANFILES = tqslbuild.h tqslcertbuild.h tqsl-@WINVER@.exe ChangeLog.txt \
	quick.txt setup-is.iss LICENSE.txt

nobase_help_DATA = help/tqslcert/*.* help/tqslapp/*.*

EXTRA_DIST = tqslbuild.h.in tqslcertbuild.h.in \
	LICENSE TrustedQSL.spec.in TrustedQSL.spec \
	help/tqslapp/*.* help/tqslcert/*.* Info.plist.in quick \
	touchver.pl tqsl.ico tqsl-key1.ico setup.iss.in wpwd.bat tqslcert.rc.in

tqslbuild.h: tqslbuild.h.in Makefile
	$(PERL) $(srcdir)/touchver.pl $(srcdir)/tqslbuild.h.in BUILD $(BUILD)
	$(CP) $(srcdir)/tqslbuild.h.in tqslbuild.h
	$(TOUCH) $(srcdir)/tqslbuild.h.in

tqslcertbuild.h: tqslcertbuild.h.in Makefile
	$(PERL) $(srcdir)/touchver.pl $(srcdir)/tqslcertbuild.h.in BUILD $(BUILD)
	$(CP) $(srcdir)/tqslcertbuild.h.in tqslcertbuild.h
	$(TOUCH) $(srcdir)/tqslcertbuild.h.in

tqslcert.app: tqslcert Info.plist.in
	$(MKDIR) -p tqslcert.app
	$(MKDIR) -p tqslcert.app/Contents
	$(MKDIR) -p tqslcert.app/Contents/MacOS
	$(MKDIR) -p tqslcert.app/Contents/Resources
	$(MKDIR) -p tqslcert.app/Contents/Resources/Help
	
	$(CP) $(srcdir)/help/tqslcert/* tqslcert.app/Contents/Resources/Help
	$(CP) $(TQSLLIB)/../share/tqsl/config.xml tqslcert.app/Contents/Resources
	
	$(ECHO) -n "APPL????" >tqslcert.app/Contents/PkgInfo
	$(STRIP) tqslcert
	$(CP) tqslcert tqslcert.app/Contents/MacOS/tqslcert
	$(CP) $(WXRSRC) tqslcert.app/Contents/Resources/tqslcert.rsrc

	$(SED) -e "s/EXEFILENAME/tqslcert/g" \
	    -e "s/ICONFILE/wxMac.icns/g"  \
	    -e "s/VERSION/$(VERSION)/g"   \
	$(srcdir)/Info.plist.in > tqslcert.app/Contents/Info.plist

tqsl.app: tqsl Info.plist.in
	$(MKDIR) -p tqsl.app
	$(MKDIR) -p tqsl.app/Contents
	$(MKDIR) -p tqsl.app/Contents/MacOS
	$(MKDIR) -p tqsl.app/Contents/Resources
	$(MKDIR) -p tqsl.app/Contents/Resources/Help
	
	$(CP) $(srcdir)/help/tqslapp/* tqsl.app/Contents/Resources/Help
	$(CP) $(TQSLLIB)/../share/tqsl/config.xml tqsl.app/Contents/Resources
	
	$(ECHO) -n "APPL????" >tqsl.app/Contents/PkgInfo
	$(STRIP) tqsl
	$(CP) tqsl tqsl.app/Contents/MacOS/tqsl
	$(CP) $(WXRSRC) tqsl.app/Contents/Resources/tqsl.rsrc
	
	$(SED) -e "s/EXEFILENAME/tqsl/g" \
	    -e "s/ICONFILE/wxMac.icns/g"  \
	    -e "s/VERSION/$(VERSION)/g"   \
	$(srcdir)/Info.plist.in > tqsl.app/Contents/Info.plist

tqslcert-resources.o: tqslcert.rc tqsl.ico tqsl-key1.ico
	windres --include-dir=@WXDIR@/include -i tqslcert.rc -o tqslcert-resources.o $(RESFLAGS)

quick.txt:
	unix2dos -n $(srcdir)/quick quick.txt

ChangeLog.txt:
	unix2dos -n $(srcdir)/ChangeLog ChangeLog.txt

LICENSE.txt:
	unix2dos -n $(srcdir)/LICENSE LICENSE.txt

setup-is.iss: setup.iss
	unix2dos -n setup.iss setup-is.iss

installer: all tqsl-@WINVER@.exe

tqsl-@WINVER@.exe: tqsl$(EXEEXT) tqslcert$(EXEEXT) quick.txt ChangeLog.txt \
	setup-is.iss LICENSE.txt
	strip tqsl$(EXEEXT)
	strip tqslcert$(EXEEXT)
	cmd /c '"@INNODIR@/ISCC.exe" setup-is.iss' || exit 1
	
helpdir:
	test -e help || $(LN_S) $(srcdir)/help help
