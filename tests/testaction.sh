#!/bin/bash

echo "Running testaction script!"
#./testall.sh
sh -x testone.sh testfolia
echo "ERRROR FILE"
cat testoutput/testfolia.err
echo "TEMP FILE"
cat testoutput/testfolia.tmp
echo "DIFF FILE"
cat testoutput/testfolia.diff
TEST_STAT=$?
echo $TEST_STAT > status.tmp
exit 0
