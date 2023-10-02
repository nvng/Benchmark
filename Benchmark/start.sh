#!/bin/bash

for i in {1..4}
do
  ./Benchmark.out --logmf=1 -s -d
  sleep 1
done

echo ""
echo "#########################################################"
ps x | grep Benchmark.out | grep -v grep
echo "#########################################################"
