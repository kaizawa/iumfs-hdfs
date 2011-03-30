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
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FileSystem;

/**
 *  READ リクエストを表すクラス
 */
public class ReadRequest extends Request {

    /**
     * HDFS 上のファイルをオープンし、結果をレスポンスヘッダをセットする
     */
    @Override
    public void process() {

        FileSystem hdfs = getFileSystem();
        try {
            FSDataInputStream fsdis = hdfs.open(getFullPath());

            /*
             * ファイルの指定オフセット/サイズのデータを書き込み用バッファに読み込む
             */
            int ret = fsdis.read(getOffset(), wbbuf.array(), Request.RESPONSE_HEADER_SIZE, (int) getSize());
            fsdis.close();
            logger.fine("read offset=" + getOffset() + ",size=" + getSize());
            /*
             * レスポンスヘッダをセット
             */
            setResponseHeader(SUCCESS, ret);

        } catch (IOException ex) {
            logger.fine("IOException happend when reading hdfs. offset=" + getOffset() + ",size=" + getSize());
            ex.printStackTrace();
            setResponseHeader(ENOENT, 0);
        }
    }
}
