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

/**
 * <p>Write Request class</p>
 */
public abstract class WriteRequest extends Request {

    private static final String CONT = "(cont) ";

    @Override
    public void execute() throws FileNotFoundException, IOException {
        File file = getFile();
        file.write(getData(), getSize(), getOffset());
        /*
         * Set response header
         */
        setResponseHeader(SUCCESS, 0);
    }
}
