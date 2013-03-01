#!/usr/bin/env python

from sys import argv
from random import randint, random, choice

""" Generate x number of QSOs in ADIF format """

TEMPLATE="""<qso_date:8:d>%s <time_on:6>%s <call:%d>%s <band:%d>%s <mode:%d>%s <freq:%d>%s <eor>\n"""

#selections for testing purposes
bands=(("160M", "0.13600"), ("80M", "3.60000"), ("40M", "7.15000"), ("20M", "14.20000"), ("10M", "28.40000"))
modes=("RTTY", "SSB", "PSK31", "CW")

chars=[chr(x) for x in range(ord('A'), ord('Z')+1)]
charsnums=chars+[str(x) for x in range(10)]

def gencall():
	#calls according to tqsl are >=3 chars
	# have an optional prefix or a suffix with a / and something following it
	# and has at least one letter and number
	chars_before_call=randint(1, 3)
	number=randint(0, 9)
	chars_after_call=randint(1, 3)
	
	call=""
	for i in xrange(chars_before_call): call+=choice(chars)
	call+=str(number)
	for i in xrange(chars_after_call): call+=choice(chars)
	
	if random()<0.05:
		#prefix slash
		call=choice(charsnums)+"/"+call
		
		
	if random()<0.05:
		#suffix slash
		call+="/"+choice(charsnums)
		
	return call

def genqso():
	dateyr=randint(2011, 2014) # my cert dates
	datemo=randint(1, 12)
	dateday=randint(1, 28) #ignore mos with more than this for simplicity
	
	date="%04d%02d%02d" % (dateyr, datemo, dateday)
	
	timehr=randint(0, 23)
	timemin=randint(0, 59)
	timesec=randint(0, 59)
	
	time="%02d%02d%02d" % (timehr, timemin, timesec)
	
	band, freq=choice(bands)
	mode=choice(modes)
	
	call=gencall()
	
	return TEMPLATE % (date, time, len(call), call, len(band), band, len(mode), mode, len(freq), freq)

if __name__=="__main__":
	if (len(argv)!=3):
		print "usage: %s count output-file" % argv[0]
		exit(1)
		
	count=int(argv[1])
	out=argv[2]
	
	with open(out, 'w') as outfile:
		 for i in xrange(count):
			outfile.write(genqso())
			
			