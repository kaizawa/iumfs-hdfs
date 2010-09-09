/*****************************************************************
 * hdfs_mount.c
 *
 * $Date: 2010/09/07 15:18:07 $, $Revision: 1.1.2.9 $                           
 * 
 *  gcc hdfs_mount.c -o mount
 *
 * hdfs の為の mount コマンド。
 * /usr/lib/fs/hdfs/ ディレクトリを作り、このディレクトリ内に
 * このプログラムを「mount」として配置すれば、/usr/sbin/mount
 * にファイルシステムタイプとして「hdfs」を指定すると、この
 * プログラムが呼ばれることになる。
 *
 *   Usage: mount -F hdfs hdfs_base_path mount_point
 *
 ******************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/mntent.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <strings.h>
#include <string.h>
#include "iumfs.h"

void print_usage(char *);
char *truncate_slash(char *);

int
main(int argc, char *argv[])
{
    char *opts = NULL;
    char *resource = NULL;
    char *mountpoint = NULL;
    int c;
    iumfs_mount_opts_t mountopts[1];

    memset(mountopts, 0x0, sizeof (iumfs_mount_opts_t));

    if (argc < 3 || argc > 5)
        print_usage(argv[0]);

    while ((c = getopt(argc, argv, "o:")) != EOF) {
        switch (c) {
            case 'o':
                // マウントオプション
                opts = optarg;
                break;
            default:
                print_usage(argv[0]);
        }
    }

    if ((argc - optind) != 2) {
        print_usage(argv[0]);
    }
    resource = truncate_slash(argv[optind++]);
    mountpoint = truncate_slash(argv[optind++]);

    strcpy(mountopts->basepath, resource);
    if (strlen(mountopts->basepath) == 0)
        strcpy(mountopts->basepath, "/");

    if (mount(resource, mountpoint, MS_DATA, "hdfs", mountopts, sizeof (mountopts)) < 0) {
        perror("mount");
        exit(0);
    }

    return (0);
}

/*****************************************************************************
 * print_usage()
 *
 * Usage を表示し、終了する。
 *****************************************************************************/
void
print_usage(char *argv)
{
    printf("Usage: %s -F hdfs hdfs_base_path mount_point\n", argv);
    printf("Example)\n");
    printf("    mount -F hdfs /user/username /mnt\n");
    exit(0);    
}

/****************************************************************************
 * truncate_slash
 *
 * 引数として渡された文字列の最後尾の全ての / を取り除く
 ****************************************************************************/
char *
truncate_slash (char *pathname)
{
    char *lastslash = NULL;
    int len = 0;

    lastslash = strrchr(pathname, '/');
    len = strlen(pathname);
    
    while ( lastslash != NULL && len > 1 && (len - (lastslash - pathname)) == 1) {
        *lastslash = '\0';
        lastslash = strrchr(pathname, '/');
        len = strlen(pathname);
    }
    return pathname;
}

