/**********************************************************
 * fstestd.c
 *
 * Copright (c) 2010  Kazuyoshi Aizawa <admin2@whiteboard.ne.jp>
 *
 * User mode daemon used for functional test of IUMFS filesystem
 * 
 *********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/varargs.h>
#include <syslog.h>
#include <libgen.h>
#include <netdb.h>
#include <strings.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/vnode.h>
#include <dirent.h>
#include <stddef.h>

#include "iumfs.h"

#define ERR_MSG_MAX    300       // syslog に出力する最長文字数
#define SELECT_CMD_TIMEOUT    10 // TEST コマンド発行時のタイムアウト
#define RETRY_SLEEP_SEC       1  // リトライまでの待ち時間
#define RETRY_MAX             1  // リトライ回数
#define FS_BLOCK_SIZE         512 // このファイルシステムのブロックサイズ

#define MAX_DIRENTS_SIZE DEVICE_BUFFER_SIZE // dirent の合計の最大サイズ

/*
 * TEST セッションの管理構造体
 */
typedef struct testcntl {
    int filefd; // ローカルファイルのFD
    int devfd; // iumfsctl デバイスファイルのFD
    int statusflag; // ステータスフラグ
    char *server; // TEST サーバ名
    char *loginname; // ログイン名
    char *loginpass; // ログインパスワード
    int dataport; // データ転送用のポート番号
    char *basepath; // クライアントが要求しているベースのパス名
    char pathname[IUMFS_MAXPATHLEN];
    char *request_buffer;
    char *response_buffer;
    char *read_buffer;
} testcntl_t;

/*
 * ステータスフラグ
 */
#define     CNTL_OPEN        0x01  // 制御セッションがオープンしている
#define     LOGGED_IN        0x02  // ログイン完了済み
#define     DATA_OPEN        0x04  // データセッションがオープンしている
#define     CNTL_ERR         0x08  // 制御セッションが回復不能なエラー状態
#define     DATA_ERR         0x10  // データセッションが回復不可能なエラー状態

#define DEVPATH "/devices/pseudo/iumfs@0:iumfscntl"

int become_daemon();
void print_usage(char *);
void print_err(int, char *, ...);
void close_filefd(testcntl_t * const);
int process_readdir_request(testcntl_t * const, char *);
int process_read_request(testcntl_t * const, char *, off_t, size_t);
int process_getattr_request(testcntl_t * const, char *);
void parse_attributes(iumfs_vattr_t *, struct stat *);
int process_write_request(testcntl_t * const, char *, off_t, size_t, size_t);
int process_create_request(testcntl_t *, const char *);
int process_remove_request(testcntl_t *, const char *);
int process_mkdir_request(testcntl_t *, const char *);
int process_rmdir_request(testcntl_t *, const char *);

int debuglevel = 0; // とりあえず デフォルトのデバッグレベルを 1 にする
int use_syslog = 0; // メッセージを STDERR でなく、syslog に出力する

#define PRINT_ERR(args) \
             if (debuglevel > 0){\
                 print_err args;\
             }    

testcntl_t *gtestp;

int
main(int argc, char *argv[])
{
    testcntl_t *testp;
    int c;
    char pathname[IUMFS_MAXPATHLEN]; // ファイルパス
    request_t *req;
    int result;
    static fd_set fds, err_fds;

    testp = gtestp = (testcntl_t *) malloc(sizeof (testcntl_t));

    memset(testp, 0x0, sizeof (testcntl_t));

    testp->filefd = -1;

    while ((c = getopt(argc, argv, "d:")) != EOF) {
        switch (c) {
            case 'd':
                //デバッグレベル
                debuglevel = atoi(optarg);
                break;
            default:
                print_usage(argv[0]);
                break;
        }
    }

    testp->devfd = open(DEVPATH, O_RDWR, 0666);
    if (testp->devfd < 0) {
        perror("open");
        exit(1);
    }
    PRINT_ERR((LOG_INFO, "main: successfully opened iumfscntl device\n"));

    /*
     * リクエスト用バッファ
     */
    if ((testp->request_buffer = (char *) malloc(MAX_REQUEST_SIZE)) == NULL) {
        perror("malloc");
        exit(1);
    }
    memset(testp->request_buffer, 0x0, MAX_REQUEST_SIZE);
    req = (request_t *) testp->request_buffer;
    PRINT_ERR((LOG_INFO, "main: malloc for request__buffer succeeded\n"));

    /*
     * レスポンス用バッファ
     */
    if ((testp->response_buffer = (char *) malloc(MAX_RESPONSE_SIZE)) == NULL) {
        perror("malloc");
        exit(1);
    }
    memset(testp->response_buffer, 0x0, MAX_RESPONSE_SIZE);
    PRINT_ERR((LOG_INFO, "main: malloc for response_buffer succeeded\n"));

    /* syslog のための設定。Facility は　LOG_USER とする */
    openlog(basename(argv[0]), LOG_PID, LOG_USER);

    sigignore(SIGPIPE);

    /*
     * ここまではとりあえず、フォアグラウンドで実行。
     * ここからは、デバッグレベル 0 （デフォルト）なら、バックグラウンド
     * で実行し、そうでなければフォアグラウンド続行。
     */
    if (debuglevel == 0) {
        PRINT_ERR((LOG_INFO, "Going to background mode\n"));
        if (become_daemon() != 0) {
            print_err(LOG_ERR, "can't become daemon\n");
            exit(1);
        }
    }

    FD_ZERO(&fds);
    FD_ZERO(&err_fds);

    do {
        size_t ret;

        /*
         * iumfscntl デバイスの FD を監視し、新規リクエストを待つ。
         */
        FD_ZERO(&fds);

        FD_SET(testp->devfd, &fds);

        ret = select(FD_SETSIZE, &fds, NULL, &err_fds, NULL);
        if (ret < 0) {
            print_err(LOG_ERR, "main: select: %s\n", strerror(errno));
            exit(1);
        }

        /*
         * ここにくるのは iumfscntl デバイスが READ 可能な状態の時だけ。
         */
        ret = read(testp->devfd, req, MAX_REQUEST_SIZE);
        if (ret < sizeof (request_t)) {
            print_err(LOG_ERR, "main: read size invalid ret(%d) < sizeof(request_t)(%d)\n",
                      ret, sizeof (request_t));
            sleep(1);
            continue;
        }

        PRINT_ERR((LOG_INFO, "==============================================\n"));

        if (req->mountopts->basepath == NULL)
            PRINT_ERR((LOG_ERR, "main: req->mountopts->basepath is NULL\n"));

        if (req->pathname == NULL)
            PRINT_ERR((LOG_ERR, "main: req->pathname is NULL\n"));

         PRINT_ERR((LOG_ERR, "main: req->mountopts->basepath=%s\n", req->mountopts->basepath));
         PRINT_ERR((LOG_ERR, "main: req->pathname=%s\n", req->pathname));
        /*
         * サーバ上の実際のパス名を得る。
         * もしベースパスがルートだったら、余計な「/」はつけない。
         */
        if (ISROOT(req->mountopts->basepath))
            snprintf(pathname, IUMFS_MAXPATHLEN, "%s", req->pathname);
        else
            snprintf(pathname, IUMFS_MAXPATHLEN, "%s%s", req->mountopts->basepath, req->pathname);

        switch (req->request_type) {
            case READ_REQUEST:
                PRINT_ERR((LOG_INFO, "READ_REQUEST\n"));
                ret = process_read_request(testp, pathname, req->offset, req->size);
                break;
            case READDIR_REQUEST:
                PRINT_ERR((LOG_INFO, "READDIR_REQUEST\n"));
                ret = process_readdir_request(testp, pathname);
                break;
            case GETATTR_REQUEST:
                PRINT_ERR((LOG_INFO, "GETATTR_REQUEST\n"));
                ret = process_getattr_request(testp, pathname);
                break;
            case WRITE_REQUEST:
                PRINT_ERR((LOG_INFO, "WRITE_REQUEST\n"));
                ret = process_write_request(testp, pathname, req->offset, req->size, req->datasize);
                break;
            case CREATE_REQUEST:
                PRINT_ERR((LOG_INFO, "CREATE_REQUEST\n"));
                ret = process_create_request(testp, pathname);
                break;
            case REMOVE_REQUEST:
                PRINT_ERR((LOG_INFO, "REMOVE_REQUEST\n"));
                ret = process_remove_request(testp, pathname);
                break;
            case MKDIR_REQUEST:
                PRINT_ERR((LOG_INFO, "MKDIR_REQUEST\n"));
                ret = process_mkdir_request(testp, pathname);
                break;
            case RMDIR_REQUEST:
                PRINT_ERR((LOG_INFO, "RMDIR_REQUEST\n"));
                ret = process_rmdir_request(testp, pathname);
                break;                                                     
            default:
                result = ENOSYS;
                PRINT_ERR((LOG_ERR, "main: Unknown request type 0x%x\n", req->request_type));
                write(testp->devfd, &result, sizeof (int));
                ret = 0;
                break;
        }
        PRINT_ERR((LOG_INFO, "main: return: %s (%d)\n", strerror(ret), ret));        
    } while (1);
    exit(0);
}

/*****************************************************************************
 * print_usage()
 *
 * Usage を表示し、終了する。
 *****************************************************************************/
void
print_usage(char *argv)
{
    printf("Usage: %s [-d level]\n", argv);
    printf("\t-d level    : Debug level[0-1]\n");
    exit(0);
}

/*****************************************************************************
 * become_daemon()
 *
 * 標準入出力、標準エラー出力をクローズし、バックグラウンドに移行する。
 *****************************************************************************/
int
become_daemon()
{
    chdir("/");
    umask(0);
    signal(SIGHUP, SIG_IGN);

    if (fork() == 0) {
        use_syslog = 1;
        close(0);
        close(1);
        close(2);
        /* 新セッションの開始 */
        if (setsid() < 0)
            return (-1);
    } else {
        exit(0);
    }
    return (0);
}

/***********************************************************
 * print_err
 *
 * エラーメッセージを表示するルーチン。
 *
 * debuglevel 0 で LOG_WARRNING 以上のメッセージを出力（デフォルト）
 * debuglevel 1 で LOG_NOTICE 以上のメッセージを出力 
 * debuglevel 2 で LOG_INFO 以上のメッセージを出力 
 * debuglevel 3 で LOG_DEBUG 以上のメッセージを出力
 *
 * この print_err() のラッパーになっている PRINT_ERR マクロは
 * DEBUG フラグが define されている場合にだけ有効になる。
 *
 ***********************************************************/
void
print_err(int level, char *format, ...)
{
    va_list ap;
    char buf[ERR_MSG_MAX];

    if (level > debuglevel + 4)
        return;

    va_start(ap, format);
    vsnprintf(buf, ERR_MSG_MAX, format, ap);
    va_end(ap);

    if (use_syslog)
        syslog(level, buf);
    else
        fprintf(stderr, buf);
}

/*****************************************************************************
 * close_filefd()
 *
 * TEST コントロールセッションをクローズする。
 * 実際の socket のクローズ処理は close_socket() が行う。
 *
 *  引数：
 *           testp : TEST セッションの管理構造体
 *
 * 戻り値：
 *           無し
 *****************************************************************************/
void
close_filefd(testcntl_t * const testp)
{
    close(testp->filefd);

    testp->filefd = -1;

    PRINT_ERR((LOG_DEBUG, "close_filefd: returned\n"));
}

/*****************************************************************************
 * process_readdir_request
 *
 * main() から呼ばれ、READDIR_REQUEST を処理する
 *
 *  引数：
 *
 *           testp      : testcntl 構造体
 *           pathname  : 読み込むディレクトリのパス
 *
 * 戻り値：
 *           正常時: 0
 *         エラー時: エラー番号 
 *
 *****************************************************************************/
int
process_readdir_request(testcntl_t * const testp, char *pathname)
{
    size_t readsize = 0;
    DIR *dirp;
    dirent64_t *dp;
    response_t *res;
    iumfs_dirent_t *idp, *idphead; // driver に受け渡し用の独自の dirent 構造体
    size_t left_dirents_size = MAX_DIRENTS_SIZE; // 処理可能な dirent の最大合計サイズ
    int count = 0;
    unsigned short namelen, reclen;
    int err = 0;

    PRINT_ERR((LOG_DEBUG, "process_readdir_request: called\n"));

    res = (response_t *) testp->response_buffer;

    if ((dirp = opendir(pathname)) == NULL) {
        // 指定のディレクトリが見つからなかった。
        print_err(LOG_ERR, "process_readdir_request: opendir couldn't open %s: %s \n",
                  pathname, strerror(errno));        
        err = errno;
        res->datasize = 0;
        goto out;
    }

    idphead = (iumfs_dirent_t *) ((char *) res + sizeof (response_t));
    idp = idphead;

    /*
     * iumfs ファイルシステムは readdir のレスポンスとして iumfs_dirent_t 構造体
     * が並んでいることを期待する。
     * See iumfs_request.c#245
     */
    while ((dp = readdir64(dirp)) != NULL) {
        namelen = dp->d_reclen - (offsetof(dirent64_t, d_name[0]));
        reclen = IUMFS_DIRENT_RECLEN(namelen);
        /*
         * 確保した一時バッファより大きくなってしまう場合、それ以降の
         * ディレクトリエントリを無視する。
         * もしこのエラーが出力されたら MAX_DIRENTS_SIZE を大きくする
         * ことを検討する。(今は 1MB) 
         */
        if (left_dirents_size < reclen) {
            print_err(LOG_ERR, "process_readdir_request: too many entries... drop rest of etnries\n");
            break;
        }
        idp->i_reclen = reclen;
        memcpy(idp->i_name, dp->d_name, namelen);
        readsize += reclen;
        PRINT_ERR((LOG_DEBUG, "process_readdir_request: #%d \"%s\" reclen=%d\n", ++count, idp->i_name, idp->i_reclen));
        left_dirents_size -= reclen;
        idp = (iumfs_dirent_t *) ((char *) idp + reclen);
    }
    PRINT_ERR((LOG_DEBUG, "process_readdir_request: readsize=%d\n", readsize));
    res->datasize = readsize;

  out:
    /*
     * Driver に response ヘッダを書き込む
     */
    res->request_type = READDIR_REQUEST;
    res->result = err;
    write(testp->devfd, res, sizeof (response_t) + res->datasize);
    PRINT_ERR((LOG_DEBUG, "process_readdir_request: successfully wrote to driver\n"));
    closedir(dirp);
    return (err);
}

/*****************************************************************************
 * process_read_request
 *
 * main() から呼ばれ、READ_REQUEST を処理する
 *
 *  引数：
 *
 *           testp      : testcntl 構造体
 *           pathname  : データを読み込むファイルのパス
 *           offset    : ファイルのデータ読み込み開始位置
 *           size      : 要求されたデータサイズ
 *
 * 戻り値：
 *           正常時: 0
 *         エラー時: エラー番号
 *
 *****************************************************************************/
int
process_read_request(testcntl_t * const testp, char *pathname, off_t offset, size_t size)
{
    int readsize;
    int fd = testp->filefd;
    response_t *res;
    char *readp;
    int err = 0;

    PRINT_ERR((LOG_DEBUG, "process_read_request: called\n"));
    PRINT_ERR((LOG_DEBUG, "process_read_request: pathhame=%s, offset=%ld, size=%ld\n", pathname, offset, size));

    res = (response_t *) testp->response_buffer;
    readp = (char *) res + sizeof (response_t);

    PRINT_ERR((LOG_DEBUG, "process_read_request: pathhame=%s, testp->pathname=%s\n", pathname, testp->pathname));
    /*
     * 既存のfilefdのパス名を確認し、もしこれから読もうとしているファイル
     * と別のパス名だったら新規のFDをオープンし、既存のFDをクローズする。
     */
    if (fd < 0 || (strcmp(testp->pathname, pathname) != 0)) {
        if ((fd = open(pathname, O_RDWR)) < 0) {
            PRINT_ERR((LOG_DEBUG, "process_read_request: failed to open %s : %s (%d)\n", pathname, strerror(errno), errno));
            err = errno;
            goto out;
        } 
        PRINT_ERR((LOG_DEBUG, "process_read_request: opened new fd=%d, closing old fd=%d \n", fd, testp->filefd));
        close(testp->filefd);
        testp->filefd = fd;
        strlcpy(testp->pathname, pathname, strlen(pathname) + 1);
    }

    readsize = pread(fd, readp, size, offset);
    PRINT_ERR((LOG_INFO, "process_read_request: pread returned %d\n", readsize));

    res->request_type = READ_REQUEST;
    if (readsize < 0) {
        res->datasize = 0;        
        err = errno;
        close_filefd(testp);
        PRINT_ERR((LOG_DEBUG, "process_read_request: %s (%d)\n",strerror(errno),errno));        
    } else {
        res->datasize = readsize;
    }

  out:        
    res->result = err;
    write(testp->devfd, res, sizeof (response_t) + res->datasize);
    return (err);
}

/*****************************************************************************
 * process_getattr_request
 *
 * main() から呼ばれ、GETATTR_REQUEST を処理する
 *
 *  引数：
 *
 *           testp     : testcntl 構造体
 *           pathname  : データを読み込むファイルのパス
 *           offset    : ファイルのデータ読み込み開始位置
 *
 * 戻り値：
 *           正常時:  0
 *         エラー時: エラー番号 
 *         
 *****************************************************************************/
int
process_getattr_request(testcntl_t * const testp, char *pathname)
{
    struct stat st[1];
    iumfs_vattr_t *ivap;
    response_t *res;
    int err = 0;

    PRINT_ERR((LOG_DEBUG, "process_getattr_request: called\n"));

    res = (response_t *) testp->response_buffer;
    ivap = (iumfs_vattr_t *) ((char *) res + sizeof (response_t));
    res->request_type = GETATTR_REQUEST;

    if ((stat(pathname, st)) < 0) {
        err = errno;
        PRINT_ERR((LOG_DEBUG, "process_getattr_request: %s (%d)\n",strerror(err),err));            
        res->result = err;
        res->datasize = 0;
        goto out;
    } 
    /*
     * stat 構造体を vatt 構造体にマップ
     */
    parse_attributes(ivap, st);
    res->datasize = sizeof (iumfs_vattr_t);
    PRINT_ERR((LOG_DEBUG, "process_getattr_request: filesize = %d\n", ivap->i_size));

  out:
    res->result = err;
    write(testp->devfd, res, sizeof (response_t) + res->datasize);
    return (err);
}

/**************************************************************
 * parse_attributes()
 *
 * stat 構造体を iumfs_vattr 構造体にマップする
 *
 * 引数
 *
 *  ivap : 解析した属性値をセットする iumfs_vattr 構造体
 *  stat : 対象のローカルファイルの stat 構造体
 *
 * 戻り値
 *
 *      無し
 *
 **************************************************************/
void
parse_attributes(iumfs_vattr_t *ivap, struct stat *stat)
{
    //time_t timenow;        // 現在時刻がセットされる time 構造体        

    PRINT_ERR((LOG_DEBUG, "parse_attributes: called\n"));

    /*
     * ファイルタイプのチェック
     * 出力結果の最初の一文字目で判断する。
     */
    if (S_ISDIR(stat->st_mode)) {
        ivap->i_type = VDIR; // ディレクトリ
        PRINT_ERR((LOG_DEBUG, "parse_attributes: is VDIR\n"));
    } else if (S_ISDOOR(stat->st_mode)) {
        ivap->i_type = VDOOR; // DOOR ファイル
        PRINT_ERR((LOG_DEBUG, "parse_attributes: is VDOOR\n"));
    } else if (S_ISLNK(stat->st_mode)) {
        ivap->i_type = VLNK; // シンボリックリンク
        PRINT_ERR((LOG_DEBUG, "parse_attributes: is VLNK\n"));
    } else if (S_ISBLK(stat->st_mode)) {
        ivap->i_type = VBLK; // ブロックデバイス
        PRINT_ERR((LOG_DEBUG, "parse_attributes: is VBLK\n"));
    } else if (S_ISCHR(stat->st_mode)) {
        ivap->i_type = VCHR; // キャラクタデバイス
        PRINT_ERR((LOG_DEBUG, "parse_attributes: is VCHR\n"));
    } else if (S_ISFIFO(stat->st_mode)) {
        ivap->i_type = VFIFO; // FIFO
        PRINT_ERR((LOG_DEBUG, "parse_attributes: is VFIFO\n"));
    } else if (S_ISPORT(stat->st_mode)) {
        ivap->i_type = VPORT; // ...?
        PRINT_ERR((LOG_DEBUG, "parse_attributes: is VPORT\n"));
    } else if (S_ISSOCK(stat->st_mode)) {
        ivap->i_type = VSOCK; // ソケット
        PRINT_ERR((LOG_DEBUG, "parse_attributes: is VSOCKET\n"));
    } else {
        ivap->i_type = VREG; // 通常ファイル。
        PRINT_ERR((LOG_DEBUG, "parse_attributes: is VREG\n"));
    }

    /*
     * モードを設定
     */
    ivap->i_mode = stat->st_mode;

    /*
     * ファイルサイズを設定
     */
    ivap->i_size = stat->st_size;

    /*
     * mtime, atime, ctime ともに現在の時間をセット
     * (timestruc_t と time_t のマッピングがめんどくさかったので・・・)
     */
    //time(&timenow);
    //ivap->mtime_sec = ivap->atime_sec = ivap->ctime_sec = timenow;
    ivap->mtime_sec = stat->st_mtime;
    ivap->atime_sec = stat->st_atime;
    ivap->ctime_sec = stat->st_ctime;
    return;
}

/*****************************************************************************
 * process_write_request
 *
 * main() から呼ばれ、WRITE_REQUEST を処理する
 *
 *  引数：
 *
 *           testp      : testcntl 構造体
 *           pathname  : データを読み込むファイルのパス
 *           offset    : ファイルのデータ読み込み開始位置
 *           size      : 要求されたデータサイズ
 *           size      : 制御デバイスから読み込んだデータサイズ
 *
 * 戻り値：
 *           正常時: 0
 *         エラー時: エラー番号
 *
 *****************************************************************************/
int
process_write_request(testcntl_t * const testp, char *pathname, off_t offset,
                      size_t size, size_t datasize)
{
    int writesize;
    int fd = testp->filefd;
    response_t *res;
    char *writep;
    int err = 0;

    PRINT_ERR((LOG_DEBUG, "process_write_request: called\n"));
    PRINT_ERR((LOG_DEBUG, "process_write_request: pathhame=%s, offset=%ld, size=%ld, datasize=%ld\n", pathname, offset, size, datasize));

    writep = (char *) testp->request_buffer + sizeof (request_t);

    PRINT_ERR((LOG_DEBUG, "process_write_request: pathhame=%s, testp->pathname=%s\n", pathname, testp->pathname));
    /*
     * 既存のfilefdのパス名を確認し、もしこれから書き込もうとしているファイル
     * と別のパス名だったら新規のFDをオープンし、既存のFDをクローズする。
     */
    if (fd < 0 || (strcmp(testp->pathname, pathname) != 0)) {
        if ((fd = open(pathname, O_RDWR)) < 0) {
            print_err(LOG_ERR, "process_readdir_request: opendir couldn't open %s\n", pathname);            
            err = errno;
            goto out;
        }
        PRINT_ERR((LOG_DEBUG, "process_write_request: opened new fd=%d, closing old fd=%d \n", fd, testp->filefd));
        close(testp->filefd);
        testp->filefd = fd;
        strlcpy(testp->pathname, pathname, strlen(pathname) + 1);
    } else {
        PRINT_ERR((LOG_DEBUG, "process_write_request: uses existing fd=%d\n", fd));
    }

    /*
     * 実際にファイルに書き込むのは要求のあった「size」分だけ
     */
    writesize = pwrite(fd, writep, size, offset);
    if (writesize < 0) {
        err = errno;
        PRINT_ERR((LOG_DEBUG, "process_write_request: %s (%d)\n",strerror(errno),errno));
        close_filefd(testp);
    } else {
        PRINT_ERR((LOG_INFO, "process_write_request: pwrite() returned %d\n", writesize));
    }

  out:
    res = (response_t *) testp->response_buffer;    
    res->request_type = WRITE_REQUEST;    
    res->result = err;
    res->datasize = 0;
    write(testp->devfd, res, sizeof (response_t));
    return (err);
}

/*****************************************************************************
 * process_create_request
 *
 * main() から呼ばれ、CREATE_REQUEST を処理する
 *
 *  引数：
 *
 *           testp     : testcntl 構造体
 *           pathname  : 作成するファイルのパス
 *           ivap      : 作成するファイルの属性情報
 *
 * 戻り値：
 *           正常時:  0
 *         エラー時: エラー番号  
 *
 *****************************************************************************/
int
process_create_request(testcntl_t * const testp, const char *pathname)
{
    int fd = testp->filefd;
    response_t *res;
    iumfs_vattr_t *ivap;
    int err = 0;
    
    PRINT_ERR((LOG_DEBUG, "process_create_request: called\n"));
    PRINT_ERR((LOG_DEBUG, "process_create_request: pathhame=%s\n", pathname));
    
    ivap = (iumfs_vattr_t *)((char *) testp->request_buffer + sizeof (request_t));

    /*
     * ファイルをオープン
     */
    if ((fd = creat(pathname, ivap->i_mode)) < 0) {
        PRINT_ERR((LOG_DEBUG, "process_create_request: open: %s\n",strerror(errno)));        
        err = errno;
    } else {        
        PRINT_ERR((LOG_DEBUG, "process_create_request: opened new fd=%d, closing old fd=%d \n", fd, testp->filefd));
        close(testp->filefd);
        testp->filefd = fd;
        strlcpy(testp->pathname, pathname, strlen(pathname) + 1);
    }
    
    res = (response_t *) testp->response_buffer;
    res->request_type = CREATE_REQUEST;
    res->result = err;
    res->datasize = 0;
    write(testp->devfd, res, sizeof (response_t));
    return (err);
}

/*****************************************************************************
 * process_remove_request
 *
 * main() から呼ばれ、REMOVE_REQUEST を処理する
 *
 *  引数：
 *           testp      : testcntl 構造体
 *           pathname  : 削除するファイルのパス
 *
 * 戻り値：
 *           正常時:  0
 *         エラー時: エラー番号 
 *         
 *****************************************************************************/
int
process_remove_request(testcntl_t * const testp, const char *pathname)
{
    response_t *res;
    int err = 0;

    PRINT_ERR((LOG_DEBUG, "process_remove_request: called\n"));

    if ((unlink(pathname)) < 0) {
        err = errno;
        PRINT_ERR((LOG_DEBUG, "process_removerequest: unlink: %s\n", strerror(errno)));
    }
    
    res = (response_t *) testp->response_buffer;
    res->request_type = REMOVE_REQUEST;
    res->datasize = 0;    
    res->result = err;
    write(testp->devfd, res, sizeof (response_t));
    return (err);
}

/*****************************************************************************
 * process_mkdir_request
 *
 * main() から呼ばれ、MKDIR_REQUEST を処理する
 *
 *  引数：
 *
 *           testp     : testcntl 構造体
 *           pathname  : 作成するディレクトリのパス
 *           ivap      : 作成するディレクトリの属性情報
 *
 * 戻り値：
 *           正常時:  0
 *         エラー時: エラー番号 
 *
 *****************************************************************************/
int
process_mkdir_request(testcntl_t * const testp, const char *pathname)
{
    response_t *res;
    iumfs_vattr_t *ivap;
    int err = 0;
    
    PRINT_ERR((LOG_DEBUG, "process_mkdir_request: called\n"));
    PRINT_ERR((LOG_DEBUG, "process_mkdir_request: pathhame=%s\n", pathname));
    ivap = (iumfs_vattr_t *)((char *) testp->request_buffer + sizeof (request_t));

    /*
     * ディレクトリを作成
     */
    if ( mkdir(pathname, ivap->i_mode) < 0) {
        PRINT_ERR((LOG_DEBUG, "process_mkdir_request: mkdir: %s\n",strerror(errno)));        
        err = errno;
    } else {
        PRINT_ERR((LOG_DEBUG, "process_mkdir_request: created new directory\n"));
    }
    
    res = (response_t *) testp->response_buffer;
    res->request_type = MKDIR_REQUEST;
    res->result = err;
    res->datasize = 0;
    write(testp->devfd, res, sizeof (response_t));
    return (err);
}

/*****************************************************************************
 * process_rmdir_request
 *
 * main() から呼ばれ、RMDIR_REQUEST を処理する
 *
 *  引数：
 *           testp      : testcntl 構造体
 *           pathname  : 削除するディレクトリのパス
 *
 * 戻り値：
 *           正常時:  0
 *         エラー時: エラー番号 
 *         
 *****************************************************************************/
int
process_rmdir_request(testcntl_t * const testp, const char *pathname)
{
    response_t *res;
    int err = 0;

    PRINT_ERR((LOG_DEBUG, "process_rmdir_request: called\n"));

    if ((rmdir(pathname)) < 0) {
        err = errno;
        PRINT_ERR((LOG_DEBUG, "process_rmdir_equest: rmdir: %s\n", strerror(errno)));
    }
    
    res = (response_t *) testp->response_buffer;
    res->request_type = RMDIR_REQUEST;
    res->datasize = 0;    
    res->result = err;
    write(testp->devfd, res, sizeof (response_t));
    return(err);
}
