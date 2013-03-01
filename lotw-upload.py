#!/usr/bin/env python

from sys import argv, stderr, stdout
import re
import pycurl
import StringIO

UPL_REGEX=re.compile("<!-- .UPL. (.*?) -->")
UPLMESSAGE_REGEX=re.compile("<!-- .UPLMESSAGE. (.*?) -->")

def upload_file(filename):
    c=pycurl.Curl()
    result=StringIO.StringIO()
    c.setopt(pycurl.URL, "https://p1k.arrl.org/lotw/upload")
    c.setopt(pycurl.WRITEFUNCTION, result.write)
    c.setopt(pycurl.HTTPPOST, [('upfile', (pycurl.FORM_FILE, filename))])
    

    try: 
        c.perform()
    except pycurl.error as e:
        return False, "CURL error %d: %s" % (e.args[0], e.args[1].strip())
    ans=result.getvalue()

    #<!-- .UPL.  accepted -->
    #<!-- .UPLMESSAGE. File queued for processing -->

    upl=UPL_REGEX.search(ans)
    if upl==None:
        return False, "Found an unexpected response; is LoTW down?"

    upl=upl.group(1).strip()
    uplmessage=UPLMESSAGE_REGEX.search(ans)
    if uplmessage!=None:
        uplmessage=uplmessage.group(1)

    if upl.lower()=='accepted':
        return True, "Success: %s" % uplmessage
    else:
        return False, "Result was %s. Message: %s" % (upl, uplmessage)

if __name__=="__main__":
    if len(argv)!=2:
        print "usage: %s <filename>.tq8" % argv[0]
        exit(1)
    success, info=upload_file(argv[1])
    if not success:
        stderr.write("Upload error: %s\n" % info)
        exit(1)
    else:
        stdout.write("%s\n" % info)
        exit(0)
