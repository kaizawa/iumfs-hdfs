/*
 * Copyright (C) 2010 Kazuyoshi Aizawa. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*****************************************************************
 * hdfs_mount.c
 *
 * mount(1) command for IUMFS filesystem
 *
 ****************************************************************/
/*
 * gcc hdfs_mount.c -o mount
 *
 * hdfs の為の mount コマンド。
 * /usr/lib/fs/hdfs/ ディレクトリを作り、このディレクトリ内に
 * このプログラムを「mount」として配置すれば、/usr/sbin/mount
 * にファイルシステムタイプとして「hdfs」を指定すると、この
 * プログラムが呼ばれることになる。
 *
 *   Usage: mount -F hdfs hdfs_base_path mount_point
 *
 */
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
    char *opt = NULL;    
    char *resource = NULL;
    char *mountpoint = NULL;
    int c;
    int verbose = 0;    
    iumfs_mount_opts_t mountopts[1];
    char *server_and_path = NULL;
    char *path = NULL;
    size_t n;

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
    /*
     * -o で指定されたオプションを解釈する。
     * サポートしているのはは以下の2つのオプションだけ。
     * user=<user name>
     * pass=<password>
     *
     * 例） -o user=root,pass=hoge
     */
    if(opts){
        char *arg;

        arg = opts;
        while((opt = strtok(arg, ",")) != NULL){

            if(!strncmp(opt, "user=", 5))
                strcpy(mountopts->user,&opt[5]);
            else if (!strncmp(opt, "pass=", 5))
                strcpy(mountopts->pass, &opt[5]);
            else if (!strncmp(opt, "verbose", 7))
                verbose = 1;
            else {
                printf("Unknown option %s\n", opt);
                print_usage(argv[0]);
            }
            
            arg = NULL;
        }
    }

    /*
     * mount コマンドに渡されたリソース部分から ftp サーバ名と、マウントする
     * ベースディレクトリを解釈する。
     */
    
    if((server_and_path = strstr(resource, "://")) == NULL){
        printf("No protocol specified\n");        
        print_usage(argv[0]);
    }
    server_and_path += 4;
    if(strlen (resource) < server_and_path - resource){
        printf("No server name specified\n");        
        print_usage(argv[0]);
    }
    if((path = strstr(server_and_path, "/")) == NULL){
        printf("No pathname specified\n");
        print_usage(argv[0]);
    }
    n = strcspn(server_and_path, "/");
    strncpy(mountopts->server, resource, path - resource);
    strcpy(mountopts->basepath, path);

    if(strlen(mountopts->basepath)==0)
        strcpy(mountopts->basepath, "/");
    
    if(verbose){
        printf("user = %s\n", mountopts->user);
        printf("pass = %s\n", mountopts->pass);
        printf("resoruce = %s\n",resource);
        printf("mountpint = %s\n", mountpoint);
        printf("server = %s\n", mountopts->server);
        printf("basepath = %s\n", mountopts->basepath);
    }

    if (mount(resource, mountpoint, MS_DATA, "iumfs", mountopts, sizeof (mountopts)) < 0) {
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
    printf("Usage: %s -F iumfs protocol://base_path mount_point\n", argv);
    printf("Example)\n");
    printf("    mount -F iumfs hdfs://user/username /mnt\n");
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

