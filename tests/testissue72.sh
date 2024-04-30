#!/bin/bash

data_dir=/home/sloot/usr/local/share/ucto
if [ ! -d $data_dir ]
then
  data_dir=/exp/sloot/usr/local/share/ucto
  if [ ! -d $data_dir ]
  then
    data_dir=/usr/local/share/ucto
  fi
fi

if [ ! -d $data_dir ]
then
  echo "unable to find $data_dir"
  exit 1
fi

\rm -f issue72_*.xml

$exe -c $data_dir/tokconfig-nld datetime.nl.txt
$exe -c $data_dir/tokconfig-nld datetime.nl.txt issue72_a.xml
$folialint --nooutput issue72_a.xml 2>&1

$exe -c small.cfg datetime.nl.txt
$exe -c small.cfg datetime.nl.txt issue72_b.xml
$folialint --nooutput issue72_b.xml 2>&1

$exe -c small-tokconfig-nld datetime.nl.txt
$exe -c small-tokconfig-nld datetime.nl.txt issue72_c.xml
$folialint --nooutput issue72_c.xml 2>&1
