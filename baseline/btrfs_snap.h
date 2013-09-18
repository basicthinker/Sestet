#ifndef ADAFS_BTRFS_SNAP_H_
#define ADAFS_BTRFS_SNAP_H_

#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <asm/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>

#define BTRFS_IOCTL_MAGIC 0x94
#define BTRFS_VOL_NAME_MAX 255

#define BTRFS_PATH_NAME_MAX 4087
struct btrfs_ioctl_vol_args {
	__s64 fd;
	char name[BTRFS_PATH_NAME_MAX + 1];
};

#define BTRFS_IOC_SNAP_CREATE _IOW(BTRFS_IOCTL_MAGIC, 1, \
				   struct btrfs_ioctl_vol_args)

//utils.c
static int open_file_or_dir(const char *fname)
{
	int ret;
	struct stat st;
	DIR *dirstream;
	int fd;

	ret = stat(fname, &st);
	if (ret < 0) {
		return -1;
	}
	if (S_ISDIR(st.st_mode)) {
		dirstream = opendir(fname);
		if (!dirstream) {
			return -2;
		}
		fd = dirfd(dirstream);
	} else {
		fd = open(fname, O_RDWR);
	}
	if (fd < 0) {
		return -3;
	}
	return fd;
}

/*
 * test if path is a subvolume:
 * this function return
 * 0-> path exists but it is not a subvolume
 * 1-> path exists and it is  a subvolume
 * -1 -> path is unaccessible
 */
static int test_issubvolume(const char *path)
{
	struct stat	st;
	int		res;

	res = stat(path, &st);
	if(res < 0 )
		return -1;

	return (st.st_ino == 256) && S_ISDIR(st.st_mode);
}

/*
 * test if path is a directory
 * this function return
 * 0-> path exists but it is not a directory
 * 1-> path exists and it is  a directory
 * -1 -> path is unaccessible
 */
static int test_isdir(const char *path)
{
	struct stat	st;
	int		res;

	res = stat(path, &st);
	if(res < 0 )
		return -1;

	return S_ISDIR(st.st_mode);
}

static int btrfs_snap(const char *subvol, char *dst)
{
	char *newname = NULL, *dstdir = NULL;
	int ret, len;
	int fd = -1, fddst = -1;
	struct btrfs_ioctl_vol_args args;
	memset(&args, 0, sizeof(args));

	ret = test_issubvolume(subvol);
	if(ret < 0){
		fprintf(stderr, "ERROR: error accessing '%s'\n", subvol);
		ret = 12;
		goto out;
	}
	if(!ret){
		fprintf(stderr, "ERROR: '%s' is not a subvolume\n", subvol);
		ret = 13;
		goto out;
	}
	fd = open_file_or_dir(subvol);

	ret = test_isdir(dst);
	if (ret == 0) {
		fprintf(stderr, "ERROR: '%s' exists and it is not a directory\n", dst);
		goto out;
	}

	if (ret > 0) {
		newname = strdup(subvol);
		newname = basename(newname);
		dstdir = dst;
	} else {
		newname = strdup(dst);
		newname = basename(newname);
		dstdir = strdup(dst);
		dstdir = dirname(dstdir);
	}

	if (!strcmp(newname, ".") || !strcmp(newname, "..") ||
	     strchr(newname, '/') ){
		fprintf(stderr, "ERROR: incorrect snapshot name ('%s')\n",
			newname);
		goto out;
	}

	len = strlen(newname);
	if (len == 0 || len >= BTRFS_VOL_NAME_MAX) {
		fprintf(stderr, "ERROR: snapshot name too long ('%s)\n",
			newname);
		goto out;
	}

	fddst = open_file_or_dir(dstdir);
	if (fddst < 0) {
		fprintf(stderr, "ERROR: can't access to '%s'\n", dstdir);
		goto out;
	}

	args.fd = fd;
	strcpy(args.name, newname);
	ret = ioctl(fddst, BTRFS_IOC_SNAP_CREATE, &args);

	if (ret < 0) {
		fprintf( stderr, "ERROR: cannot snapshot '%s'\n", subvol);
		goto out;
	}

	ret = 0;
out:
	if (fd != -1)
		close(fd);
	if (fddst != -1)
		close(fddst);
	return ret;
}

#endif

