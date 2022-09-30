// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018  Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#define taille_max 2000

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/seq_file.h>
#include <linux/buffer_head.h>

#include "ouichefs.h"
#include "test/requettes.h"

/* pour l'ioctl */
static struct inode *inode_partition;
static struct dentry *ouichefs_debug;


/*
 * Mount a ouiche_fs partition
 */
/*------------------------------------------------------------------------------*/
/*				Partie 2 :  debugfs				*/
/*------------------------------------------------------------------------------*/
/*
 * cette fonction est appelée lorsque on essaie de lire le
 * debugfs de la partition ouichefs elle affiche :
 * une colonne avec le numéro d'inode
 * une colonne avec le numéro de version
 * une colonne avec une liste des numéros de blocs d'index
 * correspondant à l'historique du fichier
 */
static int debug_ouichefs_show(struct seq_file *s_file, void *v)
{
	struct super_block *sb = inode_partition->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	/* parcourir les inodes de la partition */
	struct inode *sub_file_inode;
	int num_inode;
	struct ouichefs_inode_info *ci;
	struct buffer_head *bh_index;
	struct buffer_head *bh_index_inode_file;
	struct ouichefs_file_index_block *index;
	struct ouichefs_inode *cinode;
	int nb_versions, offset;
	/* variables pour changer de version et revenir sur la version actuelle */
	uint32_t cur_v, last_v, inode_block, inode_shift;
	char msg[taille_max];

	for (num_inode = 0; num_inode < sbi->nr_inodes; num_inode++) {

		memset(msg, 0, taille_max * (sizeof(char)));
		sub_file_inode = ouichefs_iget(sb, num_inode);

		/* soit l'inode n'est pas allouée
		 * soit le fichier n'est pas régulier
		 */
		if (sub_file_inode->i_nlink == 0) {
			iput(sub_file_inode);
			continue;
		}

		if (!S_ISREG(sub_file_inode->i_mode)) {
			iput(sub_file_inode);
			continue;
		}
		/* j'ai trouvé un file régulier */
		inode_block = (sub_file_inode->i_ino / OUICHEFS_INODES_PER_BLOCK) + 1;
		inode_shift = sub_file_inode->i_ino % OUICHEFS_INODES_PER_BLOCK;
		offset = taille_max;

		bh_index_inode_file = sb_bread(sb, inode_block);
		if (!bh_index_inode_file) {
			iput(sub_file_inode);
			return  -EIO;
		}
		cinode = (struct ouichefs_inode *)bh_index_inode_file->b_data;
		cinode += inode_shift;
		ci = OUICHEFS_INODE(sub_file_inode);
		bh_index = sb_bread(sb, cinode->last_index_block);
		if (!bh_index) {
			seq_puts(s_file, "erreur lors de la récupération des données\n");
			return 0;
		}

		index = (struct ouichefs_file_index_block *)bh_index->b_data;
		nb_versions = 0;
		cur_v = cinode->last_index_block;
		offset -= scnprintf(msg + (taille_max - offset), offset, "%d", cur_v);

		while (index->blocks[(OUICHEFS_BLOCK_SIZE >> 2) - 1] != -1
		 && index->blocks[(OUICHEFS_BLOCK_SIZE >> 2) - 1] != 0) {
			last_v = index->blocks[(OUICHEFS_BLOCK_SIZE >> 2) - 1];

			bh_index = sb_bread(sb, last_v);
			if (!bh_index) {
				seq_puts(s_file, "erreur lors de la récupération des données\n");
				return 0;
			}
			index = (struct ouichefs_file_index_block *)bh_index->b_data;
			cur_v = last_v;
			nb_versions++;
			offset -= scnprintf(msg + (taille_max - offset), offset, ",%d", cur_v);
		}
		seq_printf(s_file, "inode:%ld | nombre de versions:%d | liste des blocks de version:{%s}\n",
				sub_file_inode->i_ino,
				nb_versions + 1,
				msg);

		iput(sub_file_inode);
	}
	return 0;
}

/*
 * cette fonction est appelée à l'ouverture du debugfs
 */
static int debug_ouichefs_open(struct inode *inode, struct file *file)
{
	return single_open(file, debug_ouichefs_show, NULL);
}

const struct file_operations debug_ouichefs_ops = {
	.owner = THIS_MODULE,
	.open = debug_ouichefs_open,
	.read  = seq_read,
	.release = single_release,
};

struct dentry *ouichefs_mount(struct file_system_type *fs_type, int flags,
			      const char *dev_name, void *data)
{
	struct dentry *dentry = NULL;

	dentry = mount_bdev(fs_type, flags, dev_name, data,
			    ouichefs_fill_super);
	if (IS_ERR(dentry))
		pr_err("'%s' mount failure\n", dev_name);
	else
		pr_info("'%s' mount success\n", dev_name);

	/*--------------------------------------------------------------*/
	/* Partie 2 : création du debugfs				*/
	/*--------------------------------------------------------------*/
	inode_partition = dentry->d_inode;
	/* récupération du super block de l'inode de la partition */
	ouichefs_debug = debugfs_create_file("ouichefs_debug_file", 0600,
					NULL, NULL, &debug_ouichefs_ops);
	if (!ouichefs_debug) {
		pr_err("debugfs_create_file ouichefs_debug failed\n");
		goto err;
	}

	pr_info("module loaded\n");
	return dentry;
err:
	return NULL;
}

/*
 * Unmount a ouiche_fs partition
 */
void ouichefs_kill_sb(struct super_block *sb)
{
	/* lors de la supression de la partition,
	 * on doit enlever le debugfs
	 */
	debugfs_remove(ouichefs_debug);
	kill_block_super(sb);
	pr_info("unmounted disk\n");
}

static struct file_system_type ouichefs_file_system_type = {
	.owner = THIS_MODULE,
	.name = "ouichefs",
	.mount = ouichefs_mount,
	.kill_sb = ouichefs_kill_sb,
	.fs_flags = FS_REQUIRES_DEV,
	.next = NULL,
};



static int __init ouichefs_init(void)
{
	int ret;

	ret = ouichefs_init_inode_cache();
	if (ret) {
		pr_err("inode cache creation failed\n");
		goto end;
	}
	ret = register_filesystem(&ouichefs_file_system_type);
	if (ret) {
		pr_err("register_filesystem() failed\n");
		goto end;
	}
	pr_info("module loaded\n");

end:
	return ret;
}

static void __exit ouichefs_exit(void)
{
	int ret;

	ret = unregister_filesystem(&ouichefs_file_system_type);
	if (ret)
		pr_err("unregister_filesystem() failed\n");

	ouichefs_destroy_inode_cache();
	pr_info("module unloaded\n");
}

module_init(ouichefs_init);
module_exit(ouichefs_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Redha Gouicem, <redha.gouicem@lip6.fr>");
MODULE_DESCRIPTION("ouichefs, a simple educational filesystem for Linux");
