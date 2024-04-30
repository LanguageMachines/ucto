#!/bin/bash

echo "Running testaction script!"
./testall.sh
#./testone.sh testfoliain
TEST_STAT=$?
echo $TEST_STAT > status.tmp
exit 0
