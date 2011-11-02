#!/usr/bin/env python
#-*- coding:utf-8 -*-

import glob
import sys
import os

COLORS = [
    'BLACK', 'RED', 'GREEN', 'YELLOW',
    'BLUE', 'MAGENTA', 'CYAN', 'WHITE'
]

def color_text(text, color_name, bold=False):
    if color_name in COLORS:
        return "\033[%d;%dm%s\033[0m"  % (int(bold), COLORS.index(color_name) + 30, text)
    sys.stderr.write('ERROR: "%s" is not a valid color.\n' % color_name)
    sys.stderr.write('VALID COLORS: %s.\n' % (', '.join(COLORS)))

def checkVGsummary( id, lang ):
    retcode = os.system('grep "definitely lost" testoutput/' + id + '.' + lang + '.err > testoutput/' + id + '.err.sum')
    retcode = os.system('grep "ERROR SUMMARY" testoutput/' + id + '.' + lang + '.err >> testoutput/' + id + '.err.sum')
    infile = open('testoutput/' + id + '.err.sum',"r")
    while infile:
        line = infile.readline()
        s = line.split()
        if ( len(s) == 0 ):
            break
        num = 0
        for part in s:
            num +=1
            if part == "lost:":
                LOST = int( s[ num ] )
                break
            if part == "SUMMARY:":
                ERRORS = int( s[ num ] )
                break
    if ( LOST != 0 or ERRORS != 0 ):
        print color_text( "OK, but valgrind says:" + str(ERRORS) + " errors, " + str(LOST) + " bytes lost", 'BLUE', True) 
    else:
        print color_text("OK", 'GREEN', True) 

if len(sys.argv) == 2:
    mask = sys.argv[1]
else:
    mask = '*.txt'
    
DEBUGLEVEL = 5

if not os.path.isdir('testoutput'):
    os.mkdir('testoutput')

log = ""

if "VG" in os.environ and os.environ["VG"]:
    VG=os.environ["VG"] + " "
    print "testing with '" + VG + "'. This is slow!"
else:
    VG=""

for f in glob.glob(mask):
    fields = f.split('.')
    if len(fields) == 3 and fields[-1] == 'txt':
        id = fields[0]
        lang = fields[1]
    else:
        continue
        
    msg = "Testing: " + id + " (" + lang + ")"
    spaces = " " * (50 - len(msg) )
    print msg + spaces, 
    if not os.path.isfile(id + '.' + lang + '.tok.V'):
        print color_text("MISSING", 'RED', True)  
        continue
    cmd = VG+'../src/.libs/ucto -Q -v -c ../config/tokconfig-' + lang + ' ' + f + ' testoutput/' + id + '.' + lang + '.tok.V 2> testoutput/' + id + '.' + lang + '.err'
    #print cmd
    retcode = os.system(cmd)
    if retcode != 0:
        print color_text("CRASHED!!!", 'RED', True)
        if retcode == 2: # lame attempt to bail out on ^C
            break
    else:
        retcode = os.system('diff -q ' + id + '.' + lang + '.tok.V testoutput/' + id + '.' + lang + '.tok.V > /dev/null')
        if retcode == 0:
            if VG:
                checkVGsummary( id, lang )
            else:
                print color_text("OK", 'GREEN', True) 
        else:                   
            print color_text("FAILED", 'RED', True)
            log += "testoutput/" + id + '.' + lang + '.diff\n'
            log += "testoutput/" + id + '.' + lang + '.err\n'
            #print 'diff ' + id + '.' + lang + '.tok.V testoutput/' + id + '.' + lang + '.tok.V > testoutput/' + id + '.' + lang + '.diff'
            os.system('diff ' + id + '.' + lang + '.tok.V testoutput/' + id + '.' + lang + '.tok.V > testoutput/' + id + '.' + lang + '.diff')
            #VERBOSE RUN:
            os.system('../src/ucto -d ' + str(DEBUGLEVEL) + ' -v -c ../config/tokconfig-' + lang + ' ' + f + ' testoutput/' + id + '.' + lang + '.tok.V 2> testoutput/' + id + '.' + lang + '.err')

if log:
    print "--------------------------"
    print "Files to inspect:"
    print log           
        

