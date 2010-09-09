/*
 * RmdirRequest.java
 * 
 * Copright (c) 2010  Kazuyoshi Aizawa <admin2@whiteboard.ne.jp>
 */

import java.io.IOException;
import org.apache.hadoop.fs.FileSystem;

/**
 *  RMDIRリクエストを表すクラス
 */
class RmdirRequest extends Request {

    /**
     * Hadoop HDFS 上のディレクトリを削除し、結果をレスポンスヘッダをセットする
     */
    @Override
    public void process() {
        FileSystem hdfs = getFileSystem();
        try {
            if (hdfs.delete(getFullPath(), true) == false) {
                logger.info("cannot remove directory " + getFullPath());
                setResponseHeader(EIO, 0);
                return;
            }
            /*
             * レスポンスヘッダをセット
             */
            setResponseHeader(SUCCESS, 0);
        } catch (IOException ex) {
            logger.info("IOException happend when removing directory " + getFullPath());
            ex.printStackTrace();
            setResponseHeader(EIO, 0);
        }
    }
}
