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
import iumfs.Request;
import iumfs.RequestFactory;
import iumfs.UnknownRequestException;
import java.nio.BufferUnderflowException;

/**
 * <p>デバイスドライバから受け取った データを元に、適切なリクエストクラスの
 * インスタンスを返すファクトリ</p>
 * TODO: Instance をプールしておいて、効率的に利用する
 */
public class HdfsRequestFactory extends RequestFactory {

    @Override
    public Request createInstance(long request_type) {
        try {
            Request req = null;
            switch ((int) request_type) {
                case Request.READ_REQUEST:
                    req = new HdfsReadRequest();
                    break;
                case Request.READDIR_REQUEST:
                    req = new HdfsReadDirRequest();
                    break;
                case Request.GETATTR_REQUEST:
                    req = new HdfsGetAttrRequest();
                    break;
                case Request.WRITE_REQUEST:
                    req = new HdfsWriteRequest();
                    break;
                case Request.CREATE_REQUEST:
                    req = new HdfsCreateRequest();
                    break;
                case Request.REMOVE_REQUEST:
                    req = new HdfsRemoveRequest();
                    break;
                case Request.MKDIR_REQUEST:
                    req = new HdfsMkdirRequest();
                    break;
                case Request.RMDIR_REQUEST:
                    req = new HdfsRmdirRequest();
                    break;
                default:
                    logger.warning("Unknown request: " + request_type);
                    throw new UnknownRequestException();
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
}
