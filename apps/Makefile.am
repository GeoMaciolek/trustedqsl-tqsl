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

BUILT_SOURCES = tqslbuild.h tqslcertbuild.h helpdir icondir

EXTRA_PROGRAMS = 

bin_PROGRAMS = tqsl tqslcert @add_targets@

tqsl_LDADD = @tqslresources@ @wxwin_libs@
tqsl_DEPENDENCIES = @tqslresources@

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
	touchver.pl key.ico tqsl.ico tqsl-key1.ico setup.iss.in tqslcert.rc.in \
	icons/*.* tqslicons.icns tqsl.desktop tqslcert.desktop

tqslbuild.h: tqslbuild.h.in Makefile
	$(PERL) $(srcdir)/touchver.pl $(srcdir)/tqslbuild.h.in BUILD $(BUILD)
	$(CP) $(srcdir)/tqslbuild.h.in tqslbuild.h
	$(TOUCH) $(srcdir)/tqslbuild.h.in

tqslcertbuild.h: tqslcertbuild.h.in Makefile
	$(PERL) $(srcdir)/touchver.pl $(srcdir)/tqslcertbuild.h.in BUILD $(BUILD)
	$(CP) $(srcdir)/tqslcertbuild.h.in tqslcertbuild.h
	$(TOUCH) $(srcdir)/tqslcertbuild.h.in

dmg: TrustedQSL-$(VERSION).dmg

TrustedQSL-$(VERSION).dmg: dist/TrustedQSL/tqslcert.app dist/TrustedQSL/tqsl.app
	echo "" | cat ChangeLog - $(TQSLINC)/../share/doc/ChangeLog > dist/ChangeLog.txt
	cp LICENSE dist/LICENSE.txt
	cp quick dist/Quick\ Start.txt
	hdiutil create -srcfolder ./dist -volname TrustedQSL -nouuid -noanyowners TrustedQSL-$(VERSION).dmg

dist/TrustedQSL/tqslcert.app: tqslcert $(srcdir)/tqslicons.icns $(srcdir)/Info.plist.in
	$(MKDIR) -p dist
	$(MKDIR) -p dist/TrustedQSL
	$(MKDIR) -p dist/TrustedQSL/tqslcert.app
	$(MKDIR) -p dist/TrustedQSL/tqslcert.app/Contents
	$(MKDIR) -p dist/TrustedQSL/tqslcert.app/Contents/MacOS
	$(MKDIR) -p dist/TrustedQSL/tqslcert.app/Contents/Resources
	$(MKDIR) -p dist/TrustedQSL/tqslcert.app/Contents/Resources/Help

	$(CP) $(srcdir)/help/tqslcert/* dist/TrustedQSL/tqslcert.app/Contents/Resources/Help
	$(CP) $(TQSLLIB)/../share/tqsl/config.xml dist/TrustedQSL/tqslcert.app/Contents/Resources
	$(CP) $(srcdir)/tqslicons.icns dist/TrustedQSL/tqslcert.app/Contents/Resources

	$(ECHO) -n "APPL????" >dist/TrustedQSL/tqslcert.app/Contents/PkgInfo
	$(STRIP) tqslcert
	$(CP) tqslcert dist/TrustedQSL/tqslcert.app/Contents/MacOS/tqslcert
	$(CP) $(WXRSRC) dist/TrustedQSL/tqslcert.app/Contents/Resources/tqslcert.rsrc

	$(SED) -e "s/EXEFILENAME/tqslcert/g" \
	    -e "s/ICONFILE/tqslicons.icns/g"  \
	    -e "s/VERSION/$(VERSION)/g"   \
	$(srcdir)/Info.plist.in > dist/TrustedQSL/tqslcert.app/Contents/Info.plist

dist/TrustedQSL/tqsl.app: tqsl $(srcdir)/tqslicons.icns $(srcdir)/Info.plist.in
	$(MKDIR) -p dist
	$(MKDIR) -p dist/TrustedQSL
	$(MKDIR) -p dist/TrustedQSL/tqsl.app
	$(MKDIR) -p dist/TrustedQSL/tqsl.app/Contents
	$(MKDIR) -p dist/TrustedQSL/tqsl.app/Contents/MacOS
	$(MKDIR) -p dist/TrustedQSL/tqsl.app/Contents/Resources
	$(MKDIR) -p dist/TrustedQSL/tqsl.app/Contents/Resources/Help

	$(CP) $(srcdir)/help/tqslapp/* dist/TrustedQSL/tqsl.app/Contents/Resources/Help
	$(CP) $(TQSLLIB)/../share/tqsl/config.xml dist/TrustedQSL/tqsl.app/Contents/Resources
	$(CP) $(srcdir)/tqslicons.icns dist/TrustedQSL/tqsl.app/Contents/Resources

	$(ECHO) -n "APPL????" >dist/TrustedQSL/tqsl.app/Contents/PkgInfo
	$(STRIP) tqsl
	$(CP) tqsl dist/TrustedQSL/tqsl.app/Contents/MacOS/tqsl
	$(CP) $(WXRSRC) dist/TrustedQSL/tqsl.app/Contents/Resources/tqsl.rsrc

	$(SED) -e "s/EXEFILENAME/tqsl/g" \
	    -e "s/ICONFILE/tqslicons.icns/g"  \
	    -e "s/VERSION/$(VERSION)/g"   \
	$(srcdir)/Info.plist.in > dist/TrustedQSL/tqsl.app/Contents/Info.plist

tqsl-resources.o: tqsl.rc key.ico tqsl.ico tqsl-key1.ico
	windres --include-dir=@WXDIR@/include -i tqsl.rc -o tqsl-resources.o $(RESFLAGS)

tqslcert-resources.o: tqslcert.rc key.ico tqsl.ico tqsl-key1.ico
	windres --include-dir=@WXDIR@/include -i tqslcert.rc -o tqslcert-resources.o $(RESFLAGS)

quick.txt:
	unix2dos -n $(srcdir)/quick quick.txt

ChangeLog.txt:
	unix2dos -n $(srcdir)/ChangeLog ChangeLog1.txt
	unix2dos -n $(TQSLINC)/../share/doc/ChangeLog ChangeLog2.txt
	echo -e "\r\n" | cat ChangeLog1.txt - ChangeLog2.txt > ChangeLog.txt

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

icondir:
	test -e icons || $(LN_S) $(srcdir)/icons icons
