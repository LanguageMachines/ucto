#!/usr/bin/env python3
#-*- coding:utf-8 -*-

import glob
import sys
import os

COLORS = [
    'BLACK', 'RED', 'GREEN', 'YELLOW',
    'BLUE', 'MAGENTA', 'CYAN', 'WHITE'
]

exitcode = 0

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
                LOST = int( s[num].replace(',','') )
                break
            if part == "SUMMARY:":
                ERRORS = int( s[num].replace(',','') )
                break
    if ( LOST != 0 or ERRORS != 0 ):
        print( color_text( "OK, but valgrind says:" + str(ERRORS) + " errors, " + str(LOST) + " bytes lost", 'BLUE', True) )
    else:
        print( color_text("OK", 'GREEN', True) )

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
    print( "testing with '" + VG + "'. This is slow!",end='' )
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
    print( msg + spaces, end='' )
    if os.path.isfile(id + '.' + lang + '.tok.V'):
        reffile = id + '.' + lang + '.tok.V'
        outputfile = 'testoutput/' + id + '.' + lang + '.tok.V'
        cmd = VG+'../src/.libs/ucto -Q -v -L' + lang + ' ' + f + ' ' + outputfile + ' 2> testoutput/' + id + '.' + lang + '.err'
    elif os.path.isfile(id + '.' + lang + '.tok'):
        reffile = id + '.' + lang + '.tok'
        outputfile = 'testoutput/' + id + '.' + lang + '.tok'
        cmd = VG+'../src/.libs/ucto -Q -L' + lang + ' ' + f + ' ' + outputfile + ' 2> testoutput/' + id + '.' + lang + '.err'
    elif os.path.isfile(id + '.' + lang + '.xml'):
        reffile = id + '.' + lang + '.xml'
        outputfile = 'testoutput/' + id + '.' + lang + '.xml'
        cmd = VG+'../src/.libs/ucto --id="test" -Q -X -L' + lang + ' ' + f + ' ' + outputfile + ' 2> testoutput/' + id + '.' + lang + '.err'
    else:
        print( color_text("MISSING", 'RED', True) )
        exitcode += 1
        continue

    #print cmd
    retcode = os.system(cmd)
    if retcode != 0:
        print( color_text("CRASHED!!!", 'RED', True) )
        if retcode == 2: # lame attempt to bail out on ^C
            break
        exitcode += 1
    else:
        if outputfile[-4:] == ".xml":
            for f2 in (reffile, outputfile):
            #strip the FoLiA version number and the token-annotation declaration (different timestamp)
                os.system("sed -e 's/generator=\".*\"//g' -e 's/version=\".*\"//g' -e 's/datetime=\".*\"//g' -e '/token-annotation/d'  -e '/libfolia/d' "  + f2 + " > " + f2 + ".tmp")
            retcode = os.system('diff -w ' + reffile + '.tmp ' + outputfile  + '.tmp > testoutput/'  + id + '.' + lang + '.diff')
        else:
            retcode = os.system('diff -w ' + outputfile + ' ' + reffile  + ' > testoutput/' + id + '.' + lang + '.diff')
        if retcode == 0:
            if VG:
                checkVGsummary( id, lang )
            else:
                print( color_text("OK", 'GREEN', True) )
                os.system('rm testoutput/' + id + '.' + lang + '.diff')
                os.system('rm testoutput/' + id + '.' + lang + '.err*')
        else:
            print( color_text("FAILED", 'RED', True) )
            exitcode += 1
            log += "testoutput/" + id + '.' + lang + '.diff\n'
            log += "testoutput/" + id + '.' + lang + '.err\n'
            if outputfile[-4:] != ".xml":
                os.system('../src/.libs/ucto -Q -d ' + str(DEBUGLEVEL) + ' -v -L' + lang + ' ' + f + ' ' + outputfile + ' 2> testoutput/' + id + '.' + lang + '.err')
            else:
                os.system('../src/.libs/ucto --id="test" -X -Q -d ' + str(DEBUGLEVEL) + ' -v -L' + lang + ' ' + f + ' ' + outputfile + ' 2> testoutput/' + id + '.' + lang + '.err')

if log:
    print( "--------------------------" )
    print( "Files to inspect:" )
    print( log )
sys.exit(exitcode)
