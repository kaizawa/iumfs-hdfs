/*
 * ReadRequest.java
 *
 * Copright (c) 2010  Kazuyoshi Aizawa <admin2@whiteboard.ne.jp>
 */
import java.io.IOException;
import org.apache.hadoop.fs.FSDataInputStream;
import org.apache.hadoop.fs.FileSystem;

/**
 *  READ リクエストを表すクラス
 */
public class ReadRequest extends Request {

    /**
     * HDFS 上のファイルをオープンし、結果をレスポンスヘッダをセットする
     */
    @Override
    public void process() {

        FileSystem hdfs = getFileSystem();
        try {
            FSDataInputStream fsdis = hdfs.open(getFullPath());

            /*
             * ファイルの指定オフセット/サイズのデータを書き込み用バッファに読み込む
             */
            int ret = fsdis.read(getOffset(), wbbuf.array(), Request.RESPONSE_HEADER_SIZE, (int) getSize());
            fsdis.close();
            logger.info("read offset=" + getOffset() + ",size=" + getSize());
            /*
             * レスポンスヘッダをセット
             */
            setResponseHeader(SUCCESS, ret);

        } catch (IOException ex) {
            logger.info("IOException happend when reading hdfs. offset=" + getOffset() + ",size=" + getSize());
            ex.printStackTrace();
            setResponseHeader(ENOENT, 0);
        }
    }
}
