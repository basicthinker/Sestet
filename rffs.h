#ifndef SESTET_RFFS_H_
#define SESTET_RFFS_H_

struct inodep {
    struct inode *ptr;
    struct hlist_node hnode;
};

#define hash_add_inodep(hashtable, key, inodep)  \
		hlist_add_head(&inodep->hnode, &hashtable[hash_32((u32)key, HASH_BITS(hashtable))])

extern struct kmem_cache *rffs_inodep_cachep;
#define inodep_malloc()  \
		((struct inodep *)kmem_cache_alloc(rffs_inodep_cachep, GFP_KERNEL))
#define inodep_free(p) (kmem_cache_free(rffs_inodep_cachep, p))

struct mknod_args {
    struct inode *dir;
    struct dentry *dentry;
    int mode;
    dev_t dev;
};

struct mkdir_args {
    struct inode *dir;
    struct dentry *dentry;
    int mode;
};

struct rmdir_args {
    struct inode *dir;
    struct dentry *dentry;
};

struct create_args {
    struct inode *dir;
    struct dentry *dentry;
    int mode;
    struct nameidata *nd;
};

struct link_args {
    struct dentry *old_dentry;
    struct inode *dir;
    struct dentry *dentry;
};

struct unlink_args {
    struct inode *dir;
    struct dentry *dentry;
};

struct symlink_args {
    struct inode * dir;
    struct dentry *dentry;
    const char * symname;
};

struct rename_args {
    struct inode *old_dir;
    struct dentry *old_dentry;
    struct inode *new_dir;
    struct dentry *new_dentry;
};

struct setattr_args {
    struct dentry *dentry;
    struct iattr *iattr;
};

struct inode *rffs_get_inode(struct super_block *sb, const struct inode *dir,
        int mode, dev_t dev);
extern struct dentry *rffs_mount(struct file_system_type *fs_type, int flags,
        const char *dev_name, void *data);

#ifndef CONFIG_MMU
extern int rffs_nommu_expand_for_mapping(struct inode *inode, size_t newsize);
extern unsigned long rffs_nommu_get_unmapped_area(struct file *file,
        unsigned long addr, unsigned long len, unsigned long pgoff,
        unsigned long flags);

extern int rffs_nommu_mmap(struct file *file, struct vm_area_struct *vma);
#endif

extern struct rffs_log rffs_log;
extern const struct file_operations rffs_file_operations;
extern const struct vm_operations_struct generic_file_vm_ops;

int rffs_fill_super(struct super_block *sb, void *data, int silent);

extern const struct address_space_operations rffs_aops;
extern const struct inode_operations rffs_file_inode_operations;

#endif
