#!/bin/sh

pids=""
pids=`jps |grep Main | awk '{print $1}'`
for pid in $pids
do
	if [ "$pid" -ne "" ]; then
	    kill $pid 
	fi
done
