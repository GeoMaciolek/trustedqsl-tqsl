#!/bin/sh

TQSLLIBPATH=`pwd`/src/libtqsllib.dylib
WORKDIR=`mktemp -d /tmp/tqsl.XXXXX` || exit 1

/bin/echo -n "Copying files to image directory... "

mkdir $WORKDIR
cp apps/ChangeLog.txt $WORKDIR/ChangeLog.txt
cp LICENSE.txt $WORKDIR/
cp apps/quick "$WORKDIR/Quick Start.txt"
mkdir $WORKDIR/TrustedQSL
cp -r apps/tqsl.app $WORKDIR/TrustedQSL
cp -r apps/tqslcert.app $WORKDIR/TrustedQSL

/bin/echo "done"

/bin/echo -n "Installing the libraries and tweaking the binaries to look for them... "

for app in tqsl tqslcert
do
    cp $TQSLLIBPATH $WORKDIR/TrustedQSL/$app.app/Contents/MacOS
    install_name_tool -change $TQSLLIBPATH @executable_path/libtqsllib.dylib $WORKDIR/TrustedQSL/$app.app/Contents/MacOS/$app
    cp src/config.xml $WORKDIR/TrustedQSL/$app.app/Contents/Resources
done

/bin/echo "done"

/bin/echo -n "Installing the help... "

cp -r apps/help/tqslapp $WORKDIR/TrustedQSL/tqsl.app/Contents/Resources/Help
cp -r apps/help/tqslcert $WORKDIR/TrustedQSL/tqslcert.app/Contents/Resources/Help

/bin/echo "done"

/bin/echo "Creating the disk image..."

#hdiutil uses dots to show progress
hdiutil create -ov -srcfolder $WORKDIR -volname "TrustedQSL v1.14" tqsl-114.dmg

/bin/echo -n "Cleaning up temporary files.. "
rm -r $WORKDIR
/bin/echo "Finished!"
