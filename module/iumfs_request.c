/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright (c) 1986, 2010, Oracle and/or its affiliates. All rights reserved.
 */

/*
 * Copright (c) 2010  Kazuyoshi Aizawa <admin2@whiteboard.ne.jp>
 * All rights reserved.
 */   
/**************************************************************
 * iumfs_request
 *
 * Routines for IUMFS filesystem module to pass request
 * to intermidicate device driver.
 * 
 **************************************************************/

/* 
 * ユーザモードデーモンにリクエスト（データ要求）するための
 * ルーチンが書かれたモジュール。現在デーモンに依頼できるのは
 * 以下のリクエスト
 *
 *     iumfs_request_read()    ... ファイルのデータを読む
 *     iumfs_request_readdir() ... ディレクトリエントリを読む
 *     iumfs_request_getattr() ... ファイルの属性値を得る 
 *     iumfs_request_lookup()  ... ファイルの有無を確認
 *     iumfs_request_create()  ... ファイルを作成
 *     iumfs_request_mkdir()   ...  ディレクトリの作成
 *
 *  各リクエストのルーチンは必ず以下の関数を順番どおりに
 *  呼び、複数のリクエストが同時に実行されないことを保証している。
 *
 *     iumfs_daemon_request_enter() .. リクエストの順番待ちをする 
 *     iumfs_daemon_request_start() .. リクエストを投げる
 *     iumfs_daemon_request_exit()  .. リクエストを終了する
 *
 */

#include <sys/modctl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/ksynch.h>
#include <sys/pathname.h>
#include <sys/file.h>

#include <vm/seg.h>
#include <vm/page.h>
#include <vm/pvn.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/hat.h>
#include <vm/as.h>
#include <vm/seg_kmem.h>

#include <stddef.h>
#include "iumfs.h"

extern iumfscntl_soft_t cntlsoft_list[MAX_INST]; // iumfscntl_soft_t 構造体の配列

/******************************************************************
 * iumfs_request_read()
 *
 * iumfs_getapage() から呼ばれ、ユーザモードデーモンに指定した
 * ファイルのオフセットからサイズ分のデータを要求する。
 *
 * 引数:
 *        bp  : buf 構造体
 *        vp  : 読み込むファイルの vnode 
 *
 * 戻り値
 *
 *    正常時   : 0
 *    エラー時 : エラー番号
 *    
 *****************************************************************/
int
iumfs_request_read(struct buf *bp, vnode_t *vp)
{
    iumfscntl_soft_t *cntlsoft; // iumfscntl デバイスのデバイスステータス構造体
    request_t *req;
    response_t *res;
    offset_t offset;
    size_t size;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    iumnode_t *inp;
    iumfs_mount_opts_t *mountopts;
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_request_read called\n"));

    /*
     * block 数から byte 数へ・・・なんか無意味な操作
     * ちなみに、b_bcount は PAGESIZE より大きい可能性もある
     */
    offset = ldbtob(bp->b_lblkno);
    size = bp->b_bcount;

    DEBUG_PRINT((CE_CONT, "iumfs_request_read: offset = %D, size = %d\n", offset, size));

    /*
     * オープン中で未使用の iumfscntl デバイスを受け取る。 
     * もしすべてのデバイスが使用中ならリクエストの順番待ちをする
     */  
    if((err = iumfs_daemon_request_enter(&cntlsoft))){
        goto out;
    }
    
    mutex_enter(&cntlsoft->d_lock);
    /*
     * マウントオプション、ファイルシステム依存ノード構造体、ユーザ空間とマッピング
     * しているメモリアドレスを得る。
     */
    inp = VNODE2IUMNODE(vp);
    iumfsp = VNODE2IUMFS(vp);
    mountopts = iumfsp->mountopts;
    req = (request_t *) cntlsoft->bufaddr;
    /*
     * ユーザモードデーモンに渡すリクエストを request 構造体にセット
     */
    bzero(req, sizeof (request_t));
    req->request_type = READ_REQUEST;
    strncpy(req->pathname, inp->pathname, IUMFS_MAXPATHLEN); // マウントポイントからの相対パス名
    bcopy(mountopts, req->mountopts, sizeof (iumfs_mount_opts_t));
    req->offset = offset; // オフセット    
    req->size = size; // サイズ
    req->datasize = 0;
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;
    mutex_exit(&cntlsoft->d_lock); // checking

    /*
     * リクエスト要求を開始する
     */
    err = iumfs_daemon_request_start(cntlsoft);
    if (err) {
         //エラーが発生した模様。リクエストを解除してエラーをリターン
        iumfs_daemon_request_exit(cntlsoft);
        goto out;
    }

    /*
     * デーモンから受け取ったデータをコピー
     * 受け取ったサイズによらず、コピーするのは最大で 'size' 分だけ。
     */
    mutex_enter(&cntlsoft->d_lock);
    // TODO: cntlsoft->bufusedsize に実際に uiomove されたサイズが書かれているので、それを確認するべき！
    res = (response_t *) cntlsoft->bufaddr;
    bcopy((char *) res + sizeof (response_t), bp->b_un.b_addr, MIN(size, res->datasize));
    mutex_exit(&cntlsoft->d_lock);

    /*
     * リクエストを解除。他の待ち thread を起こす
     */
    iumfs_daemon_request_exit(cntlsoft);

    DEBUG_PRINT((CE_CONT, "iumfs_request_read: copy data done\n"));
out:
    DEBUG_PRINT((CE_CONT, "iumfs_request_read: return(%d)\n",err));
    return (err);
}

/******************************************************************
 * iumfs_request_readdir()
 *
 * iumfs_readdir() から呼ばれ、ユーザモードデーモンに指定した
 * ディレクトリ内のエントリのリストを要求する。
 *
 * 引数:
 *        dirivp : リストを要求するディレクトリの vnode 構造体
 *
 * 戻り値
 *
 *   正常時   : 0
 *   エラー時 : エラー番号
 *
 *****************************************************************/
int
iumfs_request_readdir(vnode_t *dirvp)
{
    iumfscntl_soft_t *cntlsoft; // オープン中の iumfscntl デバイスステータス構造体
    iumfs_dirent_t *headp; // デーモンから返ってきたエントリリストの先頭ポインタ
    iumfs_dirent_t *idp; //  処理用ポインタ
    request_t *req; // リクエスト構造体
    response_t *res; // レスポンス構造体
    iumnode_t *dirinp; // ディレクトリのファイルシステム依存ノード構造体
    int err = 0;
    iumfs_mount_opts_t *mountopts; // マウントオプション
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    offset_t offset = 0;
    int64_t reclen, namelen;
    char *name;
    ino_t nodeid = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_request_readdir called\n"));

    /*
     * オープン中で未使用の iumfscntl デバイスを受け取る。 
     * もしすべてのデバイスが使用中ならリクエストの順番待ちをする
     */  
    if((err = iumfs_daemon_request_enter(&cntlsoft))){
        goto out;
    }

    mutex_enter(&cntlsoft->d_lock);
    /*
     * マウントオプション、ファイルシステム依存ノード構造体、ユーザ空間とマッピング
     * しているメモリアドレスを得る。
     */
    dirinp = VNODE2IUMNODE(dirvp);
    iumfsp = VNODE2IUMFS(dirvp);
    mountopts = iumfsp->mountopts;
    req = (request_t *) cntlsoft->bufaddr;

    /*
     * ユーザモードデーモンに渡すリクエストを request 構造体にセット
     */
    bzero(req, sizeof (request_t));
    req->request_type = READDIR_REQUEST;
    strncpy(req->pathname, dirinp->pathname, IUMFS_MAXPATHLEN); // マウントポイントからの相対パス名
    bcopy(mountopts, req->mountopts, sizeof (iumfs_mount_opts_t));
    req->datasize = 0;
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;
    mutex_exit(&cntlsoft->d_lock); //checking 

    /*
     * リクエスト要求を開始する
     */
    err = iumfs_daemon_request_start(cntlsoft);

    if (err) {
        // エラーが発生した模様。リクエストを解除してエラーリターン
        iumfs_daemon_request_exit(cntlsoft);
        goto out;
    }

    /*
     * 正常にデータを取得できた模様
     * データをコピーする。
     */
    mutex_enter(&cntlsoft->d_lock);
    res = (response_t *) cntlsoft->bufaddr;
    headp = (iumfs_dirent_t *) ((char *) res + sizeof (response_t));
    idp = headp;
    /*
     * デーモンから渡された iumfs_dirent_t リストをめぐる
     */
    while (offset < res->datasize) {
        name = idp->i_name;
        reclen = idp->i_reclen;
        // このマクロを使うとパディングの分もふくまれてしまう。
        // daemon 側で null terminate するようにした。
        //namelen = IUMFS_DIRENT_NAMELEN(idp->i_reclen);
        namelen = strlen(idp->i_name);

        DEBUG_PRINT((CE_CONT, "iumfs_request_readdir: name=\"%s\",reclen=%d,namelen=%d\n", name, reclen, namelen));

        /*
         * デーモンからのデータを元に算出したサイズの正常性をチェック
         */
        if (reclen <= 0 || namelen <= 0 || offset + reclen > res->datasize) {
            // デーモンから渡されたデータ長(i_reclen)がおかしい
            cmn_err(CE_WARN, "iumfs_request_readdir: Oops, reclen(%lld) is illegal(datasize=%lld,offset=%lld)\n",
                    (longlong_t) reclen, (longlong_t) res->datasize, offset);
            break;
        }

        /*
         * もしディレクトリに既存エントリが無ければ新しいノード番号を
         * 割当てた上で読み込んだエントリを追加。
         * lock for iumnode has already been aquired, so it calls no lock version of 
         * iumfs_add_entry_to_dir.
         */
        if (!iumfs_directory_entry_exist(dirvp, name)) {
            mutex_enter(&(iumfsp->iumfs_lock));
            nodeid = ++(iumfsp->iumfs_last_nodeid);
            mutex_exit(&(iumfsp->iumfs_lock));
            iumfs_add_entry_to_dir_nolock(dirvp, name, namelen, nodeid);
        }
        offset += reclen;
        idp = (iumfs_dirent_t *) ((char *) idp + reclen);
    }
    mutex_exit(&cntlsoft->d_lock);

    /*
     * リクエストを解除。他の待ち thread を起こす
     */
    iumfs_daemon_request_exit(cntlsoft);
out:
    DEBUG_PRINT((CE_CONT, "iumfs_request_readdir: return(%d)\n", err));
    return (err);
}

/******************************************************************
 * iumfs_daemon_request_enter
 *
 * ユーザモードデーモンへのリクエスト要求を開始するための順番待ちをする。
 * オープン中で、かつリクエスト受付可能な iumfscntl のアドレスを引数
 * としてわたされたアドレスにセットする。
 * すべての iumfscntl デバイス がリクエストを処理中であれば、この関数の中
 * で待たされる。
 *
 * 引数:
 *    cntlsoft : iumfscntl デバイスのステータス構造体のポインタのアドレス
 *
 * 戻り値
 *
 *    正常時: 0
 *    異常時: エラー番号
 * 
 *****************************************************************/
int
iumfs_daemon_request_enter(iumfscntl_soft_t **cntlsoftp)
{
    iumfscntl_soft_t *cntlsoft;
    int instance;
    
    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_enter called\n"));
    /*
     * インスタンス毎の iumfscntl デバイスのステータス構造体を設定
     */ 
    for (instance = 0 ; instance < MAX_INST ; instance++ ) {
        cntlsoft = &cntlsoft_list[instance];
        // ロック待ちするのは使われていると思われるので次へ
        if (mutex_tryenter(&cntlsoft->s_lock) == 0)
            continue;
        // まだオープンされていない場合, もしくはリクエスト処理中の場合も次へ
        if (!(cntlsoft->state & IUMFSCNTL_OPENED) || cntlsoft->state & REQUEST_INPROGRESS ) {
            if(!cntlsoft->state & IUMFSCNTL_OPENED) {
                DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_enter: inst=%d not opened. state=0x%x",instance,cntlsoft->state));
            }
            if(cntlsoft->state & REQUEST_INPROGRESS) {
                DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_enter: inst=%d in progress. state=0x%x",instance,cntlsoft->state));
            }
            mutex_exit(&cntlsoft->s_lock);
            continue;
        }
        // 開いているオープン中の iumfscntl が見つかったようだ。
        cntlsoft->state |= REQUEST_INPROGRESS; // リクエスト中フラグをセット。       
        mutex_exit(&cntlsoft->s_lock);
        break;
    }

    /*
     * 待ちなしで iumfscntl を見つけられなかった場合の処理。
     * 最初に見つかったオープン中の iumfscntl で待つ。
     */ 
    if(instance >= MAX_INST){
        DEBUG_PRINT((CE_WARN, "iumfscntl_daemon_request_enter: none of open instance is available."));        
        for (instance = 0 ; instance < MAX_INST ; instance++ ) {
            cntlsoft = &cntlsoft_list[instance];        
            mutex_enter(&cntlsoft->s_lock);
            if (!(cntlsoft->state & IUMFSCNTL_OPENED)){
                // オープンされていない。つぎへ。
                mutex_exit(&cntlsoft->s_lock);
                continue;
            }                        
            /*
             * すでに他の thread がデーモンへのリクエストを実行中だったら、その処理が終わるまで待つ。
             * もし、誰もほかにリクエストを実行中でなかったら REQUEST_INPROGRESS フラグを立てて
             * 処理を進める。（他の thread が同時に実行されないことを保障する）
             */
            while (cntlsoft->state & REQUEST_INPROGRESS) {
                if (cv_wait_sig(&cntlsoft->cv, &cntlsoft->s_lock) == 0) {
                    mutex_exit(&cntlsoft->s_lock);
                    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_enter: return(EINTR)\n"));
                    return (EINTR);
                }
            }
            // やっと使える iumfscntl が確保できた!
            cntlsoft->state |= REQUEST_INPROGRESS;
            mutex_exit(&cntlsoft->s_lock);
            break;
        }
    }
    /*
     * オープン中の iumfscntl が無い模様。
     * しかたないので、最初に見つかった iumfscntl で待つ。
     * TODO: OPEN 待ちの仕組みをつくるべき。こもままでは他のインスタンスでオープンされたのが気がつけない。
     */ 
    if(instance >= MAX_INST){
        instance = 0;
        cmn_err(CE_WARN, "iumfscntl_daemon_request_enter: no instance is opened.");
        cmn_err(CE_WARN, "iumfscntl_daemon_request_enter: waiting on instance 0.");         
            cntlsoft = &cntlsoft_list[instance];        
            mutex_enter(&cntlsoft->s_lock);
            while (cntlsoft->state & IUMFSCNTL_OPENED) {
                if (cv_wait_sig(&cntlsoft->cv, &cntlsoft->s_lock) == 0) {
                    mutex_exit(&cntlsoft->s_lock);
                    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_enter: return(EINTR)\n"));
                    return (EINTR);
                }
            }
            // やっとオープンした。他のスレッドに取られちゃっているかも。INPROGRESS フラグをチェック。
            while (cntlsoft->state & REQUEST_INPROGRESS) {
                if (cv_wait_sig(&cntlsoft->cv, &cntlsoft->s_lock) == 0) {
                    mutex_exit(&cntlsoft->s_lock);
                    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_enter: return(EINTR)\n"));
                    return (EINTR);
                }
            }            
            // やっと使える iumfscntl が確保できた!
            cntlsoft->state |= REQUEST_INPROGRESS;
            mutex_exit(&cntlsoft->s_lock);
    }
    *cntlsoftp = cntlsoft;
    cmn_err(CE_CONT, "iumfs_daemon_request_enter: uses instance %d", cntlsoft->instance);  

    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_enter: return(0)\n"));
    return (0);
}

/******************************************************************
 * iumfs_daemon_request_start
 *
 * ユーザモードデーモンへリクエストを要求する。
 * この関数は最初に iumfs_daemon_request_enter() 呼び出してリターンし
 * てきてから、つまり排他的に呼ばれなくてはならない。
 *
 * 引数:
 *        cntlsoft : iumfscntl デバイスのデバイスステータス構造体
 *
 * 戻り値
 *     正常時 : 0 
 *     異常時 : エラー番号
 * 
 *****************************************************************/
int
iumfs_daemon_request_start(iumfscntl_soft_t *cntlsoft)
{
    int err;
    int ret;
    int retrans = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_start called\n"));

    /*
     * iumfscntl_read() 内で待っている thread を cv_broadcast で起こす。
     */
    mutex_enter(&cntlsoft->s_lock);
    cntlsoft->state |= REQUEST_IS_SET; // REQUEST_IS_SET フラグを立てる
    cntlsoft->state |= DAEMON_INPROGRESS; // DAEMON_IN_PROGRESS フラグを立てる    
    cv_broadcast(&cntlsoft->cv); // thread を起こす
    mutex_exit(&cntlsoft->s_lock);

    /*
     * poll(2) を起こす
     */
    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_start: wakeup poll\n"));
    pollwakeup(&cntlsoft->pollhead, POLLIN | POLLRDNORM);

    /*
     * ユーザモードデーモンの反応を待つ(write or close)
     */
    mutex_enter(&cntlsoft->s_lock);
    while (cntlsoft->state & DAEMON_INPROGRESS) {
        DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_start: waiting for data from daemon(state = 0x%x)\n", cntlsoft->state));
        if ((ret = cv_timedwait_sig(&cntlsoft->cv, &cntlsoft->s_lock, ddi_get_lbolt() + DAEMON_TIMEOUT_TICK)) < 0) {
            // タイムアウト
            retrans++;                
            pollwakeup(&cntlsoft->pollhead, POLLERR | POLLRDBAND);
            cmn_err(CE_CONT, "daemon not responding. still trying. retrans=%d\n", retrans);
            continue;
        } else if (ret == 0) {
            /*
             * 割り込みを受けた。 EINTR を返す。
             * daemon に対しても POLLERR | POLLRDBAND で通知する。
             */
            DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_start: interrupt recieved.\n"));
            cntlsoft->state |= REQUEST_IS_CANCELED;
            mutex_exit(&cntlsoft->s_lock);
            pollwakeup(&cntlsoft->pollhead, POLLERR | POLLRDBAND);
            DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_start: return(EINTR)\n"));
            return (EINTR);
        }
    }
    /*
     * DAEMON_INPROGRESS フラグの解除を待っている thread は他にはいないはずなので、
     * cv_broadcase() は呼ばない。（というか意味が無い）
     * 逆に、この thread を起こしてくれるのは iumfscntl デバイスドライバの
     * iumfscntl_close() と iumfscntl_write() だけ。
     */
    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_start: data has come from daemon\n"));

    if (cntlsoft->state & BUFFER_INVALID) {
        /*
         * デーモンが死んだ、もしくはエラーを返してきた。
         */
        cmn_err(CE_CONT, "iumfs_daemon_request_start: buffer data is invalid.(state=0x%x). daemon might be dead\n",
                cntlsoft->state);
    }
    err = cntlsoft->error;
    mutex_exit(&cntlsoft->s_lock);

    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_start: return(%d)\n", err));
    return (err);
}

/******************************************************************
 * iumfs_daemon_request_exit
 *
 * リクエスト要求の完了処理をする。
 * 他のリクエスト要求待ちをしている thread を起こす。
 *
 * 引数:
 *        cntlsoft : iumfscntl デバイスのデバイスステータス構造体
 *
 * 戻り値
 *        無し
 * 
 *****************************************************************/
void
iumfs_daemon_request_exit(iumfscntl_soft_t *cntlsoft)
{

    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_exit called\n"));

    /*
     * 無用なフラグをはずし、他の thread を起こす。
     */
    mutex_enter(&cntlsoft->s_lock);
    cntlsoft->state &= ~(REQUEST_INPROGRESS | BUFFER_INVALID | REQUEST_IS_SET | REQUEST_IS_CANCELED);
    cv_broadcast(&cntlsoft->cv); // thread を起こす
    mutex_exit(&cntlsoft->s_lock);
    DEBUG_PRINT((CE_CONT, "iumfs_daemon_request_exit: return()\n"));
    return;
}

/******************************************************************
 * iumfs_request_lookup
 *
 * iumfs_lookup() から呼ばれ、ユーザモードデーモンに指定した
 * ディレクトリ内のファイルの存在を確認をし、かつ属性値を得る。
 *
 *
 * 引数:
 *        drivp : 検索するディレクトリの vnode 構造体
 *        name  : 確認するファイルのファイルシステムルートからのパス名
 *        vap   : 正常終了した場合、属性値を格納する vattr 構造体
 *
 * 戻り値
 *     正常時 : 0
 *     異常時 : エラー番号
 * 
 *****************************************************************/
int
iumfs_request_lookup(vnode_t *dirvp, char *pathname, vattr_t *vap)
{
    iumfscntl_soft_t *cntlsoft; // iumfscntl デバイスのデバイスステータス構造体
    request_t *req;
    response_t *res;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    iumfs_mount_opts_t *mountopts;
    int err = 0;
    iumfs_vattr_t *ivap;

    DEBUG_PRINT((CE_CONT, "iumfs_request_lookup called\n"));

    /*
     * オープン中で未使用の iumfscntl デバイスを受け取る。 
     * もしすべてのデバイスが使用中ならリクエストの順番待ちをする
     */  
    if((err = iumfs_daemon_request_enter(&cntlsoft))){
        goto out;
    }     

    mutex_enter(&cntlsoft->d_lock);
    /*
     * マウントオプション、ファイルシステム依存ノード構造体、ユーザ空間とマッピング
     * しているメモリアドレスを得る。
     */
    iumfsp = VNODE2IUMFS(dirvp);
    mountopts = iumfsp->mountopts;
    req = (request_t *) cntlsoft->bufaddr;
    /*
     * ユーザモードデーモンに渡すリクエストを request 構造体にセット
     */
    bzero(req, sizeof (request_t));
    req->request_type = GETATTR_REQUEST; // LOOKUP だが、中身は GETATTR と同じ
    snprintf(req->pathname, IUMFS_MAXPATHLEN, "%s", pathname); //マウントポイントからのパス名
    bcopy(mountopts, req->mountopts, sizeof (iumfs_mount_opts_t));
    req->datasize = 0;
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;
    mutex_exit(&cntlsoft->d_lock);

    /*
     * リクエスト要求を開始する
     */
    err = iumfs_daemon_request_start(cntlsoft);
    if (err) {
        /*
         * エラーが発生した模様。リクエストを解除してエラーリターン
         */
        iumfs_daemon_request_exit(cntlsoft);
        goto out;
    }

    /*
     * デーモンから受け取ったデータは cntlsoft->bufaddr にコピーされているはずなの
     * でそれをさらにコピー。
     * TODO: コピー回数が多くて非効率.要改善.
     */
    mutex_enter(&cntlsoft->d_lock);
    res = (response_t *) cntlsoft->bufaddr;
    if (res->datasize < sizeof (iumfs_vattr_t)) {
        /*
         * データ部のサイズが小さい。リクエストを解除してエラーリターン
         * EAGAIN は適切でないかも・・・
         */
        cmn_err(CE_CONT, "iumfs_request_lookup: res->datasize(%lld) != sizeof(iumfs_vattr_t)(%lld)\n",
                (longlong_t) res->datasize, (longlong_t)sizeof (iumfs_vattr_t));
        mutex_exit(&cntlsoft->d_lock);
        iumfs_daemon_request_exit(cntlsoft);
        err = EAGAIN;
        goto out;
    }
    ivap = (iumfs_vattr_t *) ((char *) res + sizeof (response_t));
    /*
     * TODO: 呼び出し側で使われるのは va_type だけ・・・ちょっと非効率
     */
    DEBUG_PRINT((CE_CONT, "iumfs_request_lookup: i_type=%ld\n", ivap->i_type));
    vap->va_type = ivap->i_type;
    mutex_exit(&cntlsoft->d_lock);

    /*
     * リクエストを解除。他の待ち thread を起こす
     */
    iumfs_daemon_request_exit(cntlsoft);
    DEBUG_PRINT((CE_CONT, "iumfs_request_lookup: copy data done\n"));
out:
    DEBUG_PRINT((CE_CONT, "iumfs_request_lookup: return(%d)\n", err));
    return (err);
}

/******************************************************************
 * iumfs_request_getattr
 *
 * iumfs_lookup() から呼ばれ、ユーザモードデーモンに指定した
 * ディレクトリ内のファイルの属性情報を得る。
 *
 *
 * 引数:
 *        vp    : 属性値変更するファイルの vnode 
 *
 * 戻り値
 *     正常時 : 0
 *     異常時 : エラー番号
 *
 * TODO: request_getattr と request_lookup の同一コード部分を共有できないか・・・
 * 
 *****************************************************************/
int
iumfs_request_getattr(vnode_t *vp)
{
    iumfscntl_soft_t *cntlsoft; // iumfscntl デバイスのデバイスステータス構造体
    request_t *req;
    response_t *res;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    iumnode_t *inp;
    iumfs_mount_opts_t *mountopts;
    int err = 0;
    iumfs_vattr_t *ivap;

    DEBUG_PRINT((CE_CONT, "iumfs_request_getattr called\n"));

    /*
     * オープン中で未使用の iumfscntl デバイスを受け取る。 
     * もしすべてのデバイスが使用中ならリクエストの順番待ちをする
     */  
    if((err = iumfs_daemon_request_enter(&cntlsoft))){
        goto out;
    }     

    mutex_enter(&cntlsoft->d_lock);
    /*
     * マウントオプション、ファイルシステム依存ノード構造体、ユーザ空間とマッピング
     * しているメモリアドレスを得る。
     */
    inp = VNODE2IUMNODE(vp);
    iumfsp = VNODE2IUMFS(vp);
    mountopts = iumfsp->mountopts;
    req = (request_t *) cntlsoft->bufaddr;
    /*
     * ユーザモードデーモンに渡すリクエストを request 構造体にセット
     */
    bzero(req, sizeof (request_t));
    req->request_type = GETATTR_REQUEST;
    snprintf(req->pathname, IUMFS_MAXPATHLEN, "%s", inp->pathname); //マウントポイントからの相対パス
    bcopy(mountopts, req->mountopts, sizeof (iumfs_mount_opts_t));
    req->datasize = 0;
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;
    mutex_exit(&cntlsoft->d_lock);

    /*
     * リクエスト要求を開始する
     */
    err = iumfs_daemon_request_start(cntlsoft);
    if (err) {
        /*
         * エラーが発生した模様。リクエストを解除してエラーリターン
         */
        iumfs_daemon_request_exit(cntlsoft);
        goto out;
    }

    mutex_enter(&cntlsoft->d_lock);
    res = (response_t *) cntlsoft->bufaddr;
    DEBUG_PRINT((CE_CONT, "iumfs_request_getattr: got a response from daemon.\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_request_getattr: datasize=%d, result=%d\n", res->datasize, res->result));
    if (res->datasize < sizeof (iumfs_vattr_t)) {
        /*
         * データ部のサイズが小さい。リクエストを解除してエラーリターン
         * EAGAIN は適切じゃないかも・・・
         */
        cmn_err(CE_CONT, "iumfs_request_getattr: res->datasize(%lld) != sizeof(iumfs_vattr_t)(%lld)\n",
                (longlong_t) res->datasize, (longlong_t)sizeof (iumfs_vattr_t));
        mutex_exit(&cntlsoft->d_lock);
        iumfs_daemon_request_exit(cntlsoft);
        err = EAGAIN;
        goto out;
    }

    /*
     * デーモンから受け取ったデータをコピー
     * モード、サイズ、タイプ、更新時間のみ。
     */
    ivap = (iumfs_vattr_t *) ((char *) res + sizeof (response_t));
    DEBUG_PRINT((CE_CONT, "iumfs_request_getattr: i_type=%ld, i_mode=%ld, i_size=%ld\n",
            ivap->i_type, ivap->i_mode, ivap->i_size));
    inp->vattr.va_mode = ivap->i_mode;
    inp->vattr.va_size = ivap->i_size;
    inp->vattr.va_type = ivap->i_type;
    inp->vattr.va_mtime.tv_sec = ivap->mtime_sec;
    inp->vattr.va_mtime.tv_nsec = ivap->mtime_nsec;
    mutex_exit(&cntlsoft->d_lock);

    /*
     * リクエストを解除。他の待ち thread を起こす
     */
    iumfs_daemon_request_exit(cntlsoft);
    DEBUG_PRINT((CE_CONT, "iumfs_request_getattr: copy data done\n"));

out:
    DEBUG_PRINT((CE_CONT, "iumfs_request_getattr: return(%d)\n", err));
    return (err);
}

/******************************************************************
 * iumfs_request_write()
 *
 * iumfs_putapage() から呼ばれ、ユーザモードデーモンに指定した
 * ファイルのオフセットからサイズ分のデータを書き込む
 *
 * 引数:
 *        bp  : buf 構造体
 *        vp  : 読み込むファイルの vnode
 *
 * 戻り値
 *
 *    正常時   : 0
 *    エラー時 : エラー番号
 *
 *****************************************************************/
int
iumfs_request_write(struct buf *bp, vnode_t *vp)
{
    iumfscntl_soft_t *cntlsoft; // iumfscntl デバイスのデバイスステータス構造体
    request_t *req;
    offset_t offset;
    size_t size;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    iumnode_t *inp;
    iumfs_mount_opts_t *mountopts;
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_request_write called\n"));

    /*
     * block 数から byte 数へ・・・なんか無意味な操作
     * ちなみに、b_bcount は PAGESIZE より大きい可能性もある
     */
    offset = ldbtob(bp->b_lblkno);
    size = bp->b_bcount;

    DEBUG_PRINT((CE_CONT, "iumfs_request_write: offset = %D, size = %d\n", offset, size));

    /*
     * オープン中で未使用の iumfscntl デバイスを受け取る。 
     * もしすべてのデバイスが使用中ならリクエストの順番待ちをする
     */  
    if((err = iumfs_daemon_request_enter(&cntlsoft))){
        goto out;
    }

    mutex_enter(&cntlsoft->d_lock);
    /*
     * マウントオプション、ファイルシステム依存ノード構造体、ユーザ空間とマッピング
     * しているメモリアドレスを得る。
     */
    inp = VNODE2IUMNODE(vp);
    iumfsp = VNODE2IUMFS(vp);
    mountopts = iumfsp->mountopts;
    req = (request_t *) cntlsoft->bufaddr; // bufaddr の大きさは DEVICE_BUFFER_SIZE
    /*
     * ユーザモードデーモンに渡すリクエストを request 構造体にセット
     */
    bzero(req, sizeof (request_t));
    req->request_type = WRITE_REQUEST;
    strncpy(req->pathname, inp->pathname, IUMFS_MAXPATHLEN); // マウントポイントからの相対パス名
    bcopy(mountopts, req->mountopts, sizeof (iumfs_mount_opts_t));
    req->offset = offset; // オフセット
    req->size = size; // 要求されたサイズ
    req->datasize = (size & ~7) + ((size & 7) ? 8 : 0); // バッファに追記するデータサイズ(8の倍数)
    req->datasize = MIN(req->datasize, DEVICE_BUFFER_SIZE - sizeof (request_t)); // バッファサイズを超えてはいけない
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;
    bcopy(bp->b_un.b_addr, (char *) req + sizeof (request_t), size); 
    mutex_exit(&cntlsoft->d_lock); // checking

    /*
     * リクエスト要求を開始する
     */
    err = iumfs_daemon_request_start(cntlsoft);
    if (err) {
        // エラーが発生した模様。リクエストを解除
        iumfs_daemon_request_exit(cntlsoft);
        goto out;
    }

    /*
     * リクエストを解除。他の待ち thread を起こす
     */
    iumfs_daemon_request_exit(cntlsoft);
    DEBUG_PRINT((CE_CONT, "iumfs_request_write: copy data done\n"));

out:
    DEBUG_PRINT((CE_CONT, "iumfs_request_write: return(%d)\n", err));
    return (err);
}

/******************************************************************
 * iumfs_request_create()
 *
 * ユーザデーモンにに指定したファイルの作成を依頼する。
 *
 * 引数:
 *        dirvp  : ファイルを作成するディレクトリの vnode 構造体
 *        name   : 作成するファイル名
 *        vap    : 作成するファイルの属性値を格納する vattr 構造体
 *
 * 戻り値
 *
 *    正常時   : 0
 *    エラー時 : エラー番号
 *
 *****************************************************************/
int
iumfs_request_create(vnode_t *dirvp, char *name, vattr_t *vap)
{
    int err = 0;
#define SUPPORT_CREATE    
#ifndef SUPPORT_CREATE        
    DEBUG_PRINT((CE_CONT, "iumfs_request_create called\n"));
    err = ENOTSUP;
#else
    iumfscntl_soft_t *cntlsoft; // iumfscntl デバイスのデバイスステータス構造体
    request_t *req;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    iumnode_t *dirinp;
    iumfs_mount_opts_t *mountopts;
    iumfs_vattr_t *ivap;

    DEBUG_PRINT((CE_CONT, "iumfs_request_create called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_request_create: filename=%s\n", name));

    /*
     * オープン中で未使用の iumfscntl デバイスを受け取る。 
     * もしすべてのデバイスが使用中ならリクエストの順番待ちをする
     */  
    if((err = iumfs_daemon_request_enter(&cntlsoft))){
        goto out;
    }

    mutex_enter(&cntlsoft->d_lock);
    /*
     * マウントオプション、ファイルシステム依存ノード構造体、ユーザ空間とマッピング
     * しているメモリアドレスを得る。
     */
    dirinp = VNODE2IUMNODE(dirvp);
    iumfsp = VNODE2IUMFS(dirvp);
    mountopts = iumfsp->mountopts;
    req = (request_t *) cntlsoft->bufaddr; // bufaddr の大きさは DEVICE_BUFFER_SIZE
    
    /*
     * ユーザモードデーモンに渡すリクエストを request 構造体にセット
     */
    bzero(req, sizeof (request_t));
    req->request_type = CREATE_REQUEST;
    if (ISROOT(dirinp->pathname))    
        snprintf(req->pathname, IUMFS_MAXPATHLEN, "/%s",name);
    else
        snprintf(req->pathname, IUMFS_MAXPATHLEN, "%s/%s",dirinp->pathname,name);        
    bcopy(mountopts, req->mountopts, sizeof (iumfs_mount_opts_t));
    req->datasize = sizeof(iumfs_vattr_t); // かならず 8の倍数になっているはず
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;
    ivap = (iumfs_vattr_t*)((char *) req + sizeof (request_t));
    /*
     * create から渡された vattr 構造体を iumfs 固有の構造体に当てはめる。
     */ 
    ivap->i_mode = vap->va_mode;
    ivap->i_size = vap->va_size;
    ivap->i_type =vap->va_type;
    ivap->mtime_sec = vap->va_mtime.tv_sec;
    ivap->mtime_nsec = vap->va_mtime.tv_nsec;
    /*
     * 最後にopen(2)に渡されたフラグをセット
     */ 
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;    
    mutex_exit(&cntlsoft->d_lock); // checking

    /*
     * リクエスト要求を開始する
     */
    err = iumfs_daemon_request_start(cntlsoft);
    if (err) {
        // エラーが発生した模様。リクエストを解除
        iumfs_daemon_request_exit(cntlsoft);
        goto out;
    }

    /*
     * リクエストを解除。他の待ち thread を起こす
     */
    iumfs_daemon_request_exit(cntlsoft);
    DEBUG_PRINT((CE_CONT, "iumfs_request_create: file created\n"));

#endif    
out:
    DEBUG_PRINT((CE_CONT, "iumfs_request_create: return(%d)\n", err));
    return (err);

}

/******************************************************************
 * iumfs_request_remove
 *
 * 指定されたファイルを削除する
 *
 * 引数:
 *        vp    : 削除するファイルの vnode 
 *
 * 戻り値
 *     正常時 : 0
 *     異常時 : エラー番号
 *
 *****************************************************************/
int
iumfs_request_remove(vnode_t *vp)
{
    iumfscntl_soft_t *cntlsoft; // iumfscntl デバイスのデバイスステータス構造体
    request_t *req;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    iumnode_t *inp;
    iumfs_mount_opts_t *mountopts;
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_request_remove called\n"));

    /*
     * オープン中で未使用の iumfscntl デバイスを受け取る。 
     * もしすべてのデバイスが使用中ならリクエストの順番待ちをする
     */  
    if((err = iumfs_daemon_request_enter(&cntlsoft))){
        goto out;
    }     

    mutex_enter(&cntlsoft->d_lock);
    /*
     * マウントオプション、ファイルシステム依存ノード構造体、ユーザ空間とマッピング
     * しているメモリアドレスを得る。
     */
    inp = VNODE2IUMNODE(vp);
    iumfsp = VNODE2IUMFS(vp);
    mountopts = iumfsp->mountopts;
    req = (request_t *) cntlsoft->bufaddr;
    /*
     * ユーザモードデーモンに渡すリクエストを request 構造体にセット
     */
    bzero(req, sizeof (request_t));
    req->request_type = REMOVE_REQUEST;
    snprintf(req->pathname, IUMFS_MAXPATHLEN, "%s", inp->pathname); //マウントポイントからの相対パス
    bcopy(mountopts, req->mountopts, sizeof (iumfs_mount_opts_t));
    req->datasize = 0;
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;
    mutex_exit(&cntlsoft->d_lock);

    /*
     * リクエスト要求を開始する
     */
    err = iumfs_daemon_request_start(cntlsoft);
    if (err) {
         // エラーが発生した模様。リクエストを解除してエラーリターン
        iumfs_daemon_request_exit(cntlsoft);
        goto out;
    }

    /*
     * リクエストを解除。他の待ち thread を起こす
     */
    iumfs_daemon_request_exit(cntlsoft);
    DEBUG_PRINT((CE_CONT, "iumfs_request_remove: file removed\n"));
    
out:
    DEBUG_PRINT((CE_CONT, "iumfs_request_remove: return(%d)\n", err));
    return (err);
}

/******************************************************************
 * iumfs_request_mkdir()
 *
 * ユーザデーモンにに指定したディレクトリの作成を依頼する。
 *
 * 引数:
 *        dirvp  : 新しいディレクトリを作成する親ディレクトリの vnode 構造体
 *        name   : 作成するディレクトリ名
 *        vap    : 作成するディレクトリの属性値を格納する vattr 構造体
 *
 * 戻り値
 *
 *    正常時   : 0
 *    エラー時 : エラー番号
 *
 *****************************************************************/
int
iumfs_request_mkdir(vnode_t *dirvp, char *name, vattr_t *vap)
{    
    iumfscntl_soft_t *cntlsoft; // iumfscntl デバイスのデバイスステータス構造体
    request_t *req;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    iumnode_t *dirinp;
    iumfs_mount_opts_t *mountopts;
    iumfs_vattr_t *ivap;
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_request_mkdir called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_request_mkdir: filename=%s\n", name));

    /*
     * オープン中で未使用の iumfscntl デバイスを受け取る。 
     * もしすべてのデバイスが使用中ならリクエストの順番待ちをする
     */  
    if((err = iumfs_daemon_request_enter(&cntlsoft))){
        goto out;
    }

    mutex_enter(&cntlsoft->d_lock);
    /*
     * マウントオプション、ファイルシステム依存ノード構造体、ユーザ空間とマッピング
     * しているメモリアドレスを得る。
     */
    dirinp = VNODE2IUMNODE(dirvp);
    iumfsp = VNODE2IUMFS(dirvp);
    mountopts = iumfsp->mountopts;
    req = (request_t *) cntlsoft->bufaddr; // bufaddr の大きさは DEVICE_BUFFER_SIZE

    /*
     * ユーザモードデーモンに渡すリクエストを request 構造体にセット
     */
    bzero(req, sizeof (request_t));
    req->request_type = MKDIR_REQUEST;
    if (ISROOT(dirinp->pathname))        
        snprintf(req->pathname, IUMFS_MAXPATHLEN, "/%s",name);
    else
        snprintf(req->pathname, IUMFS_MAXPATHLEN, "%s/%s",dirinp->pathname,name);        
    bcopy(mountopts, req->mountopts, sizeof (iumfs_mount_opts_t));
    req->datasize = sizeof(iumfs_vattr_t); // かならず 8の倍数になっているはず
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;
    ivap = (iumfs_vattr_t*)((char *) req + sizeof (request_t));
    /*
     * iumfs_mkdir で受け取った vattr 構造体を iumfs 固有の構造体に当てはめる。
     */ 
    ivap->i_mode = vap->va_mode;
    ivap->i_size = vap->va_size;
    ivap->i_type =vap->va_type;
    ivap->mtime_sec = vap->va_mtime.tv_sec;
    ivap->mtime_nsec = vap->va_mtime.tv_nsec;
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;    
    mutex_exit(&cntlsoft->d_lock); // checking

    /*
     * リクエスト要求を開始する
     */
    err = iumfs_daemon_request_start(cntlsoft);
    if (err) {
        // エラーが発生した模様。リクエストを解除
        iumfs_daemon_request_exit(cntlsoft);
        goto out;
    }

    /*
     * リクエストを解除。他の待ち thread を起こす
     */
    iumfs_daemon_request_exit(cntlsoft);
    DEBUG_PRINT((CE_CONT, "iumfs_request_mkdir: directory created\n"));
  out:    
    DEBUG_PRINT((CE_CONT, "iumfs_request_mkdir: return(%d)\n", err));
    return (err);
}

/******************************************************************
 * iumfs_request_rmdir
 *
 * 指定されたディレクトリを削除する
 *
 * 引数:
 *        vp    : 削除するディレクトリの vnode 
 *
 * 戻り値
 *     正常時 : 0
 *     異常時 : エラー番号
 *
 *****************************************************************/
int
iumfs_request_rmdir(vnode_t *vp)
{
    iumfscntl_soft_t *cntlsoft; // iumfscntl デバイスのデバイスステータス構造体
    request_t *req;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    iumnode_t *inp;
    iumfs_mount_opts_t *mountopts;
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_request_rmdir called\n"));

    /*
     * オープン中で未使用の iumfscntl デバイスを受け取る。 
     * もしすべてのデバイスが使用中ならリクエストの順番待ちをする
     */  
    if((err = iumfs_daemon_request_enter(&cntlsoft))){
        goto out;
    }

    mutex_enter(&cntlsoft->d_lock);
    /*
     * マウントオプション、ファイルシステム依存ノード構造体、ユーザ空間とマッピング
     * しているメモリアドレスを得る。
     */
    inp = VNODE2IUMNODE(vp);
    iumfsp = VNODE2IUMFS(vp);
    mountopts = iumfsp->mountopts;
    req = (request_t *) cntlsoft->bufaddr;
    /*
     * ユーザモードデーモンに渡すリクエストを request 構造体にセット
     */
    bzero(req, sizeof (request_t));
    req->request_type = RMDIR_REQUEST;
    snprintf(req->pathname, IUMFS_MAXPATHLEN, "%s", inp->pathname); //マウントポイントからの相対パス
    bcopy(mountopts, req->mountopts, sizeof (iumfs_mount_opts_t));
    req->datasize = 0;
    cntlsoft->bufusedsize = sizeof (request_t) + req->datasize;
    mutex_exit(&cntlsoft->d_lock);

    /*
     * リクエスト要求を開始する
     */
    err = iumfs_daemon_request_start(cntlsoft);
    if (err) {
         // エラーが発生した模様。リクエストを解除してエラーリターン
        iumfs_daemon_request_exit(cntlsoft);
        goto out;
    }

    /*
     * リクエストを解除。他の待ち thread を起こす
     */
    iumfs_daemon_request_exit(cntlsoft);
    DEBUG_PRINT((CE_CONT, "iumfs_request_rmdir: directory removed\n"));
    
out:
    DEBUG_PRINT((CE_CONT, "iumfs_request_rmdir return(%d)\n", err));
    return (err);
}
