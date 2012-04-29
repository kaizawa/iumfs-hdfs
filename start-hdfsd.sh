#!/bin/sh
#
HADOOPHOME=`which hadoop | xargs dirname | xargs dirname`
CONFDIR="$HADOOPHOME/conf"

CLASSPATH="$CONFDIR:./cmd/hdfsd.jar:./cmd/lib/iumfs-daemon-core-0.2.0.jar:`hadoop classpath`"

# For Debug
hadoop -Djava.util.logging.config.file=cmd/log.prop -cp $CLASSPATH iumfs.hdfs.Main 

#hadoop -cp $CLASSPATH iumfs.hdfs.Main
