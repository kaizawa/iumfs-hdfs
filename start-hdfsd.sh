#!/bin/sh

#CLASSPATH=cmd/hdfsd.jar:${HADOOP_HOME}/conf:${HADOOP_HOME}/hadoop-common-0.21.0.jar:${HADOOP_HOME}/hadoop-hdfs-0.21.0.jar:${HADOOP_HOME}/lib/commons-logging-1.1.1.jar:. 

CLASSPATH="\
./cmd/hdfsd.jar:${HADOOP_HOME}/conf:\
./cmd/lib/iumfs-daemon-core.jar:\
${HADOOP_HOME}/hadoop-core-1.0.1.jar:\
${HADOOP_HOME}/lib/commons-lang-2.4.jar:\
${HADOOP_HOME}/lib/commons-configuration-1.6.jar:\
${HADOOP_HOME}/lib/commons-logging-1.1.1.jar"

hadoop -Djava.util.logging.config.file=log.prop -cp $CLASSPATH iumfs.hdfs.Main
#hadoop -cp $CLASSPATH hdfsd 
