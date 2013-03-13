#!/bin/sh

TQSLLIBPATH=`pwd`/src/libtqsllib.dylib

/bin/echo -n "Copying files to image directory... "

mkdir tmp-dmg
cp apps/ChangeLog.txt tmp-dmg/ChangeLog.txt
cp LICENSE.txt tmp-dmg/
cp apps/quick "tmp-dmg/Quick Start.txt"
mkdir tmp-dmg/TrustedQSL
cp -r apps/tqsl.app tmp-dmg/TrustedQSL
cp -r apps/tqslcert.app tmp-dmg/TrustedQSL

/bin/echo "done"

/bin/echo -n "Installing the libraries and tweaking the binaries to look for them... "

for app in tqsl tqslcert
do
    cp $TQSLLIBPATH tmp-dmg/TrustedQSL/$app.app/Contents/MacOS
    install_name_tool -change $TQSLLIBPATH @executable_path/libtqsllib.dylib tmp-dmg/TrustedQSL/$app.app/Contents/MacOS/$app
    cp src/config.xml tmp-dmg/TrustedQSL/$app.app/Contents/Resources
done

/bin/echo "done"

/bin/echo -n "Installing the help... "

cp -r apps/help/tqslapp tmp-dmg/TrustedQSL/tqsl.app/Contents/Resources/Help
cp -r apps/help/tqslcert tmp-dmg/TrustedQSL/tqslcert.app/Contents/Resources/Help

/bin/echo "done"

/bin/echo "Creating the disk image..."

#hdiutil uses dots to show progress
hdiutil create -srcfolder tmp-dmg -volname "TrustedQSL v1.14" tqsl-114.dmg

/bin/echo -n "Cleaning up temporary files.. "
rm -r tmp-dmg
/bin/echo "Finished!"
