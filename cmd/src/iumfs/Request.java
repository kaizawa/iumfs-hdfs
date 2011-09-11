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

import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.logging.Logger;
import java.util.Arrays;

/**
 * Abstruct class which represents various request from iumfs
 * control device driver.
 * Request would be READ/READDIR/GETATTR/MKDIR/RMDIR/DELETE/CREATE.
 */
public abstract class Request {
    final public static int MAX_USER_LEN = 40; // Must be multiple of 8
    final public static int MAX_PASS_LEN = 40; // Must be multiple of 8
    final public static int MAX_SERVER_LEN = 80; // Must be multiple of 8
    final public static int IUMFS_MAXPATHLEN = 1024; // Must be multiple of 8
    final public static int DEVICE_BUFFER_SIZE = 1024 * 1024; // buffer size of device
    final public static int MAX_RESPONSE_SIZE = DEVICE_BUFFER_SIZE;
    final public static int MAX_REQUEST_SIZE = DEVICE_BUFFER_SIZE;
    final public static int READ_REQUEST = 1;
    final public static int READDIR_REQUEST = 2;
    final public static int GETATTR_REQUEST = 3;
    final public static int WRITE_REQUEST  = 4;
    final public static int CREATE_REQUEST = 5;
    final public static int REMOVE_REQUEST = 6;
    final public static int MKDIR_REQUEST = 7;
    final public static int RMDIR_REQUEST = 8 ;
    final public static int RESPONSE_HEADER_SIZE = 24; // long x 3
    final public static int REQUEST_HEADER_SIZE = 2248; // from iumfs.h

    /*
     * Status to return to control device
     */
    final public static int SUCCESS = 0;
    final public static int ENOENT = 2;
    final public static int EIO = 5;
    final public static int EEXIST = 17;    
    final public static int ENOTSUP = 48;
    final public static int EINVAL = 22;

    private long request_type;
    private long size;
    private long offset;
    private String pathname;
    private String server;
    private String basepath;
    private String fullPath;
    private String username;
    private String password;
    private long datelen;
    protected ByteBuffer wbbuf = ByteBuffer.allocate(DEVICE_BUFFER_SIZE); // write buffer for control device
    private long dataoffset;
    private byte[] data;
    private long flags;
    protected static Logger logger = Logger.getLogger("iumfs");
    
    public long getFlags() {
        return flags;
    }

    public void setFlags(long flag) {
        this.flags = flag;
    }

    public byte[] getData() {
        return data;
    }

    public void setData(byte[] data) {
        this.data = data;
    }

    public String getBasepath() {
        return basepath;
    }

    public void setBasepath(String basepath) {
        this.basepath = basepath;
    }

    public long getOffset() {
        return offset;
    }

    public void setOffset(long offset) {
        this.offset = offset;
    }

    public String getPathname() {
        return pathname;
    }

    public void setPathname(String pathname) {
        this.pathname = pathname;
    }

    public long getRequestType() {
        return request_type;
    }

    public void setRequestType(long request_type) {
        this.request_type = request_type;
    }

    public String getServer() {
        return server;
    }

    public void setServer(String server) {
        this.server = server;
    }

    public long getSize() {
        return size;
    }

    public void setSize(long size) {
        this.size = size;
    }

    public String getFullPath() {
        if (fullPath == null) {
            fullPath = getBasepath() + getPathname();
        }
        return fullPath;
    }

    /**
     * Set response header to buffer. limit is set to header plus datalen.<br/>
     * Position is set at the end of header.<br/>
     * @param result
     * @param datalen
     */
    public void setResponseHeader(long result, long datalen) {
        /*
         * Response structure passed from damon to control device.
         *  8+8+8=24 bytes
         * typedef struct response
         * {
         *   int64_t            request_type; // request type
         *   int64_t            result;       // execution result of requestn
         *   int64_t            datasize; // size of data following this header.
         * } response_t;
         */
        wbbuf.clear();
        wbbuf.limit(Request.RESPONSE_HEADER_SIZE + (int) datalen);
        wbbuf.putLong(getRequestType());
        wbbuf.putLong(result);
        wbbuf.putLong(datalen);
    }

    public abstract void execute() throws IOException, FileNotFoundException, UnsupportedEncodingException;

    /**
     * Get a buffer which has response. Position is set to 0.<br/>
     * Limit won't be unchanged.
     * @return buffer
     */
    public ByteBuffer getResponseBuffer() {
        wbbuf.rewind();
        return wbbuf;
    }

    public void setByteOrder(ByteOrder order) {
        wbbuf.order(order);
    }

    /**
     * Return array of data start from given offset.
     *
     * @param  from start offset of data array.
     * @param  to end offset of data array.
     * @return buffer new array start from given position.
     */ 
    public byte[] getData(long from, long to) {
        return Arrays.copyOfRange(data, (int)from, (int)to);
    }
    abstract public File getFile();

    /**
     * @return the user
     */
    public String getUserName() {
        return username;
    }

    /**
     * @param user the user to set
     */
    public void setUserName(String username) {
        this.username = username;
    }

    /**
     * @return the pass
     */
    public String getPassword() {
        return password;
    }

    /**
     * @param pass the pass to set
     */
    public void setPassword(String password) {
        this.password = password;
    }
}
