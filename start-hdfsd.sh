#!/bin/sh
#
HADOOPHOME=`which hadoop | xargs dirname | xargs dirname`
CONFDIR="$HADOOPHOME/conf"

CLASSPATH="$CONFDIR:./cmd/hdfsd.jar:`/usr/bin/ls ./cmd/lib/iumfs-daemon-core*.jar`:`hadoop classpath`"

# For Debug
#hadoop -Djava.util.logging.config.file=cmd/log.prop -cp $CLASSPATH iumfs.hdfs.Main 

hadoop -cp $CLASSPATH iumfs.hdfs.Main
