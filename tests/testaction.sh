#!/bin/bash

echo "Running testaction script!"
./testall
TEST_STAT=$?
echo $TEST_STAT > status.tmp
exit 0
