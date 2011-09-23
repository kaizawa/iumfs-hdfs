package iumfs.hdfs;

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

import iumfs.IumfsFile;
import iumfs.RmdirRequest;

/**
 *  RMDIR繝ｪ繧ｯ繧ｨ繧ｹ繝医ｒ陦ｨ縺吶け繝ｩ繧ｹ
 */
class HdfsRmdirRequest extends RmdirRequest {
    /**
     * Hadoop HDFS 荳翫�繝�ぅ繝ｬ繧ｯ繝医Μ繧貞炎髯､縺励�邨先棡繧偵Ξ繧ｹ繝昴Φ繧ｹ繝倥ャ繝�ｒ繧ｻ繝�ヨ縺吶ｋ
    @Override
    public void execute() {
        FileSystem hdfs = getFileSystem();
        try {
            if (hdfs.delete(getFullPath(), true) == false) {
                logger.fine("cannot remove directory " + getFullPath());
                setResponseHeader(EIO, 0);
                return;
            }

            setResponseHeader(SUCCESS, 0);
        } catch (IOException ex) {
            logger.fine("IOException happend when removing directory " + getFullPath());
            ex.printStackTrace();
            setResponseHeader(EIO, 0);
        }
    }
     */ 
    
    @Override
    public IumfsFile getFile() {
        return HdfsFile.getFile(getServer(), getPathname());
    }       
}
