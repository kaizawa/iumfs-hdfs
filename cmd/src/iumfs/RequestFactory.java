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

import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.logging.Logger;

/**
 * <p>
 * Return appropreate Request class based on the request 
 * from control device driver.
 * Implementation must exist every file system type.
 * </p>
 * TODO: should implement instance pool for better performance?
 */
public abstract class RequestFactory {

    private static Logger logger = Logger.getLogger("iumfs");
    
    protected RequestFactory(){};

    public Request getInstance(ByteBuffer buf) {
        /*
         * Request structure which will be given by control device.
         * 
         *  8+1184+1024+8+8+8=2240 bytes
         * typedef struct request
         * {
         *   int64_t             request_type; // request type
         *   int64_t             size;     // data size 
         *   int64_t             offset;   // off set of file
         *   int64_t             datasize; // size of data following this header
         *   int64_t             flags;    // for ioflag...not used yet.
         *   char                pathname[IUMFS_MAXPATHLEN]; // File path name
         *   iumfs_mount_opts_t  mountopts[1]; // arguments of mount command
         * }
         * 
         * The size of each member of this structure is multiple of 8.
         * So the total structure size must be also mutiple of 8.

         *  40+40+80+1024=1184 bytes
         *
         * typedef struct iumfs_mount_opts
         * {
         *    char user[MAX_USER_LEN];
         *    char pass[MAX_PASS_LEN];
         *    char server[MAX_SERVER_NAME];
         *    char basepath[IUMFS_MAXPATHLEN];
         * } iumfs_mount_opts_t;
         */
        Request req = null;
        int bufsize = buf.capacity();
        long request_type; 
        long size;
        long offset;
        long datasize;
        long flags;
        byte pathname[] = new byte[Request.IUMFS_MAXPATHLEN]; // Ffile pathname
        byte basepath[] = new byte[Request.IUMFS_MAXPATHLEN]; // Base path when mounting.
        byte server[] = new byte[Request.MAX_SERVER_LEN]; // Server name(Optiona)
        byte username[] = new byte[Request.MAX_USER_LEN]; // User name(Optional)
        byte password[] = new byte[Request.MAX_PASS_LEN]; // Passowrd (Optional)
        byte data[]; // additional data for this request, if any.

        try {
            buf.rewind();
            logger.finer("Buf info pos=" + buf.position() + " limit=" + buf.limit());
            /*
             * Read each structure members from ByteBuffer.
             */
            request_type = buf.getLong();
            size = buf.getLong();
            offset = buf.getLong();
            datasize = buf.getLong();
            flags = buf.getLong();
            buf.get(pathname);
            buf.get(basepath);
            buf.get(server);
            buf.get(username);
            buf.get(password);
            
            logger.finer("request_type=" + request_type + ", size=" + size + 
                    ", offset=" + offset + ", datasize=" + datasize);
            logger.finer("basename=" + (new String(basepath)).trim());
            logger.finest("ByteOrder=" + ByteOrder.nativeOrder());
            
            req = createInstance(request_type);
            logger.fine("request=" + req.getClass().getSimpleName()
                    + ", pathname=" + (new String(pathname)).trim()
                    + ", username=" + new String(username).trim());             

            req.setByteOrder(ByteOrder.nativeOrder());
            req.setOffset(offset);
            req.setPathname((new String(pathname)).trim()); // remove space
            req.setBasepath((new String(basepath)).trim()); // remove space
            req.setServer((new String(server)).trim()); // remove space
            req.setRequestType(request_type);
            req.setSize(size);
            req.setFlags(flags);
            req.setUserName((new String(username)).trim());
            req.setPassword((new String(password)).trim());
            /*
             * Check if structure followed by data.
             * size won't exceed DEVICE_BUFFER_SIZE - REQUEST_HEADER_SIZE
             */
            if(size > 0){
                data = new byte[Math.min((int)size, Request.DEVICE_BUFFER_SIZE - Request.REQUEST_HEADER_SIZE) ];
                buf.get(data);
                req.setData(data);
            }           
            return req;
        } catch (BufferUnderflowException ex) {
            ex.printStackTrace();
            System.exit(1);
        } catch (IndexOutOfBoundsException ex) {
            ex.printStackTrace();
            System.exit(1);
        }
        return null;
    }

    /** 
     * Create FS dependent Request class.
     */
    protected abstract Request createInstance(long request_type);
}


