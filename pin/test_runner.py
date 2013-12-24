#!/usr/bin/env python

import subprocess

# this application list is a list of tuples (name, exec)
app_list = []
output_file_name = "DR_OUT"

pintool = "/home/gokhankici/pin-2.12-55942-gcc.4.4.7-linux/pin -t /home/gokhankici/pin-2.12-55942-gcc.4.4.7-linux/source/tools/datarace/pin/obj-intel64/MyPinTool.so"

def my_exec(cmd):
	subprocess.Popen(cmd, shell=True).wait()

def bcall(name, app):
	print "\nRunning %s" % (name)
	cmd = 'time %s -- %s' % (pintool, app)
	subprocess.Popen(cmd, shell=True).wait()

enableSimSmall  = 1
enableSimMedium = 0
enableKernels   = 0

if (enableSimSmall):

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/blackscholes/inst/amd64-linux.gcc/bin/blackscholes 4 /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/blackscholes/inputs/in_4K.txt prices.txt'
	name = "blackscholes (simsmall) 4 threads "
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/bodytrack/inst/amd64-linux.gcc-pthreads/bin/bodytrack /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/bodytrack/inputs/sequenceB_1 4 1 1000 5 0 9'
	name = "bodytrack (simsmall) 9 threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/fluidanimate/inst/amd64-linux.gcc-pthreads/bin/fluidanimate 8 5 /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/fluidanimate/inputs/in_35K.fluid out.fluid'
	name = "fluidanimate (simsmall) 8 threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/swaptions/inst/amd64-linux.gcc-pthreads/bin/swaptions -ns 16 -sm 10000 -nt 12'
	name = "swaptions (simsmall) 12 threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/blackscholes/inst/amd64-linux.gcc/bin/blackscholes 4 /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/blackscholes/inputs/in_16K.txt prices.txt'
	name = "blackscholes (simmedium) 4 threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/bodytrack/inst/amd64-linux.gcc-pthreads/bin/bodytrack /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/bodytrack/inputs/sequenceB_2 4 2 2000 5 0 9'
	name = "bodytrack (simmedium) 9 threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/fluidanimate/inst/amd64-linux.gcc-pthreads/bin/fluidanimate 8 5 /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/fluidanimate/inputs/in_100K.fluid out.fluid'
	name = "fluidanimate (simmedium) 8 threads"
	app_list.append((name, app))

if (enableSimMedium):

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/swaptions/inst/amd64-linux.gcc-pthreads/bin/swaptions  -ns 32 -sm 20000 -nt 24'
	name = "swaptions (simmedium) 24 threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/ferret/inst/amd64-linux.gcc/bin/ferret /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/ferret/inputs/corel lsh /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/ferret/inputs/queries 5 5 1 output.txt'
	name = "ferret (simtest)"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/ferret/inst/amd64-linux.gcc/bin/ferret /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/ferret/inputs/corel lsh /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/ferret/inputs/queries 10 20 18 output.txt'
	name = "ferret (simmedium) 18 threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/facesim/inst/amd64-linux.gcc/bin/facesim -timing -threads 6'
	name = "(simmedium) 6 threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/x264/inst/amd64-linux.gcc/bin/x264 --quiet --qp 20 --partitions b8x8,i4x4 --ref 5 --direct auto --b-pyramid --weightb --mixed-refs --no-fast-pskip --me umh --subme 7 --analyse b8x8,i4x4 --threads 15 -o eledream.264 /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/x264/inputs/eledream_640x360_8.y4m'
	name = "(simsmall) 15 threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/x264/inst/amd64-linux.gcc/bin/x264 --quiet --qp 20 --partitions b8x8,i4x4 --ref 5 --direct auto --b-pyramid --weightb --mixed-refs --no-fast-pskip --me umh --subme 7 --analyse b8x8,i4x4 --threads 15 -o eledream.264 /home/onderkalaci/TOOLS/parsec-3.0/pkgs/apps/x264/inputs/eledream_640x360_32.y4m'
	name = "(simmedium) 15 threads"
	app_list.append((name, app))

if (enableKernels):
	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/kernels/dedup/inst/amd64-linux.gcc-pthreads/bin/dedup -c -p -v -t 6 -i /home/onderkalaci/TOOLS/parsec-3.0/pkgs/kernels/dedup/inputs/media.dat -o output.dat.ddp'
	name = "(simmedium) 6 threads"
	app_list.append((name, app))

	app = '/home/onderkalaci/TOOLS/parsec-3.0/pkgs/kernels/canneal/inst/amd64-linux.gcc-pthreads/bin/canneal 16 15000 2000 /home/onderkalaci/TOOLS/parsec-3.0/pkgs/kernels/canneal/run/200000.nets 64'
	name = "canneal (simmedium) 16 threads"
	app_list.append((name, app))

print 'running tests'

for name, app in app_list:
	bcall(name, app)

	for i in range(3):
		print '###################################'

print "done"
