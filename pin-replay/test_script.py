#!/usr/bin/env python

import subprocess

# this application list is a list of tuples (name, exec)
app_list = []


pintool = "/home/ahmetcelik/pin-2.12-55942-gcc.4.4.7-linux/pin -probe -t /home/ahmetcelik/pin-2.12-55942-gcc.4.4.7-linux/source/tools/MyToolProbe/obj-intel64/MyPinTool.so"
mytest = "/home/ahmetcelik/pin-2.12-55942-gcc.4.4.7-linux/source/tools/MyToolProbe/test"

def my_exec(cmd):
	subprocess.Popen(cmd, shell=True).wait()

def bcall(name, app):
	print "\nRecording %s" % (name)
	cmd = 'time %s -- %s' % (pintool, app)
	subprocess.Popen(cmd, shell=True).wait()
	print "\nReplaying %s" % (name)
	cmd = 'time %s -mode replay -- %s' % (pintool, app)
	subprocess.Popen(cmd, shell=True).wait()
	print "\nDiff %s" % (name)
	cmd = 'diff c d'
	subprocess.Popen(cmd, shell=True).wait()

enableMyTest   = 1
enableParsec   = 0

enableSimSmall  = 1
enableSimMedium = 1
enableKernels   = 1
threadCount 	= 4


if (enableSimSmall):

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/blackscholes/inst/amd64-linux.gcc/bin/blackscholes '+str(threadCount)+' /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/blackscholes/inputs/in_4K.txt prices.txt'
	name = "blackscholes (simsmall) "+str(threadCount)+" threads "
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/bodytrack/inst/amd64-linux.gcc-pthreads/bin/bodytrack /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/bodytrack/inputs/sequenceB_1 4 1 1000 5 0 '+str(threadCount)
	name = "bodytrack (simsmall) "+str(threadCount)+" threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/swaptions/inst/amd64-linux.gcc-pthreads/bin/swaptions -ns 16 -sm 10000 -nt '+str(threadCount)
	name = "swaptions (simsmall) "+str(threadCount)+" threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/x264/inst/amd64-linux.gcc/bin/x264 --quiet --qp 20 --partitions b8x8,i4x4 --ref 5 --direct auto --b-pyramid --weightb --mixed-refs --no-fast-pskip --me umh --subme 7 --analyse b8x8,i4x4 --threads '+str(threadCount)+' -o eledream.264 /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/x264/inputs/eledream_640x360_8.y4m'
	name = "(simsmall) "+str(threadCount)+" threads"
	app_list.append((name, app))

if (enableSimMedium):
	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/blackscholes/inst/amd64-linux.gcc/bin/blackscholes '+str(threadCount)+' /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/blackscholes/inputs/in_16K.txt prices.txt'
	name = "blackscholes (simmedium) "+str(threadCount)+" threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/bodytrack/inst/amd64-linux.gcc-pthreads/bin/bodytrack /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/bodytrack/inputs/sequenceB_2 4 2 2000 5 0 '+str(threadCount)
	name = "bodytrack (simmedium) "+str(threadCount)+" threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/swaptions/inst/amd64-linux.gcc-pthreads/bin/swaptions  -ns 32 -sm 20000 -nt '+str(threadCount)
	name = "swaptions (simmedium) "+str(threadCount)+" threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/ferret/inst/amd64-linux.gcc/bin/ferret /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/ferret/inputs/corel lsh /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/ferret/inputs/queries 10 20 '+str(threadCount)+' output.txt'
	name = "ferret (simmedium) "+str(threadCount)+" threads"
	app_list.append((name, app))


	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/x264/inst/amd64-linux.gcc/bin/x264 --quiet --qp 20 --partitions b8x8,i4x4 --ref 5 --direct auto --b-pyramid --weightb --mixed-refs --no-fast-pskip --me umh --subme 7 --analyse b8x8,i4x4 --threads '+str(threadCount)+' -o eledream.264 /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/x264/inputs/eledream_640x360_32.y4m'
	name = "(simmedium) "+str(threadCount)+" threads"
	app_list.append((name, app))

if (enableKernels):
	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/kernels/dedup/inst/amd64-linux.gcc-pthreads/bin/dedup -c -p -v -t '+str(threadCount)+' -i /home/onderkalaci/TOOLS/parsec-3.0/pkgs/kernels/dedup/inputs/media.dat -o output.dat.ddp'
	name = "(simmedium) "+str(threadCount)+" threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/kernels/canneal/inst/amd64-linux.gcc-pthreads/bin/canneal '+str(threadCount)+' 15000 2000 /home/onderkalaci/TOOLS/parsec-3.0/pkgs/kernels/canneal/run/200000.nets 64'
	name = "canneal (simmedium) "+str(threadCount)+" threads"
	app_list.append((name, app))


if(enableParsec):
	print 'running parsec tests'
	for name, app in app_list:
		bcall(name, app)

		for i in range(3):
			print '###################################'

if(enableMyTest):
	print 'running my tests'
	for i in range(9):
		bcall("Test"+str(i),mytest+str(i))
		for i in range(3):
			print '###################################'

#bcall(*app_list[3])

print "done"
