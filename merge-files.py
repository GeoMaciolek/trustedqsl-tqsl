#!/usr/bin/env python

from sys import argv
from random import shuffle

if __name__=="__main__":
	if len(argv)!=5:
		print "usage: %s count big-file small-file fileout" % argv[0]
		exit(1)
		
	count=int(argv[1])
	file1f=argv[2]
	file2f=argv[3]
	fileoutf=argv[4]
	
	lineidx=range(count)
	shuffle(lineidx)
	
	with open(file1f, 'r') as file1:
		file1l=file1.readlines()
	
	with open(file2f, 'r') as file2:
		file2l=file2.readlines()
	
	with open(fileoutf, 'w') as fileout:
		for i in xrange(count):
			fileout.write(file1l[lineidx[i]])
			fileout.write(file2l[i])