# tqsllib Makefile.in
# J. Bloom, 16 Feb 2005

LIBS = @LIBS@
PLATFORM = @platform@

datadir = @datadir@/tqsl
docdir = @datadir@/doc

DLLVER=@DLLVER@
INCLUDES = -I$(OPENSSL)/include -I$(EXPAT)/include -I$(ZLIB)/include

AM_CXXFLAGS = -g3 -O -Wall $(LOTW_SERVER) $(PLATFORM) -DCONFDIR=\"$(datadir)\"
AM_LDFLAGS = @ldflags@ -L$(OPENSSL)/lib -L$(EXPAT)/lib -L$(ZLIB)/lib

EXTRA_LIBRARIES = libtqsllibd.a
lib_LIBRARIES = @add_libraries@
lib_LTLIBRARIES = libtqsllib.la
libtqsllib_la_SOURCES = tqsllib.cpp openssl_cert.cpp adif.cpp xml.cpp location.cpp \
	tqslconvert.cpp cabrillo.cpp location.h tqsllib-doc.h xml.h \
	openssl_cert.h tqslexc.h
include_HEADERS = tqsllib.h tqslerrno.h cabrillo.h adif.h tqslconvert.h
libtqsllib_la_LDFLAGS = -version-info @SHLIBVER@:@TQSLLIB_VERSION_MINOR@:0

EXTRA_PROGRAMS = gen_crq load_cert station_loc converter
noinst_PROGRAMS = $(make_extra)
bin_PROGRAMS = dumptqsldata

libtqsllibd_a_SOURCES =

dumptqsldata_SOURCES = dumptqsldata.cpp
dumptqsldata_LDADD = @app_library@

gen_crq_SOURCES = gen_crq.cpp
gen_crq_LDADD = @app_library@

load_cert_SOURCES = load_cert.cpp
load_cert_LDADD = @app_library@

station_loc_SOURCES = station_loc.cpp
station_loc_LDADD = @app_library@

converter_SOURCES = converter.cpp
converter_LDADD = @app_library@

data_DATA = config.xml

if NODOC
docfiles =
else
docfiles = LICENSE ChangeLog
endif
doc_DATA = $(docfiles)

EXTRA_DIST = config.xml LICENSE tqsllib.spec.in tqsllib.spec \
	tqsllib.depends doxygen Doxyfile tqsllib.css

MOSTLYCLEANFILES = tqsllib$(DLLVER).dll tqsllib.def tqsllib.exports

tqsllib-resources.o: tqsllib.rc
	windres -i tqsllib.rc -o tqsllib-resources.o $(RESFLAGS)

libtqsllibd.a: libtqsllib.la tqsllib-resources.o
	dllwrap.exe --add-stdcall-alias --output-def tqsllib.def \
	    --output-exp tqsllib.exports \
		--dllname tqsllib$(DLLVER).dll \
		--output-lib libtqsllibd.a \
		tqsllib-resources.o .libs/libtqsllib.a \
		-L$(OPENSSL)/lib -L$(EXPAT)/lib -lexpat -L$(ZLIB)/lib \
		-lz -lcrypto -lgdi32 -lstdc++
	strip tqsllib$(DLLVER).dll

rsync-exclude:
	echo "$(PROGRAMS)" | perl -p -e 's/ /\n/g' > rsync-exclude
	
install-exec-local:
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	test "$(build_os)" != "mingw32" || $(INSTALL_PROGRAM) tqsllib$(DLLVER).dll $(DESTDIR)$(bindir)/tqsllib$(DLLVER).dll

