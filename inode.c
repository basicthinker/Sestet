/*
 * Resizable RAM log-enhanced filesystem for Linux.
 *
 * Copyright (C) 2013 Jinglei Ren <jinglei.ren@stanzax.org>
 *               2000 Linus Torvalds.
 *               2000 Transmeta Corp.
 *
 * Usage limits added by David Gibson, Linuxcare Australia.
 * This file is released under the GPL.
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/backing-dev.h>
#include <linux/sched.h>
#include <linux/parser.h>
#include <linux/magic.h>
#include <linux/slab.h>
#include <asm/uaccess.h>

#include "hashtable.h"
#include "rffs.h"

#define RAMFS_DEFAULT_MODE	0755

static const struct super_operations rffs_ops;
static const struct inode_operations rffs_dir_inode_operations;

static struct backing_dev_info rffs_backing_dev_info = {
        .name = "rffs",
        .ra_pages = 0, /* No readahead */
        .capabilities = BDI_CAP_NO_ACCT_AND_WRITEBACK | BDI_CAP_MAP_DIRECT
                | BDI_CAP_MAP_COPY | BDI_CAP_READ_MAP | BDI_CAP_WRITE_MAP
                | BDI_CAP_EXEC_MAP,
};

DEFINE_HASHTABLE(inode_map, 8);

struct inode *rffs_get_inode(struct super_block *sb, const struct inode *dir,
        int mode, dev_t dev) {
    struct inode * inode = new_inode(sb);

    if (inode) {
        inode->i_ino = get_next_ino();
        inode_init_owner(inode, dir, mode);
        inode->i_mapping->a_ops = &rffs_aops;
        inode->i_mapping->backing_dev_info = &rffs_backing_dev_info;
        mapping_set_gfp_mask(inode->i_mapping, GFP_HIGHUSER);
        mapping_set_unevictable(inode->i_mapping);
        inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
        switch (mode & S_IFMT) {
        default:
            init_special_inode(inode, mode, dev);
            break;
        case S_IFREG:
            inode->i_op = &rffs_file_inode_operations;
            inode->i_fop = &rffs_file_operations;
            break;
        case S_IFDIR:
            inode->i_op = &rffs_dir_inode_operations;
            inode->i_fop = &simple_dir_operations;

            /* directory inodes start off with i_nlink == 2 (for "." entry) */
            inc_nlink(inode);
            break;
        case S_IFLNK:
            inode->i_op = &page_symlink_inode_operations;
            break;
        }
    }
    return inode;
}

/*
 * File creation. Allocate an inode, and we're done..
 */
/* SMP-safe */
static int rffs_mknod(struct inode *dir, struct dentry *dentry, int mode,
        dev_t dev) {
    struct inode * inode = rffs_get_inode(dir->i_sb, dir, mode, dev);
    int error = -ENOSPC;

    if (inode) {
        d_instantiate(dentry, inode);
        dget(dentry); /* Extra count - pin the dentry in core */
        error = 0;
        dir->i_mtime = dir->i_ctime = CURRENT_TIME;
    }
    return error;
}

static int rffs_mkdir(struct inode * dir, struct dentry * dentry, int mode) {
    int retval = rffs_mknod(dir, dentry, mode | S_IFDIR, 0);
    if (!retval)
        inc_nlink(dir);
    return retval;
}

static int rffs_create(struct inode *dir, struct dentry *dentry, int mode,
        struct nameidata *nd) {
    return rffs_mknod(dir, dentry, mode | S_IFREG, 0);
}

static int rffs_symlink(struct inode * dir, struct dentry *dentry,
        const char * symname) {
    struct inode *inode;
    int error = -ENOSPC;

    inode = rffs_get_inode(dir->i_sb, dir, S_IFLNK | S_IRWXUGO, 0);
    if (inode) {
        int l = strlen(symname) + 1;
        error = page_symlink(inode, symname, l);
        if (!error) {
            d_instantiate(dentry, inode);
            dget(dentry);
            dir->i_mtime = dir->i_ctime = CURRENT_TIME;
        } else
            iput(inode);
    }
    return error;
}

static const struct inode_operations rffs_dir_inode_operations = {
    .create = rffs_create,
    .lookup = simple_lookup,
    .link = simple_link,
    .unlink = simple_unlink,
    .symlink = rffs_symlink,
    .mkdir = rffs_mkdir,
    .rmdir = simple_rmdir,
    .mknod = rffs_mknod,
    .rename = simple_rename,
};

static const struct super_operations rffs_ops = {
    .statfs = simple_statfs,
    .drop_inode = generic_delete_inode,
    .show_options = generic_show_options,
};

struct rffs_mount_opts {
    umode_t mode;
};

enum {
    Opt_mode, Opt_err
};

static const match_table_t tokens =
        { { Opt_mode, "mode=%o" }, { Opt_err, NULL } };

struct rffs_fs_info {
    struct rffs_mount_opts mount_opts;
};

static int rffs_parse_options(char *data, struct rffs_mount_opts *opts) {
    substring_t args[MAX_OPT_ARGS];
    int option;
    int token;
    char *p;

    opts->mode = RAMFS_DEFAULT_MODE;

    while ((p = strsep(&data, ",")) != NULL ) {
        if (!*p)
            continue;

        token = match_token(p, tokens, args);
        switch (token) {
        case Opt_mode:
            if (match_octal(&args[0], &option))
                return -EINVAL;
            opts->mode = option & S_IALLUGO;
            break;
            /*
             * We might like to report bad mount options here;
             * but traditionally rffs has ignored all mount options,
             * and as it is used as a !CONFIG_SHMEM simple substitute
             * for tmpfs, better continue to ignore other mount options.
             */
        }
    }

    return 0;
}

int rffs_fill_super(struct super_block *sb, void *data, int silent) {
    struct rffs_fs_info *fsi;
    struct inode *inode = NULL;
    struct dentry *root;
    int err;

    save_mount_options(sb, data);

    fsi = kzalloc(sizeof(struct rffs_fs_info), GFP_KERNEL);
    sb->s_fs_info = fsi;
    if (!fsi) {
        err = -ENOMEM;
        goto fail;
    }

    err = rffs_parse_options(data, &fsi->mount_opts);
    if (err)
        goto fail;

    sb->s_maxbytes = MAX_LFS_FILESIZE;
    sb->s_blocksize = PAGE_CACHE_SIZE;
    sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
    sb->s_magic = RAMFS_MAGIC;
    sb->s_op = &rffs_ops;
    sb->s_time_gran = 1;

    inode = rffs_get_inode(sb, NULL, S_IFDIR | fsi->mount_opts.mode, 0);
    if (!inode) {
        err = -ENOMEM;
        goto fail;
    }

    root = d_alloc_root(inode);
    sb->s_root = root;
    if (!root) {
        err = -ENOMEM;
        goto fail;
    }

    return 0;
    fail: kfree(fsi);
    sb->s_fs_info = NULL;
    iput(inode);
    return err;
}

struct dentry *rffs_mount(struct file_system_type *fs_type, int flags,
        const char *dev_name, void *data) {
    return mount_nodev(fs_type, flags, data, rffs_fill_super);
}

static void rffs_kill_sb(struct super_block *sb) {
    kfree(sb->s_fs_info);
    kill_litter_super(sb);
}

static struct file_system_type rffs_fs_type = {
        .name = "rffs",
        .mount = rffs_mount,
        .kill_sb = rffs_kill_sb,
};

static int __init init_rffs_fs(void) {
    return register_filesystem(&rffs_fs_type);
}

static void __exit exit_rffs_fs(void) {
    unregister_filesystem(&rffs_fs_type);
}

module_init(init_rffs_fs)
module_exit(exit_rffs_fs)

MODULE_LICENSE("GPL");
