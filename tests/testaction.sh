#!/bin/bash

echo "Running testaction script!"
./testall.sh
TEST_STAT=$?
echo $TEST_STAT > status.tmp
exit 0
