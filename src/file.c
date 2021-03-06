/* 
 * Copyright (c) 2015-2016, Gregory M. Kurtzer. All rights reserved.
 * 
 * “Singularity” Copyright (c) 2016, The Regents of the University of California,
 * through Lawrence Berkeley National Laboratory (subject to receipt of any
 * required approvals from the U.S. Dept. of Energy).  All rights reserved.
 * 
 * This software is licensed under a customized 3-clause BSD license.  Please
 * consult LICENSE file distributed with the sources of this project regarding
 * your rights to use or distribute this software.
 * 
 * NOTICE.  This Software was developed under funding from the U.S. Department of
 * Energy and the U.S. Government consequently retains certain rights. As such,
 * the U.S. Government has been granted for itself and others acting on its
 * behalf a paid-up, nonexclusive, irrevocable, worldwide license in the Software
 * to reproduce, distribute copies to the public, prepare derivative works, and
 * perform publicly and display publicly, and to permit other to do so. 
 * 
 */


#define _XOPEN_SOURCE 500 // For nftw
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h> 
#include <string.h>
#include <fcntl.h>  
#include <libgen.h>
#include <assert.h>
#include <ftw.h>
#include <time.h>

#include "config.h"
#include "util.h"
#include "message.h"


char *file_id(char *path) {
    struct stat filestat;
    char *ret;
    uid_t uid = getuid();

    message(DEBUG, "Called file_id(%s)\n", path);

    // Stat path
    if (lstat(path, &filestat) < 0) {
        return(NULL);
    }

    ret = (char *) malloc(128);
    snprintf(ret, 128, "%d.%d.%lu", (int)uid, (int)filestat.st_dev, (long unsigned)filestat.st_ino); // Flawfinder: ignore

    message(VERBOSE2, "Generated file_id: %s\n", ret);

    message(DEBUG, "Returning file_id(%s) = %s\n", path, ret);
    return(ret);
}


int is_file(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISREG(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_fifo(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISFIFO(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_link(char *path) {
    struct stat filestat;

    // Stat path
    if (lstat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISLNK(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_dir(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISDIR(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_exec(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( (S_IXUSR & filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}

int is_owner(char *path, uid_t uid) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    if ( uid == (int)filestat.st_uid ) {
        return(0);
    }

    return(-1);
}

int is_blk(char *path) {
    struct stat filestat;

    // Stat path
    if (stat(path, &filestat) < 0) {
        return(-1);
    }

    // Test path
    if ( S_ISBLK(filestat.st_mode) ) {
        return(0);
    }

    return(-1);
}


int s_mkpath(char *dir, mode_t mode) {
    if (!dir) {
        return(-1);
    }

    if (strlength(dir, 2) == 1 && dir[0] == '/') {
        return(0);
    }

    if ( is_dir(dir) == 0 ) {
        // Directory already exists, stop...
        return(0);
    }

    if ( s_mkpath(dirname(strdupa(dir)), mode) < 0 ) {
        // Return if priors failed
        return(-1);
    }

    message(DEBUG, "Creating directory: %s\n", dir);
    if ( mkdir(dir, mode) < 0 ) {
        message(ERROR, "Could not create directory %s: %s\n", dir, strerror(errno));
        return(-1);
    }

    return(0);
}

int _unlink(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
//    printf("remove(%s)\n", fpath);
    return(remove(fpath));
}

int s_rmdir(char *dir) {

    message(DEBUG, "Removing dirctory: %s\n", dir);
    return(nftw(dir, _unlink, 32, FTW_DEPTH));
}

int copy_file(char * source, char * dest) {
    struct stat filestat;
    int c;
    FILE * fp_s;
    FILE * fp_d;

    message(DEBUG, "Called copy_file(%s, %s)\n", source, dest);

    if ( is_file(source) < 0 ) {
        message(ERROR, "Could not copy from non-existant source: %s\n", source);
        return(-1);
    }

    message(DEBUG, "Opening source file: %s\n", source);
    if ( ( fp_s = fopen(source, "r") ) == NULL ) { // Flawfinder: ignore
        message(ERROR, "Could not read %s: %s\n", source, strerror(errno));
        return(-1);
    }

    message(DEBUG, "Opening destination file: %s\n", dest);
    if ( ( fp_d = fopen(dest, "w") ) == NULL ) { // Flawfinder: ignore
        fclose(fp_s);
        message(ERROR, "Could not write %s: %s\n", dest, strerror(errno));
        return(-1);
    }

    message(DEBUG, "Calling fstat() on source file descriptor: %d\n", fileno(fp_s));
    if ( fstat(fileno(fp_s), &filestat) < 0 ) {
        message(ERROR, "Could not fstat() on %s: %s\n", source, strerror(errno));
        return(-1);
    }

    message(DEBUG, "Cloning permission string of source to dest\n");
    if ( fchmod(fileno(fp_d), filestat.st_mode) < 0 ) {
        message(ERROR, "Could not set permission mode on %s: %s\n", dest, strerror(errno));
        return(-1);
    }

    message(DEBUG, "Copying file data...\n");
    while ( ( c = fgetc(fp_s) ) != EOF ) { // Flawfinder: ignore (checked boundries)
        fputc(c, fp_d);
    }

    message(DEBUG, "Done copying data, closing file pointers\n");
    fclose(fp_s);
    fclose(fp_d);

    message(DEBUG, "Returning copy_file(%s, %s) = 0\n", source, dest);

    return(0);
}


int fileput(char *path, char *string) {
    FILE *fd;

    message(DEBUG, "Called fileput(%s, %s)\n", path, string);
    if ( ( fd = fopen(path, "w") ) == NULL ) { // Flawfinder: ignore
        message(ERROR, "Could not write to %s: %s\n", path, strerror(errno));
        return(-1);
    }

    fprintf(fd, "%s", string);
    fclose(fd);

    return(0);
}

char *filecat(char *path) {
    char *ret;
    FILE *fd;
    int c;
    long length;
    long pos = 0;

    message(DEBUG, "Called filecat(%s)\n", path);
    
    if ( is_file(path) < 0 ) {
        message(ERROR, "Could not find %s\n", path);
        return(NULL);
    }

    if ( ( fd = fopen(path, "r") ) == NULL ) { // Flawfinder: ignore
        message(ERROR, "Could not read from %s: %s\n", path, strerror(errno));
        return(NULL);
    }


    if ( fseek(fd, 0L, SEEK_END) < 0 ) {
        message(ERROR, "Could not seek to end of file %s: %s\n", path, strerror(errno));
        return(NULL);
    }

    length = ftell(fd);

    rewind(fd);

    ret = (char *) malloc(length+1);

    while ( ( c = fgetc(fd) ) != EOF ) { // Flawfinder: ignore (checked boundries)
        ret[pos] = c;
        pos++;
    }
    ret[pos] = '\0';

    fclose(fd);

    return(ret);
}

char * container_basedir(char *containerdir, char *dir) {
    char * testdir = strdup(dir);
    char * prevdir = NULL;
    if ( containerdir == NULL || dir == NULL ) {
        return(NULL);
    }

    while ( testdir != NULL && ( strcmp(testdir, "/") != 0 ) ) {
        if ( is_dir(joinpath(containerdir, testdir)) == 0 ) {
            return(testdir);
        }
        prevdir = strdup(testdir);
        testdir = dirname(strdup(testdir));
    }
    return(prevdir);
}

