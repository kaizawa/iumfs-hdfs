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
import org.apache.hadoop.fs.FileSystem;

/**
 * <p>MKDIR リクエストを表すクラス</p>
 */
class MkdirRequest extends Request{
    /**
     * <p>HDFS 上にディレクトリを作成し、結果をレスポンスヘッダをセットする</p>
     * <p>FileSystem.mkdir は既存ディレクトリがあっても例外やエラーには
     * ならないので、MkdirRequest は自身の内部で既存ディレクトリを調べて
     * もし既存ディレクトリがあった場合には EEXIST を返す。</p>
     */
    @Override
    public void process() {
        /*
         * Hadoop HDFS 上に新規ディレクトリを作成する
         */
        FileSystem hdfs = getFileSystem();
        try {
            if(hdfs.exists(getFullPath()) == true){
                logger.fine("cannot create directory");
                setResponseHeader(EEXIST, 0);
                return;
            }
            
            if (hdfs.mkdirs(getFullPath()) == false){
                logger.fine("cannot create directory");
                setResponseHeader(EIO, 0);
                return;
            }
            /*
             * レスポンスヘッダをセット
             */
            setResponseHeader(SUCCESS, 0);
        } catch (IOException ex) {
            logger.fine("IOException happend when removing directory");
            ex.printStackTrace();
            setResponseHeader(EIO, 0);
        }
    }

}
