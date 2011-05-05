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
 * iumfs_vfs
 *
 * node operations for IUMFS filesystem.
 *   
 **************************************************************/
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

/************************************************************************
 * iumfs_alloc_node()
 *
 *   新しい vnode 及び iumnode を確保する。
 *
 * 引数:
 *     vfsp   : vfs 構造体
 *     vpp    : 呼び出し側から渡された vnode 構造体のポインタのアドレス
 *     flag   : 作成する vnode のフラグ(VROOT, VISSWAP 等)
 *     type   : 作成する vnode のタイプ(VDIR, VREG 等)
 *     nodeid : 作成する vnode のノード番号(0の場合自動割当)
 *
 * 戻り値
 *    正常時   : SUCCESS(=0)
 *    エラー時 : 0 以外
 *
 ************************************************************************/
int
iumfs_alloc_node(vfs_t *vfsp, vnode_t **nvpp, uint_t flag, enum vtype type, ino_t nodeid)
{
    vnode_t *vp;
    iumnode_t *inp;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体

    DEBUG_PRINT((CE_CONT, "iumfs_alloc_node is called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_alloc_node: type=%d\n",type));
    
    iumfsp = VFS2IUMFS(vfsp);

    // vnode 構造体を確保
#ifdef SOL10
    // Solaris 10 では直接 vnode 構造体を alloc してはいけない。
    vp = vn_alloc(KM_NOSLEEP);
#else
    // Solaris 9 ではファイルシステム自身で vnode 構造体を alloc する。
    vp = (vnode_t *) kmem_zalloc(sizeof (vnode_t), KM_NOSLEEP);
#endif    

    //ファイルシステム型依存のノード情報(iumnode 構造体)を確保
    inp = (iumnode_t *) kmem_zalloc(sizeof (iumnode_t), KM_NOSLEEP);

    /*
     * どちらかでも確保できなかったら ENOMEM を返す
     */
    if (vp == NULL || inp == NULL) {
        cmn_err(CE_WARN, "iumfs_alloc_node: kmem_zalloc failed\n");
        if (vp != NULL)
#ifdef SOL10
            vn_free(vp);
#else        
            kmem_free(vp, sizeof (vnode_t));
#endif            
        if (inp != NULL)
            kmem_free(inp, sizeof (iumnode_t));
        DEBUG_PRINT((CE_CONT, "iumfs_alloc_node return(ENOMEM)\n"));
        return (ENOMEM);
    }

    DEBUG_PRINT((CE_CONT, "iumfs_alloc_node: allocated vnode = 0x%p\n", vp));

    /*
     * 確保した vnode を初期化
     * VN_INIT マクロの中で、v_count の初期値を 1 にセットする。
     * これによって、ファイルシステムの意図しないタイミングで iumfs_inactive()
     * が呼ばれてしまうのを防ぐ。
     */
    VN_INIT(vp, vfsp, type, 0);

    // ファイルシステム型依存の vnode 操作構造体のアドレスをセット
#ifdef SOL10
    vn_setops(vp, iumfs_vnodeops);
#else        
    vp->v_op = &iumfs_vnodeops;
#endif

    // v_flag にフラグをセット
    vp->v_flag &= flag;

    /*
     * 確保した iumnode を初期化 (IN_INIT マクロは使わない）
     */
    mutex_init(&(inp)->i_dlock, NULL, MUTEX_DEFAULT, NULL);
    inp->vattr.va_mask = AT_ALL;
    inp->vattr.va_uid = 0;
    inp->vattr.va_gid = 0;
    inp->vattr.va_blksize = BLOCKSIZE;
    inp->vattr.va_nlink = 1;
    inp->vattr.va_rdev = 0;
    rw_init(&(inp)->i_listlock,NULL,RW_DRIVER,NULL);
#ifdef SOL10
#else    
    inp->vattr.va_vcode = 1;
#endif
    /*
     * vattr の va_fsid は dev_t(=ulong_t), これに対して vfs の
     * vfs_fsid は int 型の配列(int[2])を含む構造体。
     * なので、iumfs_mount() でもとめたデバイス番号を入れておく。
     */
    inp->vattr.va_fsid = vfsp->vfs_dev;
    inp->vattr.va_type = type;
    inp->vattr.va_atime =      \
    inp->vattr.va_ctime =      \
    inp->vattr.va_mtime = iumfs_get_current_time();

    DEBUG_PRINT((CE_CONT, "iumfs_alloc_node: va_fsid = 0x%x\n", inp->vattr.va_fsid));

    /*
     * vnode に iumnode 構造体へのポインタをセット
     * 逆に、iumnode にも vnode 構造体へのポインタをセット
     */
    vp->v_data = (caddr_t) inp;
    inp->vnode = vp;

    /*
     * ノード番号（iノード番号）をセット。
     * もし指定されている場合はそれを使い、指定が無い場合には
     * 単純に１づつ増やしていく。
     */
    if( (inp->vattr.va_nodeid = nodeid) == 0) {
        mutex_enter(&(iumfsp->iumfs_lock));
        inp->vattr.va_nodeid = ++(iumfsp->iumfs_last_nodeid);
        mutex_exit(&(iumfsp->iumfs_lock));
    }

    DEBUG_PRINT((CE_CONT, "iumfs_alloc_node: new nodeid = %d \n", inp->vattr.va_nodeid));

    //新しい iumnode をノードのリンクリストに新規のノードを追加
    iumfs_add_node_to_list(vfsp, vp);

    // 渡された vnode 構造体のポインタに確保した vnode のアドレスをセット
    *nvpp = vp;
    DEBUG_PRINT((CE_CONT, "iumfs_alloc_node: return(%d)\n", SUCCESS));
    return (SUCCESS);
}

/************************************************************************
 * iumfs_free_node()
 *
 *   指定された vnode および iumnode を解放する
 *
 *     1. iumnode に関連づいたリソースを解放
 *     2. iumnode 構造体を解放
 *     3. vnode 構造体を解放
 *
 *   これが呼ばれるのは、iumfs_inactive() もしくは iumfs_unmount() 経由の
 *   iumfs_free_all_node() だけ。つまり、v_count が 1（未参照状態）である
 *   事が確かな場合だけ。
 *
 * 引数:
 *
 *     vp: 解放する vnode 構造体のポインタ
 *     cr    : システムコールを呼び出したユーザのクレデンシャル
 *
 * 戻り値:
 *     無し
 *
 ************************************************************************/
void
iumfs_free_node(vnode_t *vp, struct cred *cr)
{
    iumnode_t *inp; // ファイルシステム型依存のノード情報(iumnode構造体)
    vnode_t *rootvp; // ファイルシステムのルートディレクトリの vnode。
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    vfs_t *vfsp; // ファイルシステム構造体
    int err;

    DEBUG_PRINT((CE_CONT, "iumfs_free_node is called\n"));

    iumfsp = VNODE2IUMFS(vp);
    vfsp = VNODE2VFS(vp);
    inp = VNODE2IUMNODE(vp);

    DEBUG_PRINT((CE_CONT, "iumfs_free_node: vnode=%p, vp->v_count=%d\n", vp, vp->v_count));

    /*
     * 最初にノードリンクリストから iumnode をはずす。誰かが利用中(EBUSY)だったらリターン。
     * 仮にノードリストに入っていなかったとしても、（ありえないはずだが) vnode のフリーは行う。
     */
    if((err = iumfs_remove_node_from_list(vfsp, vp)) != 0){
        if (err == ENOENT)
            cmn_err(CE_CONT, "iumfs_free_node: can't find vnode in the list. Free it anyway.\n");
        else
            return;
    }

    // debug 用
    rootvp = VNODE2ROOT(vp);
    if (rootvp != NULL && VN_CMP(rootvp, vp) != 0) {
        DEBUG_PRINT((CE_CONT, "iumfs_free_node: rootvnode is being freed\n"));
        mutex_enter(&(iumfsp->iumfs_lock));
        iumfsp->rootvnode = NULL;
        mutex_exit(&(iumfsp->iumfs_lock));
    }

    /*
     * もし iumnode にデータ（ディレクトリエントリ等）
     * を含んでいたらそれらも解放する。
     */
    if (inp->data != NULL) {
        kmem_free(inp->data, inp->dlen);
    }

    /*
     * この vnode に関連した page を無効にする
     */
    err = pvn_vplist_dirty(vp, 0, iumfs_putapage, B_INVAL, cr);
    DEBUG_PRINT((CE_CONT, "iumfs_free_node: pvn_vplist_dirty returned with (%d)\n", err));

    // iumnode を解放
    mutex_destroy(&(inp)->i_dlock);
    rw_destroy(&(inp)->i_listlock);
    kmem_free(inp, sizeof (iumnode_t));

    // vnode を解放
#ifdef SOL10
    vn_free(vp);
#else
    mutex_destroy(&(vp)->v_lock);
    kmem_free(vp, sizeof (vnode_t));
#endif                
    DEBUG_PRINT((CE_CONT, "iumfs_free_node: return\n"));
    return;
}

/************************************************************************
 * iumfs_create_fs_root()
 *
 *   ファイルシステムのルートを作る。
 *   ファイルシステムのマウント時に iumfs_mount() から呼ばれる。
 *   
 *    o ルートディレクトリのをさす vnode の作成
 *    o ルートディレクトリのディレクトリエントリを作成
 *
 * 引数:
 *
 *     vfsp  : vfs 構造体
 *     vpp   : 呼び出し側から渡された vnode 構造体のポインタのアドレス
 *     mvp   : ディレクトリマウントポイントの vnode 構造体のポインタ
 *
 ************************************************************************/
int
iumfs_create_fs_root(vfs_t *vfsp, vnode_t **rootvpp, vnode_t *mvp,
        struct cred *cr)
{
    int err = SUCCESS;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    vnode_t *rootvp;
    iumnode_t *rootinp;

    DEBUG_PRINT((CE_CONT, "iumfs_create_fs_root is called\n"));

    iumfsp = VFS2IUMFS(vfsp);

    /*
     * ディレクトリを作成する。
     * iumfs_make_directory() の第３引数は、新規作成するディレクトリの親ディレクトリ
     * の vnode を指定するのだが、ここではマウントポイントのディレクトリの vnode を
     * 渡している。このため、マウント後のファイルシステムルートディレクトリの「..」
     * は実際の親ディレクトリの inode を示していない。実際問題 cd .. でも問題は
     * 発生しないので、あえてマウントポイントの親ディレクトリの情報を探すことはしない
     * でおく。
     */
    if ((err = iumfs_make_directory(vfsp, rootvpp, mvp, cr, 0)) != SUCCESS) {
        cmn_err(CE_CONT, "iumfs_create_fs_root: failed to create directory\n");
        DEBUG_PRINT((CE_CONT, "iumfs_create_fs_root: return(%d)\n", err));
        return (err);
    }
    rootvp = *rootvpp;

    /*
     * ファイルシステムルートのパス名を「/」にセット。
     * 以降、このディレクトリ配下のファイル、ディレクトリは
     * 「/」からの相対パスを pathname に持つことになる。
     */
    rootinp = VNODE2IUMNODE(rootvp);
    snprintf(rootinp->pathname, IUMFS_MAXPATHLEN, "/");

    /*
     * ファイルシステムのルートディレクトリの vnode をセット
     */
    iumfsp->rootvnode = rootvp;
    DEBUG_PRINT((CE_CONT, "iumfs_create_fs_root: return(%d)\n", SUCCESS));
    return (SUCCESS);
}

/*****************************************************************************
 * iumfs_add_iumnode_list()
 *
 * iumfs ファイルシステムプライベートデータ構造からリンクしている、
 * ファイルシステムのノードリストに指定された vnode を追加する。
 *
 *
 *  引数：
 *      vfsp   : vfs 構造体 
 *      newvp  : ノードリストに追加する vnode のポインタ
 *  
 * 戻り値：
 *          常に 0
 *****************************************************************************/
int
iumfs_add_node_to_list(vfs_t *vfsp, vnode_t *newvp)
{
    iumnode_t *newinp, *headinp, *previnp, *inp;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体

    DEBUG_PRINT((CE_CONT, "iumfs_add_node_to_list called\n"));

    newinp = VNODE2IUMNODE(newvp);
    iumfsp = VFS2IUMFS(vfsp);
    headinp = &iumfsp->node_list_head;

    previnp = headinp;
    rw_enter(&(previnp->i_listlock),RW_WRITER);
    while (previnp->next != NULL) {
        inp = previnp->next;
        rw_enter(&(inp->i_listlock),RW_WRITER);
        rw_exit(&(previnp->i_listlock));
        previnp = inp;
    }
    previnp->next = newinp;
    rw_exit(&(previnp->i_listlock));
    DEBUG_PRINT((CE_CONT, "iumfs_add_node_to_list: return(%d)\n", SUCCESS));
    return (SUCCESS);
}

/*****************************************************************************
 * iumfs_remove_node_from_list()
 * 
 * iumfs ファイルシステムプライベートデータ構造からリンクしている、
 * ファイルシステムのノードリストから指定された vnode を取り除く。
 *
 *  引数：
 *      vfsp   : vfs 構造体 
 *      rmvp  : ノードリストから取り除く vnode のポインタ
 *      
 * 戻り値：
 *          成功: SUCCESS(=0)
 *          失敗: 0 以外
 *****************************************************************************/
int
iumfs_remove_node_from_list(vfs_t *vfsp, vnode_t *rmvp)
{
    iumnode_t *rminp; // これから削除する vnode のノード情報
    iumnode_t *headinp; // ファイルシステムのノードリストのヘッド
    iumnode_t *inp; // 操作用のノード情報のポインタ
    iumnode_t *previnp; // 操作用のノード情報のポインタ
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    int err = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_remove_node_from_list called\n"));

    rminp = VNODE2IUMNODE(rmvp);
    iumfsp = VFS2IUMFS(vfsp);
    headinp = &iumfsp->node_list_head;

    previnp = headinp;
    rw_enter(&(previnp->i_listlock),RW_WRITER);
    while (previnp->next) {
        inp = previnp->next;
        rw_enter(&(inp->i_listlock),RW_WRITER);
        if (inp == rminp) {
            mutex_enter(&rmvp->v_lock);
            if(rmvp->v_count > 1){
                // ここまで来る間に他のスレッドでこのノードの参照を開始したようだ。
                // vn_rele で増加された参照カウントを減らしてリターン
                rmvp->v_count--;
                mutex_exit(&(rmvp->v_lock));
                rw_exit(&(inp->i_listlock));
                rw_exit(&(previnp->i_listlock));
                err = EBUSY;
                goto out;
            }
            mutex_exit(&(rmvp->v_lock));            
            previnp->next = inp->next;
            rw_exit(&(inp->i_listlock));
            rw_exit(&(previnp->i_listlock));
            err = SUCCESS;
            goto out;
        }
        rw_exit(&(previnp->i_listlock));
        previnp = inp;
    }
    rw_exit(&(previnp->i_listlock));
    err = ENOENT;
    cmn_err(CE_CONT, "iumfs_remove_node_from_list: cannot find node\n");
    
  out:
    DEBUG_PRINT((CE_CONT, "iumfs_remove_node_from_list: return(%d)\n", err));
    return (err);
}

/*****************************************************************************
 * iumfs_free_all_node()
 *
 *  ファイルシステムの全ての vnode および iumnode を開放する。
 *  全ての vnode が利用されていないと分かった場合に iumfs_unmount() から呼ばれる。
 *
 *  引数：
 *         vfsp  : vnode を開放するファイルシステムの vfs 構造体
 *         cr    : システムコールを呼び出したユーザのクレデンシャル
 *          
 * 戻り値：
 *          無し
 *****************************************************************************/
void
iumfs_free_all_node(vfs_t *vfsp, struct cred *cr)
{
    vnode_t *rmvp; // これから削除する vnode
    iumnode_t *rminp; // これから削除する vnode のノード情報
    iumnode_t *headinp; // ファイルシステムのノードリストのヘッド
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体

    DEBUG_PRINT((CE_CONT, "iumfs_free_all_node called\n"));

    iumfsp = VFS2IUMFS(vfsp);
    headinp = &iumfsp->node_list_head;

    rminp = headinp->next;
    while (rminp != NULL) {
        rmvp = IUMNODE2VNODE(rminp);
        iumfs_free_node(rmvp, cr);
        rminp = headinp->next;
    }
    DEBUG_PRINT((CE_CONT, "iumfs_free_all_node: return\n"));
    return;
}

/************************************************************************
 * iumfs_make_directory()
 *
 *   新しいディレクトリを作成する。
 *   
 *    o 新規ディレクトリ用の vnode,iumnode の確保、初期化
 *    o 新規ディレクトリのデフォルトのエントリ（. と ..)を作成
 *
 *    ** iumnode 構造体の pathname に新しいディレクトリのパス名をセット
 *       するのは呼び出し側の責任。ここでは行わない。
 *
 * 引数:
 *
 *     vfsp  : vfs 構造体
 *     vpp   : 呼び出し側から渡された vnode 構造体のポインタのアドレス
 *     mvp   : ディレクトリマウントポイントの vnode 構造体のポインタ
 *     cr    : システムコールを呼び出したユーザのクレデンシャル
 *     nodeid: 作成するディレクトリの nodeid(0の場合は自動割当）
 *
 * 戻り値:
 *
 *     成功時   : SUCCESS(=0)
 *     エラー時 : エラー番号
 *     
 ************************************************************************/
int
iumfs_make_directory(vfs_t *vfsp, vnode_t **vpp, vnode_t *parentvp,
                     struct cred *cr, ino_t nodeid)
{
    int err = SUCCESS;
    iumnode_t *newdirinp = NULL; // 新しいディレクトリの iumnode（ノード情報）
    vnode_t *newdirvp = NULL; // 新しいディレクトリの vnode
    char curdir[] = ".";
    char pardir[] = "..";
    vattr_t parentvattr;

    DEBUG_PRINT((CE_CONT, "iumfs_make_directory is called\n"));
    
    // 途中で break するためだけの do-while 文
    do {
        /*
         * 親ディレクトリ vnode、属性情報を得る
         * ディレクトリエントリ中の 「..」 の情報として使う。
         */
        parentvattr.va_mask = AT_ALL;
#ifdef OPENSOLARIS	
        err = VOP_GETATTR(parentvp, &parentvattr, 0, cr, NULL);
#else
        err = VOP_GETATTR(parentvp, &parentvattr, 0, cr);
#endif // ifdef OPENSOLARIS
        if (err != SUCCESS) {
            cmn_err(CE_CONT, "iumfs_make_directory: can't get parent directory's attribute\n");
            break;
        }

        // 新しいディレクトリの vnode を作成
        err = iumfs_alloc_node(vfsp, &newdirvp, VROOT, VDIR, nodeid);
        if (err != SUCCESS) {
            cmn_err(CE_CONT, "iumfs_make_directory: cannot allocate vnode for new directory\n");
            break;
        }

        // 新しいディレクトリのノード情報(iumnode) を得る。
        newdirinp = VNODE2IUMNODE(newdirvp);

        // 新しいディレクトリの vnode の属性情報をセット
        newdirinp->vattr.va_mode = 0040755; //00755;
        newdirinp->vattr.va_size = 0;
        newdirinp->vattr.va_nblocks = 1;

        /*
         * ディレクトリにデフォルトのエントリを追加する。
         * 最初にディレクトリに存在するエントリは以下の２つ
         *   .    : カレントディレクトリ
         *   ..   : 親ディレクトリ
         */
        if (iumfs_add_entry_to_dir(newdirvp, curdir, strlen(curdir), newdirinp->vattr.va_nodeid) < 0)
            break;
        if (iumfs_add_entry_to_dir(newdirvp, pardir, strlen(pardir), parentvattr.va_nodeid) < 0)
            break;

        // 引数として渡されたポインタに新しい vnode のアドレスをセット
        *vpp = newdirvp;

    } while (0);

    // エラー時は確保したリソースを解放する。
    if (err) {
        if (newdirvp != NULL)
            iumfs_free_node(newdirvp, cr);
    }
    DEBUG_PRINT((CE_CONT, "iumfs_make_directory return(%d)\n", err));
    return (err);
}

/***********************************************************************
 * iumfs_add_entry_to_dir()
 *
 *  ディレクトリに、引数で渡された名前の新しいエントリを追加する。
 *  This function just calls iumfs_add_entry_to_dir_nolock after aquiring
 *  a lock of iumnode structure.
 *
 *  引数:
 *
 *     dirvp     : ディレクトリの vnode 構造体
 *     name      : 追加するディレクトリエントリ（ファイル）の名前
 *     name_size : ディレクトリエントリの名前の長さ。
 *     nodeid    : 追加するディレクトリエントリのノード番号(inode番号)
 *
 *  戻り値
 *
 *  　　正常時   : 確保したディレクトリエントリ用のメモリのサイズ
 *                すでにエントリが存在しているときは 0
 *      エラー時 : -1
 *
 ***********************************************************************/
int
iumfs_add_entry_to_dir(vnode_t *dirvp, char *name, int name_size, ino_t nodeid)
{
    int err = 0;
    iumnode_t *dirinp;

    DEBUG_PRINT((CE_CONT, "iumfs_add_entry_to_dir is called\n"));
    dirinp = VNODE2IUMNODE(dirvp);
    /*
     *  ディレクトリの iumnode のデータを変更するので、まずはロックを取得
     */
    mutex_enter(&(dirinp->i_dlock));
    err = iumfs_add_entry_to_dir_nolock( dirvp, name, name_size, nodeid);
    mutex_exit(&(dirinp->i_dlock));

    DEBUG_PRINT((CE_CONT, "iumfs_add_entry_to_dir: return(%d)", err));
    return (err);
}

/***********************************************************************
 * iumfs_add_entry_to_dir_nolock()
 *
 *  ディレクトリに、引数で渡された名前の新しいエントリを追加する。
 *  implementation of iumfs_add_entry_to_dir().
 *  This function must be called afer getting lock of dirinp->i_dlock.
 *
 *  引数:
 *
 *     dirvp     : ディレクトリの vnode 構造体
 *     name      : 追加するディレクトリエントリ（ファイル）の名前
 *     name_size : ディレクトリエントリの名前の長さ。
 *     nodeid    : 追加するディレクトリエントリのノード番号(inode番号)
 *
 *  戻り値
 *
 *  　　正常時   : 確保したディレクトリエントリ用のメモリのサイズ
 *                すでにエントリが存在しているときは 0
 *      エラー時 : -1
 *
 ***********************************************************************/
int
iumfs_add_entry_to_dir_nolock(vnode_t *dirvp, char *name, int name_size, ino_t nodeid)
{
    iumnode_t *dirinp;
    dirent64_t *new_dentp; // 新しいディレクトリエントリのポインタ
    uchar_t *newp; // kmem_zalloc() で確保したメモリへのポインタ
    offset_t dent_total; // 全てのディレクトリエントリの合計サイズ
    int err;
    offset_t offset;
    dirent64_t *dentp;    

    DEBUG_PRINT((CE_CONT, "iumfs_add_entry_to_dir_nolock is called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_add_entry_to_dir_nolock: name=\"%s\", name_size=%d, nodeid=%d\n", name, name_size, nodeid));

    dirinp = VNODE2IUMNODE(dirvp);
    ASSERT(mutex_owned(&(dirinp->i_dlock))); // must hold the mutext before called.

    /*
     * ディレクトリの中にすでにエントリがないかどうかをチェックする。
     */
    dentp = (dirent64_t *) dirinp->data;    
    for (offset = 0; offset < dirinp->dlen; offset += dentp->d_reclen) {
        dentp = (dirent64_t *) ((char *) dirinp->data + offset);
        if ((strcmp(dentp->d_name, name) == 0)) {
            // すでにエントリが存在する。
            return(0);
        }
    }    

    /*
     * 確保するディレクトリエントリ分のサイズを求める
     */
    if (dirinp->data != NULL && dirinp->dlen > 0)
        dent_total = dirinp->dlen + DIRENT64_RECLEN(name_size);
    else
        dent_total = DIRENT64_RECLEN(name_size);

    DEBUG_PRINT((CE_CONT, "iumfs_add_entry_to_dir_nolock: dent_total=%d\n", dent_total));

    /*
     * ディレクトリエントリ用の領域を新たに確保
     */
    newp = (uchar_t *) kmem_zalloc(dent_total, KM_NOSLEEP);
    if (newp == NULL) {
        cmn_err(CE_CONT, "iumfs_add_entry_to_dir_nolock: kmem_zalloc failed");
        err = ENOMEM;
        goto error;
    }

    /*
     * 既存のディレクトリエントリがあれば、それを新しく確保した
     * メモリにコピーし、その後開放する。
     */
    if (dirinp->data != NULL && dirinp->dlen > 0) {
        bcopy(dirinp->data, newp, dirinp->dlen);
        /*
         * 危険な感じだが、ロックを取らずにこのデータを読む関数は
         * いないので、ここでフリーしても Panic は起きない・・はず。
         */
        kmem_free(dirinp->data, dirinp->dlen);

        new_dentp = (dirent64_t *) (newp + dirinp->dlen);
    } else {
        new_dentp = (dirent64_t *) newp;
    }

    /*
     * dirent 構造体に新しいディレクトリエントリの情報をセット
     */
    new_dentp->d_ino = nodeid;
    new_dentp->d_off = dirinp->dlen; /* 新しい dirent のオフセット。つまり現在の総 diernt の長さ */
    new_dentp->d_reclen = DIRENT64_RECLEN(name_size);
    bcopy(name, new_dentp->d_name, name_size);

    /*
     *  ディレクトリの iumnode の "data" が新しく確保したアドレスを
     *  さすように変更。
     */
    dirinp->data = (void *) newp;
    dirinp->dlen = dent_total;
    DEBUG_PRINT((CE_CONT, "iumfs_add_entry_to_dir: new directory size=%d\n", dirinp->dlen));

    DIRENT_SANITY_CHECK("iumfs_add_entry_to_dir", dirinp);    

    /*
     * ディレクトリのサイズも新しく確保したメモリのサイズに変更
     * 同時にアクセス時間、変更時間も変更
     */
    dirinp->vattr.va_size = dent_total;
    dirinp->vattr.va_atime = iumfs_get_current_time();
    dirinp->vattr.va_mtime = iumfs_get_current_time();

    /*
     * 正常終了。最終的に確保したディレクトリエントリのサイズを返す。
     */
    DEBUG_PRINT((CE_CONT, "iumfs_add_entry_to_dir_nolock: return(%d)", dent_total));
    return (dent_total);

error:
    if (newp)
        kmem_free(newp, dent_total);
    DEBUG_PRINT((CE_CONT, "iumfs_add_entry_to_dir_nolock: return(-1)"));
    return (-1);
}


/***********************************************************************
 * iumfs_find_nodeid_by_name
 *
 *  ディレクトリの中から、引数で渡された名前のエントリを探す。
 *
 *  引数:
 *
 *     dirinp    : ディレクトリの iumnode 構造体
 *     name      : 検索するディレクトリエントリの名前
 *
 *  戻り値
 *
 *  　　見つかった時       : 見つかったディレクトリエントリの nodeid
 *      見つからなかった時 :  0
 *
 ***********************************************************************/
ino_t
iumfs_find_nodeid_by_name(iumnode_t *dirinp, char *name)
{
    offset_t offset;
    ino_t nodeid = 0;
    dirent64_t *dentp;

    DEBUG_PRINT((CE_CONT, "iumfs_find_nodeid_by_name is called\n"));

    mutex_enter(&(dirinp->i_dlock));
    DIRENT_SANITY_CHECK("iumfs_find_nodeid_by_name", dirinp);    
    dentp = (dirent64_t *) dirinp->data;
    /*
     * ディレクトリの中に、引数で渡されたファイル名と同じ名前のエントリ
     * があるかどうかをチェックする。
     */
    DEBUG_PRINT((CE_CONT, "iumfs_find_nodeid_by_name: look for %s\n", name));
    for (offset = 0; offset < dirinp->dlen; offset += dentp->d_reclen) {
        dentp = (dirent64_t *) ((char *) dirinp->data + offset);
        if (strcmp(dentp->d_name, name) == 0) {
            nodeid = dentp->d_ino;
            DEBUG_PRINT((CE_CONT, "iumfs_find_nodeid_by_name: found \"%s\"(nodeid=%d)\n", name, nodeid));
            break;
        }
    }
    mutex_exit(&(dirinp->i_dlock));
    DEBUG_PRINT((CE_CONT, "iumfs_find_nodeid_by_name return(%d)\n", nodeid));
    return (nodeid);
}

/***********************************************************************
 * iumfs_dir_is_empty
 *
 * 指定されたディレクトリが空であるかをチェック。
 * （空 とは「.」と「..」しかない状態を指す）
 *
 *  引数:
 *
 *     dirvp    :  確認するディレクトリの vnode 構造体
 *
 *  戻り値:
 *
 *      空だった場合       : 1
 *      空じゃなかった場合 : 0
 *
 ***********************************************************************/
int
iumfs_dir_is_empty(vnode_t *dirvp)
{
    offset_t offset;
    dirent64_t *dentp;
    iumnode_t *dirinp;
    int found = FALSE;

    DEBUG_PRINT((CE_CONT, "iumfs_dir_is_empty is called\n"));

    dirinp = VNODE2IUMNODE(dirvp);

    mutex_enter(&(dirinp->i_dlock));
    DIRENT_SANITY_CHECK("iumfs_dir_is_empty", dirinp);    
    dentp = (dirent64_t *) dirinp->data;
    /*
     * ディレクトリの中に、「.」と「..」以外の名前を持ったエントリ
     * があるかどうかをチェックする。
     */
    for (offset = 0; offset < dirinp->dlen; offset += dentp->d_reclen) {
        dentp = (dirent64_t *) ((char *) dirinp->data + offset);
        if ((strcmp(dentp->d_name, ".") == 0) || (strcmp(dentp->d_name, "..") == 0)) {
            // 「.」と「..」は無視、次へ
            continue;
        }
        // 「.」と「..」以外のなにかが見つかった。
        //nodeid = dentp->d_ino;
        found = TRUE;
        //DEBUG_PRINT((CE_CONT,"iumfs_dir_is_empty: found \"%s\" nodeid = %d\n", dentp->d_name, nodeid));
        DEBUG_PRINT((CE_CONT, "iumfs_dir_is_empty: found \"%s\"\n", dentp->d_name));
        break;
    }
    mutex_exit(&(dirinp->i_dlock));

    if (found) {
        DEBUG_PRINT((CE_CONT, "iumfs_dir_is_empty: return(FALSE)\n"));
        return (FALSE); // 空じゃない
    } else {
        DEBUG_PRINT((CE_CONT, "iumfs_dir_is_empty: return(TRUE)\n"));
        return (TRUE); // 空だ
    }
}

/***********************************************************************
 * iumfs_remove_entry_from_dir()
 *
 *  ディレクトリに、引数で渡された名前の新しいエントリを削除
 *
 *  引数:
 *
 *     vp       : ディレクトリの vnode 構造体
 *     name      : 削除するディレクトリエントリ（ファイル）の名前
 *
 *  戻り値
 *
 *  　　正常時   : 確保したディレクトリエントリ用のメモリのサイズ
 *      エラー時 : -1 
 *
 ***********************************************************************/
int
iumfs_remove_entry_from_dir(vnode_t *dirvp, char *name)
{
    offset_t offset;
    iumnode_t *dirinp;
    dirent64_t *dentp; // 作業用ポインタ
    uchar_t *newp = NULL; // kmem_zalloc() で確保したメモリへのポインタ
    uchar_t *workp; // bcopy() でディレクトリエントリをコピーするときの作業用ポインタ
    offset_t dent_total = 0; // 全てのディレクトリエントリの合計サイズ
    offset_t remove_dent_len = 0; // 削除するエントリのサイズ

    DEBUG_PRINT((CE_CONT, "iumfs_remove_entry_from_dir is called\n"));

    DEBUG_PRINT((CE_CONT, "iumfs_remove_entry_from_dir: file name = \"%s\"\n", name));

    dirinp = VNODE2IUMNODE(dirvp);

    /*
     *  ディレクトリの iumnode のデータを変更するので、まずはロックを取得
     */
    mutex_enter(&(dirinp->i_dlock));
    DIRENT_SANITY_CHECK("iumfs_remove_entry_from_dir(1)",dirinp);    
    dentp = (dirent64_t *) dirinp->data;
    /*
     * ディレクトリの中から引数で渡されたファイル名と同じ名前のエントリ
     * を探し、あれば、そのエントリの長さを得る。
     */
    for (offset = 0; offset < dirinp->dlen; offset += dentp->d_reclen) {
        dentp = (dirent64_t *) ((char *) dirinp->data + offset);

        if (strcmp(dentp->d_name, name) == 0) {
            remove_dent_len = dentp->d_reclen;
            break;
        }
    }
    
    /*
     * 削除するエントリの長さが 0 だったら、それは先のループでエントリが
     * 見つけられなかったことを意味する。その場合、エラーを返す
     */
    if (remove_dent_len == 0) {
        DEBUG_PRINT((CE_CONT, "iumfs_remove_entry_from_dir: cannot find requested entry\n"));
        goto error;
    }

    /*
     * 新たに確保するディレクトリエントリ分のサイズを求める。
     * 計算は単純に　
     * 　新しいサイズ = 前のサイズ - 削除するエントリのサイズ
     */
    dent_total = dirinp->dlen - remove_dent_len;

    /*
     * ディレクトリエントリ用の領域を新たに確保
     */
    newp = (uchar_t *) kmem_zalloc(dent_total, KM_NOSLEEP);
    if (newp == NULL) {
        cmn_err(CE_CONT, "iumfs_remove_entry_from_dir: kmem_zalloc failed");
        goto error;
    }

    DIRENT_SANITY_CHECK("iumfs_remove_entry_from_dir(2)",dirinp)

    /*
     * 効率が悪いが、ディレクトの中のエントリをもう一度巡り、削除する
     * エントリを除く既存のエントリを、新しく確保した領域にコピーする。
     */
    dentp = (dirent64_t *) dirinp->data;
    workp = newp;
    for (offset = 0; offset < dirinp->dlen; offset += dentp->d_reclen) {
        dentp = (dirent64_t *) ((char *) dirinp->data + offset);
        if (strcmp(dentp->d_name, name) == 0) {
            // 削除するエントリはコピーしない。
            continue;
        }
        // そのほかの既存エントリは新しい領域にコピー
        bcopy(dentp, workp, dentp->d_reclen);
        workp += dentp->d_reclen;
    }

    kmem_free(dirinp->data, dirinp->dlen);

    /*
     *  ディレクトリの iumnode の "data" が新しく確保したアドレスを
     *  さすように変更。
     */
    dirinp->data = (void *) newp;
    dirinp->dlen = dent_total;
    DEBUG_PRINT((CE_CONT, "iumfs_remove_entry_from_dir: new directory size = %d\n", dirinp->dlen));
    /*
     * ディレクトリのサイズも新しく確保したメモリのサイズに変更
     * ディレクトリの、参照時間、変更時間も変更
     */
    dirinp->vattr.va_size = dent_total;
    dirinp->vattr.va_atime = iumfs_get_current_time();
    dirinp->vattr.va_mtime = iumfs_get_current_time();

    /*
     * 正常終了。最終的に確保したディレクトリエントリのサイズを返す。
     */
    mutex_exit(&(dirinp->i_dlock));
    DEBUG_PRINT((CE_CONT, "iumfs_remove_entry_from_dir: return(%d)\n", dent_total));
    return (dent_total);

error:
    if (newp)
        kmem_free(newp, dent_total);
    mutex_exit(&(dirinp->i_dlock));
    DEBUG_PRINT((CE_CONT, "iumfs_remove_entry_from_dir: return(-1)\n"));
    return (-1);
}

/***********************************************************************
 * iumfs_find_vnode_by_nodeid
 *
 *  ファイルシステム毎のノード一覧のリンクリストから、指定された nodeid
 *  をもつノードを検索し、vnode を返す。
 *
 *  引数:
 *
 *     iumfsp    : ファイルシステムのプライベートデータ構造体(iumfs_t)
 *     nodeid    : 検索する nodeid
 *
 *  戻り値
 *
 *      見つかった場合       : vnode 構造体のアドレス
 *      見つからなかった場合 : NULL
 *
 ***********************************************************************/
vnode_t *
iumfs_find_vnode_by_nodeid(iumfs_t *iumfsp, ino_t nodeid)
{
    iumnode_t *inp, *previnp, *headinp;
    vnode_t *vp = NULL;

    DEBUG_PRINT((CE_CONT, "iumfs_find_vnode_by_nodeid is called\n"));

    headinp = &iumfsp->node_list_head;

    /*
     * ノードのリンクリストの中から該当する nodeid のものを探す。
     */
    previnp = headinp;
    rw_enter(&(previnp->i_listlock),RW_READER);
    while (previnp->next) {
        inp = previnp->next;
        rw_enter(&(inp->i_listlock),RW_READER);
        if (inp->vattr.va_nodeid == nodeid) {
            vp = IUMNODE2VNODE(inp);
            /*
             * iumnode のロックを取得している間に、vnode の参照カウントを
             * 増加させておく。こうすることで、vnode を返答したあとで、
             * その vnode が free されてしまうという問題を防ぐ。
             */
            VN_HOLD(vp);
            DEBUG_PRINT((CE_CONT, "iumfs_find_vnode_by_nodeid: found vnode 0x%p\n", vp));
            rw_exit(&(inp->i_listlock));
            break;
        }
        rw_exit(&(previnp->i_listlock));
        previnp = inp;
    }
    rw_exit(&(previnp->i_listlock));

#ifdef DEBUG    
    if (vp == NULL)
        DEBUG_PRINT((CE_CONT, "iumfs_find_vnode_by_nodeid: cannot find vnode \n"));
#endif    
    DEBUG_PRINT((CE_CONT, "iumfs_find_vnode_by_nodeid: return(0x%p)\n", vp));
    return (vp);
}

/************************************************
 * iumfs_set_current_time
 *
 * 現在時をセットした timestruct 構造体を返す。
 * 主に vattr 構造体の va_atime, va_ctime,va_mtime
 * をセットするために使う。
 * 
 ************************************************/
timestruc_t
iumfs_get_current_time()
{
    return (time);
}

/************************************************************************
 * iumfs_make_directory_with_name()
 *
 *   名前付で新しいディレクトリを作成する。
 *   内部で、iumfs_make_directory() を呼び、かつ iumnode 構造体の pathname
 *   に新しいディレクトリのパス名をセットする。
 *   
 * 引数:
 *
 *     vfsp    : vfs 構造体
 *     vpp     : 呼び出し側から渡された vnode 構造体のポインタのアドレス
 *     mvp     : ディレクトリマウントポイントの vnode 構造体のポインタ
 *     cr      : システムコールを呼び出したユーザのクレデンシャル
 *     dirname : 新しいディレクトリの名前
 *     nodeid  : 新しいディレクトリがもつ node 番号(0だと自動割当)
 *
 * 戻り値:
 *
 *     成功時   : SUCCESS(=0)
 *     エラー時 : エラー番号
 *     
 ************************************************************************/
int
iumfs_make_directory_with_name(vfs_t *vfsp, vnode_t **vpp, vnode_t *parentvp,
                               struct cred *cr, char *dirname, ino_t nodeid)
{
    int err = SUCCESS;
    iumnode_t *newdirinp; // 新しいディレクトリの iumnode（ノード情報）
    vnode_t *newdirvp; // 新しいディレクトリの vnode
    iumnode_t *parentinp; // 親ディレクトリの iumnode（ノード情報）
    int namelen;

    DEBUG_PRINT((CE_CONT, "iumfs_make_directory_with_name is called\n"));

    parentinp = VNODE2IUMNODE(parentvp);

    /*
     * まずは、渡されたディレクトリ名の長さをチェック
     */
    namelen = strlen(dirname);
    if (namelen > MAXNAMLEN) {
        DEBUG_PRINT((CE_CONT, "iumfs_make_directory_with_name: return(ENAMETOOLONG)\n"));
        return (ENAMETOOLONG);
    }
    /*
     * つづいて、パス名の長さもチェック
     */
    if (strlen(parentinp->pathname) + namelen > IUMFS_MAXPATHLEN) {
        DEBUG_PRINT((CE_CONT, "iumfs_make_directory_with_name: return(ENAMETOOLONG)\n"));
        return (ENAMETOOLONG);
    }

    /*
     * iumfs_make_directory() を呼び新規ディレクトリを作成する。
     */
    err = iumfs_make_directory(vfsp, vpp, parentvp, cr, nodeid);
    if (err) {
        DEBUG_PRINT((CE_CONT, "iumfs_make_directory_with_name: return(%d)\n", err));
        return (err);
    }

    /*
     * 新しいディレクトリのパス名をセットする。
     * パス名は「親ディレクトリのパス名　＋　新しいディレクトリ名」
     * もし親ディレクトリがファイルシステムルートだったら、余計な「/」はつけない。
     */
    newdirvp = *vpp;
    newdirinp = VNODE2IUMNODE(newdirvp);
    if (ISROOT(parentinp->pathname))
        snprintf(newdirinp->pathname, IUMFS_MAXPATHLEN, "/%s", dirname);        
    else
        snprintf(newdirinp->pathname, IUMFS_MAXPATHLEN, "%s/%s", parentinp->pathname, dirname);
    
    DEBUG_PRINT((CE_CONT, "iumfs_make_directory_with_name: return(0)\n"));
    return (0);
}

/***********************************************************************
 * iumfs_find_vnode_by_pathname
 *
 *  ファイルシステム毎のノード一覧のリンクリストから、指定されたパス名
 *  をもつノードを検索し、vnode を返す。
 *
 *  引数:
 *
 *     iumfsp    : ファイルシステムのプライベートデータ構造体(iumfs_t)
 *     pathname  : 検索するパス名
 *
 *  戻り値
 *
 *      見つかった場合       : vnode 構造体のアドレス
 *      見つからなかった場合 : NULL
 *
 ***********************************************************************/
vnode_t *
iumfs_find_vnode_by_pathname(iumfs_t *iumfsp, char *pathname)
{
    iumnode_t *inp, *previnp, *headinp;
    vnode_t *vp = NULL;
    int namelen, exnamelen;

    DEBUG_PRINT((CE_CONT, "iumfs_find_vnode_by_pathname is called\n"));

    headinp = &iumfsp->node_list_head;
    namelen = strlen(pathname);

    /*
     * ノードのリンクリストの中からパス名が一致するものを探す
     */
    previnp = headinp;
    rw_enter(&(previnp->i_listlock),RW_READER);
    while (previnp->next) {
        inp = previnp->next;
        rw_enter(&(inp->i_listlock),RW_READER);
        exnamelen = strlen(inp->pathname);
        if (exnamelen == namelen && strcmp(inp->pathname, pathname) == 0) {
            vp = IUMNODE2VNODE(inp);
            /*
             * iumnode のロックを取得している間に、vnode の参照カウントを
             * 増加させておく。こうすることで、vnode を返答したあとで、
             * その vnode が free されてしまうという問題を防ぐ。
             */
            VN_HOLD(vp);
            DEBUG_PRINT((CE_CONT, "iumfs_find_vnode_by_pathname: found vnode 0x%p\n", vp));
            rw_exit(&(inp->i_listlock));
            break;
        }
        rw_exit(&(previnp->i_listlock));
        previnp = inp;
    }
    rw_exit(&(previnp->i_listlock));

#ifdef DEBUG    
    if (vp == NULL)
        DEBUG_PRINT((CE_CONT, "iumfs_find_vnode_by_pathname: cannot find vnode \n"));
#endif    
    DEBUG_PRINT((CE_CONT, "iumfs_find_vnode_by_pathname: return(%p)\n", vp));
    return (vp);
}

/***********************************************************************
 * iumfs_directory_entry_exist
 *
 *  ディレクトリに、引数で渡された名前のエントリがあるかどうかをチェック
 *  Must be called after aquiring iumnode i_dlock.
 *
 *  引数:
 *
 *     vp       : ディレクトリの vnode 構造体
 *     name      : チェックするディレクトリエントリ（ファイル）の名前
 *
 *  戻り値
 *
 *  　　既存エントリが見つかった : 1
 *      既存エントリは無い       : 0
 *
 ***********************************************************************/
int
iumfs_directory_entry_exist(vnode_t *dirvp, char *name)
{
    offset_t offset;
    iumnode_t *dirinp;
    dirent64_t *dentp; // 作業用ポインタ
    int found = 0;

    DEBUG_PRINT((CE_CONT, "iumfs_directory_entry_exist is called\n"));
    DEBUG_PRINT((CE_CONT, "iumfs_directory_entry_exist: file name = \"%s\"\n", name));

    dirinp = VNODE2IUMNODE(dirvp);
    ASSERT(mutex_owned(&(dirinp->i_dlock)));

    dentp = (dirent64_t *) dirinp->data;
    /*
     * ディレクトリの中から引数で渡されたファイル名と同じ名前のエントリを探す。
     */
    for (offset = 0; offset < dirinp->dlen; offset += dentp->d_reclen) {
        dentp = (dirent64_t *) ((char *) dirinp->data + offset);
        if (strcmp(dentp->d_name, name) == 0) {
            found = 1;
            break;
        }
    }

    if (found) {
        DEBUG_PRINT((CE_CONT, "iumfs_directory_entry_exist: return(1)\n"));
        return (1);
    } else {
        DEBUG_PRINT((CE_CONT, "iumfs_directory_entry_exist: return(0)\n"));
        return (0);
    }
}

/***********************************************************************
 * iumfs_find_parent_vnode
 *
 *  指定された vnode が属している親ディレクトリの vnode を返す。
 *  iumfs_find_vnode_by_pathname を利用しているため、返却される
 *  vnode の参照カウントは +1 されたものになっている。
 *
 *  引数:
 *     vp    :  対象の vnode ポインタ
 *
 *  戻り値
 *      見つかった場合       : vnode 構造体のアドレス
 *      見つからなかった場合 : NULL
 *
 ***********************************************************************/
vnode_t *
iumfs_find_parent_vnode(vnode_t *vp)
{
    iumnode_t *inp;
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体
    char *pathname;
    char parentpathname[IUMFS_MAXPATHLEN]; // 親ディレクトリのパス
    size_t namelen;
    vnode_t *parentvp = NULL;
    char *ptr;

    DEBUG_PRINT((CE_CONT, "iumfs_find_parent_vnode is called\n"));

    iumfsp = VNODE2IUMFS(vp);
    inp = VNODE2IUMNODE(vp);
    pathname = inp->pathname;

    if ((ptr = strrchr(pathname, '/')) == NULL) {
        cmn_err(CE_CONT, "iumfs_find_parent_vnode: vnode(%s) doesn have / char.\n", pathname);
        DEBUG_PRINT((CE_CONT, "iumfs_find_parent_vnode: return(NULL)\n"));
        return (vnode_t *) NULL;
    }
    // 最低でも一文字(/)はコピーする    
    namelen = MAX(ptr - pathname, 1);

    strncpy(parentpathname, pathname, namelen);

    parentvp = iumfs_find_vnode_by_pathname(iumfsp, parentpathname);
    DEBUG_PRINT((CE_CONT, "iumfs_find_parent_vnode: return(%p)\n", parentvp));
    return (parentvp);
}

/************************************************************************
 * iumfs_is_root()
 *
 * 引数で与えられた vnode が ファイルシステムのルートの vnode であるか
 * どうかをチェックする。
 *
 *  引数:
 *
 *     vp     : チェックする vnode 構造体
 *
 *  戻り値
 *
 *  　　ファイルシステム・ルートの場合   : 1
 *                          違う場合   : 0
 *
 *
 *************************************************************************/
int
iumfs_is_root(vnode_t *vp)
{
    iumfs_t *iumfsp; // ファイルシステム型依存のプライベートデータ構造体

    DEBUG_PRINT((CE_CONT, "iumfs_is_root called\n"));
    iumfsp = VNODE2IUMFS(vp);

    if (vp == iumfsp->rootvnode) {
        DEBUG_PRINT((CE_CONT, "iumfs_is_root: return(TRUE)\n"));
        return (TRUE);
    } else {
        DEBUG_PRINT((CE_CONT, "iumfs_is_root: return(FALSE)\n"));
        return (FALSE);
    }
}

