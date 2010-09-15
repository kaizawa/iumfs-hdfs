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

/**************************************************************
 * fstest.c
 *
 * Test comand for IUMFS filesystem.
 *
 ***************************************************************/

/*
 * iumfs ファイルシステムモジュールのテスト用のコマンド。
 * テスト用に決められたディレクトリ、ファイルを読み込んで
 * 期待通りの動作をするかどうかを確認する
 *
 * TODO: 4G を超えるサイズのファイルの読み書き
 * TODO: O_APPEND フラグつきでの open
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <errno.h>
#include <strings.h>
#include <unistd.h>
#include <sys/param.h>
#include <unistd.h>
#include <ctype.h>

#define BUF 8192
#define NUM_TARGET 4

#define TEXT "testtext"
#define TEXT_1 "wr1i1tes1st1"
#define TEXT_2 "wr2i2tes2st2"
#define TEXT_3 "wr3i3tes3st3"

#define DIR_PATH   "/var/tmp/iumfsmnt"

#define FILE_NAME  "testfile"
#define FILE_PATH  DIR_PATH "/" FILE_NAME

#define NEW_FILE_NAME_1  "testnewfile1"
#define NEW_FILE_PATH_1  DIR_PATH "/" NEW_FILE_NAME_1

#define NEW_FILE_NAME_2  "testnewfile2"
#define NEW_FILE_PATH_2  DIR_PATH "/" NEW_FILE_NAME_2

#define NEW_FILE_NAME_3  "testnewfile3"
#define NEW_FILE_PATH_3  DIR_PATH "/" NEW_FILE_NAME_3

#define NEW_FILE_NAME_4  "testnewfile4"
#define NEW_FILE_PATH_4  DIR_PATH "/" NEW_FILE_NAME_4


#define NEW_FILE_NAME_6  "testnewfile6"
#define NEW_FILE_PATH_6  DIR_PATH "/" NEW_FILE_NAME_6

#define NEW_FILE_NAME_7  "testnewfile7"
#define NEW_FILE_PATH_7  DIR_PATH "/" NEW_FILE_NAME_7

#define NEW_DIR_NAME_1  "testnewdir1"
#define NEW_DIR_PATH_1  DIR_PATH "/" NEW_DIR_NAME_1

#define NEW_DIR_NAME_2  "testnewdir2"
#define NEW_DIR_PATH_2  DIR_PATH "/" NEW_DIR_NAME_2

#define NEW_DIR_NAME_3  "testnewdir3"
#define NEW_DIR_PATH_3  NEW_DIR_PATH_1 "/" NEW_DIR_NAME_3

#define DUMMY_DIR_NAME_1  "testdummydir"
#define DUMMY_DIR_PATH_1  DIR_PATH "/" DUMMY_DIR_NAME_1

#define DUMMY_FILE_NAME_1  "testdummyfile"
#define DUMMY_FILE_PATH_1  DIR_PATH "/" DUMMY_FILE_NAME_1

#define NEW_FILE_NAME_5  "testnewfile5"
#define NEW_FILE_PATH_5  NEW_DIR_PATH_2 "/" NEW_FILE_NAME_5

int getattr_test();
int readdir_test();
int open_test();
int read_test();
int write_test();
int do_read(const char *, const uchar_t *, offset_t, size_t, size_t *);
int do_write(const char *, const uchar_t *, offset_t, size_t);
int do_getattr(const char *);
int do_readdir(const char *, const char *);
int do_open(const char *, int , mode_t );
int get_filesize(const char *);
int remove_test();
int mkdir_test();
int rmdir_test();
int do_remove(const char *);
int do_mkdir(const char *, mode_t);
int do_rmdir(const char *);
int do_creat(const char *, mode_t );

int pagesize;

char *targets[] = {"getattr", "readdir", "open", "read", "write"};

int
main(int argc, char *argv[])
{
    int ret = 0;

    if (argc != 2) {
        printf("Usage: %s [getattr|readdir|open|read]\n", argv[0]);
        ret = 1;
        goto out;
    }

    pagesize = getpagesize();

    if (strcmp(argv[1], "getattr") == 0) {
        ret = getattr_test();
    } else if (strcmp(argv[1], "readdir") == 0) {
        ret = readdir_test();
    } else if (strcmp(argv[1], "open") == 0) {
        ret = open_test();
    } else if (strcmp(argv[1], "read") == 0) {
        ret = read_test();
    } else if (strcmp(argv[1], "write") == 0) {
        ret = write_test();
    } else if (strcmp(argv[1], "remove") == 0) {
        ret = remove_test();
    }else if (strcmp(argv[1], "mkdir") == 0) {
        ret = mkdir_test();
    }else if (strcmp(argv[1], "rmdir") == 0) {
        ret = rmdir_test();
    }
out:
    exit(ret);
}

int
open_test()
{
    int ret = 0;

    printf("open_test: Start\n");    
    
    /*
     * Create new file
     */
    printf("\tbegin: Create new file(%s) \n", NEW_FILE_PATH_1);
    if((ret = do_creat(NEW_FILE_PATH_1, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) != 0){            
        goto out;
    }
    printf("\tend: Success\n");


    /*
     * Open Existing file
     */
    printf("\tbegin: Open existing file(%s) with O_RDONLY flag\n", NEW_FILE_PATH_1);
    if((ret = do_open(NEW_FILE_PATH_1, O_RDONLY, 0)) != 0){
        goto out;
    }
    printf("\tend: Success\n");

    /*
     * Open new file with O_CREAT
     */
    printf("\tbegin: Open new file(%s) with O_CREAT flag\n", NEW_FILE_PATH_2);
    if((ret = do_open(NEW_FILE_PATH_2, O_WRONLY | O_CREAT | O_TRUNC| O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) != 0){
        goto out;
    }
    printf("\tend: Success\n");    

    /*
     * Open Existing file with O_EXCL & O_CREATE flag
     */
    printf("\tbegin: Open existing file(%s) with O_EXCL & O_CREAT flag\n", NEW_FILE_PATH_1);
    if((do_open(NEW_FILE_PATH_1, O_WRONLY | O_CREAT | O_TRUNC| O_EXCL, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) != EEXIST){
        ret = 1;
        goto out;
    }
    printf("\tend: Success\n");


    /*
     * Create new file on newly created dir
     */
    printf("\tbegin: Open new file(%s) on new directory\n", NEW_FILE_PATH_5);
    if((ret = do_creat(NEW_FILE_PATH_5, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) != 0){            
        goto out;
    }
    printf("\tend: Success\n");    

out:
    if (ret) {
        printf("\tend: Failed\n");
    }
    printf("open_test: Finish\n");
    return ret;
}

int
do_open(const char *filepath, int oflag, mode_t mode)
{
    int fd = -1;

    fd = open(filepath, oflag, mode);
    if (fd < 0) {
        printf("\t\tdo_open: open(%s): %s\n", filepath, strerror(errno));
        return (errno);
    }
    close(fd);
    return 0;
}

int
do_creat(const char *filepath, mode_t mode)
{
    int fd = -1;

    fd = creat(filepath, mode);
    if (fd < 0) {
        printf("\t\tdo_creat: creat(%s): %s\n", filepath, strerror(errno));
        return (errno);
    }
    close(fd);
    return 0;
}

int getattr_test() {
    int ret = 0;
    printf("getattr_test: Start\n");

    printf("\tbegin: Check entry type of %s\n", NEW_DIR_PATH_1);
    if((ret = do_getattr(NEW_DIR_PATH_1)) != 0) {
        goto out;
    }
    printf("\tend: Success\n");

out:
    if (ret) {
        printf("\tend: Failed\n");
    }
    printf("getattr_test: Finish\n");
    return ret;
}

int 
do_getattr(const char *pathname)
{
    struct stat st[1];

    if ((stat(pathname, st)) < 0) {
        printf("\t\tdo_getattr: stat(%s): %s\n", pathname, strerror(errno));
        return(errno);
    }

    if (S_ISDIR(st->st_mode) == 0) {
        printf("\t\tdo_getattr: %s is not recognized as a directory\n", pathname);
        return(errno);
    }
    return(0);
}

int
readdir_test()
{
    int ret = 0;

    printf("readdir_test: Start\n");

    printf("\tbegin: look for \".\"\n");
    if((ret = do_readdir(DIR_PATH, ".")) != 0){
        goto out;
    }
    printf("\tend: Success\n");

    printf("\tbegin: look for \"..\"\n");
    if((ret = do_readdir(DIR_PATH, "..")) != 0){
        goto out;
    }
    printf("\tend: Success\n");

    printf("\tbegin: look for \"%s\"\n", FILE_NAME);
    if((ret = do_readdir(DIR_PATH, NEW_FILE_NAME_1)) != 0){
        goto out;
    }
    printf("\tend: Success\n");

out:
    if (ret) {
        printf("\tend: Failed\n");
    }
    printf("getattr_test: Finish\n");
    return ret;
}

int
do_readdir(const char *dirpath, const char *entryname)
{
    DIR *dirp = NULL;
    struct dirent *direntp = NULL;
    int entry_found = 0;

    if ((dirp = opendir(dirpath)) == NULL) {
        printf("\t\tdo_readdir: opendir(%s): %s\n", dirpath, strerror(errno));
        return(errno);
    }

    do {
        if ((direntp = readdir(dirp)) != NULL) {
            if (strcmp(direntp->d_name, entryname) == 0) {
                printf("\t\tdo_readdir: found [\"%s\"] \n", entryname);
                entry_found = 1;
                break;
            }
        }
    } while (direntp != NULL);

    if (!entry_found) {
        printf("\t\tdo_readdir: can not find  [\"%s\"]\n", entryname);
        return(errno);
    }
    return(0);
}

int
read_test()
{
    int ret = 0;
    offset_t off = 0;
    uchar_t *buf = NULL;
    size_t size = 0;
    size_t rsize = 0;
    
    printf("read_test: Start\n");

    /*
     * ファイルサイズ以内のページファイルサイズ以下のサイズの読み込みテスト
     */
    printf("\tbegin: Read bytes less than file size at offset 0(%s)\n", NEW_FILE_PATH_1);
    if((ret = do_read(NEW_FILE_PATH_1, (uchar_t *) TEXT_1, 0, strlen(TEXT_1),&rsize)) != 0){
        goto out;
    }
    printf("\tend: Success\n");

    /*
     * ファイルサイズを超えるオフセットからの読み込みテスト
     * 期待される結果は､おそらく pread が 0 を返すこと。
     */
    off = 1024;
    size = 1024;
    printf("\tbegin: Read bytes at offset(%lld) greater than file size(%s)\n", off, NEW_FILE_PATH_1);
    if((ret = do_read(NEW_FILE_PATH_1, buf, off, size, &rsize)) != 0){
        goto out;
    }
    printf("\tend: Success\n");    

  out :
    if (ret) {
        printf("\tend: Failed");
    }
    printf("read_test: end\n");
    return ret;
}

int
do_read(const char *path, const uchar_t *data, offset_t off, size_t size, size_t *rsizep)
{
    int fd = -1;
    int i;
    uchar_t *buf;
    struct stat st[1];
    int no_compare = 0;

    /*
     * 引数 data に NULLを渡された場合はデータの比較の必要はない。
     */ 
    if(data == NULL){
        no_compare = 1;
    }

    buf = malloc(size);
    if (buf == NULL) {
        printf("\t\tdo_read: malloc(): %s\n", strerror(errno));
        return(errno);
    }

    fd = open64(path, O_RDONLY);
    if (fd < 0) {
        printf("\t\tdo_read: open(%s): %s\n", path, strerror(errno));
        return(errno);
    }

    if ((fstat(fd, st)) < 0) {
        printf("\t\tdo_read: fstat(%s): %s\n", path, strerror(errno));
        return(errno);
    }

    if ((*rsizep = pread(fd, buf, size, off)) < 0) {
        printf("\t\tread_tesst: pread(%s): %s\n", path, strerror(errno));
        return(errno);
    }

    printf("\t\tdo_read: read %zd bytes from offset %lld.\n", *rsizep, off);    

    if(no_compare){
        return(0);
    }

    if (*rsizep != size) {
        printf("\t\tdo_read: read %zd bytes of data. (!= %zd bytes. Wrong size!!)\n", *rsizep, size);
        return(-1);
    }

    if (bcmp(buf, data, size) != 0) {
        printf("\t\tdo_read: bcmp failed\n");
        for (i = 0; i < size; i++) {
            if (isprint(buf[i]))
                printf("\t\tbuf[%d]= 0x%0x \"%c\"\n", i, buf[i], buf[i]);
            else
                printf("\t\tbuf[%d]= 0x%0x \n", i, buf[i]);                    
        }
        free(buf);
        return(-1);
    }
    free(buf);    
    printf("\t\tdo_read: success.\n");
    return (0);
}

int
write_test()
{
    int ret = 0;
    uchar_t *dummydata = NULL;
//    size_t datasize = 0;    
    size_t rsize = 0;
    offset_t offset = 0;
#ifdef REWRITABLE_FILESYSTEM        
    size_t filesize = 0;
#endif

    printf("write_test: Start\n");

    /*
     * 空の既存ファイルに小さな文字列を書き込むテスト
     */    
    printf("\tbegin: Write small bytes to exising empty file(%s)\n",NEW_FILE_PATH_1);
    if ((ret = do_write(NEW_FILE_PATH_1, (uchar_t *) TEXT_1,
            0, strlen(TEXT_1))) != 0) {
        goto out;
    }

    if ((ret = do_read(NEW_FILE_PATH_1, (uchar_t *) TEXT_1,
                       0, strlen(TEXT_1),&rsize)) != 0) {
        goto out;
    }
    printf("\tend: Success\n");

    /*
     * 既存ファイルに追記するテスト
     */
    offset = get_filesize(NEW_FILE_PATH_1);    
    printf("\tbegin: append small bytes to exising file(%s)\n",NEW_FILE_PATH_1);
    if ((ret = do_write(NEW_FILE_PATH_1, (uchar_t *) TEXT_2,
                        offset, strlen(TEXT_2))) != 0) {
        goto out;
    }

    if ((ret = do_read(NEW_FILE_PATH_1, (uchar_t *) TEXT_2,
                       offset, strlen(TEXT_2),&rsize)) != 0) {
        goto out;
    }
    printf("\tend: Success\n");

#ifdef REWRITABLE_FILESYSTEM
    
    /*
     * PAGE サイズを超える(5KB)書き込みテスト
     */    
    printf("\tbegin: Write bytes greater than PAGESIZE(%d) to new file(%s)\n",pagesize, NEW_FILE_PATH_3);
    datasize = 5 * 1024;
    dummydata = malloc(datasize);
    memset(dummydata, 0x77, datasize);
    if ((ret = do_write(NEW_FILE_PATH_3, dummydata, 0, datasize)) != 0) {
        goto out;
    }

    if ((ret = do_read(NEW_FILE_PATH_3, dummydata, 0, datasize, &rsize)) != 0) {
        goto out;
    }
    free(dummydata);    
    printf("\tend: Success\n");

    /*
     * MAXBSIZE サイズを超える(16KB)書き込みテスト
     */    
    printf("\tbegin: Write bytes greater than MAXBSIZE(%d) to new file(%s)\n",MAXBSIZE, NEW_FILE_PATH_4);
    datasize = MAXBSIZE * 2;
    dummydata = malloc(datasize);
    memset(dummydata, 0x77, datasize);
    if ((ret = do_write(NEW_FILE_PATH_4, dummydata, 0, datasize)) != 0) {
        goto out;
    }

    if ((ret = do_read(NEW_FILE_PATH_4, dummydata, 0, datasize,&rsize)) != 0) {
        goto out;
    }
    free(dummydata);    
    printf("\tend: Success\n");

//#ifdef REWRITABLE_FILESYSTEM
    // HDFS は オフセット指定の書き込みができないので以下のテストは行えない。    
    /*
     * PAGE サイズ以内で、ファイルサイズ以上の書き込みテスト
    */    
    printf("\tbegin: Write bytes less than PAGESIZE(%d) to new file(%s)\n", pagesize, NEW_FILE_PATH_1);
    if ((ret = do_write(NEW_FILE_PATH_1, (uchar_t *) TEXT_1,
            0, strlen(TEXT_2))) != 0) {
        goto out;
    }
    if ((ret = do_read(NEW_FILE_PATH_1, (uchar_t *) TEXT_1,
                       0, strlen(TEXT_1),&rsize)) != 0) {
        goto out;
    }
    printf("\tend: Success\n");

    /*
     * ファイルサイズを 9KB (>PAGESIZE * 2) 超えるオフセットに対する書き込み
     */
    printf("\tbegin: Write bytes where offset is beyond PAGESIZE(%d) from files size to existing file(%s)\n",pagesize, NEW_FILE_PATH_1);
    if((filesize = get_filesize(NEW_FILE_PATH_1)) < 0){
        ret = 1;
        goto out;
    }
    offset = filesize + (pagesize * 2) + 1024;;
    if ((ret = do_write(NEW_FILE_PATH_1, (uchar_t *)TEXT_1,
                        offset, strlen(TEXT_1))) != 0) {
        goto out;
    }
    if ((ret = do_read(NEW_FILE_PATH_1, (uchar_t *)TEXT_1,
                       offset, strlen(TEXT_1),&rsize)) != 0) {
        goto out;
    }
    printf("\tend: Success\n");    

    /*
     * ファイルサイズを 17KB (>MAXBSIZE * 2) 超えるオフセットに対する書き込み
     */
    printf("\tbegin: Write bytes where offset is beyond MAXBSIZE(%d) from files size to existing file(%s)\n",MAXBSIZE, NEW_FILE_PATH_1);
    if((filesize = get_filesize(NEW_FILE_PATH_1)) < 0){        
        ret = 1;
        goto out;
    }
    offset = filesize + (MAXBSIZE * 2)  + 1024;    
    if ((ret = do_write(NEW_FILE_PATH_1, (uchar_t *)TEXT_2,
                        offset, strlen(TEXT_2))) != 0) {
        goto out;
    }
    if ((ret = do_read(NEW_FILE_PATH_1, (uchar_t *)TEXT_2,
                       offset, strlen(TEXT_2),&rsize)) != 0) {
        goto out;
    }    
    printf("\tend: Success\n");
#endif // #ifdef REWRITABLE_FILESYSTEM
    
  out:
    if(dummydata){
        free(dummydata);
    }
    if (ret) {
        printf("\tend: Failed\n");
    }
    printf("write_test: Finish\n");    
    return ret;
}

int
do_write(const char *path, const uchar_t *data, offset_t off, size_t size)
{
    int fd = -1;
    int ret = 0;
    ssize_t wsize = 0;
    size_t newsize;
    struct stat st[1];

    //fd = open64(path, O_RDWR|O_CREAT|O_TRUNC);
    fd = open64(path, O_RDWR|O_TRUNC|O_CREAT);
    if (fd < 0) {
        printf("\t\tdo_write: open(%s): %s\n", path, strerror(errno));
        ret = errno;
        goto out;
    }

    if ((wsize = pwrite(fd, data, size, off)) < 0) {
        printf("\t\tdo_write: pwrite(%s): %s\n", path, strerror(errno));
        ret = errno;
        goto out;
    }

    if (wsize != size) {
        printf("\t\tdo_write: wrote %zd bytes of data. (!= %zd bytes. Wrong size!!)\n", wsize, size);
        ret = errno;
        goto out;
    }
    printf("\t\tdo_write: wrote %zd bytes of data at offset %lld.\n", wsize, off);

    if ((fstat(fd, st)) < 0) {
        printf("\t\tdo_write: fstat(%s): %s\n", path, strerror(errno));
        ret = errno;
        goto out;
    }

    newsize = st->st_size;
    printf("\t\tdo_write: new file size = %zd bytes\n", newsize);

out:
    if (fd != -1) {
        close(fd);
    }
    return (ret);
}

int
get_filesize(const char *pathname)
{
    struct stat st[1];
    size_t size = 0;

    if ((stat(pathname, st)) < 0) {
        printf("\t\tget_filesize: stat(%s): %s\n", pathname, strerror(errno));
        return(-1);
    }
    size = st->st_size;
    
    printf("\t\tget_filesize: file size = %zd\n", size);
    return(size);
}

int
remove_test()
{
    int ret = 0;

    printf("remove_test: Start\n");    
    
    /*
     * Remove existing file 
     */
    printf("\tbegin: Remove existing file(%s)\n", NEW_FILE_PATH_2);
    if((ret = do_remove(NEW_FILE_PATH_2)) != 0){
        goto out;        
    }
    printf("\tend: Success\n");
    
    /*
     * Remove non-existing file 
     */
    printf("\tbegin: Remove non-existing file(%s)\n", DUMMY_FILE_PATH_1);
    if((ret = do_remove(DUMMY_FILE_PATH_1)) == 0){
        printf("\tNon-existing file was removed without error...\n");
        ret = 1;
        goto out;        
    }
    ret = 0;    
    printf("\tend: Success\n");    

    /*
     * Remove directory
     */
    printf("\tbegin: Remove directory (%s) using unlink\n", NEW_DIR_PATH_1);
    ret = do_remove(NEW_DIR_PATH_1);
    switch(ret){
        case 0:
            printf("\tdirectory was removed!!\n");
            ret = 1;
            goto out;
        case EISDIR:
            break;
        default:
            printf("\terror is not EISDIR, but \"%s\"\n", strerror(ret));
    }
    ret = 0;
    printf("\tend: Success\n");                
    
out:
    if (ret) {
        printf("\tend: Failed\n");
    }
    printf("remove_test: Finish\n");
    return ret;
}

int
do_remove(const char *filepath)
{
    if(unlink(filepath) < 0){
        printf("\t\tdo_remove: unlink(%s): %s\n", filepath, strerror(errno));
        return (errno);
    }
    return 0;
}

int
mkdir_test()
{
    int ret = 0;

    printf("mkdir_test: Start\n");
    /*
     * Create new directory 
     */
    printf("\tbegin: Create new directory(%s)\n", NEW_DIR_PATH_1);
    if((ret = do_mkdir(NEW_DIR_PATH_1, 
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) != 0){        
        goto out;        
    }
    printf("\tend: Success\n");

    /*
     * re-Create new directory
     */
    printf("\tbegin: try to re-create existing directory(%s)\n", NEW_DIR_PATH_1);
    ret = do_mkdir(NEW_DIR_PATH_1, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    switch(ret){
        case 0:
            printf("\texisting directory can be re-created!!\n");
            ret = 1;
            goto out;
        case EEXIST:
            break;
        default:
            printf("\terror is not EEXIT, but %s\n", strerror(ret));            
    }
    ret = 0;
    printf("\tend: Success\n");
    
    /*
     * Create another new directory 
     */
    printf("\tbegin: Create new directory(%s)\n", NEW_DIR_PATH_2);
    if((ret = do_mkdir(NEW_DIR_PATH_2, 
                      S_IRUSR | S_IXUSR | S_IWUSR | S_IRGRP | S_IXUSR | S_IROTH |S_IXUSR )) != 0){        
        goto out;        
    }
    printf("\tend: Success\n");    
    
out:
    if (ret) {
        printf("\tend: Failed\n");
    }
    printf("mkdir_test: Finish\n");
    return ret;
}

int
do_mkdir(const char *dirpath, mode_t mode)
{
    if (mkdir(dirpath, mode) < 0) {
        printf("\t\tdo_mkdir: mkdir(%s): %s\n", dirpath, strerror(errno));
        return (errno);
    }
    return 0;
}

int
rmdir_test()
{
    int ret = 0;

    printf("rmdir_test: Start\n");
    /*
     * Remove existing directory (=NEW_DIR_PATH_1)
     * which was created within mkdir_test
     */
    printf("\tbegin: Remove existing directory(%s)\n", NEW_DIR_PATH_1);
    if((ret = do_rmdir(NEW_DIR_PATH_1)) != 0){
        goto out;        
    }
    printf("\tend: Success\n");

    /*
     * Remove non-existing directory
     */
    printf("\tbegin: Remove non-existing directory(%s)\n", DUMMY_DIR_PATH_1);
    ret = do_rmdir(DUMMY_DIR_PATH_1);
    switch(ret){
        case 0:
            printf("\tnon existing directory was removed!\n");
            ret = 1;
            goto out;
        case ENOENT:
            break;
        default:
            printf("\terror is not ENOENT, but %s\n", strerror(ret));                        
    }                
    ret = 0;
    printf("\tend: Success\n");
    
    /*
     * Remove non-empty directory
     */
    printf("\tbegin: Remove non-empty directory(%s)\n", NEW_DIR_PATH_2);
    ret = do_rmdir(NEW_DIR_PATH_2);
    switch(ret){
        case 0:
            printf("\tnon-empty directory was removed!\n");
            ret = 1;
            goto out;
        case ENOTEMPTY:
            break;
        default:
            printf("\terror is not ENOTEMPTY, but %s\n", strerror(ret));                        
    }        
    ret = 0;
    printf("\tend: Success\n");        
    
out:
    if (ret) {
        printf("\tend: Failed\n");
    } 
    printf("rmdir_test: Finish\n");
    return ret;
}

int
do_rmdir(const char *dirpath)
{
    if (rmdir(dirpath) < 0) {
        printf("\t\tdo_rmdir: rmdir(%s): %s\n", dirpath, strerror(errno));
        return (errno);
    }
    return 0;
}
