/*
 * CreateRequest.java
 *
 * Copright (c) 2010  Kazuyoshi Aizawa <admin2@whiteboard.ne.jp>
 */
import java.io.IOException;
import org.apache.hadoop.fs.FSDataOutputStream;
import org.apache.hadoop.fs.FileSystem;
import org.apache.hadoop.hdfs.protocol.AlreadyBeingCreatedException;

/**
 * <p>CREATE リクエストを表すクラス</p>
 */
class CreateRequest extends Request {

    /**
     * <p>FileSystem.create を実行する</p>
     * <p>creat(2) が呼ばれたということは既存ファイルがあった場合
     * 既存データを削除(O_TRUNC相当)しなければならないが、 HDFS では データの途中
     * 変更はできないので、既存ファイルがあったらエラーリターンする</p>
     */
    @Override
    public void process() {
        /*
         * HDFS 上に新規ファイルを作成し、結果をレスポンスヘッダをセットする
         */
        FileSystem hdfs = getFileSystem();
        try {
            //ファイルが存在したら EEXIST を返す
            if(hdfs.exists(getFullPath()) == true){
                logger.info("cannot create file");
                setResponseHeader(EEXIST, 0);
                return;
            }
            
            FSDataOutputStream fsdos = hdfs.create(getFullPath());
            /*
             * レスポンスヘッダをセット
             */
            fsdos.close();
            setResponseHeader(SUCCESS, 0);
        } catch (AlreadyBeingCreatedException ex) {
            logger.info("AlreadyBeingCreatedException when writing");
            ex.printStackTrace();
            setResponseHeader(EEXIST, 0);
        } catch (IOException ex) {
            logger.info("IOException happend when writing");
            ex.printStackTrace();
            setResponseHeader(ENOTSUP, 0);
        }
    }
}
