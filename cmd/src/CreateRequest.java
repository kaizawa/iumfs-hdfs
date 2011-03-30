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
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.hdfs.protocol.AlreadyBeingCreatedException;

/**
 * <p>CREATE リクエストを表すクラス</p>
 */
class CreateRequest extends Request {

    /**
     * <p>FileSystem.create を実行する</p>
     * <p>creat(2) が呼ばれたということは既存ファイルがあった場合
     * 既存データを削除(O_TRUNC相当)しなければならないが、 HDFS では データの途中
     * 変更はできないので、既存ファイルがあったらエラーリターンする</p>
     */
    @Override
    public void process() {
        /*
         * HDFS 上に新規ファイルを作成し、結果をレスポンスヘッダをセットする
         */
        FileSystem hdfs = getFileSystem();
        try {
            //ファイルが存在したら EEXIST を返す
            if(hdfs.exists(getFullPath()) == true){
                logger.fine("cannot create file");
                setResponseHeader(EEXIST, 0);
                return;
            }
            
            FSDataOutputStream fsdos = hdfs.create(getFullPath());
            /*
             * レスポンスヘッダをセット
             */
            fsdos.close();
            setResponseHeader(SUCCESS, 0);
        } catch (AlreadyBeingCreatedException ex) {
            logger.fine("AlreadyBeingCreatedException when writing");
            setResponseHeader(EEXIST, 0);
        } catch (IOException ex) {
            logger.fine("IOException happend when writing");
            ex.printStackTrace();
            setResponseHeader(ENOTSUP, 0);
        }
    }
}
