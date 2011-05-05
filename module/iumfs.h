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
/************************************************************
 * iumfs.h
 *
 * Header file for IUMFS pseudo filesystem
 *
 *************************************************************/

#include <sys/time.h>

#ifndef __IUMFS_H
#define __IUMFS_H

#ifdef OPENSOLARIS
#include <sys/vfs_opreg.h>
#endif
#include "iumfs.h"

/*
 * アライン対策のため MAX_USER_LEN、MAX_PATH_LEN
 * MAX_SERVER_NAME、MAX_PATH_LEN は必ず 8 の倍数（64bit境界）
 * になるようにする。
 */ 
#define MAX_USER_LEN    40 // 必ず 8 の倍数
#define MAX_PASS_LEN    40 // 必ず 8 の倍数
#define MAX_SERVER_LEN  80 // 必ず 8 の倍数
#define IUMFS_MAXPATHLEN  1024 // 必ず 8 の倍数
#define DEVICE_BUFFER_SIZE 1024 * 1024 // iumfscntl デバイスのインスタンス毎のバッファサイズ
#define MAX_RESPONSE_SIZE DEVICE_BUFFER_SIZE
#define MAX_REQUEST_SIZE  DEVICE_BUFFER_SIZE

#define MMAPSIZE      PAGESIZE

/*
 * 各メンバは x8 バイトとなっているので、この構造体の
 * 最後は必ず 64bit アラインになっているはず。
 *  40+40+80+1024=1184 bytes
 */ 
typedef struct iumfs_mount_opts 
{
    char basepath[IUMFS_MAXPATHLEN];
    char server[MAX_SERVER_LEN];
    char user[MAX_USER_LEN];
    char pass[MAX_PASS_LEN];    
} iumfs_mount_opts_t;

/*
 * iumfs から iumfsd デーモンに渡されるリクエストの為の構造体
 * READ_REQUEST の場合、datasize は 0。
 * WRITE_REQUEST の場合、datasize は 8バイトの倍数に丸め込んだ size
 *  8*5+1024+1184=2248 bytes
 */
typedef struct request
{
    int64_t             request_type; // リクエストのタイプ
    int64_t             size;     // 要求するファイルデータのサイズ
    int64_t             offset;   // 要求するファイルデータのオフセット
    int64_t             datasize; // リクエスト構造体に続くデータのサイズ
    int64_t             flags;    // ioflag の代わり・・と思ったがつかってない。
    char                pathname[IUMFS_MAXPATHLEN]; // 操作対象のファイルのパス名    
    iumfs_mount_opts_t  mountopts[1]; // mount コマンドからの引数
} request_t;

/*
 * デーモン から iumfs に渡されるレスポンス構造体
 *  8+8+8=24 bytes
 */
typedef struct response
{
    int64_t            request_type; // 対応するリクエストタイプ
    int64_t            result;       //リクエストの実行結果
    int64_t            datasize; // レスポンス構造体に続くデータのサイズ
    
} response_t;

/*
 * iumfs 専用の 簡易 dirent 構造体
 * Java アプリでのデータの扱いを簡単にするためシンプルにした。
 */
typedef struct iumfs_dirent
{
    int64_t           i_reclen;
    char              i_name[1];
} iumfs_dirent_t;

#define	IUMFS_DIRENT_RECLEN(namelen) \
	((offsetof(iumfs_dirent_t, i_name[0]) + 1 + (namelen) + 7) & ~ 7)
#define	IUMFS_DIRENT_NAMELEN(reclen)	\
	((reclen) - (offsetof(iumfs_dirent_t, i_name[0])))

/*
 * iumfs 専用の簡易 vattr 構造体
 * Java アプリでのデータの扱いを簡単にするためシンプルにした。
 */
typedef struct iumfs_vattr 
{
    uint64_t  i_mode; // ファイルモード
    uint64_t  i_size; // ファイルサイズ
    int64_t   i_type; // ファイルタイプ
    int64_t   mtime_sec; // 変更時間(sec) 
    int64_t   mtime_nsec;// 変更時間(nsec)
    int64_t   atime_sec; // 属性変更時間(sec) 
    int64_t   atime_nsec;// 属性変更時間(nsec)
    int64_t   ctime_sec; // 作成時間(sec)
    int64_t   ctime_nsec;// 作成時間(nsec)
} iumfs_vattr_t;

/*
 * 現在定義されているリクエストタイプ
 */
#define READ_REQUEST      0x01
#define READDIR_REQUEST   0x02
#define GETATTR_REQUEST   0x03
#define WRITE_REQUEST     0x04
#define CREATE_REQUEST    0x05
#define REMOVE_REQUEST    0x06
#define MKDIR_REQUEST     0x07
#define RMDIR_REQUEST     0x08

/*
 * デーモンが iumfscntl デバイスに報告する要求の実行結果
 * 通常は
 *   0        -> 成功
 *   それ以外 -> system error 番号
 * だが、readdir リクエスト用に以下の特別なエラー番号もつかう
 */
#define MOREDATA          240 // iumfscntl で使う特別なエラー番号

/*
 * 渡された文字列が「/」一文字であるかをチェック
 */
#define ISROOT(path) (strlen(path) == 1 && !strcmp(path, "/"))

#define SUCCESS         0       // 成功
#define TRUE            1       // 真
#define FALSE           0       // 偽

#ifdef _KERNEL

#define MAX_MSG         256     // SYSLOG に出力するメッセージの最大文字数 
#define MAXNAMLEN       255     // 最大ファイル名長
#define BLOCKSIZE       512     // iumfs ファイルシステムのブロックサイズ
#define MAX_INST         10     // iumfscntl の最大インスタンス数
#define DAEMON_TIMEOUT   10     // daemon からのリクエストを待つ時間(秒)
#define DAEMON_TIMEOUT_TICK SEC_TO_TICK(DAEMON_TIMEOUT)

/*
 * 以下の2つのマクロは MINOR デバイス番号 0 をつかってはだめ
 * だと考えて minor = instance -1 とするために作ったもの。
 * が、テストの結果問題なさそうなので、 minor = instance
 * としている。やっぱりこのほうが分り易い。。。
 */ 
#define MINOR2INST(minor) minor //- 1
#define INST2MINOR(instance) instance //+ 1

#ifdef DEBUG
#define  DEBUG_PRINT(args)  debug_print args
#else
#define DEBUG_PRINT(args)
#endif

/*
 * debug ビルド時は VN_RELE と VN_HOLD 時に vnode の参照数をカウント
 */
#ifdef DEBUG
#ifdef VN_RELE
#undef VN_RELE
#endif // #ifdef VN_RELE
#ifdef VN_HOLD
#undef VN_HOLD
#endif // #ifdef VN_HOLD
#define	VN_RELE(vp)	{\
        DEBUG_PRINT((CE_CONT, "VN_RELE called: v_count=%d\n", vp->v_count)); \
	vn_rele(vp); \
}
#define	VN_HOLD(vp)	{ \
        DEBUG_PRINT((CE_CONT, "VN_HOLD called: v_count=%d\n", vp->v_count)); \
	mutex_enter(&(vp)->v_lock);\
	(vp)->v_count++; \
	mutex_exit(&(vp)->v_lock); \
}
#endif // #ifdef DEBUG

#define VFS2IUMFS(vfsp)     ((iumfs_t *)(vfsp)->vfs_data)
#define VFS2ROOT(vfsp)      ((VFS2IUMFS(vfsp))->rootvnode)
#define IUMFS2ROOT(iumfsp)  ((iumfsp)->rootvnode)
#define VNODE2VFS(vp)       ((vp)->v_vfsp)
#define VNODE2IUMNODE(vp)   ((iumnode_t *)(vp)->v_data)
#define IUMNODE2VNODE(inp)  ((inp)->vnode)
#define VNODE2IUMFS(vp)     (VFS2IUMFS(VNODE2VFS((vp))))
#define VNODE2ROOT(vp)      (VNODE2IUMFS((vp))->rootvnode)
#define IN_INIT(inp) {\
              mutex_init(&(inp)->i_dlock, NULL, MUTEX_DEFAULT, NULL);\
              inp->vattr.va_uid      = 0;\
              inp->vattr.va_gid      = 0;\
              inp->vattr.va_blksize  = BLOCKSIZE;\
              inp->vattr.va_nlink    = 1;\
              inp->vattr.va_rdev     = 0;\
              inp->vattr.va_vcode    = 1;\
              inp->vattr.va_mode     = 00644;\
         }

#ifdef SOL10
// Solaris 10 には VN_INIT マクロが無いので追加。
#define	VN_INIT(vp, vfsp, type, dev)	{ \
	mutex_init(&(vp)->v_lock, NULL, MUTEX_DEFAULT, NULL); \
	(vp)->v_flag = 0;\
	(vp)->v_count = 1;     \
        (vp)->v_vfsp = (vfsp); \
	(vp)->v_type = (type); \
	(vp)->v_rdev = (dev); \
	(vp)->v_stream = NULL; \
}
#else
#endif

// 引数で渡された数値をポインタ長の倍数に繰り上げるマクロ
#define ROUNDUP(num)        ((num)%(sizeof(char *)) ? ((num) + ((sizeof(char *))-((num)%(sizeof(char *))))) : (num))


#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) < (b) ? (b) : (a))

/*
 * ディレクトリエントリの健全性チェック。おかしければ PANIC!
 * このマクロは iumnode のロック(i_dlock)を取得してから呼び出すこと!
 */
#ifdef DEBUG
#define DIRENT_SANITY_CHECK(name, dirinp) { \
    offset_t offset;\
    dirent64_t *dentp;\
    vnode_t *dirvp = IUMNODE2VNODE(dirinp);\
    for (offset = 0; offset < dirinp->dlen; offset += dentp->d_reclen) {\
        dentp = (dirent64_t *) ((char *) dirinp->data + offset);\
        if(dentp->d_reclen == 0) {\
            cmn_err(CE_PANIC, "%s: d_reclen is 0. dirvp=0x%p,dirinp=0x%p,dentp=0x%p\n",name,dirvp,dirinp,dentp);\
        }\
    }\
    if(offset != dirinp->dlen){\
        cmn_err(CE_PANIC, "%s: sum of d_reclen is not dlen\n", name); \
    }\
}
#else
#define DIRENT_SANITY_CHECK(name, dirinp)
#endif  // ifdef DEBUG

/*
 * ファイルシステム型依存のノード情報構造体。（iノード）
 * vnode 毎（open/create 毎）に作成される。
 * next, vattr, data, dlen については初期化以降も変更される
 * 可能性があるため、参照時にはロック(i_dlock)をとらなければ
 * ならない。
 * pathname はこのノードに対応するファイルのファイルシステムルート  
 * からの相対パス名をあらわす。本来ファイル名はディレクトリにのみ  
 * 存在し、ノード情報にはファイル名は含まれないが、参照するリモート
 * サービス（FTP等）がファイル毎のユニーな ID を持っているとは限ら
 * ないので、ファイルのパス名をノード情報の検索キーとして使うことにする。            
 * 別途ノードID(vattr.va_nodeid) も内部処理用に保持する。
 */
typedef struct iumnode
{
    struct iumnode    *next;      // iumnode 構造体のリンクリストの次の iumnode 構造体
    kmutex_t           i_dlock;   // 構造体のデータの保護用のロック  
    krwlock_t          i_listlock;// ノードリスト巡回時に使う next ポインタ用の rwlock
    vnode_t           *vnode;     // 対
                                  // 応する vnode 構造体へのポインタ
    vattr_t            vattr;     // getattr, setattr で使われる vnode の属性情報
#define fsize vattr.va_size
#define iumnodeid vattr.va_nodeid    
    void              *data;      // vnode がディレクトリの場合、ディレクトリエントリへのポインタが入る
    offset_t           dlen;      // ディレクトリエントリのサイズ
    char               pathname[IUMFS_MAXPATHLEN]; // ファイルシステムルートからの相対パス
} iumnode_t;

/*
 * ファイルシステム型依存のファイルシステムプライベートデータ構造体。
 * ファイルシステム毎（mount 毎）に作成される。
 */
typedef struct iumfs
{
    kmutex_t      iumfs_lock;        // 構造体のデータの保護用のロック
    vnode_t      *rootvnode;         // ファイルシステムのルートの vnode
    ino_t         iumfs_last_nodeid; // 割り当てた最後のノード番号    
    iumnode_t     node_list_head;    // 作成された iumnode 構造体のリンクリストのヘッド。
                                     // 構造体の中身は、ロック以外は参照されない。また、
                                     // ファイルシステムが存在する限りフリーされることもない。
    iumfs_mount_opts_t mountopts[1]; // mount(2) から渡されたオプション
    dev_t         dev;               // このファイルシステムのデバイス番号
} iumfs_t;

/*
 * iumfscntl デバイスのステータス構造体
 */
typedef struct iumfscntl_soft 
{
    kmutex_t          d_lock;         // この構造体のデータを保護するロック
    kmutex_t          s_lock;         // ステータスを保護するロック
    kcondvar_t	      cv;             // condition variable
    dev_info_t        *dip;           // device infor 構造体
    int               instance;       // インスタンス番号
    ddi_umem_cookie_t umem_cookie;
    int               state;          // ステータスフラグ
    size_t            size;           // マッピングするメモリのサイズ
    request_t         req;            // ユーザモードデーモンに対するリクエストを格納する
    int               error;          // デーモンから返ってきたエラー番号
    struct pollhead   pollhead;
    caddr_t           bufaddr;        // Request/Response ヘッダとデータを格納する一時バッファのアドレス
    size_t            bufusedsize;    // bufaddr 中有効なデータ長
    
} iumfscntl_soft_t;

/*
 * iumfscntl デバイスのステータスフラグ
 */
#define IUMFSCNTL_OPENED        0x01  // /dev/iumfscntl はすでにオープンされている
#define REQUEST_INPROGRESS      0x02  // READ 処理が実行中
#define REQUEST_IS_SET          0x04  // 要求内容がセット済み
#define DAEMON_INPROGRESS       0x08  // デーモンが処理中
#define BUFFER_INVALID          0x10  // マップされているアドレスのデータは不正なもの（読み込んではダメ）
                                      // デーモンが死んだか、もしくはデーモンがエラーを返してきた
#define REQUEST_IS_CANCELED     0x20  // リクエストがキャンセルされた

extern timestruc_t time; // システムの現在時

/* 関数のプロトタイプ宣言 */
extern void debug_print(int , char *, ...);
extern int iumfs_alloc_node(vfs_t *, vnode_t **, uint_t , enum vtype, ino_t);
extern void iumfs_free_node(vnode_t *, struct cred *);
extern int iumfs_add_node_to_list(vfs_t *, vnode_t *);
extern int iumfs_remove_node_from_list(vfs_t *, vnode_t *);
extern void iumfs_free_all_node(vfs_t *, struct cred*);
extern int iumfs_make_directory(vfs_t *, vnode_t **, vnode_t *, struct cred *,
        ino_t);
extern int iumfs_add_entry_to_dir(vnode_t *, char *, int, ino_t );
extern int iumfs_add_entry_to_dir_nolock(vnode_t *, char *, int, ino_t );
extern int iumfs_remove_entry_from_dir(vnode_t *, char *);
extern ino_t iumfs_find_nodeid_by_name(iumnode_t *, char *);
extern int iumfs_dir_is_empty(vnode_t *);
extern vnode_t *iumfs_find_vnode_by_nodeid(iumfs_t *, ino_t);
extern timestruc_t iumfs_get_current_time();
extern int iumfs_make_directory_with_name(vfs_t *, vnode_t **, vnode_t *,
        struct cred *, char *, ino_t);
extern vnode_t *iumfs_find_vnode_by_pathname(iumfs_t *, char *);
extern int iumfs_directory_entry_exist(vnode_t *, char *);
extern int iumfs_request_read(struct buf *, vnode_t *);
extern int iumfs_request_readdir(vnode_t *);
extern int iumfs_request_lookup(vnode_t *, char *, vattr_t *);
extern int iumfs_request_getattr(vnode_t *);
extern int iumfs_daemon_request_enter(iumfscntl_soft_t  **);
extern int iumfs_daemon_request_start(iumfscntl_soft_t  *);
extern void iumfs_daemon_request_exit(iumfscntl_soft_t  *);
extern vnode_t *iumfs_find_parent_vnode(vnode_t *);
extern int iumfs_is_root(vnode_t *);
extern int iumfs_request_write(struct buf *, vnode_t *);
extern int iumfs_request_create(vnode_t *, char *, vattr_t *);
extern int iumfs_request_remove(vnode_t *);
extern int iumfs_request_mkdir(vnode_t *, char *, vattr_t *);
extern int iumfs_request_rmdir(vnode_t *);
extern int iumfs_create_fs_root(vfs_t *, vnode_t **, vnode_t *, struct cred *);
extern int iumfs_getapage(vnode_t *, u_offset_t, size_t, uint_t *,
        struct page *[], size_t, struct seg *, caddr_t, enum seg_rw,
        struct cred *);
extern int iumfs_putapage(vnode_t *, page_t *, u_offset_t *, size_t *, int,
                          struct cred *);
#ifdef SOL10
extern struct vnodeops *iumfs_vnodeops;
extern fs_operation_def_t iumfs_vnode_ops_def_array[];
#else
extern struct vnodeops iumfs_vnodeops;
#endif

extern struct modldrv iumfs_modldrv; // ドライバーのリンケージ構造体
extern void *iumfscntl_soft_root; //  ドライバーのソフトステート構造体

/* Solaris 10 以外の場合の gcc 対策用ラッパー関数 */
#ifndef SOL10
static void  *memcpy(void *,  const  void  *, size_t );
static int    memcmp(const void *, const void *, size_t );
static void  *memset(void *, int , size_t );
#endif
    
#ifndef SOL10
/**************************************************
 * memcpy()
 *
 * gcc 対策。bcopy(9f) のラッパー
 * Solaris 10 では memcpy(9f) が追加されているので不要
 **************************************************/
static void *
memcpy(void *s1,  const  void  *s2, size_t n)
{
    bcopy(s2, s1, n);
    return(s1);
}
/**************************************************
 * memcmp()
 *
 * gcc 対策。bcmp(9f) のラッパー
 * Solaris 10 では memcmp(9f) が追加されているので不要
 **************************************************/
static int
memcmp(const void *s1, const void *s2, size_t n)
{
    return(bcmp(s1, s1, n));
}
/**************************************************
 * memset()
 *
 * gcc 対策。
 * Solaris 10 では memset(9f) が追加されているので不要
 **************************************************/
static void *
memset(void *s, int c, size_t n)
{
    int i;
    uchar_t *p;

    p = (uchar_t *)s;
    
    for (i = 0 ; i < n ; i++){
        p[i] = c;
    }
    return(s);
}
#endif // #ifndef SOL10

#endif // #ifdef _KERNEL

#endif // #ifndef __IUMFS_H
