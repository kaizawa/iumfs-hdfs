#!/bin/sh

pids=""
pids=`jps -l |grep "iumfs.hdfs.Main" | awk '{print $1}'`
for pid in $pids
do
	if [ "$pid" -ne "" ]; then
	    kill $pid 
	fi
done
