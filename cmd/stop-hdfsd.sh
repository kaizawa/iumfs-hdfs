#!/bin/sh

pid=""
pid=`jps |grep hdfsd | awk '{print $1}'`
if [ "$pid" -ne "" ]; then
    kill $pid 
fi