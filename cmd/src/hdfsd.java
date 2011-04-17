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

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
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
public class hdfsd {
    private static Logger logger = Logger.getLogger(hdfsd.class.getName());
    private static final int maxDaemons = 4;

    public static void main(String args[]) {
        for(int i = 0 ; i < maxDaemons ; i++){
            new DaemonThread().start();
        }
    }
}            
