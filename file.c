// SPDX-License-Identifier: GPL-2.0
/*
 * ouiche_fs - a simple educational filesystem for Linux
 *
 * Copyright (C) 2018 Redha Gouicem <redha.gouicem@lip6.fr>
 */

#define pr_fmt(fmt) "ouichefs: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/mpage.h>
#include "test/requettes.h"
#include "ouichefs.h"
#include "bitmap.h"

/*
 * Map the buffer_head passed in argument with the iblock-th block of the file
 * represented by inode. If the requested block is not allocated and create is
 * true,  allocate a new block on disk and map it.
 */

static int affiche_data_in_block(struct ouichefs_inode_info *ci,
				 struct super_block *sb)
{
	int err = 0;
	struct buffer_head *bh_current_block;
	struct buffer_head *buffer_head;
	struct ouichefs_file_index_block *index;

	if ((ci == NULL) || (sb == NULL)) {
		pr_err("Un block n'est pas alloué\n");
		err = -1;
		goto err_1;
	}
	bh_current_block = sb_bread(sb, ci->index_block);
	if (!bh_current_block) {
		err = -EIO;
		goto err_1;
	}
	/*
	 * récupère le pointeur vers le début des numéros de block où sont
	 * stocké les données du fichier
	 */
	index = (struct ouichefs_file_index_block *)bh_current_block->b_data;
	int i = 0;

	while (index->blocks[i] > 0) {
		buffer_head = sb_bread(sb, index->blocks[i]);
		if (!buffer_head) {
			pr_info("erreur I/O\n");
			err = -EIO;
			goto err_1;
		}
		pr_info("{current block : %d} | \
			[Data in block number %d] :%s\n",
			ci->index_block, index->blocks[i],
			buffer_head->b_data);

		brelse(buffer_head);
		i++;
	}
err_2:
	brelse(bh_current_block);
err_1:
	return err;
}



static int ouichefs_file_get_block(struct inode *inode, sector_t iblock,
				   struct buffer_head *bh_result, int create)
{
	struct super_block *sb = inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct ouichefs_file_index_block *index;
	struct buffer_head *bh_index;
	bool alloc = false;
	int ret = 0, bno;
	/* If block number exceeds filesize, fail */
	if (iblock >= (OUICHEFS_BLOCK_SIZE >> 2) - 1)
		return -EFBIG;
	/* Read index block from disk */
	bh_index = sb_bread(sb, ci->index_block);
	if (!bh_index)
		return -EIO;
	index = (struct ouichefs_file_index_block *)bh_index->b_data;
	/*
	 * Check if iblock is already allocated. If not and create is true,
	 * allocate it. Else, get the physical block number.
	 */
	if (index->blocks[iblock] == 0) {
		if (!create)
			return 0;
		bno = get_free_block(sbi);
		if (!bno) {
			ret = -ENOSPC;
			goto brelse_index;
		}
		index->blocks[iblock] = bno;
		alloc = true;
	} else {
		bno = index->blocks[iblock];
	}
	/* Map the physical block to the given buffer_head */
	map_bh(bh_result, sb, bno);
brelse_index:
	brelse(bh_index);
	return ret;
}
/*
 * Called by the page cache to read a page from the physical disk and map it in
 * memory.
 */
static int ouichefs_readpage(struct file *file, struct page *page)
{
	return mpage_readpage(page, ouichefs_file_get_block);
}

/*
 * Called by the page cache to write a dirty page to the physical disk (when
 * sync is called or when memory is needed).
 */
static int ouichefs_writepage(struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, ouichefs_file_get_block, wbc);
}

/*
 * Called by the VFS when a write() syscall occurs on file before writing the
 * data in the page cache. This functions checks if the write will be able to
 * complete and allocates the necessary blocks through block_write_begin().
 */
static int ouichefs_write_begin(struct file *file,
				struct address_space *mapping, loff_t pos,
				unsigned int len, unsigned int flags,
				struct page **pagep, void **fsdata)
{
	struct buffer_head *bh_current_block;
	struct buffer_head *bh_new;
	struct buffer_head *bh_block;
	struct buffer_head *new_bh_block;
	struct inode *inode = file->f_inode;
	struct ouichefs_file_index_block *new_index;
	struct ouichefs_file_index_block *index;
	struct super_block *sb = inode->i_sb;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct buffer_head *bh_version;
	struct buffer_head *bh;
	struct ouichefs_file_index_block *index_version;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(file->f_inode->i_sb);
	int err, v, no_block_new_version, i, block_number, k;
	uint32_t nr_allocs = 0;
	uint32_t inode_block, inode_shift;

	/* mise à jour de la latest_version et test si droit à l'ecriture */
	struct ouichefs_inode *cinode = NULL;
	/* Check if the write can be completed (enough space or have right?) */

	if (pos + len > OUICHEFS_MAX_FILESIZE)
		return -ENOSPC;

	nr_allocs = max(pos + len, file->f_inode->i_size) / \
				OUICHEFS_BLOCK_SIZE;
	if (nr_allocs > file->f_inode->i_blocks - 1)
		nr_allocs -= file->f_inode->i_blocks - 1;
	else
		nr_allocs = 0;
	if (nr_allocs > sbi->nr_free_blocks)
		return -ENOSPC;

	/* dans quel block se trouve l inode */
	inode_block = (inode->i_ino / OUICHEFS_INODES_PER_BLOCK) + 1;
	inode_shift = inode->i_ino % OUICHEFS_INODES_PER_BLOCK;

	bh = sb_bread(sb, inode_block);
	if (!bh) {
		err = -EIO;
		goto err_1;
	}
	cinode = (struct ouichefs_inode *)bh->b_data;
	cinode += inode_shift;

/*---------------------------------------------------------------------------*/
/*			Partie 1 :  historique de versions		     */
/*---------------------------------------------------------------------------*/
	pr_info("index de bloc actuel avant toute modification \
		%d\n", ci->index_block);
	bh_current_block = sb_bread(sb, ci->index_block);
	if (!bh_current_block) {
		err = -EIO;
		goto err_2;
	}
	/*
	 * récupère le pointeur vers le début des numéros de block ou sont
	 * stocké les données du fichier.
	 */
	index = (struct ouichefs_file_index_block *)bh_current_block->b_data;

	if (index->blocks[(OUICHEFS_BLOCK_SIZE >> 2) - 1] == 0) {
		pr_info("/* première fois qu'on écrit sur le fichier */\n");
		index->blocks[(OUICHEFS_BLOCK_SIZE >> 2) - 1] = -1;
		cinode->can_write = 1;
		cinode->nb_versions = 0;
		mark_buffer_dirty(bh_current_block);
		sync_dirty_buffer(bh_current_block);
		mark_buffer_dirty(bh);
		sync_dirty_buffer(bh);
		mark_inode_dirty(inode);
		write_inode_now(inode, 1);
	} else {
		pr_info("/* passage de version %d à %d */\n",
			cinode->nb_versions, cinode->nb_versions+1);
		if (cinode->can_write == 0) {
			pr_err("Read-only file system\n");
			affiche_data_in_block(ci, sb);
			err = -EROFS;
			goto err_3;
		}
		/* récupère un block free pour copier les données de l'ancien */
		no_block_new_version = get_free_block(sbi);
		bh_new = sb_bread(sb, no_block_new_version);
		if (!bh_new) {
			pr_err("Erreur récupération du \
			buffer_head du nouveau block\n");
			put_block(sbi, no_block_new_version);
			err = -EIO;
			goto err_3;
		}
		new_index = (struct ouichefs_file_index_block *)bh_new->b_data;
		k = 0;
		/* Pour chaque blocs alloués on copie les données */
		while (index->blocks[k] != 0) {
			pr_info("nouvelle copie\n");
			block_number = get_free_block(sbi);
			new_bh_block = sb_bread(sb, block_number);
			bh_block = sb_bread(sb, index->blocks[k]);
			if (bh_block && !new_bh_block)
				brelse(bh_block);
			if (!bh_block || !new_bh_block) {
				pr_info("Erreur de création \
				d'une nouvelle version\n");
				/*il faut libérer tous les blocks alloués*/
				while (k >= 0) {
					put_block(sbi, new_index->blocks[k]);
					k--;
				}
				err = -EIO;
				goto err_3;
			}
			/* remettre les données à 0 si elle sont différentes pour ne pas
			 * perturber la copie des vrais données
			 */
			memcpy(new_bh_block->b_data, bh_block->b_data,\
			 OUICHEFS_BLOCK_SIZE);
			new_index->blocks[k] = block_number;
			mark_buffer_dirty(new_bh_block);
			sync_dirty_buffer(new_bh_block);
			mark_buffer_dirty(bh_new);
			sync_dirty_buffer(bh_new);
			mark_inode_dirty(inode);
			write_inode_now(inode, 1);
			brelse(new_bh_block);
			brelse(bh_block);
			k++;
		}
		/* Ajout du numéro de blocs contenant l'ancienne version */
		new_index->blocks[(OUICHEFS_BLOCK_SIZE >> 2) - 1]\
			 = ci->index_block;
		/* les données de la nouvelle version */
		ci->index_block = no_block_new_version;
		mark_buffer_dirty(bh_new);
		sync_dirty_buffer(bh_new);
		mark_inode_dirty(inode);
		write_inode_now(inode, 1);
		brelse(bh_new);


	}
	ci = OUICHEFS_INODE(inode);
	cinode->last_index_block = ci->index_block;
	cinode->nb_versions++; /* on viens de write on incrémente */
	mark_buffer_dirty(bh);
	sync_dirty_buffer(bh);
	mark_inode_dirty(inode);
	write_inode_now(inode, 1);
	pr_info("La dernière version est :%d\n", cinode->last_index_block);
	/* prepare the write */
	err = block_write_begin(mapping, pos, len, flags, pagep,
				ouichefs_file_get_block);
	/* if this failed, reclaim newly allocated blocks */
	if (err < 0) {
		pr_err("%s:%d: newly allocated blocks reclaim not implemented yet\n",
		       __func__, __LINE__);
	}

err_3:
	brelse(bh_current_block);
err_2:
	brelse(bh);
err_1:
	return err;

}

/*
 * Called by the VFS after writing data from a write() syscall to the page
 * cache. This functions updates inode metadata and truncates the file if
 * necessary.
 */
static int ouichefs_write_end(struct file *file, struct address_space *mapping,
			      loff_t pos, unsigned int len, unsigned int copied,
			      struct page *page, void *fsdata)
{
	int ret;
	struct inode *inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(inode);
	struct super_block *sb = inode->i_sb;
	/* Complete the write() */
	ret = generic_write_end(file, mapping, pos, len, copied, page, fsdata);
	if (ret < len) {
		pr_err("%s:%d: wrote less than asked... what do I do? nothing for now...\n",
		       __func__, __LINE__);
	} else {
		uint32_t nr_blocks_old = inode->i_blocks;

		/* Update inode metadata */
		inode->i_blocks = inode->i_size / OUICHEFS_BLOCK_SIZE + 2;
		inode->i_mtime = inode->i_ctime = current_time(inode);
		mark_inode_dirty(inode);
		write_inode_now(inode, 1);
		void *addr = kmap(page);

		pr_info("données à copier : %s\n", (char *)addr);
		/* If file is smaller than before, free unused blocks */
		if (nr_blocks_old > inode->i_blocks) {
			int i;
			struct buffer_head *bh_index;
			struct ouichefs_file_index_block *index;

			/* Free unused blocks from page cache */
			truncate_pagecache(inode, inode->i_size);

			/* Read index block to remove unused blocks */
			bh_index = sb_bread(sb, ci->index_block);
			if (!bh_index) {
				pr_err("failed truncating '%s'. we just lost %llu blocks\n",
				       file->f_path.dentry->d_name.name,
				       nr_blocks_old - inode->i_blocks);
				goto end;
			}
			index = (struct ouichefs_file_index_block *)
				bh_index->b_data;

			for (i = inode->i_blocks - 1; i < nr_blocks_old - 1;
			     i++) {
				put_block(OUICHEFS_SB(sb), index->blocks[i]);
				index->blocks[i] = 0;
			}
			mark_buffer_dirty(bh_index);
			brelse(bh_index);
		}
	}
end:
	kunmap(page);
	sync_filesystem(sb);
	return ret;
}

const struct address_space_operations ouichefs_aops = {
	.readpage    = ouichefs_readpage,
	.writepage   = ouichefs_writepage,
	.write_begin = ouichefs_write_begin,
	.write_end   = ouichefs_write_end
};

/*---------------------------------------------------------------------------*/
/*			Prtie 3 :  Requettes ioctl			     						 */
/*---------------------------------------------------------------------------*/
/**
 * cette fonction lorsque une requette ioctl est lancée
 * 0 CHANGE_VERSION-> permet de changer la version courante vers une
 * vers une version plus ancienne si celle-ci existe
 *
 * 1 RESTOR_VERSION-> permet de supprimer toutes les versions à partir
 * celle donnée
 * 
 * 2 RLEASE_VERSION-> permet de restorer la version ourante lorsque celle-ci
 * a ete modifiée avec la requette CHANGE_VERSION permettant ainsi de pouvoir
 * à nouveau réecrir dans le ficher
 */

long ouichefs_change_version(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	int ret = 0;
	int size = 100;
	char *request = kvzalloc(100 * sizeof(char), GFP_KERNEL);
	int requested_version;

	ret = copy_from_user(request, (char *)arg, size);
	if (ret) {
		pr_err("Erreur récupération de la requette\n");
		ret = -EFAULT;
		goto err1;
	}
	ret = kstrtoint(request, 0, &requested_version);
	if (ret < 0) {
		pr_err("token kstrtoint\n");
		goto err1;
	}
	struct inode *file_inode = file->f_inode;
	struct ouichefs_inode_info *ci = OUICHEFS_INODE(file_inode);
	struct buffer_head *bh_index_inode_file;
	struct buffer_head *bh_index;
	struct ouichefs_inode *cinode;
	struct super_block *sb = file_inode->i_sb;
	struct ouichefs_sb_info *sbi = OUICHEFS_SB(sb);
	struct ouichefs_file_index_block *index;



	uint32_t inode_block = (file_inode->i_ino / OUICHEFS_INODES_PER_BLOCK) + 1;
	uint32_t inode_shift = file_inode->i_ino % OUICHEFS_INODES_PER_BLOCK;

	bh_index_inode_file = sb_bread(sb, inode_block);
	if (!bh_index_inode_file) {
		ret = -EIO;
		goto err_1;

	}
	cinode = (struct ouichefs_inode *)bh_index_inode_file->b_data;
	cinode += inode_shift;




	if ((requested_version > cinode->nb_versions) || (requested_version < 0)) {
		pr_err("invalid version\n");
		ret = -EINVAL;
		goto err1;
	}

	if (cmd == RLEASE_VERSION) {
		pr_info("changement de version from %d to %d\n",
		ci->index_block, cinode->last_index_block);
		cinode->can_write = 1;
		ci->index_block = cinode->last_index_block;
		mark_buffer_dirty(bh_index_inode_file);
		sync_dirty_buffer(bh_index_inode_file);
		mark_inode_dirty(file_inode);
		write_inode_now(file_inode, 1);
	}

	if (cmd == CHANGE_VERSION) {
		pr_info("[Chagnge versions]\n");
		cinode->can_write = 0;
		mark_buffer_dirty(bh_index_inode_file);
		sync_dirty_buffer(bh_index_inode_file);
		mark_inode_dirty(file_inode);
		write_inode_now(file_inode, 1);

		bh_index = sb_bread(sb, cinode->last_index_block);
		if (!bh_index) {
			ret = -EIO;
			goto err1;

		}
		index = (struct ouichefs_file_index_block *)bh_index->b_data;
		int my_version = 0;
		int cur_version, last_version;

		cur_version = cinode->last_index_block;
		last_version = index->blocks[(OUICHEFS_BLOCK_SIZE >> 2) - 1];
		while (last_version != -1) {
			if (my_version == requested_version) {
				if (requested_version == 0) {
					cinode->can_write = 1;
					mark_buffer_dirty(bh_index_inode_file);
					sync_dirty_buffer(bh_index_inode_file);
					mark_inode_dirty(file_inode);
					write_inode_now(file_inode, 1);
				}
				ci->index_block = cur_version;
				mark_buffer_dirty(bh_index_inode_file);
				sync_dirty_buffer(bh_index_inode_file);
				mark_inode_dirty(file_inode);
				write_inode_now(file_inode, 1);
				goto fin;
			}
			cur_version = last_version;
			brelse(bh_index);
			bh_index = sb_bread(sb, last_version);
			if (!bh_index) {
				pr_err("erreur lors de la récupération des données\n");
				ret = -EIO;
				goto err1;
			}
			index = (struct ouichefs_file_index_block *)bh_index->b_data;
			last_version = index->blocks[(OUICHEFS_BLOCK_SIZE >> 2) - 1];
			my_version++;
		}
fin:
	pr_info("changement de version from %d to %d\n",
	cinode->last_index_block, ci->index_block);
	brelse(bh_index);
	}


	if (cmd == RESTOR_VERSION) {
		struct buffer_head *bh_index_remouve_block;

		pr_info("[restore version]\n");
		bh_index = sb_bread(sb, cinode->last_index_block);
		if (!bh_index) {
			ret = -EIO;
			goto err1;
		}
		index = (struct ouichefs_file_index_block *)bh_index->b_data;
		int my_version = 0;
		int cur_version, last_version, init_version;
		/* sert uniquement pour l'affichage */
		init_version = cinode->last_index_block;
		cur_version = cinode->last_index_block;
		last_version = index->blocks[(OUICHEFS_BLOCK_SIZE >> 2) - 1];
		while ((last_version != -1) && my_version != requested_version) {
			int k = 0;
			/* Pour chaque blocs alloués on le put */
			while (k < ((OUICHEFS_BLOCK_SIZE >> 2) - 2)) {
				if (index->blocks[k] != 0) {
					bh_index_remouve_block = sb_bread(sb, index->blocks[k]);
					if (!bh_index_remouve_block) {
						pr_err("erreur lors de la récupération des données\n");
						ret = -EIO;
						brelse(bh_index);
						goto err1;
					}
					memset(bh_index_remouve_block->b_data, 0, OUICHEFS_BLOCK_SIZE);
					put_block(sbi, index->blocks[k]);
				}
				k++;
			}
			memset(bh_index->b_data, 0, OUICHEFS_BLOCK_SIZE);
			put_block(sbi, cur_version);
			brelse(bh_index);
			/* lire le block d'avant qui deviens le courant */
			bh_index = sb_bread(sb, last_version);
			if (!bh_index) {
				pr_err("erreur lors de la récupération des données\n");
				ret = -EIO;
				goto err1;
			}
			cur_version = last_version;

			index = (struct ouichefs_file_index_block *)bh_index->b_data;
			last_version = index->blocks[(OUICHEFS_BLOCK_SIZE >> 2) - 1];
			my_version++;
		}
		if (my_version == requested_version) {
			ci->index_block = cur_version;
			cinode->last_index_block = cur_version;
			pr_info("changement de version from %d to %d\n", init_version, ci->index_block);
		} else {
			ci->index_block = cur_version;
			cinode->last_index_block = cur_version;
			pr_info("changement de version impossible un problème\n est survenu. On arrive à la fin des\nversions et ce n'est pas la version\ndemmandée on garde la toute première\n");
		}
		mark_buffer_dirty(bh_index_inode_file);
		sync_dirty_buffer(bh_index_inode_file);
		mark_inode_dirty(file_inode);
		write_inode_now(file_inode, 1);
		pr_info("changement de version from %d to %d\n", init_version, ci->index_block);
		brelse(bh_index);

	}
	invalidate_inode_buffers(file_inode);
err1:
	brelse(bh_index_inode_file);
err_1:
	return ret;
}


const struct file_operations ouichefs_file_ops = {
	.owner      = THIS_MODULE,
	.llseek     = generic_file_llseek,
	.read_iter  = generic_file_read_iter,
	.write_iter = generic_file_write_iter,
	.unlocked_ioctl = ouichefs_change_version
};
