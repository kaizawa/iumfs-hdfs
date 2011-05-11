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
 * iumfs_vfsops
 *
 * VFS Operations for IUMFS pseudo filesystem.
 * 
 **************************************************************/
/*
 * 各ソースに加えて全体としてやるべきこと一覧
 *  TODO: /dev/iumfscntl をクローンデバイス化。
 *        これにより複数のユーザスレッドが同時に iumfscntl
 *        デバイスをオープンして平行して動作することを可能にする。
 *  TODO: iumfscntl が detach された後（もしくはattachされる前）
 *        に iumfs モジュールのコードが呼ばれる可能性がある。
 *        iumfscntl softstate がないと いろんなとこで panic する！
 *  TODO: lock 取得ポリシーのチェック
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
#include <sys/systm.h>
#include <sys/varargs.h>
#include <vm/pvn.h>
#include <stddef.h>
// OpenSolaris の場合必要
#ifdef OPENSOLARIS
#include <sys/vfs_opreg.h>
#endif
#include "iumfs.h"

static mntopts_t iumfs_optproto;
kmutex_t iumfs_global_lock; // グローバルロック。
static int iumfs_fstype; // ファイルシステムタイプ
static major_t iumfs_major; // メジャーデバイス番号。モジュールロード後は固定
static major_t iumfs_last_minor = 0; // 最後にマウントしたファイルシステムのマイナーデバイス番号

/*
 * VFSOPS プロトタイプ宣言
 */
#ifdef SOL10
static int iumfs_init(int, char *);
#else 
static int iumfs_init(struct vfssw *, int);
static int iumfs_reserved(vfs_t *, vnode_t **, char *);
#endif
static int iumfs_mount(vfs_t *, vnode_t *, struct mounta *, struct cred *);
static int iumfs_unmount(vfs_t *, int, struct cred *);
static int iumfs_root(vfs_t *, vnode_t **);
static int iumfs_statvfs(vfs_t *, struct statvfs64 *);
static int iumfs_sync(vfs_t *, short, struct cred *);
static int iumfs_vget(vfs_t *, vnode_t **, struct fid *);
static int iumfs_mountroot(vfs_t *, enum whymountroot);
static void iumfs_freevfs(vfs_t *);

/*
 * このファイルシステムでサーポートする VFS オペレーション
 */
#ifdef SOL10
/*
 * Solaris 10 の場合、vfsops 構造体は vn_make_ops() にて得る。
 * OpenSolaris の場合さらに fs_operation_def_t の func メンバが union に代わっている
 */
static struct vfsops *iumfs_vfsops;
#ifdef OPENSOLARIS
fs_operation_def_t iumfs_vfs_ops_def_array[] = {
    { VFSNAME_MOUNT,
        {&iumfs_mount}}, // mount(2) システムコールに対応
    { VFSNAME_UNMOUNT,
        {&iumfs_unmount}}, // umount(2)システムコールに対応
    { VFSNAME_ROOT,
        {&iumfs_root}}, // ファイルシステムの root direcotry を返す
    { VFSNAME_SYNC,
        {(fs_generic_func_p) & iumfs_sync}}, // 変更されたページをディスクに書き込む
    { VFSNAME_STATVFS,
        {&iumfs_statvfs}}, //ファイルシステムの統計情報を返す
    { VFSNAME_VGET,
        {&iumfs_vget}}, // 指定されたファイル ID から vnode を返す
    { VFSNAME_MOUNTROOT,
        {&iumfs_mountroot}}, // システムのルートにファイルシステムをマウントする。
    { VFSNAME_FREEVFS,
        {(fs_generic_func_p) & iumfs_freevfs}}, // -- 用途不明 --
    { NULL,
        {NULL}}
};
#else
fs_operation_def_t iumfs_vfs_ops_def_array[] = {
    { VFSNAME_MOUNT, &iumfs_mount},     // mount(2) システムコールに対応
    { VFSNAME_UNMOUNT, &iumfs_unmount}, // umount(2)システムコールに対応
    { VFSNAME_ROOT, &iumfs_root},       // ファイルシステムの root direcotry を返す
    { VFSNAME_SYNC, (fs_generic_func_p) & iumfs_sync}, // 変更されたページをディスクに書き込む
    { VFSNAME_STATVFS, &iumfs_statvfs},     //ファイルシステムの統計情報を返す
    { VFSNAME_VGET, &iumfs_vget},           // 指定されたファイル ID から vnode を返す
    { VFSNAME_MOUNTROOT, &iumfs_mountroot}, // システムのルートにファイルシステムをマウントする。
    { VFSNAME_FREEVFS, (fs_generic_func_p) & iumfs_freevfs}, // -- 用途不明 --
    { NULL, NULL}
};
#endif //ifdef OPENSOLARIS
#else
/*
 * Solaris 9 の場合、vfsops 構造体はファイルシステムが確保し、直接参照できる。
 */
static struct vfsops iumfs_vfsops = {
    &iumfs_mount, // vfs_mount     mount(2) システムコールに対応
    &iumfs_unmount, // vfs_unmount   umount(2)システムコールに対応
    &iumfs_root, // vfs_root      ファイルシステムの root direcotry を返す
    &iumfs_statvfs, // vfs_statvfs   ファイルシステムの統計情報を返す
    &iumfs_sync, // vfs_sync      変更されたページをディスクに書き込む
    &iumfs_vget, // vfs_vget      指定されたファイル ID から vnode を返す
    &iumfs_mountroot, // vfs_mountroot システムのルートにファイルシステムをマウントする。
    &iumfs_reserved, // vfs_reserved  -- 用途不明 --
    &iumfs_freevfs // vfs_freevs.   -- 用途不明 --
};
#endif

#ifdef SOL10
// Solaris 10 の場合、vfssw 構造体は直接作成しない。
static vfsdef_t iumfs_vfsdef = {
    VFSDEF_VERSION, // int   バージョン
    "iumfs", // char  ファイルシステムタイプを示す名前
    &iumfs_init, // int   初期化ルーチン
    0, // int   フラグ
    &iumfs_optproto // mntopts_tmount マウントオプションテーブルのプロトタイプ
};
#else
/*
 * ファイルシステムタイプスイッチテーブル
 *  /usr/include/sys/vfs.h
 */
static struct vfssw iumfs_vfssw = {
    "iumfs", // vsw_name   ファイルシステムタイプを示す名前
    &iumfs_init, // vsw_init   初期化ルーチン
    &iumfs_vfsops, // vsw_vfsops このファイルシステムの操作ベクタ
    0, // vsw_flag   フラグ
    &iumfs_optproto // vsw_optproto マウントオプションテーブルのプロトタイプ
};
#endif

/* 
 * ファイルシステムの linkage 構造体
 * ファイルシステム固有の情報を知らせるために使う。
 * vfssw(Sol9) もしくは vfsdev(Sol10) へのポインターが入っている
 */
static struct modlfs iumfs_modlfs = {
    &mod_fsops, // fsmodops
    PACKAGE_NAME " filesystem ver " PACKAGE_VERSION, // fs_linkinfo
#ifdef SOL10
    &iumfs_vfsdef // fs_vfsdef
#else    
    & iumfs_vfssw // fs_vfssw
#endif    
};

/* 
 * kernel module を kernel に load する _init() 内の
 * mod_install によって使われる。
 * modlfs, modldrv へのポインターが入っている
 */
static struct modlinkage modlinkage = {
    MODREV_1, // モジュールシステムのリビジョン。固定値
    {
        (void *) & iumfs_modlfs, // ファイルシステムのリンケージ構造体
        (void *) & iumfs_modldrv, // デバイスドライバのリンケージ構造体
        NULL // NULL terminate
    }
};

/*************************************************************************
 * _init(9e), _info(9e), _fini(9e)
 * 
 * ローダブルカーネルモジュールのエントリーポイント
 *************************************************************************/
int
_init()
{
    int err;
    
    cmn_err(CE_CONT, "%s Filesystem Ver %s \n", PACKAGE_NAME, PACKAGE_VERSION);        

    /*
     * デバイス管理構造体の管理用の iumfscntl_soft_root を初期化
     * iumfscntl のデバイス管理構造体は iumfscntl_soft_t として定義されている。
     */
    if (ddi_soft_state_init(&iumfscntl_soft_root, sizeof (iumfscntl_soft_t), 1) != 0) {
        return (DDI_FAILURE);
    }

    err = mod_install(&modlinkage);
    if (err != 0) {
        ddi_soft_state_fini(&iumfscntl_soft_root);
        cmn_err(CE_CONT, "_init: mod_install returned with error %d", err);
    }

    /*
     * グローバルロックを初期化
     */
    mutex_init(&iumfs_global_lock, NULL, MUTEX_DEFAULT, NULL);
    
    return (err);
}

int
_info(struct modinfo *modinfop)
{
    return mod_info(&modlinkage, modinfop);
}

int
_fini()
{
    int err;

    err = mod_remove(&modlinkage);
    if (err != 0) {
        cmn_err(CE_CONT, "_fini: mod_remove returned with error %d", err);
        return (err);
    }
    ddi_soft_state_fini(&iumfscntl_soft_root);

    mutex_destroy(&iumfs_global_lock);

    return (err);
}

/*****************************************************************************
 * bebug_print()
 *
 * デバッグ出力用関数
 *
 *  引数：
 *           level  :  エラーの深刻度。cmn_err(9F) の第一引数に相当
 *           format :  メッセージの出力フォーマットcmn_err(9F) の第二引数に相当
 * 戻り値：
 *           なし。
 *****************************************************************************/
void
debug_print(int level, char *format, ...)
{
    va_list ap;
    char buf[MAX_MSG];

    va_start(ap, format);
    vsprintf(buf, format, ap);
    va_end(ap);
    cmn_err(level, "%s", buf);
}

/************************************************************************
 * iumfs_init() 
 *
 * 初期化ルーチン
 *************************************************************************/
static int
#ifdef SOL10
iumfs_init(int fstype, char *fsname)
#else
iumfs_init(struct vfssw *iumfs_vfssw, int fstype)
#endif
{
    int err;

    DEBUG_PRINT((CE_CONT, "iumfs_init called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_init: fstype = %d(0x%x)\n", fstype, fstype));

    iumfs_fstype = fstype;

#ifdef SOL10
    do {
        err = vfs_setfsops(fstype, iumfs_vfs_ops_def_array, &iumfs_vfsops);
        if (err)
            break;
        err = vn_make_ops(fsname, iumfs_vnode_ops_def_array, &iumfs_vnodeops);
        if (err)
            break;
    } while (FALSE);

    if (err) {
        if (iumfs_vfsops != NULL) {
            vfs_freevfsops_by_type(fstype);
        }
        if (iumfs_vnodeops != NULL) {
            vn_freevnodeops(iumfs_vnodeops);
        }
    }
#endif
    /* 
     * filesystem id に使うデバイス番号を決める。
     */
    iumfs_major = getudev();
    DEBUG_PRINT((CE_CONT, "iumfs_init: major = %d(0x%x)\n", iumfs_major, iumfs_major));
    DEBUG_PRINT((CE_CONT, "iumfs_init: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_mount()  VFS オペレーション
 *
 * マウントルーチン
 * 
 *  vfsp   : kernel が確保した、これからマウントする新しいファイルシステム
 *           の為の vfs 構造体のポインタ
 *  mvnode : ディレクトリマウントポイントの vnode
 *  mntarg : mount の引数（注: ユーザ空間のデータ）
 *  cr     : ユーザクレデンシャル(UID, GID 等）
 *
 *  戻り値
 *    正常時   : 0
 *    エラー時 : 0 以外
 * 
 *************************************************************************/
static int
iumfs_mount(vfs_t *vfsp, vnode_t *mvnode, struct mounta *mntarg,
        struct cred *cr)
{
    iumfs_t *iumfsp = NULL; // ファイルシステム型依存のプライベートデータ構造体
    vnode_t *rootvp = NULL;
    int err = 0;
    dev_t dev;

    DEBUG_PRINT((CE_CONT, "iumfs_mount called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_mount: vfs_count = %d\n", vfsp->vfs_count));

    /*
     * このファイルシステム用のデバイス番号をもとめ、そのデバイス番号
     * からファイルシステム ID をもとめる。
     */
    mutex_enter(&iumfs_global_lock);
    dev = makedevice(iumfs_major, ++iumfs_last_minor);
    DEBUG_PRINT((CE_CONT, "iumfs_mount: new minor : %d\n", iumfs_last_minor));
    mutex_exit(&iumfs_global_lock);
    vfs_make_fsid(&vfsp->vfs_fsid, dev, iumfs_fstype);

    DEBUG_PRINT((CE_CONT, "iumfs_mount: new fsid  : 0x%x 0x%x \n",
            vfsp->vfs_fsid.val[0], vfsp->vfs_fsid.val[1]));

    // 途中で break するためだけの do-while 文
    do {
        /*
         * ファイルシステムのプライベートデータ構造体を確保
         */
        iumfsp = (iumfs_t *) kmem_zalloc(sizeof (iumfs_t), KM_NOSLEEP);
        if (iumfsp == NULL) {
            cmn_err(CE_CONT, "iumfs_mount: kmem_zalloc failed");
            err = ENOMEM;
            break;
        }
        /*
         * ロックを初期化
         */
        mutex_init(&(iumfsp->iumfs_lock), NULL, MUTEX_DEFAULT, NULL);
        mutex_init(&(iumfsp->node_list_head.i_dlock), NULL, MUTEX_DEFAULT, NULL);

        /*
         * vfs 構造体にファイルシステムのプライベートデータ構造体をセット
         */
        vfsp->vfs_data = (char *) iumfsp;

        /*
         * ファイルシステムのルートディレクトリを作成
         */
        if ((err = iumfs_create_fs_root(vfsp, &rootvp, mvnode, cr)) != SUCCESS)
            break;

        /*
         * マウントコマンドから渡されたオプション（サーバ名や、パスワードなど）を格納
         */
        if (mntarg->flags & MS_SYSSPACE) {
            DEBUG_PRINT((CE_CONT, "iumfs_mount: MS_SYSSPACE flag is set\n"));
            // FKIOCTL を指定すると bcopy と同様に振舞う
            ddi_copyin(mntarg->dataptr, iumfsp->mountopts, mntarg->datalen, FKIOCTL);
        } else {
            DEBUG_PRINT((CE_CONT, "iumfs_mount: MS_SYSSPACE flag is not set\n"));
            ddi_copyin(mntarg->dataptr, iumfsp->mountopts, mntarg->datalen, 0);
        }
        DEBUG_PRINT((CE_CONT, "iumfs_mount:  user=%s, pass=%s, server=%s, basepath=%s\n",
                iumfsp->mountopts->user,
                iumfsp->mountopts->pass,
                iumfsp->mountopts->server,
                iumfsp->mountopts->basepath));
        /*
         * 上でもとめたデバイス番号をセット
         */
        vfsp->vfs_dev = iumfsp->dev = dev;

        vfsp->vfs_fstype = iumfs_fstype;
        vfsp->vfs_bsize = 0;

    } while (FALSE);

    /*
     * エラーが発生した場合には確保したリソースを解放し、エラーを返す
     */
    if (err) {
        if (iumfsp != NULL) {
            if (rootvp != NULL) {
                iumfs_free_all_node(vfsp, cr);
            }
            // ロックを削除し、確保したメモリを開放
            mutex_destroy(&(iumfsp->iumfs_lock));
            mutex_destroy(&(iumfsp->node_list_head.i_dlock));
            kmem_free(iumfsp, sizeof (iumfs_t));
            vfsp->vfs_data = (char *) NULL;
        }
    }
    DEBUG_PRINT((CE_CONT, "iumfs_mount: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_unmount()  VFS オペレーション
 *
 * アンマウントルーチン
 *************************************************************************/
static int
iumfs_unmount(vfs_t *vfsp, int val, struct cred *cr)
{
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    vnode_t *vp;
    iumnode_t *inp, *previnp;

    DEBUG_PRINT((CE_CONT, "iumfs_unmount is called\n"));

    iumfsp = VFS2IUMFS(vfsp);

    /*
     * iumnode のリンクリストを廻り、利用されている vnode (v_count != 1) 
     * があれば EBUSY を返す。この時点ではまだフリーしない。
     * （一部をフリーしてしまった後で、利用中の vnode があることが分かって
     * しまうのを避けるための動作）
     */
    previnp = &iumfsp->node_list_head;
    mutex_enter(&(previnp->i_dlock));
    while (previnp->next) {
        inp = previnp->next;
        mutex_enter(&(inp->i_dlock));
        vp = IUMNODE2VNODE(inp);
        if (vp->v_count != 1) {
            /*
             * まだ利用されている vnode がある、ロックを開放し、EBUSY を返す。
             */
            DEBUG_PRINT((CE_CONT, "iumfs_unmount: %s vp->v_count = %d\n", inp->pathname, vp->v_count));
            mutex_exit(&(inp->i_dlock));
            mutex_exit(&(previnp->i_dlock));
            DEBUG_PRINT((CE_CONT, "iumfs_unmount: return(EBUSY)\n"));
            return (EBUSY);
        }
        mutex_exit(&(previnp->i_dlock));
        previnp = inp;
    }
    mutex_exit(&(previnp->i_dlock));

    /*
     * 全ての vnode が利用されていないのが分かった。
     * このまま全ての vnode を開放する。
     */
    iumfs_free_all_node(vfsp, cr);

    // ファイルシステム型依存のファイルシステムデータ（iumfs 構造体）を解放
    mutex_destroy(&(iumfsp->iumfs_lock));
    kmem_free(iumfsp, sizeof (iumfs_t));
    DEBUG_PRINT((CE_CONT, "iumfs_unmount: return(%d)\n", SUCCESS));
    return (SUCCESS);
}

/************************************************************************
 * iumfs_root()  VFS オペレーション
 *
 * ファイルシステムのルートの vnode を返す。
 *************************************************************************/
static int
iumfs_root(vfs_t *vfsp, vnode_t **vpp)
{
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    int err = SUCCESS;
    vnode_t *rootvp;

    DEBUG_PRINT((CE_CONT, "iumfs_root called\n"));

    if ((iumfsp = (iumfs_t *) vfsp->vfs_data) == NULL) {
        cmn_err(CE_WARN, "iumfs_root: vfsp->vfs_data is NULL\n");
        DEBUG_PRINT((CE_CONT, "iumfs_root: return(EINVAL)\n"));
        return (EINVAL);
    }

    mutex_enter(&(iumfsp->iumfs_lock));
    rootvp = iumfsp->rootvnode;

    // 途中で break するためだけの do-while 文
    do {
        if (rootvp == NULL) {
            /*
             * ファイルシステムのルートディレクトリの vnode が無い。
             * ルートディレクトリはマウント時に作成されているはず。
             */
            cmn_err(CE_CONT, "iumfs_root: BUG root vnode doesn't exit!!\n");
            err = ENOENT;
            break;
        }

        DEBUG_PRINT((CE_CONT, "iumfs_root: rootvp->v_count = %d\n", rootvp->v_count));

        // vnode を返すので、参照カウントをあげる
        VN_HOLD(rootvp);

        // 引数として渡された vnode のポインタに新しいファイルシステムのルートの vnode をセット
        *vpp = rootvp;

    } while (0);

    mutex_exit(&(iumfsp->iumfs_lock));
    DEBUG_PRINT((CE_CONT, "iumfs_root: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_statvfs()  VFS オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_statvfs(vfs_t *vfsp, struct statvfs64 *statvfs)
{
    DEBUG_PRINT((CE_CONT, "iumfs_statvfs called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_statvfs: return(ENOTSUP)\n"));
    return (ENOTSUP);
}

/************************************************************************
 * iumfs_sync()  VFS オペレーション
 *
 *  常に 0 （成功）を返す
 *************************************************************************/
static int
iumfs_sync(vfs_t *vfsp, short val, struct cred *cr)
{
    //    DEBUG_PRINT((CE_CONT,"iumfs_sync is called\n"));

    return (SUCCESS);
}

/************************************************************************
 * iumfs_vget()  VFS オペレーション
 *
 * 未サポート
 *************************************************************************/
static int
iumfs_vget(vfs_t *vfsp, vnode_t **vnode, struct fid *fid)
{
    DEBUG_PRINT((CE_CONT, "iumfs_vget called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_mountroot()  VFS オペレーション
 *
 * 未サポート
 *************************************************************************/
static int
iumfs_mountroot(vfs_t *vfsp, enum whymountroot whymountroot)
{
    DEBUG_PRINT((CE_CONT, "iumfs_mountroot called\n"));

    return (ENOTSUP);
}

#ifndef SOL10

/************************************************************************
 * iumfs_reserved()  VFS オペレーション
 *
 * 未サポート
 *************************************************************************/
static int
iumfs_reserved(vfs_t *vfsp, vnode_t **vnode, char *strings)
{
    DEBUG_PRINT((CE_CONT, "iumfs_reserved called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_reserved: return(ENOTSUP)\n"));
    return (ENOTSUP);
}
#endif

/************************************************************************
 * iumfs_freevfs()  VFS オペレーション
 *
 * 未サポート
 *************************************************************************/
static void
iumfs_freevfs(vfs_t *vfsp)
{
    DEBUG_PRINT((CE_CONT, "iumfs_freevfs called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_freevfs: return(ENOTSUP)\n"));
    return;
}
