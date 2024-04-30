# /bin/sh -x

export folialint="folialint"
export foliadiff="foliadiff.sh"
export exe="$VG ../src/ucto"

OK="\033[1;32m OK  \033[0m"
FAIL="\033[1;31m  FAILED  \033[0m"

inputparameter=$1
inputparameter=$(basename -- "$inputparameter")
file="${inputparameter%.*}"

keep=""
if [ "$2" != "" ]
then
  keep=$2
fi

mkdir testoutput 2> /dev/null
if test -x $file.sh
then
    \rm -f $file.diff
    \rm -f testoutput/$file.tmp
    \rm -f testoutput/$file.err
    \rm -f testoutput/$file.diff
    echo -n "testing  $file "
    ./$file.sh > testoutput/$file.tmp 2> testoutput/$file.err
    diff -wb --ignore-matching-lines=".*getaddrinfo.*" testoutput/$file.tmp $file.ok > testoutput/$file.diff 2>& 1
    if [ $? -ne 0 ];
    then
      echo -e $FAIL;
      echo "differences logged in testoutput/$file.diff";
      echo "stderr messages logged in testoutput/$file.err";
      exit 1
    else
      echo -e $OK $keep
      if [ "$keep" == "" ];
      then
        \rm -f testoutput/$file.diff
        \rm -f testoutput/$file.err
        exit 0
      fi
    fi
fi
