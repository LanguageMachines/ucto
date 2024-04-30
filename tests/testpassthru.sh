#/bin/sh

$exe -v --passthru passthru.txt

$exe -v --passthru passthru2.txt

$exe -v --passthru encoding.nl.txt

$exe -v --passthru -e LATIN1  encoding2.nl

$exe -v --passthru empty_line.txt

$exe -v --passthru -m empty_line.txt
