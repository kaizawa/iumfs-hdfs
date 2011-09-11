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

import java.io.UnsupportedEncodingException;
import java.util.Collection;

/**
 *  READDIR Request class
 */
public abstract class ReadDirRequest extends Request {

    /**
     * <p>Read virtual directory entry 
     * </p>
     */
    @Override
    public void execute() throws UnsupportedEncodingException {
        /*
         * proceed the position until heder size.
         * header information including data size will be
         * set after we know actuall data size.
         */
        wbbuf.position(Request.RESPONSE_HEADER_SIZE);

        for (File f : getFileList()) {
            int namelen = f.getName().getBytes("UTF-8").length;
            namelen++; // null terminate。

            /*
             * For data alinment, reclen must be multiple of 8.
             *
             * typedef struct iumfs_dirent
             * {
             *   int64_t           i_reclen;
             *   char              i_name[1];
             * } iumfs_dirent_t; *
             */
            int reclen = (8 + 1 + (namelen) + 7) & ~7;
            logger.finer("name=" + f.getName() + ",namelen=" + namelen + ",reclen=" + reclen);
            wbbuf.putLong(reclen);
            for (byte b : f.getName().getBytes("UTF-8")) {
                wbbuf.put(b);
            }
            wbbuf.put((byte) 0); // null terminate           
                /*
             * Position を reclen 分だけ進めるためにパディングする
             */
            wbbuf.position(wbbuf.position() + (reclen - 8 - namelen));
        }
        /*
         * Set response header
         */
        setResponseHeader(SUCCESS, wbbuf.position() - RESPONSE_HEADER_SIZE);
    }

    abstract public Collection<File> getFileList();
}
