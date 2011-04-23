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
 * iumfs_vnops
 *
 * VNODE Operations for IUMFS pseudo filesystem.
 *   
 **************************************************************/
/*
 * TOOD: lookup  で daemon 経由で見つかったときの dirent への追加は？
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
#include <sys/uio.h>
#include <sys/vm.h>
#include <sys/exec.h>

#include "iumfs.h"

/* VNODE 操作プロトタイプ宣言 */
#ifdef SOL10
static int iumfs_open(vnode_t **, int, struct cred *);
static int iumfs_close(vnode_t *, int, int, offset_t, struct cred *);
static int iumfs_read(vnode_t *, struct uio *, int, struct cred *);
static int iumfs_getattr(vnode_t *, vattr_t *, int, struct cred *);
static int iumfs_access(vnode_t *, int, int, struct cred *);
static int iumfs_lookup(vnode_t *, char *, vnode_t **, struct pathname *, int,
        vnode_t *, struct cred *);
static int iumfs_readdir(vnode_t *, struct uio *, struct cred *, int *);
static int iumfs_fsync(vnode_t *, int, struct cred *);
static void iumfs_inactive(vnode_t *, struct cred *);
static int iumfs_seek(vnode_t *, offset_t, offset_t *);
static int iumfs_cmp(vnode_t *, vnode_t *);
static int iumfs_getpage(vnode_t *, offset_t, size_t, uint_t *, struct page **,
        size_t, struct seg *, caddr_t, enum seg_rw, struct cred *);
static int iumfs_putpage(vnode_t *, offset_t, size_t, int, cred_t *);
static int iumfs_map(vnode_t *, offset_t, struct as *, caddr_t *, size_t,
        uchar_t, uchar_t, uint_t, struct cred *);
static int iumfs_write(vnode_t *, struct uio *, int, struct cred *);
static int iumfs_setattr(vnode_t *, vattr_t *, int, struct cred *);
static int iumfs_create(vnode_t *, char *, vattr_t *, vcexcl_t, int,
        vnode_t **, struct cred *, int);
static int iumfs_remove(vnode_t *, char *, struct cred *);
static int iumfs_rename(vnode_t *, char *, vnode_t *, char *, struct cred *);
static int iumfs_mkdir(vnode_t *, char *, vattr_t *, vnode_t **, struct cred *);
static int iumfs_rmdir(vnode_t *, char *, vnode_t *, struct cred *);
#else
static int iumfs_ioctl(vnode_t *, int, intptr_t, int, struct cred *, int *);
static int iumfs_setfl(vnode_t *, int, int, struct cred *);
static int iumfs_setattr(vnode_t *, vattr_t *, int, struct cred *);
static int iumfs_create(vnode_t *, char *, vattr_t *, vcexcl_t, int,
        vnode_t **, struct cred *, int);
static int iumfs_remove(vnode_t *, char *, struct cred *);
static int iumfs_link(vnode_t *, vnode_t *, char *, struct cred *);
static int iumfs_rename(vnode_t *, char *, vnode_t *, char *, struct cred *);
static int iumfs_mkdir(vnode_t *, char *, vattr_t *, vnode_t **, struct cred *);
static int iumfs_rmdir(vnode_t *, char *, vnode_t *, struct cred *);
static int iumfs_symlink(vnode_t *, char *, vattr_t *, char *, struct cred *);
static int iumfs_readlink(vnode_t *, struct uio *, struct cred *);
static int iumfs_fid(vnode_t *, struct fid *);
static void iumfs_rwlock(vnode_t *, int);
static void iumfs_rwunlock(vnode_t *, int);
static int iumfs_frlock(vnode_t *, int, struct flock64 *, int, offset_t,
        struct flk_callback *, struct cred *);
static int iumfs_space(vnode_t *, int, struct flock64 *, int, offset_t,
        struct cred *);
static int iumfs_realvp(vnode_t *, vnode_t **);
static int iumfs_addmap(vnode_t *, offset_t, struct as *, caddr_t, size_t,
        uchar_t, uchar_t, uint_t, struct cred *);
static int iumfs_delmap(vnode_t *, offset_t, struct as *, caddr_t, size_t,
        uint_t, uint_t, uint_t, struct cred *);
static int iumfs_poll(vnode_t *, short, int, short *, struct pollhead **);
static int iumfs_dump(vnode_t *, caddr_t, int, int);
static int iumfs_pathconf(vnode_t *, int, ulong_t *, struct cred *);
static int iumfs_pageio(vnode_t *, struct page *, u_offset_t, size_t, int,
        struct cred *);
static int iumfs_dumpctl(vnode_t *, int, int *);
static void iumfs_dispose(vnode_t *, struct page *, int, int, struct cred *);
static int iumfs_setsecattr(vnode_t *, vsecattr_t *, int, struct cred *);
static int iumfs_getsecattr(vnode_t *, vsecattr_t *, int, struct cred *);
static int iumfs_shrlock(vnode_t *, int, struct shrlock *, int);
#endif // idfef SOL10

/*
 * このファイルシステムでサーポートする vnode オペレーション
 */
#ifdef SOL10
/*
 * Solaris 10 の場合、vnodeops 構造体は vfs_setfsops() にて得る。
 * OpenSolaris の場合さらに fs_operation_def_t の func メンバが union に代わって
 * いる
 */
struct vnodeops *iumfs_vnodeops;
#ifdef OPENSOLARIS
fs_operation_def_t iumfs_vnode_ops_def_array[] = {
    { VOPNAME_OPEN,
        {&iumfs_open}},
    { VOPNAME_CLOSE,
        {&iumfs_close}},
    { VOPNAME_READ,
        {&iumfs_read}},
    { VOPNAME_GETATTR,
        {&iumfs_getattr}},
    { VOPNAME_ACCESS,
        {&iumfs_access}},
    { VOPNAME_LOOKUP,
        {&iumfs_lookup}},
    { VOPNAME_READDIR,
        {&iumfs_readdir}},
    { VOPNAME_FSYNC,
        {&iumfs_fsync}},
    { VOPNAME_INACTIVE,
        {(fs_generic_func_p) & iumfs_inactive}},
    { VOPNAME_SEEK,
        {&iumfs_seek}},
    { VOPNAME_CMP,
        {&iumfs_cmp}},
    { VOPNAME_GETPAGE,
        {&iumfs_getpage}},
    { VOPNAME_PUTPAGE,
        {&iumfs_putpage}},
    { VOPNAME_MAP,
        {(fs_generic_func_p) & iumfs_map}},
    { VOPNAME_WRITE,
        {&iumfs_write}},
    { VOPNAME_SETATTR,
        {&iumfs_setattr}},
    { VOPNAME_CREATE,
        {&iumfs_create}},
    { VOPNAME_REMOVE,
        {&iumfs_remove}},
    { VOPNAME_RENAME,
        {&iumfs_rename}},
    { VOPNAME_MKDIR,
        {&iumfs_mkdir}},
    { VOPNAME_RMDIR,
        {&iumfs_rmdir}},
    { NULL,
        {NULL}},
};
#else
fs_operation_def_t iumfs_vnode_ops_def_array[] = {
    { VOPNAME_OPEN, &iumfs_open},
    { VOPNAME_CLOSE, &iumfs_close},
    { VOPNAME_READ, &iumfs_read},
    { VOPNAME_GETATTR, &iumfs_getattr},
    { VOPNAME_ACCESS, &iumfs_access},
    { VOPNAME_LOOKUP, &iumfs_lookup},
    { VOPNAME_READDIR, &iumfs_readdir},
    { VOPNAME_FSYNC, &iumfs_fsync},
    { VOPNAME_INACTIVE, (fs_generic_func_p) & iumfs_inactive},
    { VOPNAME_SEEK, &iumfs_seek},
    { VOPNAME_CMP, &iumfs_cmp},
    { VOPNAME_GETPAGE, &iumfs_getpage},
    { VOPNAME_PUTPAGE, &iumfs_putpage},
    { VOPNAME_MAP, (fs_generic_func_p) & iumfs_map},
    { VOPNAME_WRITE, &iumfs_write},
    { VOPNAME_SETATTR, &iumfs_setattr},
    { VOPNAME_CREATE, &iumfs_create},
    { VOPNAME_REMOVE, &iumfs_remove},
    { VOPNAME_RENAME, &iumfs_rename},
    { VOPNAME_MKDIR, &iumfs_mkdir},
    { VOPNAME_RMDIR, &iumfs_rmdir},
    { NULL, NULL},
};
#endif // ifdef OPENSOLARIS
#else
/*
 * Solaris 9 の場合、vnodeops 構造体はファイルシステムが領域を確保し、直接参照できる
 */
struct vnodeops iumfs_vnodeops = {
    &iumfs_open,
    &iumfs_close,
    &iumfs_read,
    &iumfs_write,
    &iumfs_ioctl,
    &iumfs_setfl,
    &iumfs_getattr,
    &iumfs_setattr,
    &iumfs_access,
    &iumfs_lookup,
    &iumfs_create,
    &iumfs_remove,
    &iumfs_link,
    &iumfs_rename,
    &iumfs_mkdir,
    &iumfs_rmdir,
    &iumfs_readdir,
    &iumfs_symlink,
    &iumfs_readlink,
    &iumfs_fsync,
    &iumfs_inactive,
    &iumfs_fid,
    &iumfs_rwlock,
    &iumfs_rwunlock,
    &iumfs_seek,
    &iumfs_cmp,
    &iumfs_frlock,
    &iumfs_space,
    &iumfs_realvp,
    &iumfs_getpage,
    &iumfs_putpage,
    &iumfs_map,
    &iumfs_addmap,
    &iumfs_delmap,
    &iumfs_poll,
    &iumfs_dump,
    &iumfs_pathconf,
    &iumfs_pageio,
    &iumfs_dumpctl,
    &iumfs_dispose,
    &iumfs_setsecattr,
    &iumfs_getsecattr,
    &iumfs_shrlock
};
#endif // ifdef SOL10

/************************************************************************
 * iumfs_open()  VNODE オペレーション
 *
 * open(2) システムコールに対応。
 *
 * 戻り値
 *    正常時   : 0
 *    エラー時 : errno * 
 *************************************************************************/
static int
iumfs_open(vnode_t **vpp, int ioflag, struct cred *cr)
{
    vnode_t *vp;
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_open is called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_open: vpp = 0x%p, vp = 0x%p\n", vpp, *vpp));

    vp = *vpp;

    if (vp == NULL) {
        DEBUG_PRINT((CE_CONT, "iumfs_open: vnode is null\n"));
        err = EINVAL;
        goto out;
    }

    /*
     * ファイルへのアクセスの可否を検証する
     */
    if((err = iumfs_access(vp, VWRITE, ioflag, cr))){
        DEBUG_PRINT((CE_CONT, "iumfs_open: file access denied.\n"));            
        VN_RELE(vp);
        goto out;
    }

  out:
    DEBUG_PRINT((CE_CONT, "iumfs_open: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_close()  VNODE オペレーション
 *
 *************************************************************************/
static int
iumfs_close(vnode_t *vp, int flag, int count, offset_t offset,
        struct cred *cr)
{
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_close is called\n"));
    /*
     * B_INVAL フラグは処理が終わったあとそのーページを破棄するように
     * 依頼すること。putpage をサイズ0で呼び出すことで、全ページの
     * dirty page の put と page の破棄が行われる。
     */
    err = iumfs_putpage(vp, 0, 0, B_INVAL, cr);
    DEBUG_PRINT((CE_CONT, "iumfs_close: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_read()  VNODE オペレーション
 *
 * read(2) システムコールに対応する。
 * ファイルのデータを uiomove を使って、ユーザ空間のアドレスにコピーする。
 *************************************************************************/
static int
iumfs_read(vnode_t *vp, struct uio *uiop, int ioflag, struct cred *cr)
{
    iumnode_t *inp;
    int err = 0;
    caddr_t base = NULL; // vnode とマップされたカーネル空間のアドレス
    offset_t mapoff = 0; // block の境界線までのオフセット値
    offset_t reloff = 0; // block の境界線からの相対的なオフセット値
    size_t mapsz = 0; // マップするサイズ
    size_t rest = 0; // ファイルサイズと要求されているオフセット値との差
    uint_t flags = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_read is called\n"));

    // ファイルシステム型依存のノード構造体を得る
    inp = VNODE2IUMNODE(vp);

    mutex_enter(&(inp->i_lock));

    if (!(inp->vattr.va_type | VREG)) {
        DEBUG_PRINT((CE_CONT, "iumfs_read: file is not regurar file\n"));
        err = ENOTSUP;
        goto out;
    }

    // オフセット値がファイルサイズを超えている
    if (uiop->uio_loffset > inp->fsize) {
        DEBUG_PRINT((CE_CONT, "iumfs_read: offset(%d) exceeds file size(%u)\n", uiop->uio_loffset, inp->fsize));
        err = 0;
        goto out;
    }

    do {
        /*
         * uio 構造体の loffset/resid と各ローカル変数の関係 (MAXBSIZE  は 8192)
         *   
         *  | MAXBSIZE | MAXBSIZE | MAXBSIZE | MAXBSIZE | MAXBSIZE | MAXBSIZE |
         * -|----------|----------|----------|----------|----------|----------|
         * ----------- File size -------------------------------------->|
         * - uiop->loffset ----------->|<-- uiop->resid----->|
         * -- mapoff ------------>|
         *                        |<-->|<--->|
         *                        reloff mapsz
         *                             |<---------- rest -------------->|
         *
         * 一回の segmap_getmapflt でマップ できるのは MAXBSIZE 分だけなので、
         * uiop->resid 分だけマップするために繰り返し segmap_getmapflt を呼ぶ
         * 必要がある。
         */
        mapoff = uiop->uio_loffset & MAXBMASK;
        reloff = uiop->uio_loffset & MAXBOFFSET;
        mapsz = MAXBSIZE - reloff;
        rest = inp->fsize - uiop->uio_loffset;
        /*
         * もし要求されているオフセット値からの残りのサイズが 0 以下だった場合は
         * リターンする。
         */
        if (rest <= 0)
            goto out;
        /*
         * もし mapsz がファイルの残りのサイズ(rest) よりも大きかったら rest を
         * mapsz とする。
         */
        mapsz = MIN(mapsz, rest);

        /*
         * もし resid が mapsz より小さければ（つまり最後のマッピング処理の場合）
         * 、resid を mapsz とする。
         */
        mapsz = MIN(mapsz, uiop->uio_resid);

        DEBUG_PRINT((CE_CONT, "iumfs_read: uiop->uio_loffset=%d\n", uiop->uio_loffset));
        DEBUG_PRINT((CE_CONT, "iumfs_read: uiop->uio_resid=%d\n", uiop->uio_resid));
        DEBUG_PRINT((CE_CONT, "iumfs_read: mapoff=%d\n", mapoff));
        DEBUG_PRINT((CE_CONT, "iumfs_read: reloff=%d\n", reloff));
        DEBUG_PRINT((CE_CONT, "iumfs_read: mapsz=%d\n", mapsz));

        /*
         * ファイルの指定領域とカーネルアドレス空間のマップを行う。
         * segmap_getmapflt の第 5 引数の forcefault を 1 にすると、
         * segmap_getmapflt の中でページフォルトで発生し iumfs_getpage が呼ばれる。
         * もし 0 とすると uiomove() が呼ばれてページフォルトが発生した段階で初めて
         * iumfs_getpage 呼ばれることになる。
         */
        DEBUG_PRINT((CE_CONT, "iumfs_read: calling segmap_getmapflt\n"));
        base = segmap_getmapflt(segkmap, vp, mapoff + reloff, mapsz, 1, S_READ);
        if (base == NULL) {
            cmn_err(CE_WARN, "iumfs_read: segmap_getmapflt failed\n");
            err = ENOMEM;
            goto out;
        }
        DEBUG_PRINT((CE_CONT, "iumfs_read: segmap_getmapflt succeeded \n"));

        /*
         * 読み込んだデータをユーザ空間にコピーする。
         * もし、この時点で pagefault が発生したら VOP_GETPAGE ルーチン
         * （iumfs_getpage）が呼ばれ、ユーザモードデーモンにデータの取得を依頼する。
         */
        DEBUG_PRINT((CE_CONT, "iumfs_read: calling uiomove\n"));
        err = uiomove(base + reloff, mapsz, UIO_READ, uiop);
        if (err != SUCCESS) {
            cmn_err(CE_WARN, "iumfs_read: uiomove failed (%d)\n", err);
            goto out;
        }
        DEBUG_PRINT((CE_CONT, "iumfs_read: copyout %d bytes of data \n", mapsz));

        /*
         * マッピングを解放する。フリーリストに追加される。
         */
        err = segmap_release(segkmap, base, flags);
        if (err != SUCCESS) {
            DEBUG_PRINT((CE_CONT, "iumfs_read: segmap_release failed (%d)\n", err));
            goto out;
        }

        DEBUG_PRINT((CE_CONT, "iumfs_read: segmap_release succeeded \n"));

    } while (uiop->uio_resid > 0);

out:
    inp->vattr.va_atime = iumfs_get_current_time();
    mutex_exit(&(inp->i_lock));
    DEBUG_PRINT((CE_CONT, "iumfs_read: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_getattr()  VNODE オペレーション
 *
 * GETATTR ルーチン
 *************************************************************************/
static int
iumfs_getattr(vnode_t *vp, vattr_t *vap, int flags, struct cred *cr)
{
    iumnode_t *inp;
    int err;
    timestruc_t prev_mtime; // キャッシュしていた更新日時
    timestruc_t curr_mtime; // 最新の更新日時

    DEBUG_PRINT((CE_CONT, "iumfs_getattr is called\n"));

    inp = VNODE2IUMNODE(vp);
    prev_mtime = inp->vattr.va_mtime;

    DEBUG_PRINT((CE_CONT, "iumfs_getattr: pathname=%s\n", inp->pathname));

    /*
     * まずは Dirty Page を処理する。そうじゃないと、あやまったファイルサイズを
     * 取得してしまう可能性がある。
     */
    err = iumfs_putpage(vp, 0, 0, B_INVAL, cr);

    /*
     * ユーザモードデーモンに最新の属性情報を問い合わせる。
     */
    if((err = iumfs_request_getattr(vp)) != 0){
        return (err);
    }
    
    /*
     * TODO: ここで vnode の 参照カウントを減らしては駄目だ。上位ではこの後も vnode を使うので
     * ここで参照カウント減らすと、free されて fop_getattr 内でpanic に至る。
     * しかし、じゃあだれが減らすのか?という問題も残る。
     * iumfs_request_readdir とかでサーバ側にエントリが亡くなった時点で参照カウント外すべきなのかも。。。
     * 
    if (err && iumfs_is_root(vp) == FALSE) {
        //
        // 対象ファイルが Server 上に見つからなかった。エントリを削除する。
        // ファイルシステム・ルートの場合は削除はせず、現状のデータを返す。
        //
        DEBUG_PRINT((CE_CONT, "iumfs_getattr: can't update latest attr of vnode(=%p)", vp));
        // 親ディレクトリを探す
        if ((parentvp = iumfs_find_parent_vnode(vp)) == NULL) {
            cmn_err(CE_CONT, "iumfs_getattr: failed to find parent vnode of \"%s\"\n", inp->pathname);
            DEBUG_PRINT((CE_CONT, "iumfs_getattr: return(%d)\n", err));
            return (err);
        }
        // パス名より名前を得る
        if ((name = strrchr(inp->pathname, '/')) == NULL) {
            cmn_err(CE_CONT, "iumfs_getattr: failed to get name of \"%s\"\n", inp->pathname);
            DEBUG_PRINT((CE_CONT, "iumfs_getattr: return(%d)\n", err));
            return (err);
        }
        // スラッシュから始まっているので、一文字ずらす
        name++;

        //
        // 親ディレクトリからエントリを削除
        // その後、iumfs_find_parent_vnode() で増やされたの親ディレクトリの参照カウント分を減らす 
        //
        iumfs_remove_entry_from_dir(parentvp, name);
        VN_RELE(parentvp);

        //
        // 最後にこの vnode の参照カウントを減らす。
        // この vnode を参照中の人がいるかもしれないので（たとえば shell の
        // カレントディレクトリ）、ここでは free はしない。
        // 参照数が 1 になった段階で iumfs_inactive() が呼ばれ、iumfs_inactive()
        // から free される。
        //
        VN_RELE(vp); // vnode 作成時に増加された参照カウント分を減らす。
        DEBUG_PRINT((CE_CONT, "iumfs_getattr: return(%d)\n", err));
        return (err);
    }
    */
    
    curr_mtime = inp->vattr.va_mtime;

    /*
     * 更新日が変更されていたら vnode に関連したページを無効化する
     */
    if ((curr_mtime.tv_sec != prev_mtime.tv_sec)
            || (curr_mtime.tv_nsec != prev_mtime.tv_nsec)) {
        DEBUG_PRINT((CE_CONT, "iumfs_getattr: mtime has been changed. invalidating pages."));
        // ページを vnode に関連した全ページを無効化する。
        if ((err = iumfs_putpage(vp, 0, 0, B_INVAL, cr))) {
            DEBUG_PRINT((CE_CONT, "iumfs_getattr: return(%d)\n", err));
            return (err);
        }
    }

    /*
     * ファイルシステム型依存のノード情報(iumnode 構造体)から vnode の属性情報をコピー。
     * 本来は、va_mask にて bit が立っている属性値だけをセットすればよいの
     * だが、めんどくさいので、全ての属性値を vap にコピーしてしまう。
     */
    bcopy(&inp->vattr, vap, sizeof (vattr_t));
    DEBUG_PRINT((CE_CONT, "iumfs_getattr: fsize=%u\n", inp->fsize));

    /* 
     * va_mask;      // uint_t           bit-mask of attributes        
     * va_type;      // vtype_t          vnode type (for create)      
     * va_mode;      // mode_t           file access mode             
     * va_uid;       // uid_t            owner user id                
     * va_gid;       // gid_t            owner group id               
     * va_fsid;      // dev_t(ulong_t)   file system id (dev for now) 
     * va_nodeid;    // ino_t          node id                      
     * va_nlink;     // nlink_t          number of references to file 
     * va_size;      // u_offset_t       file size in bytes           
     * va_atime;     // timestruc_t      time of last access          
     * va_mtime;     // timestruc_t      time of last modification    
     * va_ctime;     // timestruc_t      time file ``created''        
     * va_rdev;      // dev_t            device the file represents   
     * va_blksize;   // uint_t           fundamental block size       
     * va_nblocks;   // ino_t          # of blocks allocated        
     * va_vcode;     // uint_t           version code                
     */

    DEBUG_PRINT((CE_CONT, "iumfs_getattr: return(0)\n"));
    return (0);
}

/************************************************************************
 * iumfs_access()  VNODE オペレーション
 *
 * vnode に対する mode, flags でのアクセスの可否を判定する。
 * TODO: 現在アクセス判定のルールをこのルーチンの中に直接書いてしまってい
 * るが、本来であれば iumfs_init 時に設定されたファイルシステムタイプ特有
 * のルールを判定すべき。（汎用性という意味で）
 *
 * 戻り値
 *    正常時   : 0
 *    エラー時 : errno
 *************************************************************************/
static int
iumfs_access(vnode_t *vp, int mode, int ioflag, struct cred *cr)
{
    iumnode_t *inp = NULL;
    int err = 0;
    
    DEBUG_PRINT((CE_CONT, "iumfs_access is called\n"));
    inp = VNODE2IUMNODE(vp);

    /*
     * 既存データがある状態(fize!=0)で TRUNCATE は許可されない
     * ファイルができて最初の書き込みであればいい。
     */ 
    if((inp->fsize != 0) && (mode & VWRITE) && (ioflag & FTRUNC)){
        DEBUG_PRINT((CE_CONT, "iumfs_access: retuested to truncate file.\n"));
        err = ENOTSUP;
    }

    DEBUG_PRINT((CE_CONT, "iumfs_access: return(%d)\n", err));
return (err);
}

/************************************************************************
 * iumfs_lookup()  VNODE オペレーション
 *
 *  引数渡されたファイル/ディレクトリ名（name）をディレクトリエントリから
 *  探し、もし存在すれば、そのファイル/ディレクトリの vnode のアドレスを
 *  引数として渡された vnode のポインタにセットする。
 *
 *************************************************************************/
static int
iumfs_lookup(vnode_t *dirvp, char *name, vnode_t **vpp, struct pathname *pnp,
        int flags, vnode_t *rdir, struct cred *cr) {
    vnode_t *vp = NULL;
    iumnode_t *dirinp, *inp;
    ino_t foundid = 0; // 64 bit のノード番号（ inode 番号）
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    vfs_t *vfsp;
    int err = 0;
    vattr_t vap[1];
    char pathname[IUMFS_MAXPATHLEN]; // マウントポイントからのパス名

    DEBUG_PRINT((CE_CONT, "iumfs_lookup is called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_lookup: name=\"%s\"\n", name));

    iumfsp = VNODE2IUMFS(dirvp);
    dirinp = VNODE2IUMNODE(dirvp);
    vfsp = VNODE2VFS(dirvp);

    /*
     * ファイルシステムルートからのパス名を得る。
     * もし親ディレクトリがファイルシステムルートだったら、余計な「/」はつけない。
     */
    if (ISROOT(dirinp->pathname))
        snprintf(pathname, IUMFS_MAXPATHLEN, "/%s", name);
    else
        snprintf(pathname, IUMFS_MAXPATHLEN, "%s/%s", dirinp->pathname, name);

    DEBUG_PRINT((CE_CONT, "iumfs_lookup: pathname=\"%s\"\n", pathname));

    foundid = iumfs_find_nodeid_by_name(dirinp, name);
    if (foundid == 0) {
        /*
         * ディレクトリエントリの中に該当するファイルが見つらなかった。
         * readdir されていない（既知でない）エントリの場合ここに来る。
         */
        DEBUG_PRINT((CE_CONT, "iumfs_lookup: can't get node id of \"%s\" in existing dir entry\n", name));
        vp = iumfs_find_vnode_by_pathname(iumfsp, pathname);
    } else {
        /*
         * ディレクトリエントリの中に該当するファイルが見つかった。
         */
        vp = iumfs_find_vnode_by_nodeid(iumfsp, foundid);
    }

    if (vp != NULL) {
        DEBUG_PRINT((CE_CONT, "iumfs_lookup: found existing vnode of \"%s\"\n", name));
    } else {
        if (strcmp(name, "..") == 0) {
            /*
             * ここにくるのはマウントポイントでの「..」の検索要求の時だけ。
             * 現在は決めうちで / の vnode を返している・・
             * TODO: マウントポイントの親ディレクトリの vnode を探してやる
             */
            DEBUG_PRINT((CE_CONT, "iumfs_lookup: look for a vnode of parent directory\n"));
            err = lookupname("/", UIO_SYSSPACE, FOLLOW, 0, &vp);
            /*
             * lookupname() が正常終了した場合は、親ディレクトリが存在するファイルシステムが
             * vnode の参照カウントを増加させていると期待される。
             * なので、ここでは vnode に対して VN_HOLD() は使わない。
             */
            if (err) {
                DEBUG_PRINT((CE_CONT, "iumfs_lookup: cannot find vnode of parent directory\n"));
                err = ENOENT;
                goto out;
            }
        } else {
            if ((err = iumfs_request_lookup(dirvp, pathname, vap)) != 0) {
                DEBUG_PRINT((CE_CONT, "iumfs_lookup: cannot find \"%s\"\n", name));
                // サーバ上にも見つからなかった・・エラーを返す
                goto out;
            }

            DEBUG_PRINT((CE_CONT, "iumfs_lookup: found file \"%s\" on server\n", name));
            /*
             * リモートサーバ上にファイルが見つかったので新しいノードを作成する。
             * ディレクトリの場合、「.」と「..」の 2 つディレクトリエントリを追加
             * しなければいけないので、iumfs_make_directory() 経由でノードの
             * 追加を行う。
             */
            if (vap->va_type & VDIR) {
                if ((err = iumfs_make_directory(vfsp, &vp, dirvp, cr, foundid)) != 0) {
                    cmn_err(CE_CONT, "iumfs_lookup: failed to create directory \"%s\"\n", name);
                    goto out;
                }
            } else {
                if ((err = iumfs_alloc_node(vfsp, &vp, 0,
                        vap->va_type, foundid)) != SUCCESS) {
                    cmn_err(CE_CONT, "iumfs_lookup: failed to create new node \"%s\"\n", name);
                    goto out;
                }
            }
            inp = VNODE2IUMNODE(vp);
            snprintf(inp->pathname, IUMFS_MAXPATHLEN, "%s", pathname);

            DEBUG_PRINT((CE_CONT, "iumfs_lookup: allocated new node \"%s\"(nodeid=%d)\n", inp->pathname,inp->vattr.va_nodeid));            
            /*
             * もしまだディレクトリにのファイルのエントリがなかったら(foundid==0だったら）
             * 割り当てられた nodeid を使ってディレクトリにエントリを追加する。
             */
            if(foundid == 0){
                DEBUG_PRINT((CE_CONT, "iumfs_lookup: adding entry to directory"));
                if (iumfs_add_entry_to_dir(dirvp, name, strlen(name), inp->vattr.va_nodeid) < 0) {
                    cmn_err(CE_CONT, "iumfs_create: cannot add new entry to directory\n");
                    err = ENOSPC;
                    goto out;
                }
            }

            // vnode の参照カウントを増やす            
            VN_HOLD(vp);
        }
    }
    *vpp = vp;
    
  out:    
    DEBUG_PRINT((CE_CONT, "iumfs_lookup: return(%d)\n",err));
    return (err);
}

/************************************************************************
 * iumfs_readdir()  VNODE オペレーション
 *
 * getdent(2) システムコールに対応する。
 * 引数で指定された vnode がさすディレクトリのデータを読み、dirent 構造体
 * を返す。
 *************************************************************************/
static int
iumfs_readdir(vnode_t *dirvp, struct uio *uiop, struct cred *cr, int *eofp)
{
    offset_t dent_total;
    iumnode_t *dirinp;
    int err;
    dirent64_t *dentp;
    offset_t offset;
    offset_t readoff = 0; // directory エントリの境界を考えた offset
    size_t readsize = 0;
    int count_start = 0;
    //time_t       prev_mtime = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_readdir is called.\n"));

    // ノードのタイプが VDIR じゃ無かったらエラーを返す
    if (!(dirvp->v_type & VDIR)) {
        DEBUG_PRINT((CE_CONT, "iumfs_readdir: vnode is not a directory.\n"));
        DEBUG_PRINT((CE_CONT, "iumfs_readdir: return(ENOTDIR)\n"));
        return (ENOTDIR);
    }

    // ファイルシステム型依存のノード構造体を得る
    dirinp = VNODE2IUMNODE(dirvp);

    DEBUG_PRINT((CE_CONT, "iumfs_readdir: pathname=%s\n", dirinp->pathname));

    /*
     * サーバ上のディレクトリエントリを読みにいく
     */
    err = iumfs_request_readdir(dirvp);

    /*
     * TODO: サーバ上のディレクトリの更新の有無によってキャッシュのエントリを返す
     * ようにする。以下はテスト実装。
     * キャッシュにある更新時間(mtime)をセーブしておいて比較の材料にする。
     * 実はこの時点ですでに getattr が呼ばれてしまっているので、mtime は必ず一致し
     * てしまう罠。これではディレクトリエントリの更新はされない。要検討。
     */
    /*
     prev_mtime = dirinp->vattr.va_mtime.tv_sec;

    // 最新の更新時間(mtime)を得る
    err = iumfs_request_getattr(dirvp);    
    if(err){
        DEBUG_PRINT((CE_CONT,"iumfs_readdir: can't update latest attributes"));
        DEBUG_PRINT((CE_CONT,"iumfs_readdir: return(%d)\n",err));
        return(err);
    }
    
     // 以下の条件にあった場合にディレクトリエントリを読む
     // o ディレクトリの更新時間が変わっている。
     // o ディレクトリの更新時間が変わっていないが現在ディレクトリは空。

    if (dirinp->vattr.va_mtime.tv_sec != prev_mtime){
        err = iumfs_request_readdir(dirvp)
    } else if (iumfs_dir_is_empty(dirvp)){
        err = iumfs_request_readdir(dirvp);        
    }
    */

    mutex_enter(&(dirinp->i_lock));
    DIRENT_SANITY_CHECK("iumfs_readdir",dirinp);
    dent_total = dirinp->dlen;

    DEBUG_PRINT((CE_CONT, "iumfs_readdir: dent_total = %d\n", dent_total));
    DEBUG_PRINT((CE_CONT, "iumfs_readdir: uiop->uio_loffset = %d\n", uiop->uio_loffset));
    DEBUG_PRINT((CE_CONT, "iumfs_readdir: uiop->uio_resid  = %d\n", uiop->uio_resid));

    /*
     * ユーザが指定したオフセット(uio_loffset)から指定サイズ(uio_resid)まで
     * 範囲でのディレクトリエントリのサイズ(readsize)を計算する。
     */
    for (offset = 0 ; offset < dirinp->dlen ; offset += dentp->d_reclen) {
        dentp = (dirent64_t *) ((char *) dirinp->data + offset);

        if (!count_start) {
            if (offset >= uiop->uio_loffset) {
                readoff = offset;
                count_start = 1;
            }
        }
        if (count_start) {
            if (readsize + dentp->d_reclen > uiop->uio_resid)
                break;
            readsize += dentp->d_reclen;
        }
    }

    if (readsize == 0) {
        err = uiomove(dirinp->data, 0, UIO_READ, uiop);
        if (err == SUCCESS)
            DEBUG_PRINT((CE_CONT, "iumfs_readdir: 0 byte copied\n"));
    } else {
        err = uiomove((caddr_t) dirinp->data + readoff, readsize, UIO_READ, uiop);
        if (err == SUCCESS)
            DEBUG_PRINT((CE_CONT, "iumfs_readdir: %d byte copied\n", readsize));
    }
    dirinp->vattr.va_atime = iumfs_get_current_time();

    mutex_exit(&(dirinp->i_lock));
    DEBUG_PRINT((CE_CONT, "iumfs_readdir: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_fsync()  VNODE オペレーション
 *
 * TODO: iumfs_fsync なぜかまったく呼ばれない・・・要チェック
 * 
 *************************************************************************/
static int
iumfs_fsync(vnode_t *vp, int syncflag, struct cred *cr)
{
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_fsync is called\n"));
    err = iumfs_putpage(vp, 0, 0, B_INVAL, cr);
    DEBUG_PRINT((CE_CONT, "iumfs_fsync: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_inactive()  VNODE オペレーション
 *
 * vnode の 参照数（v_count）が 0 になった場合に VFS サブシステムから
 * 呼ばれる・・と思っていたが、これが呼ばれるときは v_count はかならず 1
 * のようだ。
 * v_count が 0 になるのは、iumfs_rmdir で明示的に参照数を 1 にしたときのみ。
 *
 *************************************************************************/
static void
iumfs_inactive(vnode_t *vp, struct cred *cr)
{
    vnode_t *rootvp;
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_inactive is called\n"));
    cmn_err(CE_CONT, "iumfs_inactive is called %s\n", vp->v_path);    
    
    /*
     * 変更されたページのディスクへの書き込みが行う
     */
    err = iumfs_putpage(vp, 0, 0, B_INVAL, cr);

    rootvp = VNODE2ROOT(vp);

    if (rootvp == NULL) {
        DEBUG_PRINT((CE_CONT, "iumfs_inactive: rootvp is NULL\n"));
        DEBUG_PRINT((CE_CONT, "iumfs_inactive: return\n"));
        return;
    }

    // この関数が呼ばれるときは v_count は常に 1 のようだ。
    DEBUG_PRINT((CE_CONT, "iumfs_inactive: vp->v_count = %d\n", vp->v_count));

    if (VN_CMP(rootvp, vp) != 0) {
        DEBUG_PRINT((CE_CONT, "iumfs_inactive: vnode is rootvp\n"));
    } else {
        DEBUG_PRINT((CE_CONT, "iumfs_inactive: vnode is not rootvp\n"));
    }

    // iumfsnode, vnode を free する。
    iumfs_free_node(vp, cr);
    DEBUG_PRINT((CE_CONT, "iumfs_inactive: return\n"));
    return;
}

/************************************************************************
 * iumfs_seek()  VNODE オペレーション
 *
 *************************************************************************/
static int
iumfs_seek(vnode_t *vp, offset_t ooff, offset_t *noffp)
{
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_seek is called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_seek: ooff = %d, noffp = %d\n", ooff, *noffp));

    if (*noffp < 0 || *noffp > MAXOFFSET_T) {
        err = EINVAL;
        goto out;
    }

out:
    DEBUG_PRINT((CE_CONT, "iumfs_seek: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_cmp()  VNODE オペレーション
 *
 * 二つの vnode のアドレスを比較。
 *
 * 戻り値
 *    同じ vnode : 1
 *    違う vnode : 0
 *************************************************************************/
static int
iumfs_cmp(vnode_t *vp1, vnode_t *vp2)
{
    DEBUG_PRINT((CE_CONT, "iumfs_cmp is called\n"));

    // VN_CMP マクロに習い、同じだったら 1 を返す
    if (vp1 == vp2) {
        DEBUG_PRINT((CE_CONT, "iumfs_cmp: return(1)\n"));
        return (1);
    } else {
        DEBUG_PRINT((CE_CONT, "iumfs_cmp: return(0)\n"));
        return (0);
    }
}

/************************************************************************
 * iumfs_getpage()  VNODE オペレーション
 *
 * vnode に関連するページを得るための処理だが実際のページの取得処理は
 * iumfs_getapage() で行う。
 *          ^
 * len が PAGESIZE を超えている場合は pvn_getpage() を呼び出し、PAGESIZE
 * 以下であれば iumfs_getapage() を呼び出す。どちらにしても最終的には
 * iumfs_getapage() が呼ばれる。
 *************************************************************************/
static int
iumfs_getpage(vnode_t *vp, offset_t off, size_t len, uint_t *protp,
        struct page **plarr, size_t plsz, struct seg *seg, caddr_t addr,
        enum seg_rw rw, struct cred *cr)
{
    int err;

    DEBUG_PRINT((CE_CONT, "iumfs_getpage is called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_getpage: vnode=%p", vp));
    DEBUG_PRINT((CE_CONT, "iumfs_getpage: off=%d,len=%d,plsz=%d\n", off, len, plsz));

    if (len <= PAGESIZE) {
        err = iumfs_getapage(vp, off, len, protp, plarr, plsz, seg, addr, rw, cr);
    } else {
        err = pvn_getpages(iumfs_getapage, vp, off, len, protp, plarr, plsz, seg, addr, rw, cr);
    }
    DEBUG_PRINT((CE_CONT, "iumfs_getpage: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_putpage()  VNODE オペレーション
 *
 * ページサイズ毎に iumfs_putapage() を呼ぶ
 *
 *************************************************************************/
int
iumfs_putpage(vnode_t *vp, offset_t off, size_t len, int flags, cred_t *cr)
{
    u_offset_t preloff = 0; //ページ内の相対オフセット
    u_offset_t poff; // ループ処理用のオフセット
    size_t psz = 0; // ページ内の要求されたサイズ
    size_t rest = 0;
    int err = 0;
    page_t *pp = NULL;

    DEBUG_PRINT((CE_CONT, "iumfs_putpage is called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_putpage: vnode=%p", vp));
    DEBUG_PRINT((CE_CONT, "iumfs_putpage: off=%d, len=%d\n", off, len));

    if (len == 0) {
        DEBUG_PRINT((CE_CONT, "iumfs_putpage: calling pvn_vplist_dirty\n"));
        if ((err = pvn_vplist_dirty(vp, off, iumfs_putapage, B_INVAL, cr))) {
            cmn_err(CE_WARN, "iumfs_putpage: pvn_vplist_dirty failed (%d)\n", err);
        }
        DEBUG_PRINT((CE_CONT, "iumfs_putpage: pvn_vplist_dirty returned with (%d)\n", err));
        goto out;
    }

    /*
     * PAGESIZE 単位で iumfs_putapage を呼ぶ
     *
     *  --|-------PAGESIZE--------|-------PAGESIZE------|-----PAGESIZE--------|
     *  ------ poff ---->|
     *    |<---preloff-->|<--psz->|
     *                   |<--------------- rest ------------->|
     */
    rest = len;
    poff = off;
    while (err == 0 && off + len > poff) {
        DEBUG_PRINT((CE_CONT, "iumfs_putpage: rest=%u, poff=%u\n", rest, poff));
        /*
         * ページを無効化するのが目的じゃない場合はページをフリーリストから
         * 取得するのを避けるため page_lookup_nowait() を使う。
         */
        if (flags & (B_INVAL | B_FREE)) {
            pp = page_lookup(vp, poff, SE_EXCL);
        } else {
            pp = page_lookup_nowait(vp, poff, SE_SHARED);
        }

        preloff = poff & PAGEOFFSET;
        psz = PAGESIZE - preloff;
        psz = MIN(psz, rest);

        if (pp) {
            if (pvn_getdirty(pp, flags) != 0) {
                err = iumfs_putapage(vp, pp, &poff, &psz, flags, cr);
            }
        } else {
            /*
             * オフセットに該当するページが見つからなかった場合は、putpage 処理は
             * 行わずに次のページに進む。
             */
            psz = PAGESIZE;
        }
        rest -= psz;
        poff += psz;
    }

out:
    DEBUG_PRINT((CE_CONT, "iumfs_putpage: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_map()  VNODE オペレーション
 *
 * サポートしていない
 * 
 *       実行ファイルの exec(2) にはこの vnode オペレーションの
 *       サポートが必須。なので、現在はこのファイルシステム上にある実行
 *       可能ファイルを exec(2) することはできない。
 *************************************************************************/
static int
iumfs_map(vnode_t *vp, offset_t off, struct as *as, caddr_t *addrp, size_t len,
        uchar_t prot, uchar_t maxprot, uint_t flags, struct cred *cr)
{
    struct segvn_crargs vn_a;
    int err;

    DEBUG_PRINT((CE_CONT, "iumfs_map is called\n"));

    /*
    if (vp->v_flag & VNOMAP)
        Return (ENOSYS);
 
    if (off > UINT32_MAX || off + len > UINT32_MAX)
        return (ENXIO);
     */

    as_rangelock(as);
    if ((flags & MAP_FIXED) == 0) {
        map_addr(addrp, len, off, 1, flags);
        if (*addrp == NULL) {
            as_rangeunlock(as);
            DEBUG_PRINT((CE_CONT, "iumfs_map: return(ENOMEM)\n"));
            return (ENOMEM);
        }
    } else {
        /*
         * User specified address - blow away any previous mappings
         */
        (void) as_unmap(as, *addrp, len);
    }

    vn_a.vp = vp;
    vn_a.offset = off;
    vn_a.type = flags & MAP_TYPE;
    vn_a.prot = prot;
    vn_a.maxprot = maxprot;
    vn_a.flags = flags & ~MAP_TYPE;
    vn_a.cred = cr;
    vn_a.amp = NULL;
    vn_a.szc = 0;
    vn_a.lgrp_mem_policy_flags = 0;

    err = as_map(as, *addrp, len, segvn_create, &vn_a);
    as_rangeunlock(as);
    DEBUG_PRINT((CE_CONT, "iumfs_map: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_getapage()  
 *
 * iumfs_getpage() もしくは vpn_getpages() から呼ばれ、vnode に関連する
 * ページを得る。
 * TODO: ファイルサイズを超えるオフセット値のリクエストが来た場合の好ましい挙動は？
 *  
 *************************************************************************/
int
iumfs_getapage(vnode_t *vp, u_offset_t off, size_t len, uint_t *protp,
        page_t *plarr[], size_t plsz, struct seg *seg, caddr_t addr,
        enum seg_rw rw, struct cred *cr)
{
    page_t *pp = NULL;
    size_t io_len;
    u_offset_t io_off;
    int err = 0;
    struct buf *bp = NULL;

    DEBUG_PRINT((CE_CONT, "iumfs_getapage is called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_getapage: vnode=%p", vp));
    DEBUG_PRINT((CE_CONT, "iumfs_getapage: off=%d,len=%d,plsz=%d\n", off, len, plsz));

    if (plarr == NULL) {
        DEBUG_PRINT((CE_CONT, "iumfs_getapage: plarr is NULL\n"));
        DEBUG_PRINT((CE_CONT, "iumfs_getapage: return(0)\n"));
        return (0);
    }
    plarr[0] = NULL;

    do {
        err = 0;
        bp = NULL;
        pp = NULL;

        /*
         * まずページキャッシュの中に該当ページが存在するかを確認する。
         * もしあればそのページを plarr にセットして返す。
         *  MEMO: page_exists はロックせずにページの存在確認をする。
         *        page_lookup は見つかったページをロックしてから返す。
         */
        if (page_exists(vp, off)) {
            DEBUG_PRINT((CE_CONT, "iumfs_getapage: page exits\n"));
            pp = page_lookup(vp, off, SE_SHARED);
            if (pp) {
                // TODO: なぜかここを通らない。おかしい。
                DEBUG_PRINT((CE_CONT, "iumfs_getapage: page found in cache\n"));
                plarr[0] = pp;
                plarr[1] = NULL;
                break;
            }
            //はじめからやり直し
            continue;
        }

        DEBUG_PRINT((CE_CONT, "iumfs_getapage: page not found in cache\n"));

        /*
         * addr で指定されたアドレス範囲から、指定された vnode のオフセットとサイズ
         * に適合する連続したページを見つける。
         * 第３引数で指定されたアドレスを含む、７引数を上限としたの連続したページを
         * 見つけ出し、効率的にディスク（仮想的な）から読み込む。
         * 現在は第７引数を PAGESIZE にしたため、io_len が PAGESIZE より大きくなるこ
         * とは無いが、実ファイルシステムのブロックサイズ（ネットワークの転送サイズ）
         * にあわせたほうが効率的と思われる。
         * TODO: 第７引数をFSのブロックサイズへ。
         */
        pp = pvn_read_kluster(vp, off, seg, addr, &io_off, &io_len, off,
                PAGESIZE, 0);
        /*
         * pvn_read_kluster が NULL を返してきた場合、他の thread が
         * すでに page を参照している可能性がある。lookup からやり直し。
         */
        if (pp == NULL) {
            DEBUG_PRINT((CE_CONT, "iumfs_getapage: pvn_read_kluster returned NULL, try lookup again.."));
            continue;
        }

        DEBUG_PRINT((CE_CONT, "iumfs_getapage: pvn_read_kluster succeeded io_off=%d,io_len=%d\n", io_off, io_len));

        /*
         * 読み込むサイズをページサイズに丸め込む。
         */
        io_len = ptob(btopr(io_len));

        DEBUG_PRINT((CE_CONT, "iumfs_getapage: ptob(btopr(io_len)) = %d\n", io_len));

        /*
         * buf 構造体を確保し、初期化する。
         *   ..bp->b_bcount = io_len;
         *   ..bp->b_bufsize = io_len;
         *   ..bp->b_vp = vp など。
         */
        bp = pageio_setup(pp, io_len, vp, B_READ);

        /*
         * block（DEV_BSIZE）数から byte 数へ
         */
        bp->b_lblkno = lbtodb(io_off); // 512 で割った数を計算？
#ifdef SOL10
        /*
         * solaris 9 の buf 構造体には以下のメンバーは含まれない。
         * これらのメンバーには何の意味が・・？ 一応セット
         */
        bp->b_file = vp; // vnode
        bp->b_offset = (offset_t) off; // vnode offset
#endif
        /*
         * カーネルの仮想アドレス空間にアドレスを確保し、ページの
         * リストにマップする。確保したアドレスはbp->b_un.b_addr
         * にセットする。man bp_mapin(9F)
         */
        bp_mapin(bp);

        /*
         * ユーザモードデーモンへリクエストを投げる。
         */
        err = iumfs_request_read(bp, vp);

        /*
         * man bp_mapout(9F)
         */
        bp_mapout(bp);
        pageio_done(bp);

        if (err)
            break;

        /*
         *  ページリストの配列(plarr)を初期化する
         */
        pvn_plist_init(pp, plarr, plsz, off, io_len, rw);
        break;
    } while (1);

    /*
     * 通常は pageio_done() から呼ばれるので通常
     * pvn_read_done はエラーの時だけで良いらしい。
     */
    if (err) {
        if (pp != NULL)
            pvn_read_done(pp, B_ERROR);
    }
    DEBUG_PRINT((CE_CONT, "iumfs_getapage: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_putapage()  VNODE オペレーション
 *
 * iumfs_putpage() から呼ばれる。
 * ページサイズ単位に書き込み処理を行う。
 * 実際には iumfscntl デバイスドライバに WRITE_REQUEST を投げる。
 *
 *************************************************************************/
int
iumfs_putapage(vnode_t *vp, page_t *pp, u_offset_t *offp, size_t *lenp,
        int flags, struct cred *cr)
{
    size_t io_len = 0;
    u_offset_t io_off = 0;
    int err = 0;
    struct buf *bp = NULL;
    iumnode_t *inp;

    DEBUG_PRINT((CE_CONT, "iumfs_putapage is called\n"));
#ifdef DEBUG
    if (offp)
        DEBUG_PRINT((CE_CONT, "iumfs_putapage: *offp=%d", *offp));
    if (lenp)
        DEBUG_PRINT((CE_CONT, "iumfs_putapage: *lenp=%d\n", *lenp));
#endif // ifdef DEBUG
    DEBUG_PRINT((CE_CONT, "iumfs_putapage: vnode=%p", vp));

    // ファイルシステム型依存のノード構造体を得る
    inp = VNODE2IUMNODE(vp);

    // TODO: 外出ししても良いかもしれないブロック
    do {
        /*
         * addr で指定されたアドレス範囲から、指定された vnode のオフセットとサイズ
         * に適合する連続したページブロックを見つける。
         * io_off, io_len は戻り値で実際に見つかった連続したダーティーページ郡の
         * vnode に対するオフセットと長さ。
         * 現在第 6 引数を PAGESIZE にしてるので、io_len が PAGESIZE より大きくな
         * ることはない。
         */
        pp = pvn_write_kluster(vp, pp, &io_off, &io_len, pp->p_offset,
                PAGESIZE, flags);
        DEBUG_PRINT((CE_CONT, "iumfs_putapage: pvn_write_kluster succeeded \
                    io_off=%d,io_len=%d\n", io_off, io_len));

        /*
         * デーモンに対して書き込み要求するサイズを調整。
         * iumfs_write() にて設定されたファイルサイズ以上に書き込むことはしない
         */
        if (io_off + io_len > inp->fsize) {
            io_len = inp->fsize - io_off;
            DEBUG_PRINT((CE_CONT, "iumfs_putapage: shorten io_len to %d\n", io_len));
        }

        if(io_len <= 0) {
            cmn_err(CE_WARN, "iumfs_putapage: io_len <= 0\n");
            err = EINVAL;
            break;
        }

        /*
         * buf 構造体を確保し、初期化する。
         *   ..bp->b_bcount = io_len;
         *   ..bp->b_bufsize = io_len;
         *   ..bp->b_vp = vp など。
         */
        bp = pageio_setup(pp, io_len, vp, B_WRITE | flags);

        /*
         * block（DEV_BSIZE）数から byte 数へ
         */
        bp->b_lblkno = lbtodb(io_off); // 512 で割った数を計算？
#ifdef SOL10
        /*
         * solaris 9 の buf 構造体には以下のメンバーは含まれない。
         * これらのメンバーには何の意味が・・？ 一応セット
         */
        bp->b_file = vp; // vnode
        bp->b_offset = (offset_t) io_off; // vnode offset
#endif

        /*
         * カーネルの仮想アドレス空間にアドレスを確保し、ページの
         * リストにマップする。確保したアドレスはbp->b_un.b_addr
         * にセットする。man bp_mapin(9F)
         */
        bp_mapin(bp);

        /*
         * ユーザモードデーモンへリクエストを投げる。
         */
        err = iumfs_request_write(bp, vp);

        /*
         * man bp_mapout(9F)
         */
        bp_mapout(bp);
        pageio_done(bp);

    } while (0);

    /*
     * read と違い同期 write の場合 pvn_write_done は常に自分で呼び出す必要がある。
     * （非同期の場合 pageio_done() のなかで自動的によばれるらしい）
     */
    if (err) {
        flags |= B_ERROR;
    }
    pvn_write_done(pp, flags | B_WRITE);

    if (offp)
        *offp = io_off;
    if (lenp)
        *lenp = io_len;

    DEBUG_PRINT((CE_CONT, "iumfs_putapage: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_write()  VNODE オペレーション
 *
 * write(2) システムコールに対応する。
 *
 *************************************************************************/
static int
iumfs_write(vnode_t *vp, struct uio *uiop, int ioflag, struct cred *cr)
{
    iumnode_t *inp;
    int err = 0;
    caddr_t base = NULL; // vnode とマップされたカーネル空間のアドレス
    offset_t mapoff = 0; // block の境界線までのオフセット値
    offset_t reloff = 0; // block の境界線からの相対的なオフセット値
    size_t mapsz = 0; // マップするサイズ
    uint_t flags = 0;
    int pagecreated = 0; // 新しいページを作ったかどうか
    int pagecreateva = 0; // 新しいページがpage_create_va()で作られたかどうか。

    offset_t off = 0; // uiomove 前のオフセット値
    size_t wsize = 0; // uiomove で書き込まれたサイズ
    offset_t poff = 0; // ページ境界線までのオフセット値    
    offset_t preloff = 0; // ページ境界からの相対的なオフセット値
    size_t maprest = 0; // マップサイズ内でまだ uiomove されていないサイズ。
    size_t psz = 0; // ページ内の要求されたサイズ
    caddr_t uiomvbase = NULL; // uiomove を行うアドレス

    DEBUG_PRINT((CE_CONT, "iumfs_write is called\n"));

    // ファイルシステム型依存のノード構造体を得る
    inp = VNODE2IUMNODE(vp);

    mutex_enter(&(inp->i_lock));

    if (!(inp->vattr.va_type | VREG)) {
        DEBUG_PRINT((CE_CONT, "iumfs_write: file is not regurar file\n"));
        err = ENOTSUP;
        goto out;
    }

    /*
     * ファイルへのアクセスの可否を検証する
     */
    if((err = iumfs_access(vp, VWRITE, ioflag, cr))){
        DEBUG_PRINT((CE_CONT, "iumfs_write: file access denied.\n"));            
        VN_RELE(vp);
        goto out;
    }    

    /*
     * O_APPEND フラグ付きで open(2) されている場合。
     * offset 値をファイルサイズまで調整。
     */ 
    if (ioflag & FAPPEND){
        uiop->uio_loffset = inp->fsize;
    }
    
    do {
        /*
         * uio 構造体の loffset/resid と各ローカル変数の関係 
         * (MAXBSIZE はファイルシステムを問わずシステム決められた最大ブロックサイズ。
         * 8192bytes)
         *   
         *   | MAXBSIZE | MAXBSIZE | MAXBSIZE | MAXBSIZE | MAXBSIZE | MAXBSIZE 
         *  -|----------|----------|----------|----------|----------|----------
         *  -----File size ----------------------------------------------->|
         *  ------------ uiop->loffset ----------->|<-- uiop->resid--->|
         *  ------------- mapoff ------------>|
         *                                    |<-->|<--->|
         *                                    reloff mapszx
         *
         *   一回の segmap_getmapflt でマップ できるのは MAXBSIZE 分だけなので、
         *   uiop->resid 分だけマップするために繰り返し segmap_getmapflt を呼ぶ
         *   必要がある。
         */
        /*
         * NOTE: PAGESIZE 倍数値 uiop->loffset & ~(PAGESIZE - 1)。
         */
        if (uiop->uio_loffset > inp->fsize) {
            DEBUG_PRINT((CE_CONT, "iumfs_write: offset(%d) exceeds file size(%u)\n", uiop->uio_loffset, inp->fsize));            
        }

        mapoff = uiop->uio_loffset & MAXBMASK;
        reloff = uiop->uio_loffset & MAXBOFFSET;
        mapsz = MAXBSIZE - reloff;

        /*
         * もし resid が mapsz より小さければ（つまり最後のマッピング処理の場合）
         * resid を mapsz とする。
         */
        mapsz = MIN(mapsz, uiop->uio_resid);
        DEBUG_PRINT((CE_CONT, "iumfs_write: uio_loffset=%d\n", uiop->uio_loffset));
        DEBUG_PRINT((CE_CONT, "iumfs_write: uio_resid=%d\n", uiop->uio_resid));
        DEBUG_PRINT((CE_CONT, "iumfs_write: mapoff=%d\n", mapoff));
        DEBUG_PRINT((CE_CONT, "iumfs_write: reloff=%d\n", reloff));
        DEBUG_PRINT((CE_CONT, "iumfs_write: mapsz=%d\n", mapsz));

        /*
         * ファイルの指定領域とカーネルアドレス空間のマップを行う。
         * segmap_getmapfltの第 5 引数の forcefault を 1 にすると、
         * segmap_getmapfltの中でページフォルトで発生し iumfs_getpage が呼ばれる。
         * もし 0 とすると uiomove() が呼ばれてページフォルトが発生した段階で初めて
         * iumfs_getpage 呼ばれることになる。
         * リターン値はマップされたカーネルアドレス空間のアドレス。
         */
        DEBUG_PRINT((CE_CONT, "iumfs_write: calling segmap_getmapflt\n"));
        base = segmap_getmapflt(segkmap, vp, mapoff + reloff, mapsz, 0, S_WRITE);
        if (base == NULL) {
            cmn_err(CE_WARN, "iumfs_write: segmap_getmapflt failed\n");
            err = ENOMEM;
            goto out;
        }
        DEBUG_PRINT((CE_CONT, "iumfs_write: segmap_getmapflt succeeded \n"));

        /*
         * PAGESIZE 単位で uiomove を行う。
         * -------|-----------------------------MAXBSIZE--------------------|
         * -------|-------PAGESIZE----|----PAGESIZE-----|-----PAGESIZE------|
         * -poff->|       
         *        |<-preloff->|<-psz->|
         *                    |<--------- mapreset ----------->|
         */
        /*
         * base は  MAXBSIZE アラインのアドレスのはず。そこから実際に書き込む
         * オフセット(reloff)を足す。
         */
        uiomvbase = base + reloff;
        maprest = mapsz;
        while (maprest > 0) {
            off = uiop->uio_loffset;
            preloff = uiop->uio_loffset & PAGEOFFSET;
            poff = uiop->uio_loffset & PAGEMASK;
            psz = PAGESIZE - preloff;
            psz = MIN(psz, maprest);
            pagecreated = 0;
            pagecreateva = 0;

            DEBUG_PRINT((CE_CONT, "iumfs_write: uiop->uio_loffset=%d,preloff=%d\n", uiop->uio_loffset, preloff));
            DEBUG_PRINT((CE_CONT, "iumfs_write: maprest=%d\n", maprest));
            DEBUG_PRINT((CE_CONT, "iumfs_write: poff=%d\n", poff));
            DEBUG_PRINT((CE_CONT, "iumfs_write: psz=%d\n", psz));

            /*
             * 以下のいずれかの場合新しいページを作成
             * 1) 要求されているオフセット値のページ境界線のサイズがファイルサイズ
             *    より大きい場合。（つまりホントにまだ存在しないページ上のデータを
             *    要求されている場合）
             * 2) オフセットが0でかつ、サイズがページサイズもしくはオフセット＋
             *    サイズがファイルサイズ以上だった場合。
             *    （つまり、元データの読み込みが必要ない場合。）
             *     これによって、不要なページデータの取得(=getpage())を防ぐ。
             */
            if ((poff > inp->fsize) ||
                    (preloff == 0 && (psz == PAGESIZE ||
                    (uiop->uio_loffset + psz > inp->fsize)))) {
                DEBUG_PRINT((CE_CONT, "iumfs_write: calling segmap_pagecreate\n"));

#ifdef FORCE_LOCK_ON_PAGECREATE
                /*
                 * 最後の引数を1にしてロックをかならず取得してあとで  segmap_fault
                 * でロックをはずす。(こうしないと問題があるケースがあるらしい・・）
                 * これは page_create_va() でとられたページも unlock するので、
                 * あえて segmap_pageunlock を呼ぶ必要がない.
                 */
                pagecreateva = segmap_pagecreate(segkmap, uiomvbase, psz, 1);
#else                
                pagecreateva = segmap_pagecreate(segkmap, uiomvbase, psz, 0);
#endif
                pagecreated = 1;                
                DEBUG_PRINT((CE_CONT, "iumfs_write: new page created\n"));
            }
            /*
             * ユーザ空間のデータをマップしたアドレスにコピーする。
             * もし、この時点で pagefault が発生したら VOP_GETPAGE ルーチン
             * （iumfs_getpage）が呼ばれ、ユーザモードデーモンにデータの取得
             * を依頼する。
             * 万一 uiomove が失敗したらマップをリリースし、page を全て無効にする。
             */
            DEBUG_PRINT((CE_CONT, "iumfs_write: calling uiomove\n"));
            if((err = uiomove(uiomvbase, psz, UIO_WRITE, uiop))){
                cmn_err(CE_WARN, "iumfs_write: uiomove failed (%d)\n", err);
                DEBUG_PRINT((CE_CONT, "iumfs_write: calling segmap_release with SM_INVAL\n"));
                segmap_release(segkmap, base, SM_INVAL);
                goto out;
            }
            wsize = uiop->uio_loffset - off;
            DEBUG_PRINT((CE_CONT, "iumfs_write: copyin %d bytes of data \n", wsize));

            /*
             * 作成したページの未使用の領域を 0 で埋める。
             */
            if (pagecreated) {
                if (uiop->uio_loffset & PAGEOFFSET || wsize == 0) {
                    (void) kzero(uiomvbase + wsize, PAGESIZE - wsize);
                }
#ifdef FORCE_LOCK_ON_PAGECREATE
                // segmap_fault によって unlock を行う。
                DEBUG_PRINT((CE_CONT, "iumfs_write: calling segmap_fault\n"));
                err = segmap_fault(kas.a_hat, segkmap, uiomvbase, wsize,
                        F_SOFTUNLOCK, S_WRITE);
                if (err != 0) {
                    cmn_err(CE_WARN, "iumfs_write: segmap_fault failed (%d)\n", err);
                    DEBUG_PRINT((CE_CONT, "iumfs_write: calling segmap_release with SM_INVAL\n"));
                    segmap_release(segkmap, base, SM_INVAL);                
                    goto out;
                }
#else            
                if (pagecreateva) {                
                    // page_create_va() よって作られたページは unlock しなければならない。
                    segmap_pageunlock(segkmap, uiomvbase,  wsize, S_WRITE);
                }
#endif
            }
            
            maprest -= wsize;
            uiomvbase += wsize;
            /*
             * ここで vnode ひもづいた iumfsnode がもっているファイルサイズを変更
             * する。これによって後で iumfs_putapage() が最終ページのどこまでデータ
             * を書いていいか判断することができる。（ファイルサイズは超えない）
             * さもないと、ファイルサイズはページサイズ単位になってしまい、後ろのほう
             * はゴミだらけになってしまう。
             * ただ、ファイルサイズを変える場所としてここが適切かどうかは疑問がある
             */
            if (uiop->uio_loffset > inp->fsize) {
                inp->fsize = uiop->uio_loffset;
                DEBUG_PRINT((CE_CONT, "iumfs_write: file size is changed to %u\n", inp->fsize));
            }
        }

        /*
         * マッピングを解放する。フリーリストに追加される。
         * SM_WRITE フラグをつけ、write back を強制する。
         * これにより segmap_release の中で iumfs_putpage が呼ばれ、putpage の
         * 処理が終わるまで戻らない。（同期書き込み）
         */
        flags |= SM_WRITE; // write back を強制する。
        DEBUG_PRINT((CE_CONT, "iumfs_write: calling segmap_release\n"));        
        err = segmap_release(segkmap, base, flags);
        if (err != SUCCESS) {
            DEBUG_PRINT((CE_CONT, "iumfs_write: segmap_release failed (%d)\n", err));
            goto out;
        }
        DEBUG_PRINT((CE_CONT, "iumfs_write: segmap_release succeeded \n"));
    } while (uiop->uio_resid > 0);

    inp->vattr.va_mtime = iumfs_get_current_time();
out:
    inp->vattr.va_atime = iumfs_get_current_time();
    mutex_exit(&(inp->i_lock));
    DEBUG_PRINT((CE_CONT, "iumfs_write: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_setattr()  VNODE オペレーション
 *
 * SETATTR ルーチン。サポートされていない。が、正常リターン。
 *
 *************************************************************************/
static int
iumfs_setattr(vnode_t *vp, vattr_t *vap, int flags, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_setattr is called\n"));

    return (0);
}

/************************************************************************
 * iumfs_create()  VNODE オペレーション
 *
 * creat(2) システムコールに対応する。
 *
 *************************************************************************/
static int
iumfs_create(vnode_t *dirvp, char *name, vattr_t *vap, vcexcl_t excl,
        int mode, vnode_t **vpp, struct cred *cr, int flag)
{
    vfs_t *vfsp;
    int err = SUCCESS;
    vnode_t *vp = NULL;
    iumnode_t *newinp, *inp;
    iumnode_t *dirinp; // ディレクトリのファイルシステム依存ノード構造体    

    DEBUG_PRINT((CE_CONT, "iumfs_create is called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_create: name=%s,flag = 0x%x\n", name, flag));
    vfsp = VNODE2VFS(dirvp);
    dirinp = VNODE2IUMNODE(dirvp);    

    /*
     * VOP_LOOKUP の引数の rdir の目的が不明・・・iumfs では使っていないが？
     * 一応 NULL を渡しておく。dirvp が他のファイルシステム上のディレクトリだった
     * ら問題がでる(panic!)かもしれない。
     */
    err = iumfs_lookup(dirvp, name, &vp, NULL, flag, NULL, cr);
    if (!err) {
        /*
         * 既存のファイルが見つかった！ vnode が存在する。
         * VOP_LOOKUP によって参照カウントはインクリメントされている.
         */
        DEBUG_PRINT((CE_CONT, "iumfs_create: file already exists.\n"));
        inp = VNODE2IUMNODE(vp);

        if (excl == EXCL) {
            /*
             * O_EXCL フラグつきで open された模様. EEXIT エラーを返す.
             */
            err = EEXIST;
            goto out;
        }
        
        if((err = iumfs_access(vp, mode, flag, cr))){
            DEBUG_PRINT((CE_CONT, "iumfs_create: file access denied.\n"));            
            VN_RELE(vp);
            goto out;
        }

        /*
         * もし va_mask の AT_SIZE フラグが立っており、va_size が 0 で
         * あった場合には、ファイルサイズを 0 にする。
         * open(2) に O_TRUNC フラグが渡されたら、この関数の flag には FTRUNC
         * フラグが立っていると思っていたが、違った。
         * HDFS の場合には TRUNC はできないので iumfs_access で ENOTSUP が
         * 返され、ここには到達しないはず。
         */
        if ((vap->va_mask & AT_SIZE) && (vap->va_size == 0)) {
            // ファイルの vnode の属性情報をセット            
            mutex_enter(&(inp->i_lock));
            inp->vattr.va_size = 0; // inp->fsize と等価
            inp->vattr.va_nblocks = 0;
            inp->vattr.va_atime = iumfs_get_current_time();
            mutex_exit(&(inp->i_lock));
        }

        // 引数として渡されたポインタに新しい vnode のアドレスをセット
        *vpp = vp;
        goto out;
    }

    /*
     * 既存のファイルは無い。 
     * ユーザモードデーモンに新規ファイルの作成を依頼し、成功したら vnode
     * を作成する。
     */
    if ((err = iumfs_request_create(dirvp, name, vap)) != 0) {
        DEBUG_PRINT((CE_CONT, "iumfs_create: cannot create\"%s\"\n", name));
        // サーバ上でファイルを作成できなかった・・エラーを返す
        goto out;
    }

    /*
     * 新しいファイルの vnode を作成
     */
    if((err = iumfs_alloc_node(vfsp, &vp, 0, VREG, 0))){
        cmn_err(CE_CONT, "iumfs_create: cannot allocate vnode for file\n");
        goto out;
    }

    newinp = VNODE2IUMNODE(vp);

    /*
     * ファイルシステムルートからのパス名を得る。
     * もし親ディレクトリがファイルシステムルートだったら、余計な「/」はつけない。
     */    
    if (ISROOT(dirinp->pathname))
        snprintf(newinp->pathname, IUMFS_MAXPATHLEN, "/%s", name);        
    else
        snprintf(newinp->pathname, IUMFS_MAXPATHLEN, "%s/%s", dirinp->pathname,name);            
    
    DEBUG_PRINT((CE_CONT, "iumfs_create: allocated new node \"%s\"\n", newinp->pathname));    

    // ファイルの vnode の属性情報をセット
    //newinp->vattr.va_mode     = 0100644; //0040644; //00644;
    //newinp->vattr.va_size     = 0;
    //newinp->vattr.va_nblocks  = 0;

    /*
     * ディレクトリにこのファイルのエントリを追加する。
     */
    if (iumfs_add_entry_to_dir(dirvp, name, strlen(name),
            newinp->vattr.va_nodeid) < 0) {
        cmn_err(CE_CONT, "iumfs_create: cannot add new entry to directory\n");
        err = ENOSPC;
        goto out;
    }

    // 引数として渡されたポインタに新しい vnode のアドレスをセット
    *vpp = vp;

    // vnode の参照カウントを増やす
    VN_HOLD(vp);

out:
    // エラー時は確保したリソースを解放する。
    if (err && vp != NULL){
        iumfs_free_node(vp, cr);
        *vpp = NULL;
    }
    DEBUG_PRINT((CE_CONT, "iumfs_create: return(%d)\n", err));
    return (err);
}

/************************************************************************
 * iumfs_remove()  VNODE オペレーション
 *
 *************************************************************************/
static int
iumfs_remove(vnode_t *pdirvp, char *name, struct cred *cr)
{
    vnode_t        *vp = NULL; // 削除するファイルの vnode 構造体
    iumnode_t      *pdirinp;   // 親ディレクトリの iumnode 構造体    
    ino_t    nodeid  = 0;     // 削除するファイルのノード番号
    int             namelen;
    int             err = SUCCESS;
    iumfs_t        *iumfsp;
    
    DEBUG_PRINT((CE_CONT,"iumfs_remove is called\n"));

    namelen = strlen(name);
    iumfsp = VNODE2IUMFS(pdirvp);

    DEBUG_PRINT((CE_CONT,"iumfs_remove: removing \"%s\"\n", name));    

    pdirinp   = VNODE2IUMNODE(pdirvp);

    nodeid = iumfs_find_nodeid_by_name(pdirinp, name);
    
    if(nodeid == 0){
        /*
         * 親ディレクトリ中に該当するファイルが見つからなかった。
         */
        DEBUG_PRINT((CE_CONT,"iumfs_remove: can't find file \"%s\"\n", name));
        err = ENOENT;
        goto out;
    }

    vp = iumfs_find_vnode_by_nodeid(iumfsp, nodeid);
    if(vp == NULL){
        DEBUG_PRINT((CE_CONT,"iumfs_remove: can't find file \"%s\"\n", name));
        err = ENOENT;
        goto out;
    }

    /*
     * ノードのタイプが VREG じゃ無かったらエラーを返す
     */
    if(!(vp->v_type & VREG)){
        DEBUG_PRINT((CE_CONT,"iumfs_remove: vnode is not a regular file.\n"));
        err = ENOTSUP;
        goto out;
    }

    /*
     * ユーザモードデーモンに削除を依頼
     */
    if((err = iumfs_request_remove(vp))){
        goto out;
    }
    
    /*
     * 親ディレクトリからエントリを削除
     */
    iumfs_remove_entry_from_dir(pdirvp, name);

    /*
     * 最後にディレクトリの vnode の参照カウントを減らす。
     * この vnode を参照中の人がいるかもしれないので（たとえば shell の
     * カレントディレクトリ）、ここでは free はしない。
     * 参照数が 1 になった段階で iumfs_inactive() が呼ばれ、iumfs_inactive()
     * から free される。
     */
    VN_RELE(vp); // vnode 作成時に増加された参照カウント分を減らす。

  out:
    /*
     * 上の iumfs_find_vnode_by_nodeid() で増やされたの参照カウント分を減らす。 
     */
    if (vp != NULL)
        VN_RELE(vp);
    
    DEBUG_PRINT((CE_CONT, "iumfs_remove: return(%d)\n", err));
    return(err); 
}

/************************************************************************
 * iumfs_rename()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_rename(vnode_t *sdvp, char *snm, vnode_t *tdvp, char *tnm,
        struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_rename is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_mkdir()  VNODE オペレーション
 *
 * mkdir(2) システムコールに対応する。
 * 指定された名前の新規ディレクトリを作成する。
 *
 *************************************************************************/
static int
iumfs_mkdir(vnode_t *dirvp, char *name, vattr_t *vap, vnode_t **vpp,
        struct cred *cr)
{
    int         err;
    vfs_t      *vfsp;
    vnode_t    *vp = NULL;
    iumnode_t  *inp;
    int         namelen;
    
    DEBUG_PRINT((CE_CONT,"iumfs_mkdir is called\n"));

    /*
     * まずは、渡されたディレクトリ名の長さをチェック
     */
    namelen = strlen(name);
    if (namelen > MAXNAMLEN)
        return(ENAMETOOLONG);

    vfsp = VNODE2VFS(dirvp);

    /*
     * ユーザモードデーモンにディレクトリの作成を依頼する
     */
    if ((err = iumfs_request_mkdir(dirvp, name, vap)) != 0) {        
        DEBUG_PRINT((CE_CONT, "iumfs_mkdir: failed to create directory \"%s\" on server\n", name));        
        goto out;
    }
    
    /*
     * 指定された名前のディレクトリを作成する。
     */
    if ((err = iumfs_make_directory_with_name(vfsp, vpp, dirvp, cr, name, 0))
            != SUCCESS){
        cmn_err(CE_CONT, "iumfs_mkdir: failed to create directory \"%s\"\n", name);
        goto out;
    }

    vp = *vpp;
    inp = VNODE2IUMNODE(vp);

    /*
     * 親ディレクトリ(dirvp) に新しく作成したディレクトリのエントリを追加する。
     */
    if (iumfs_add_entry_to_dir(dirvp, name, namelen, inp->vattr.va_nodeid) < 0 ){
        cmn_err(CE_CONT, "iumfs_mkdir: cannot add \"%s\" to directory\n", name);
        err = ENOSPC;
        goto out;
    }

    /*
     * vnode のポインタを返すので、参照数を増加させる
     */
    VN_HOLD(vp);
    
  out:
    if(err && vp != NULL)
        iumfs_free_node(vp, cr);
    
    DEBUG_PRINT((CE_CONT, "iumfs_mkdir: return(%d)\n", err));    
    return(err);   
}

/************************************************************************
 * iumfs_rmdir()  VNODE オペレーション
 *
 * rmdir(2) システムコールに対応する。
 * 
 *************************************************************************/
static int
iumfs_rmdir(vnode_t *pdirvp, char *name, vnode_t *cdirvp, struct cred *cr)
{
    vnode_t        *vp = NULL; // 削除するディレクトリの vnode 構造体
    iumnode_t      *pdirinp;   // 親ディレクトリの iumnode 構造体    
    ino_t    dir_nodeid  = 0; // 削除するディレクトリのノード番号
    int             namelen;
    int             err = SUCCESS;
    iumfs_t        *iumfsp;
    
    DEBUG_PRINT((CE_CONT,"iumfs_rmdir is called\n"));

    namelen = strlen(name);
    iumfsp = VNODE2IUMFS(pdirvp);

    DEBUG_PRINT((CE_CONT,"iumfs_rmdir: removing \"%s\"\n", name));    

    /*
     * 削除しようとしているのが、「.」や、「..」だったらエラーを返す
     */
    if ( (namelen == 1 && strcmp(name, ".") == 0) ||
            (namelen == 2 && strcmp(name, "..") == 0)) {
        DEBUG_PRINT((CE_CONT,"iumfs_rmdir: cannot remove \".\" or \"..\"\n"));
        err = EINVAL;
        goto out;
    }

    pdirinp   = VNODE2IUMNODE(pdirvp);

    dir_nodeid = iumfs_find_nodeid_by_name(pdirinp, name);
    
    if(dir_nodeid == 0){
        /*
         * 親ディレクトリ中に該当するディレクトリが見つからなかった。
         */
        DEBUG_PRINT((CE_CONT,"iumfs_rmdir: can't find directory \"%s\" under %s\n", name, pdirinp->pathname));
        err = ENOENT;
        goto out;
    }

    vp = iumfs_find_vnode_by_nodeid(iumfsp, dir_nodeid);
    if(vp == NULL){
        DEBUG_PRINT((CE_CONT,"iumfs_rmdir: can't find directory \"%s\"\n", name));
        err = ENOENT;
        goto out;
    }

    /*
     * ノードのタイプが VDIR じゃ無かったらエラーを返す
     */
    if(!(vp->v_type & VDIR)){
        DEBUG_PRINT((CE_CONT,"iumfs_rmdir: vnode is not a directory.\n"));
        err = ENOTDIR;
        goto out;
    }

    // カレントディレクトリを削除しようとしていないかチェック
    if (vp == cdirvp){
        err = EINVAL;
        goto out;
    }

    // 削除しようとしているディレクトリの中身をチェック
    if ( iumfs_dir_is_empty(vp) == 0){
        //ディレクトリは空ではない。
        DEBUG_PRINT((CE_CONT,"iumfs_rmdir: directory \"%s\" is not empty\n", name));
        err = ENOTEMPTY;
        goto out;
    }

    /*
     * ユーザモードデーモンにディレクトリの削除を依頼
     */
    if((err = iumfs_request_rmdir(vp)) != 0 ){
        DEBUG_PRINT((CE_CONT, "iumfs_rmdir: failed to remove directory \"%s\" on server\n", name));                
        goto out;
    }    

    /*
     * TODO: vp->v_count（参照カウント）をチェックしなければ・・
     * iumfs_inactive() を活用か？ iumfs_free_node() は v_count のチェックはし
     * ていない。一貫性のある vnode の削除方針を決めなければ・・
     */

    /*
     * TODO: ロックの取り方の検討
     * 上の、iumfs_dir_is_empty() と、下の iumfs_remove_entry_from_dir() の処理の
     * 間に新しいエントリが追加されると有無を言わさずそのエントリが削除されて
     * しまう・・ここでディレクトリの iumnode のロックを取ればよいのだが、
     * ここではロックは取りたくない。
     * (デッドロックの発生を防ぐため、iumnode ロックを取った状態で、他の
     * iumfs の関数を呼び出さないような方針にしているので）
     */

    /*
     * 親ディレクトリからエントリを削除
     */
    iumfs_remove_entry_from_dir(pdirvp, name);

    /*
     * 最後にディレクトリの vnode の参照カウントを減らす。
     * この vnode を参照中の人がいるかもしれないので（たとえば shell の
     * カレントディレクトリ）、ここでは free はしない。
     * 参照数が 1 になった段階で iumfs_inactive() が呼ばれ、iumfs_inactive()
     * から free される。
     */
    VN_RELE(vp); //vnode 作成時に増加された参照カウント分を減らす    
    VN_RELE(vp); //iumfs_find_vnode_by_nodeid()で増やされたの参照カウント分を減らす

  out:
    /*
     * もし削除しようとしていたディレクトリの vnode のポインタを得ていたら、
     * vnode の参照数を減らす。
     */
    if (err && vp != NULL)
        VN_RELE(vp);

    DEBUG_PRINT((CE_CONT, "iumfs_rmdir: return(%d)\n", err));
    return(err);
}

#ifndef SOL10
/*
 *  この ifndef 内の関数は現在は未使用。
 */

/************************************************************************
 * iumfs_ioctl()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_ioctl(vnode_t *vp, int cmd, intptr_t arg, int flag, struct cred *cr,
        int *rvalp){
    DEBUG_PRINT((CE_CONT, "iumfs_ioctl is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_setfl()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_setfl(vnode_t *vp, int oflags, int nflags, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_setfl is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_link()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_link(vnode_t *tdvp, vnode_t *svp, char *tnm, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_link is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_symlink()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_symlink(vnode_t *dvp, char *linkname, vattr_t *vap, char *target,
        struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_symlink is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_readlink()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_readlink(vnode_t *vp, struct uio *uiop, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_readlink is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_fid()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_fid(vnode_t *vp, struct fid *fidp)
{
    DEBUG_PRINT((CE_CONT, "iumfs_fid is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_rwlock()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static void
iumfs_rwlock(vnode_t *vp, int write_lock)
{
    DEBUG_PRINT((CE_CONT, "iumfs_rwlock is called\n"));

    return;
}

/************************************************************************
 * iumfs_rwunlock()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static void
iumfs_rwunlock(vnode_t *vp, int write_lock)
{
    DEBUG_PRINT((CE_CONT, "iumfs_rwunlock is called\n"));

    return;
}

/************************************************************************
 * iumfs_frlock()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_frlock(vnode_t *vp, int cmd, struct flock64 *bfp, int flag,
        offset_t offset, struct flk_callback *callback, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_frlock is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_space()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_space(vnode_t *vp, int cmd, struct flock64 *bfp, int flag,
        offset_t offset, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_space is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_realvp()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_realvp(vnode_t *vp, vnode_t **vpp)
{
    DEBUG_PRINT((CE_CONT, "iumfs_realvp is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_addmap()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_addmap(vnode_t *vp, offset_t off, struct as *as, caddr_t addr,
        size_t len, uchar_t prot, uchar_t maxprot, uint_t flags, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_addmap is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_delmap()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_delmap(vnode_t *vp, offset_t off, struct as *as, caddr_t addr, size_t len,
        uint_t prot, uint_t maxprot, uint_t flags, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_delmap is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_poll()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_poll(vnode_t *vp, short ev, int any, short *revp, struct pollhead **phpp)
{
    DEBUG_PRINT((CE_CONT, "iumfs_poll is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_dump()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_dump(vnode_t *vp, caddr_t addr, int lbdn, int dblks)
{

    DEBUG_PRINT((CE_CONT, "iumfs_dump is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_pathconf()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_pathconf(vnode_t *vp, int cmd, ulong_t *valp, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_pathconf is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_pageio()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_pageio(vnode_t *vp, struct page *pp, u_offset_t io_off, size_t io_len,
        int flags, struct cred *cr) {
    DEBUG_PRINT((CE_CONT, "iumfs_pageio is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_dumpctl()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_dumpctl(vnode_t *vp, int action, int *blkp)
{
    DEBUG_PRINT((CE_CONT, "iumfs_dumpctl is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_dispose()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static void
iumfs_dispose(vnode_t *vp, struct page *pp, int flag, int dn, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_dispose is called\n"));

    return;
}

/************************************************************************
 * iumfs_setsecattr()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_setsecattr(vnode_t *vp, vsecattr_t *vsap, int flag, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_setsecattr is called\n"));

    return (ENOTSUP);
}

/************************************************************************
 * iumfs_getsecattr()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_getsecattr(vnode_t *vp, vsecattr_t *vsap, int flag, struct cred *cr)
{
    DEBUG_PRINT((CE_CONT, "iumfs_getsecattr is called\n"));

    //    return(ENOTSUP);
    return (SUCCESS);
}

/************************************************************************
 * iumfs_shrlock()  VNODE オペレーション
 *
 * サポートしていない
 *************************************************************************/
static int
iumfs_shrlock(vnode_t *vp, int cmd, struct shrlock *shr, int flag)
{
    DEBUG_PRINT((CE_CONT, "iumfs_shrlock is called\n"));

    return (ENOTSUP);
}

#endif // #ifndef SOL10
