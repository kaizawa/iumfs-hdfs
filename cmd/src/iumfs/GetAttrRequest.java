/*
 * Copyright 2011 Kazuyoshi Aizawa
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
package iumfs;

import java.util.Date;
import java.util.logging.Level;
import javax.imageio.stream.FileCacheImageInputStream;

/**
 *  GETATTR request class
 */
public abstract class GetAttrRequest extends Request {

    final public static int ATTR_DATA_LEN = 72; // long x 9 フィールド
    private static final long start_time = new Date().getTime();

    /**
     * Return file attribute information of file on filesystem
     */
    @Override
    public void execute() {
        /*
         * get File Object
         */
        File file = getFile();

        if (file == null) {
            /*
             * Unknown file
             * return ENOENT
             */
            setResponseHeader(ENOENT, 0);
            return;
        }

        /*
         * レスポンスヘッダをセット
         */
        setResponseHeader(0, GetAttrRequest.ATTR_DATA_LEN);

        /*
         * ファイル属性をバッファにセット
         * typedef struct iumfs_vattr
         * {
         *   uint64_t  i_mode; // file mode
         *   uint64_t  i_size; // file size
         *   int64_t   i_type; // file type
         *   int64_t   mtime_sec; // modify time(sec)
         *   int64_t   mtime_nsec;// modify time(nsec)
         *   int64_t   atime_sec; // access time(sec)
         *   int64_t   atime_nsec;// access time(nsec)
         *   int64_t   ctime_sec; // change time(sec)
         *   int64_t   ctime_nsec;// change time(nsec)
         * } iumfs_vattr_t;
         */
        Date now = new Date();
        wbbuf.putLong(file.getPermission());
        wbbuf.putLong(file.getFileSize());
        wbbuf.putLong(file.getFileType());
        if (file == null) {
            wbbuf.putLong(start_time / 1000);
            wbbuf.putLong((start_time % 1000) * 1000);
            wbbuf.putLong(start_time / 1000);
            wbbuf.putLong((start_time % 1000) * 1000);
            wbbuf.putLong(start_time / 1000);
            wbbuf.putLong((start_time % 1000) * 1000);
        } else {
            wbbuf.putLong(file.getMtime() / 1000);
            wbbuf.putLong((file.getMtime() % 1000) * 1000);
            wbbuf.putLong(file.getAtime() / 1000);
            wbbuf.putLong((file.getAtime() % 1000) * 1000);
            wbbuf.putLong(file.getCtime() / 1000);
            wbbuf.putLong((file.getCtime() % 1000) * 1000);
        }
        logger.finer("filename=" + file.getName() + ", Permission="
                + String.format("%1$o", file.getPermission()) + " ,FileSize=" + file.getFileSize()
                + ", FileType=" + file.getFileType());
    }
}
