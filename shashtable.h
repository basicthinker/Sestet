/*
 * shashtable.h
 *
 *  Created on: Jul 19, 2013
 *      Author: Jinglei Ren <jinglei.ren@stanzax.org>
 *  Copyright (C) 2013 Microsoft Research Asia. All rights reserved.
 */

#ifndef ADAFS_SHASHTABLE_H_
#define ADAFS_SHASHTABLE_H_

#include <linux/list.h>
#include <linux/hash.h>
#include <linux/spinlock.h>

struct sht_list {
	rwlock_t lock;
	struct hlist_head hlist;
};

struct shashtable {
	struct sht_list *lists;
	unsigned int bits;
};

#define SHASHTABLE_UNINIT(bits) \
		{ (struct sht_list []) \
				{ [0 ... ((1 << (bits)) - 1)] = { .hlist = HLIST_HEAD_INIT } }, \
		bits }

#define sht_init(sht) do { \
		int i; \
		for (i = (1 << ((sht)->bits)) - 1; i >= 0; --i) { \
			rwlock_init(&((sht)->lists + i)->lock); \
		} } while(0)

#define sht_get_possible_hlist(sht, key) ({ \
		struct sht_list *sl = (sht)->lists + hash_32((u32)(long)(key), (sht)->bits); \
		read_lock(&sl->lock); \
		&sl->hlist; })

#define sht_put_possible_hlist(hl) \
		read_unlock(&container_of((hl), struct sht_list, hlist)->lock)

#define sht_get_possible_hlist_safe(sht, key) ({ \
		struct sht_list *sl = (sht)->lists + hash_32((u32)(long)(key), (sht)->bits); \
		write_lock(&sl->lock); \
		&sl->hlist; })

#define sht_put_possible_hlist_safe(hl) \
		write_unlock(&container_of((hl), struct sht_list, hlist)->lock)

/**
 * sht_add_entry - add an entry to the safe hashtable
 * @sht: struct shashtable *, the safe hashtable to add to
 * @obj: the entry to add
 * @key_member: the key member of the entry
 * @node_member: the hlist_node member of the entry
 */
#define sht_add_entry(sht, obj, key_member, node_member) do { \
		struct hlist_head *hl = sht_get_possible_hlist_safe(sht, (obj)->key_member); \
		hlist_add_head(&(obj)->node_member, hl); \
		sht_put_possible_hlist_safe(hl); } while(0)

#define sht_find_entry(sht, key, type, key_member, node_member) ({ \
		struct hlist_head *hl = sht_get_possible_hlist(sht, key); \
		struct hlist_node *pos; \
		hlist_for_each(pos, hl) { \
			if (hlist_entry(pos, type, node_member)->key_member == (key)) break; \
		} \
		sht_put_possible_hlist(hl); \
		pos ? hlist_entry(pos, type, node_member) : NULL; })

#define for_each_hlist(sht, sl, hl) \
		for (sl = (sht)->lists + ((1 << (sht)->bits) - 1); \
			sl >= (sht)->lists ? read_lock(&sl->lock), hl = &sl->hlist : 0; \
			read_unlock(&sl->lock), --sl)

#define for_each_hlist_safe(sht, sl, hl) \
		for (sl = (sht)->lists + ((1 << (sht)->bits) - 1); \
			sl >= (sht)->lists ? write_lock(&sl->lock), hl = &sl->hlist : 0; \
			write_unlock(&sl->lock), --sl)

#endif /* SHASHTABLE_H_ */
