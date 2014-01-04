#!/usr/bin/env python

import subprocess

parsec3 ='/home/celikeins/parsec-3.0/pkgs/apps/'

ferret=parsec3+'ferret/inputs/input_test/'

def bcall(cmd):
	print '\n','+',cmd,'\n'
	subprocess.Popen(cmd,shell=True).wait()

bcall('mkdir -p temp')
cmd = "time "


app = parsec3+'ferret/inst/amd64-linux.gcc/bin/ferret '+ferret+'corel lsh '+ferret+'queries 5 5 1 '+ferret+'output.txt' # (simtest)

bcall(app+" > temp/ferret.out 2>&1")
