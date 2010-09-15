/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Copright (c) 2010  Kazuyoshi Aizawa <admin2@whiteboard.ne.jp>
 * All rights reserved.
 */
/*
 * ReadDirRequest
 *
 */
import java.io.IOException;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;

/**
 *  READDIR リクエストを表すクラス
 */
class ReadDirRequest extends Request {

    /**
     * <p>HDFS 上のディレクトリエントリを読み込み、結果をレスポンス
     * ヘッダをセットする</p>
     */
    @Override
    public void process() {
        /*
         * Hadoop HDFS 上のファイルをオープンする
         */
        FileSystem hdfs = getFileSystem();
        /*
         * 指定のディレクトリ以下のファイルの FileStatus の配列を得る
         */
        FileStatus fstats[];
        try {
            fstats = hdfs.listStatus(getFullPath());
        } catch (IOException ex) {
            setResponseHeader(ENOENT, 0);
            return;
        }

        /*
         * まず最初にヘッダ分だけバッファの位置を進めておく。
         * ヘッダはデータ長がわかってから改めてセットする
         */
	wbbuf.position(Request.RESPONSE_HEADER_SIZE);

        for (FileStatus fstat : fstats) {
            int namelen = fstat.getPath().getName().getBytes().length;

            /*
             * 受け取り側の driver でのアライメント対策のため reclen
             * (レコード長）は必ず 8 の倍数になるようにする。
             * typedef struct iumfs_dirent
             * {
             *   int64_t           i_reclen;
             *   char              i_name[1];
             * } iumfs_dirent_t; *
             */
            int reclen = (8 + 1 + (namelen) + 7) & ~7;
            logger.finer("name="+fstat.getPath().getName()+",namelen="+namelen+",reclen="+reclen);
            wbbuf.putLong(reclen);
            for (byte b : fstat.getPath().getName().getBytes()) {
                wbbuf.put(b);
            }
            /*
             * Position を reclen 分だけ進めるためにパディングする
             */
            wbbuf.position(wbbuf.position() + (reclen - 8 - namelen));
        }
        /*
         * レスポンスヘッダをセット
         */
        setResponseHeader(SUCCESS, wbbuf.position() - Request.RESPONSE_HEADER_SIZE);
    }
}
