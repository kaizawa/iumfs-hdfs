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
import iumfs.GetAttrRequest;
import org.apache.hadoop.fs.FileStatus;
import org.apache.hadoop.fs.FileSystem;

/**
 *  GETATTR リクエストを表すクラス
 */
class HdfsGetAttrRequest extends GetAttrRequest {

    final public static int ATTR_DATA_LEN = 72; // long x 9 フィールド

    @Override
    public IumfsFile getFile() {
        return HdfsFile.getFile(getServer(), getPathname());
    }
}
