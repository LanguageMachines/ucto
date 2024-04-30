# /bin/sh -x

OK="\033[1;32m OK  \033[0m"
FAIL="\033[1;31m  FAILED  \033[0m"
ACCEPT="\033[1;33m  FOR NOW ACCEPTABLE  \033[0m"

sum=0
expected=0

mkdir testoutput 2> /dev/null
\rm testoutput/*.diff 2> /dev/null

for file in testusage testlanguage testconf1 testconf2 testinclude \
   	    testfiles1 testfiles2 testoption1 testoption2 testoption-s\
	    testnormalisation testencoding2 testpassthru testfolia testfolia2\
	    testfoliain testslash testquotes testquotes2 testtwitter testutt \
	    testpunctuation testpunctfilter testclassnormalization testlang \
	    testtokens testoption-P testoption-split testissue64 testissue66 \
	    testissue71 testissue72 testissue70 testnbsp testcorrect \
	    testtag testissue81 testissue83 testissue84 testissue87 \
	    testissue68 testissue93 testoption-m testbatch
do
   ./testone.sh $file
   if [ $? -ne 0 ]; then
      sum=$(($sum + 1))
   fi
done

python3 test.py
erc=$?
if [ $erc -ne 0 ]; then
   sum=$(($sum + $erc))
fi

if [ $sum != "0" ]
then
	if [ $sum = $expected ]
	then
		echo -e "$ACCEPT, $sum errors, as expected"
		exit 0
	else
		echo -e "$FAIL, $sum errors, but expected $expected"
		exit $sum
	fi
else
	echo -e "ALL $OK"
fi

exit $sum
