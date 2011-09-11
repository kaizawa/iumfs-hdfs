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

import java.io.IOException;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.apache.hadoop.fs.FileSystem;

/**
 *  REMOVE リクエストを表すクラス
 */
class RemoveRequest extends Request {

    /**
     * Hadoop HDFS 上のファイルを削除し、結果をレスポンスヘッダをセットする
     */
    @Override
    public void process() {

        FileSystem hdfs = getFileSystem();
        try {
            if (hdfs.delete(getFullPath(), true) == false) {
                logger.fine("cannot remove " + getFullPath());
                setResponseHeader(EIO, 0);
                return;
            }
            /*
             * レスポンスヘッダをセット
             */
            setResponseHeader(SUCCESS, 0);
        } catch (IOException ex) {
            logger.fine("IOException happend when removing " + getFullPath());
            ex.printStackTrace();
            setResponseHeader(EIO, 0);
        }
    }
}
