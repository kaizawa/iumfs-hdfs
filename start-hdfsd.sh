#!/bin/sh

CLASSPATH=cmd/hdfsd.jar:${HADOOP_HOME}/conf:${HADOOP_HOME}/hadoop-common-0.21.0.jar:${HADOOP_HOME}/hadoop-hdfs-0.21.0.jar:${HADOOP_HOME}/lib/commons-logging-1.1.1.jar:. 

hadoop -Djava.util.logging.config.file=log.prop -cp $CLASSPATH hdfsd 
#hadoop -cp $CLASSPATH hdfsd 
