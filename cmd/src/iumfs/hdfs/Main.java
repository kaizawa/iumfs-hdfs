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
 * hadoop -cp $CLASSPATH" iumfs.hdfs.Main
 * 
 * Example)
 *   hadoop -cp "./cmd/hdfsd.jar:./cmd/lib/iumfs-daemon-core-0.2.0.jar:/usr/local/hadoop/conf:/usr/local/hadoop/hadoop-core-1.0.2.jar" iumfs.hdfs.Main
 * 
 * For debug
 * -Djava.util.logging.config.file=log.prop
 *
 * It would be easier to use an output of hadoop classpath command.
 * It will output the list of libraries to be used.
 */
public class Main {
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
