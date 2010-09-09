/*
 * MkdirRequest.java
 * 
 * Copright (c) 2010  Kazuyoshi Aizawa <admin2@whiteboard.ne.jp>
 */
import java.io.IOException;
import org.apache.hadoop.fs.FileSystem;

/**
 * <p>MKDIR リクエストを表すクラス</p>
 */
class MkdirRequest extends Request{
    /**
     * <p>HDFS 上にディレクトリを作成し、結果をレスポンスヘッダをセットする</p>
     * <p>FileSystem.mkdir は既存ディレクトリがあっても例外やエラーには
     * ならないので、MkdirRequest は自身の内部で既存ディレクトリを調べて
     * もし既存ディレクトリがあった場合には EEXIST を返す。</p>
     */
    @Override
    public void process() {
        /*
         * Hadoop HDFS 上に新規ディレクトリを作成する
         */
        FileSystem hdfs = getFileSystem();
        try {
            if(hdfs.exists(getFullPath()) == true){
                logger.info("cannot create directory");
                setResponseHeader(EEXIST, 0);
                return;
            }
            
            if (hdfs.mkdirs(getFullPath()) == false){
                logger.info("cannot create directory");
                setResponseHeader(EIO, 0);
                return;
            }
            /*
             * レスポンスヘッダをセット
             */
            setResponseHeader(SUCCESS, 0);
        } catch (IOException ex) {
            logger.info("IOException happend when removing directory");
            ex.printStackTrace();
            setResponseHeader(EIO, 0);
        }
    }

}
