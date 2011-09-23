package iumfs.hdfs;

/*
 * Copyright 2010 Kazuyoshi Aizawa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import java.util.logging.Logger;

/** 
 * User mode daemon for HDFS
 *
 * hadoop -cp ${HADOOP_HOME}/hdfsd.jar:${HADOOP_HOME}/conf:${HADOOP_HOME}/hadoop-0.20.2-core.jar:${HADOOP_HOME}/lib/commons-logging-1.0.4.jar hdfsd
 * 
 * デバッグ出力する場合
 * -Djava.util.logging.config.file=log.prop
 *
 * 以下を CLASSPATH に含める必要がある。 
 * 
 * ${HADOOPHOME}/Ehadoop-xxx.core.jar
 * ${HADOOPHOME}/lib/commons-loggin-xxx.jar
 * ${HADOOPHOME}/conf  .... core-site.xml
 */
public class Main {
    static final String version = "0.1.1";  // version
    private static final Logger logger = Logger.getLogger(Main.class.getName());
    private static final int maxThreads = 4;

    public static void main(String args[]) {
        Main instance = new Main();
        instance.startDaemonThreads();
    }
    
    public void startDaemonThreads() {
        for (int i = 0; i <  maxThreads ; i++) {
            new HdfsDaemonThread("HdfsDaemonThread").start();            
        }
    }    
}            
