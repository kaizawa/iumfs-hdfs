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

/**
 * <p>CREATE Request Class<br>
 * </p>
 */
public abstract class CreateRequest extends Request {

    /**
     * <p>Excecute FileSystem.create</p>
     * <p>
     * </p>
     */
    public void execute() {
        /*
         * HDFS 上に新規ファイルを作成し、結果をレスポンスヘッダをセットする
         */
        File file = getFile();


        file.create(getFullPath());
        /*
         * レスポンスヘッダをセット
         */
        fsdos.close();
        setResponseHeader(SUCCESS, 0);
    }

    abstract public File getFile();
}
