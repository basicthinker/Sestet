#ifndef SESTET_RFFS_H_
#define SESTET_RFFS_H_

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
extern int __init init_rffs_fs(void);

int rffs_fill_super(struct super_block *sb, void *data, int silent);

extern const struct address_space_operations rffs_aops;
extern const struct inode_operations rffs_file_inode_operations;

#endif
