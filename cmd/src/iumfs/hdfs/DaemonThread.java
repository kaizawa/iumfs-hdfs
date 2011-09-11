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
 * Worker Thread which opens iumfscntl device and communicate with data node.
 */
public class DaemonThread extends Thread {
    protected static Logger logger = Logger.getLogger(hdfsd.class.getName());
    
    public void run() {
        ByteBuffer rbbuf = ByteBuffer.allocate(Request.DEVICE_BUFFER_SIZE);
        rbbuf.order(ByteOrder.nativeOrder());
        FileInputStream devis = null;
        FileOutputStream devos = null;
        int len = 0;
        Request req = null;
        RandomAccessFile raf = null;

        try {
            raf = new RandomAccessFile("/dev/iumfscntl", "rw");
        } catch (FileNotFoundException ex) {
            ex.printStackTrace();
            System.exit(1);
        }
        FileChannel ch = raf.getChannel();
        logger.fine("Successfully open device.");

        logger.info("Started");

        while (true) {
            try {
                /*
                 * iumfs デバイスからリクエストデータを読み込む
                 */
                rbbuf.clear();
                if ((len = ch.read(rbbuf)) < 0) {
                    logger.severe("read from device failed");
                    System.exit(1);
                }

                logger.fine("device returns " + len + " bytes");

                /*
                 * リクエストオブジェクトを生成
                 */
                req = RequestFactory.getInstance(rbbuf);

                if (req == null) {
                    logger.severe("Request object is null");
                    System.exit(1);
                }
                /*
                 * リクエストを実行
                 */
                logger.fine("calling " + req.getClass().getName() + ".process()");
                req.process();
                /*
                 * デバイスに書き込み
                 */
                ch.write(req.getResponseBuffer());
                logger.fine("request for " + req.getClass().getName() + " finished.");
            } catch (IOException ex) {
                /*
                 * ここでキャッチされるのはデバイスドライバとの read/write 処理の
                 * IOException のみ。
                 */
                ex.printStackTrace();
                System.exit(1);
            } 
        }
    }
}            
