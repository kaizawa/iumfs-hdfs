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
package iumfs;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Date;

abstract public class File {
    final public static int VREG = 1; // Normal File
    final public static int VDIR = 2; // Directory
    private String name;
    protected long file_size = 0;
    private long atime; // Last access time (msec)
    private long ctime; // Modify time(csec)
    private long mtime; // Modify time(msec)
    
    public File(String name){
        this.name = name;
        init();
    }

    private void init() {
        Date now = new Date();
        setAtime(now.getTime());
        setCtime(now.getTime());
        setMtime(now.getTime());
    }

    public String getName() {
        return name;
    }

    public void setFileSize(long file_size){
        this.file_size = file_size;
    }
    
    public long getFileSize() {
        return file_size;
    }

    /**
     * Read requested data from requested offset/size and copy to specified buffer.
     * 
     * @param  buffer where read data to be copied
     * @param  size of data
     * @param  offset of date to be read
     * @return read byte
     * @throws ngException 
     */
    abstract public long read(ByteBuffer buf, long size, long offset)
            throws FileNotFoundException, IOException, NotSupportedException;             
    
    /**
     * Write data to requested offset/size.
     * 
     * @param  buf
     * @param  size to be written
     * @param  offset to be written
     * @return 0
     * @throws ngException 
     */
    abstract public long write(byte[] buf, long size, long offset)
            throws FileNotFoundException, IOException, NotSupportedException; 

    /**
     * @return the atime
     */
    public long getAtime() {
        return atime;
    }

    /**
     * @param atime the atime to set
     */
    public void setAtime(long atime) {
        this.atime = atime;
    }

    /**
     * @return the ctime
     */
    public long getCtime() {
        return ctime;
    }

    /**
     * @param ctime the ctime to set
     */
    public void setCtime(long ctime) {
        this.ctime = ctime;
    }

    /**
     * @return the mtime
     */
    public long getMtime() {
        return mtime;
    }

    /**
     * @param mtime the mtime to set
     */
    public void setMtime(long mtime) {
        this.mtime = mtime;
    }
    
    /**
     * Get filetype like VDIR, VREG
     * @return filetype
     */
    abstract public long getFileType();
    /**
     * Get prmission of this file
     * @return permission
     */
    abstract public long getPermission();
    /**
     * Check if this file represents directory.
     * @return 
     */
    abstract public boolean isDir(); 
}