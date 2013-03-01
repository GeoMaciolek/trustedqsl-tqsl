#!/usr/bin/env python
from sys import argv

def fixline(line):
	if line.startswith('_'):
		line=line.strip()
		return "%s\n%s=%s\n%s=%s\n" % (line, line[1:], line, line[1:][0:line.rindex('@')-1], line)
	return line

if __name__=="__main__":
	if len(argv)!=3:
		print "usage %s <input.def> <output.def>" % argv[0]
		exit(1)
		
	with open(argv[1], 'r') as input:
		with open(argv[2], 'w') as output:
			for line in input:
				output.write(fixline(line))