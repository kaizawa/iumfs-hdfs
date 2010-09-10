/*
 * RmoveRequest.java
 *  
 * Copright (c) 2010  Kazuyoshi Aizawa <admin2@whiteboard.ne.jp>
 */

import java.io.IOException;
import java.util.logging.Level;
import java.util.logging.Logger;
import org.apache.hadoop.fs.FileSystem;

/**
 *  REMOVE リクエストを表すクラス
 */
class RemoveRequest extends Request {

    /**
     * Hadoop HDFS 上のファイルを削除し、結果をレスポンスヘッダをセットする
     */
    @Override
    public void process() {

        FileSystem hdfs = getFileSystem();
        try {
            if (hdfs.delete(getFullPath(), true) == false) {
                logger.fine("cannot remove " + getFullPath());
                setResponseHeader(EIO, 0);
                return;
            }
            /*
             * レスポンスヘッダをセット
             */
            setResponseHeader(SUCCESS, 0);
        } catch (IOException ex) {
            logger.fine("IOException happend when removing " + getFullPath());
            ex.printStackTrace();
            setResponseHeader(EIO, 0);
        }
    }
}
