#!/bin/sh
#
# You might need to correct jar file name to
# refrect your environment.
#  - Following version of jar files are from
#    defautl jar file of Hadoop 1.0.2
# HADOOP_HOME variable seems to get deprecated.
# But there's no reasonable way to know the 
# location of jar files, I continue to use
# HADOOP_HOME for now.
#
CLASSPATH="\
./cmd/hdfsd.jar:${HADOOP_HOME}/conf:\
./cmd/lib/iumfs-daemon-core.jar:\
${HADOOP_HOME}/hadoop-core-1.0.2.jar:\
${HADOOP_HOME}/lib/commons-lang-2.4.jar:\
${HADOOP_HOME}/lib/commons-configuration-1.6.jar:\
${HADOOP_HOME}/lib/commons-logging-1.1.1.jar"

# For Debug
#hadoop -Djava.util.logging.config.file=cmd/log.prop -cp $CLASSPATH iumfs.hdfs.Main 

hadoop -cp $CLASSPATH iumfs.hdfs.Main
