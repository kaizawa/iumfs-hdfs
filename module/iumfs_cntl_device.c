
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
 * iumfs_cntl_device.c
 *  
 * Device driver to intermidicate between IUMFS filesystem
 * module and user mode daemon.
 *
 * 構想。
 * soft_state 構造体を get_soft_zalloc で確保するのをやめ、自前で
 * あらかじめ確保するようにする。
 * 最大インスタンス数分だけ確保してから使いたい人がロックをとって state を
 * 変更して解放されないようにする。
 *   
 **************************************************************/
#include <sys/modctl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/kmem.h>
#include <sys/ddi.h>
#include <sys/conf.h> 
#include <sys/sunddi.h>
#include <sys/vfs.h>
#include <sys/mount.h>
#include <sys/dirent.h>
#include <sys/ksynch.h>
#include <sys/pathname.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/uio.h>

#include "iumfs.h"

static int iumfscntl_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int iumfscntl_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int iumfscntl_open(dev_t *dev, int flag, int otyp, cred_t *cred);
static int iumfscntl_close(dev_t dev, int flag, int otyp, cred_t *cred);
static int iumfscntl_read(dev_t dev, struct uio *uiop, cred_t *credp);
static int iumfscntl_write(dev_t dev, struct uio *uiop, cred_t *credp);
static int iumfscntl_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp, int *rvalp);
static int iumfscntl_devmap(dev_t dev, devmap_cookie_t handle, offset_t off, size_t len, size_t *maplen, uint_t model);
static int iumfscntl_poll(dev_t dev, short events, int anyyet, short *reventsp, struct pollhead **phpp);

void *iumfscntl_soft_root = NULL; // iumfscntl デバイスのソフトステート構造体
extern kmutex_t iumfs_global_lock; // グローバルロック。

struct ddi_device_acc_attr iumfscntl_acc_attr = {
    DDI_DEVICE_ATTR_V0,
    DDI_NEVERSWAP_ACC,
    DDI_STRICTORDER_ACC
};

/*
 * キャラクター用エントリーポイント構造体
 */
static struct cb_ops iumfscntl_cb_ops = {
    iumfscntl_open, /* cb_open     */
    iumfscntl_close, /* cb_close    */
    nodev, /* cb_strategy */
    nodev, /* cb_print    */
    nodev, /* cb_dump     */
    iumfscntl_read, /* cb_read     */
    iumfscntl_write, /* cb_write    */
    iumfscntl_ioctl, /* cb_ioctl    */
    iumfscntl_devmap, /* cb_devmap   */
    nodev, /* cb_mmap     */
    nodev, /* cb_segmap   */
    iumfscntl_poll, /* cb_chpoll   */
    ddi_prop_op, /* cb_prop_op  */
    NULL, /* cb_stream   */
    D_MP /* cb_flag     */
};

/*
 * デバイスオペレーション構造体
 */
static struct dev_ops iumfscntl_ops = {
    (DEVO_REV), /* devo_rev      */
    (0), /* devo_refcnt   */
    (nodev), /* devo_getinfo  */
    (nulldev), /* devo_identify */
    (nulldev), /* devo_probe    */
    (iumfscntl_attach), /* devo_attach   */
    (iumfscntl_detach), /* devo_detach   */
    (nodev), /* devo_reset    */
    &(iumfscntl_cb_ops), /* devo_cb_ops   */
    (struct bus_ops *) (NULL) /* devo_bus_ops  */
};

/*
 * ドライバーのリンケージ構造体
 */
struct modldrv iumfs_modldrv = {
    &mod_driverops, //  mod_driverops
    PACKAGE_NAME " driver ver " PACKAGE_VERSION, // ドライバの説明
    &iumfscntl_ops // driver ops
};

//static int iumfscntl_cnt = NMINDEV; // NMINDEV はマイナーデバイス数(=可能なオープン数)
iumfscntl_soft_t cntlsoft_list[MAX_INST]; // iumfscntl_soft_t 構造体の配列
static dev_info_t *iumfscntl_dev_info;	/* private devinfo pointer */

/*****************************************************************************
 * iumfscntl_attach
 *
 * iumfscntl の attach(9E) ルーチン
 *
 *****************************************************************************/
static int
iumfscntl_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
    iumfscntl_soft_t *cntlsoft = NULL;    
    int instance;
    
    DEBUG_PRINT((CE_CONT, "iumfscntl_attach called\n"));
    mutex_enter(&iumfs_global_lock);            

    if (cmd != DDI_ATTACH) {
        cmn_err(CE_WARN, "iumfscntl_attach: cmd is not DDI_ATTACH\n");
        DEBUG_PRINT((CE_CONT, "iumfscntl_attach: return(DDI_FAILURE)\n"));
        goto err;
    }

    // devicese/pseudo 以下にデバイスファイルを作成する
    if (ddi_create_minor_node(dip, "iumfscntl", S_IFCHR, 0, DDI_PSEUDO, 0) == DDI_FAILURE) {
        ddi_remove_minor_node(dip, NULL);
        cmn_err(CE_CONT, "iumfscntl_attach: failed to create minor node\n");
        goto err;
    }

    /*
     * インスタンス毎の iumfscntl デバイスのステータス構造体を設定
     */ 
    for(instance = 0 ; instance < MAX_INST ; instance++ ){
        cntlsoft = &cntlsoft_list[instance];
        caddr_t bufaddr = kmem_zalloc(DEVICE_BUFFER_SIZE, KM_NOSLEEP);
        if (bufaddr == NULL) {
            cmn_err(CE_CONT, "iumfscntl_open: kmem_zalloc failed\n");
            goto err;
        }
        cntlsoft->state = 0; // フラグ初期化
        cntlsoft->instance = instance; // インスタンス番号
        cntlsoft->bufaddr = bufaddr; // read/write 時の uiomove に使う一時バッファ。
        cntlsoft->dip = dip; // dev_info 構造体. TODO: 一つしか無いのでコピーする必要は無い。
        mutex_init(&cntlsoft->d_lock, NULL, MUTEX_DRIVER, NULL);
        mutex_init(&cntlsoft->s_lock, NULL, MUTEX_DRIVER, NULL);
        cv_init(&cntlsoft->cv, NULL, CV_DRIVER, NULL);
    }
    iumfscntl_dev_info = dip;
    mutex_exit(&iumfs_global_lock);
    DEBUG_PRINT((CE_CONT, "iumfscntl_attach: return(DDI_SUCCESS)\n"));
    return (DDI_SUCCESS);

err:
    mutex_exit(&iumfs_global_lock);                    
    DEBUG_PRINT((CE_CONT, "iumfscntl_attach: return(DDI_FAILURE)\n"));
    return (DDI_FAILURE);
}

/*****************************************************************************
 * iumfscntl_dettach
 *
 * iumfscntl の dettach(9E) ルーチン
 *
 *****************************************************************************/
static int
iumfscntl_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
    iumfscntl_soft_t *cntlsoft = NULL;
    int instance;
    
    DEBUG_PRINT((CE_CONT, "iumfscntl_dettach called\n"));
    mutex_enter(&iumfs_global_lock);    

    if (cmd != DDI_DETACH) {
        cmn_err(CE_WARN, "iumfscntl_dettach: cmd is not DDI_DETACH\n");
        DEBUG_PRINT((CE_CONT, "iumfscntl_dettach: return(DDI_FAILURE)\n"));
        return (DDI_FAILURE);
    }

    /*
     * インスタンス毎の iumfscntl デバイスのステータス構造体を設定
     */ 
    for(instance = 0 ; instance < MAX_INST ; instance ++){
        cntlsoft = &cntlsoft_list[instance];
        mutex_enter(&cntlsoft->s_lock);
        /*
         * state の DAEMON_INPROGRESS フラグが解除されるのを待っている thread が
         * いるかもしれないので、フラグを解除。
         */
        if(cntlsoft->state & IUMFSCNTL_OPENED) {
            cmn_err(CE_WARN, "iumfscntl_detach: instance %d is opened.\n", instance);
        }

        if (cntlsoft->state & DAEMON_INPROGRESS) {
            cmn_err(CE_WARN, "iumfscntl_detach: instance %d is inprogress.\n", instance);
            cntlsoft->state &= ~DAEMON_INPROGRESS; // DAEMON_INPROGRESS フラグを解除
            cntlsoft->state |= BUFFER_INVALID; // マップされたアドレスのデータが不正であることを知らせる
            cntlsoft->error = EIO; // エラーをセット
            cv_broadcast(&cntlsoft->cv); // thread を起こす
        }
        cntlsoft->state &= ~IUMFSCNTL_OPENED;
        mutex_exit(&cntlsoft->s_lock);

        /*
         *  もしここで上記のワーニングの状況が発生するならここで開放作業をするのは良くないかもしれない。
         */
        mutex_destroy(&cntlsoft->d_lock);
        mutex_destroy(&cntlsoft->s_lock);
        kmem_free(cntlsoft->bufaddr, DEVICE_BUFFER_SIZE);
    }    

    ddi_remove_minor_node(dip, NULL);

    mutex_exit(&iumfs_global_lock);    
    DEBUG_PRINT((CE_CONT, "iumfscntl_dettach: return(DDI_SUCCESS)\n"));
    return (DDI_SUCCESS);
}

/*****************************************************************************
 * iumfscntl_open
 *
 * iumfscntl の open(9E) ルーチン
 *
 *****************************************************************************/
static int
iumfscntl_open(dev_t *dev, int flag, int otyp, cred_t *cred)
{
    int instance = 0;
    iumfscntl_soft_t *cntlsoft = NULL;
    int err = 0;
    minor_t newminor;

    DEBUG_PRINT((CE_CONT, "iumfscntl_open called\n"));

    if (otyp != OTYP_CHR) {
        err = EINVAL;
        goto out;
    }

    for (instance = 0 ; instance < MAX_INST ; instance++ ) {
        cntlsoft = &cntlsoft_list[instance];
        // ロック待ちするのは使われていると思われるので次へ
        if (mutex_tryenter(&cntlsoft->s_lock) == 0)
            continue;
        // すでにオープンされている場合も次へ
        if (cntlsoft->state & IUMFSCNTL_OPENED) {
            mutex_exit(&cntlsoft->s_lock);
            continue;
        }
        newminor = INST2MINOR(instance);
        *dev = makedevice(getmajor(*dev), newminor);
        cntlsoft->state |= IUMFSCNTL_OPENED; // 準備完了。フラグをOPENにセット。
        cv_broadcast(&cntlsoft->cv); // 待ってる thread がいるかも知れないので通知        
         mutex_exit(&cntlsoft->s_lock);
        break;
    }
    
    if(instance >= MAX_INST){
        cmn_err(CE_WARN, "iumfscntl_open: all instances were in use.\n");        
        err = ENXIO;        
        goto out;
    }
    DEBUG_PRINT((CE_CONT, "iumfscntl_open: new instance = %d\n",instance));

  out:
    DEBUG_PRINT((CE_CONT, "iumfscntl_open: return(%d)\n", err));
    return (err);
}

/*****************************************************************************
 * iumfscntl_close
 *
 * iumfscntl の close(9E) ルーチン
 *
 *****************************************************************************/
static int
iumfscntl_close(dev_t dev, int flag, int otyp, cred_t *cred)
{
    int instance;
    iumfscntl_soft_t *cntlsoft;
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfscntl_close called\n"));

    if (otyp != OTYP_CHR) {
        err = EINVAL;
        goto out;
    }
    instance = MINOR2INST(getminor(dev)); // get instance number from minor
    cntlsoft = &cntlsoft_list[instance];
    mutex_enter(&cntlsoft->s_lock);
    /*
     * state の DAEMON_INPROGRESS フラグが解除されるのを待っている thread が
     * いるかもしれないので、フラグを解除。
     */
    if (cntlsoft->state & DAEMON_INPROGRESS) {
        cntlsoft->state &= ~DAEMON_INPROGRESS; // DAEMON_INPROGRESS フラグを解除
        cntlsoft->state |= BUFFER_INVALID; // マップされたアドレスのデータが不正であることを知らせる
        cntlsoft->error = EIO; // エラーをセット
        cv_broadcast(&cntlsoft->cv); // thread を起こす
    }
    cntlsoft->state &= ~IUMFSCNTL_OPENED; // オープン中フラグを外す。
    mutex_exit(&cntlsoft->s_lock);
    
  out:
    DEBUG_PRINT((CE_CONT, "iumfscntl_close: return(%d)\n",err));
    return (err);
}

/*****************************************************************************
 * iumfscntl_read
 *
 * iumfscntl の read(9E) ルーチン
 *
 *****************************************************************************/
static int
iumfscntl_read(dev_t dev, struct uio *uiop, cred_t *credp)
{
    int instance;
    iumfscntl_soft_t *cntlsoft;
    int err = 0;
    size_t size;

    DEBUG_PRINT((CE_CONT, "iumfscntl_read called\n"));

    instance = MINOR2INST(getminor(dev)); // get instance number from minor
    cntlsoft = &cntlsoft_list[instance];
    if (cntlsoft == NULL) {
        err = ENXIO;
        goto out;
    }

    size = uiop->uio_resid;
    DEBUG_PRINT((CE_CONT, "iumfscntl_read: uiop->uio_resid=%d\n", size));

    /*
     * request 構造体 より小さな read 要求は無効
     * daemon の実装ミス(read(2)のサイズ指定誤り）と思われる。
     */
    if (size < sizeof (request_t)) {
        cmn_err(CE_WARN, "iumfscntl_read: read request size from daemon is too small\n");
        err = EINVAL;
        goto out;
    }

    DEBUG_PRINT((CE_CONT, "iumfscntl_read: waiting for request from filesystem module. state=0x%x\n", cntlsoft->state));

    mutex_enter(&cntlsoft->s_lock);
    while (!(cntlsoft->state & REQUEST_IS_SET)) {
        if (cv_wait_sig(&cntlsoft->cv, &cntlsoft->s_lock) == 0) {
            mutex_exit(&cntlsoft->s_lock);
            err = EINTR;
            goto out;
        }
    }
    DEBUG_PRINT((CE_CONT, "iumfscntl_read: awake. new request from filesystem has come.\n"));

    /*
     * デーモンの read(2) の要求サイズを超得ない範囲で request 構造体 + データ(あれば）
     * を含んだサイズ分だけユーザ空間にコピーする
     */
    err = uiomove(cntlsoft->bufaddr, MIN(size, cntlsoft->bufusedsize), UIO_READ, uiop);
    if (err) {
        cmn_err(CE_CONT, "iumfscntl_read: uiomove failed\n");
    } else {
        DEBUG_PRINT((CE_CONT, "iumfscntl_read: copyout %d bytes to daemon\n", MIN(size, cntlsoft->bufusedsize)));
    }

    cntlsoft->state &= ~REQUEST_IS_SET;
    mutex_exit(&cntlsoft->s_lock);
out:
    DEBUG_PRINT((CE_CONT, "iumfscntl_read: return(%d)\n", err));
    return (err);
}

/*****************************************************************************
 * iumfscntl_write
 *
 * iumfscntl の write(9E) ルーチン
 *
 * 呼ばれたら、ユーザプロセスからのデータの確認やコピーなどはせず、単に
 * iumfscntl_soft 構造体のステートフラグに DAEMON_INPROGRESS を解除し、iumfs_bio()
 * の中でまっているであろう thread を起こす。
 *
 * 戻り値
 *
 *    常に 0 成功
 *
 *****************************************************************************/
static int
iumfscntl_write(dev_t dev, struct uio *uiop, cred_t *credp)
{
    int instance;
    iumfscntl_soft_t *cntlsoft;
    int err = 0;
    response_t *res;
    size_t size;

    DEBUG_PRINT((CE_CONT, "iumfscntl_write called\n"));

    instance = MINOR2INST(getminor(dev)); // get instance number from minor    
    cntlsoft = &cntlsoft_list[instance];    
    if (cntlsoft == NULL) {
        cmn_err(CE_CONT, "iumfscntl_write: can't get soft state structure.\n");
        err = ENXIO;
        goto out;
    }

    size = uiop->uio_resid;
    // バッファサイズ以上の書き込みリクエストがきたらエラーを返す。
    if(size > DEVICE_BUFFER_SIZE) {
        DEBUG_PRINT((CE_WARN, "iumfscntl_write: large write request. (%d bytes). Could be bug on daemon program.\n", size));
        err = ENXIO;
        goto out;
    }

    DEBUG_PRINT((CE_CONT, "iumfscntl_write: uiop->uio_resid=%d\n", size));
    DEBUG_PRINT((CE_CONT, "iumfscntl_write: get response from daemon. wake iumfs_daemon_request_start\n"));
    DEBUG_PRINT((CE_CONT, "iumfscntl_write: state=0x%x size=%d\n", cntlsoft->state, size));

    err = uiomove(cntlsoft->bufaddr, size, UIO_WRITE, uiop);

    mutex_enter(&cntlsoft->s_lock);
    if (err) {
        cmn_err(CE_CONT, "iumfscntl_write: uiomove failed\n");
        cntlsoft->state |= BUFFER_INVALID;
        cntlsoft->error = err;
    } else {
        res = (response_t *) cntlsoft->bufaddr;
        cntlsoft->error = res->result;
        cntlsoft->bufusedsize = size;
        DEBUG_PRINT((CE_CONT, "iumfscntl_write: copyin %d bytes from daemon. (error=%d)\n", size, res->result));
    }

    cntlsoft->state &= ~DAEMON_INPROGRESS; // DAEMON_INPROGRESS フラグを解除

    DEBUG_PRINT((CE_CONT, "iumfscntl_write: state = 0x%x\n", cntlsoft->state));

    cv_broadcast(&cntlsoft->cv); // thread を起こす
    mutex_exit(&cntlsoft->s_lock);
out:
    DEBUG_PRINT((CE_CONT, "iumfscntl_write: return(%d)\n",err));
    return (err);
}

/*****************************************************************************
 * iumfscntl_ioctl
 *
 * iumfscntl の ioctl(9E) ルーチン
 *
 *****************************************************************************/
static int
iumfscntl_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *credp, int *rvalp)
{
    DEBUG_PRINT((CE_CONT, "iumfscntl_ioctl called\n"));
    DEBUG_PRINT((CE_CONT, "iumfscntl_ioctl: return(EINVAL)\n"));
    return (EINVAL);
}

/*****************************************************************************
 * iumfscntl_devmap
 *
 * iumfscntl の devmap(9E) ルーチン
 *
 *****************************************************************************/
static int
iumfscntl_devmap(dev_t dev, devmap_cookie_t handle, offset_t off, size_t len, size_t *maplen, uint_t model)
{
    int instance;
    iumfscntl_soft_t *cntlsoft;
    size_t length;
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfscntl_devmap called\n"));

    instance = MINOR2INST(getminor(dev)); // get instance number from minor        
    cntlsoft = &cntlsoft_list[instance];    
    if (cntlsoft == NULL) {
        cmn_err(CE_WARN, "iumfscntl_devmap: can't get soft state structure.\n");
        err = ENXIO;
        goto out;
    }

    length = ptob(btopr(len));
    if (off + length > cntlsoft->size) {
        cmn_err(CE_WARN, "iumfscntl_devmap: request size too large.\n");
        err = EINVAL;
        goto out;
    }

    err = devmap_umem_setup(handle, cntlsoft->dip, NULL, cntlsoft->umem_cookie,
            off, length, PROT_ALL, 0, &iumfscntl_acc_attr);

    if (err) {
        cmn_err(CE_CONT, "iumfscntl_devmap: devmap_umem_setup failed (%d)\n", err);
        goto out;
    }

    *maplen = length;
out:
    DEBUG_PRINT((CE_CONT, "iumfscntl_devmap: return(%d)\n", err));
    return (err);
}

/*****************************************************************************
 * iumfscntl_poll
 *
 * iumfscntl の chpoll(9E) ルーチン
 *
 *****************************************************************************/
static int
iumfscntl_poll(dev_t dev, short events, int anyyet, short *reventsp, struct pollhead **phpp)
{
    int instance;
    iumfscntl_soft_t *cntlsoft;
    short revent;
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfscntl_poll called\n"));

    instance = MINOR2INST(getminor(dev)); // get instance number from minor            
    cntlsoft = &cntlsoft_list[instance];    
    if (cntlsoft == NULL) {
        cmn_err(CE_WARN, "iumfscntl_poll: can't get soft state structure.\n");
        err = ENXIO;
        goto out;
    }

    revent = 0;
    /*
     * 有効なイベント
     * POLLIN | POLLOUT | POLLPRI | POLLHUP | POLLERR
     * 現在は POLLIN | POLLRDNORM と POLLERR|POLLRDBAND しかサポートしていない。
     */
    if ((events & (POLLIN | POLLRDNORM)) && (cntlsoft->state & REQUEST_IS_SET)) {
        DEBUG_PRINT((CE_CONT, "iumfscntl_poll: request can be read\n"));
        revent |= POLLIN | POLLRDNORM;
    }
    if ((events & (POLLERR | POLLRDBAND)) && (cntlsoft->state & REQUEST_IS_CANCELED)) {
        DEBUG_PRINT((CE_CONT, "iumfscntl_poll: request is canceled\n"));
        revent |= (POLLERR | POLLRDBAND);
        /*
         * 現在は iumfs_daemon_request_exit() でフラグを解除している。
         * cntlsoft->state &= ~REQUEST_IS_CANCELED;
         */
    }
    /*
     * 通知すべきイベントは発生していない
     */
    if (revent == 0) {
        DEBUG_PRINT((CE_CONT, "iumfscntl_poll: unsupported event happened.(events = 0x%x)\n", events));
        if (!anyyet) {
            *phpp = &cntlsoft->pollhead;
        }
    }

    *reventsp = revent;
out:
    DEBUG_PRINT((CE_CONT, "iumfscntl_poll: return(%d)\n", err));
    return (err);
}

