/* 
 * File:   nvm_provisioning.h
 * Author: thesis
 *
 * Created on April 7, 2017, 11:45 AM
 */

#include <sys/queue.h>
#include <liblightnvm.h>

#define NBLK_PER_VBLK 1;

#ifndef NVM_PROVISIONING_H
#define	NVM_PROVISIONING_H

#ifdef	__cplusplus
extern "C" {
#endif
    
    //static char nvm_dev_path[NVM_DEV_PATH_LEN] = "/dev/nvme0n1";
    
    enum IO_TYPE {
        NVM_RD = 0x0,	///< Flag for read op
	NVM_WR = 0x1,	///< Flag for write op
	NVM_RW = 0x2,	///< Flag for mixed op
	NVM_ER = 0x4,	///< Flag for erase op
    };
    
    struct nvm_vblk {
	struct nvm_dev* dev;
	struct nvm_addr blks[128];
	int nblks;
	size_t nbytes;
	size_t pos_write;
	size_t pos_read;
	int nthreads;
    };
    
    struct free_blk{
        struct nvm_addr* addr;
        struct nvm_vblk* blk;
        LIST_ENTRY(free_blk) entry;
    };
    
    struct nvm_lun {
        struct nvm_addr* addr;
        enum IO_TYPE io_type;
        uint32_t nfree_blks;
        struct free_blk** index;
        pthread_mutex_t l_mutex;
        LIST_HEAD(free_blk_list, free_blk) free_blk_head;
    };
    
    struct v_dev {
        struct nvm_dev* dev;
        const struct nvm_geo* geo;
        struct nvm_lun* free_blks;
    };

    int init_free_blk_list(struct nvm_dev* dev, const struct nvm_geo* geo);

    int exit_free_blk_list();

    int gen_list_per_lun(int ch, int l);
    
    struct nvm_vblk* alloc_vblk(struct nvm_dev* dev, struct nvm_addr* addr);

    const struct nvm_geo* get_geo(struct nvm_dev* dev);
    
    const struct nvm_bbt* get_bbt(struct nvm_dev *dev, struct nvm_addr addr, 
            struct nvm_ret *ret);
    
    int get_vblock(size_t ch, size_t lun, struct nvm_vblk* vblk);
    
    int put_vblock(struct nvm_vblk* vblock);

    ssize_t vblock_pread(struct nvm_vblk* vblk, void* buf, size_t count, 
                size_t offset);
    
    ssize_t vblock_pwrite(struct nvm_vblk* vblk, const void* buf, size_t count, 
                size_t offset);
    
    ssize_t vblock_erase(struct nvm_vblk* vblk);

    void free_blk_pr();

    void nvm_lun_pr(struct nvm_lun lun);

#ifdef	__cplusplus
}
#endif

#endif	/* NVM_PROVISIONING_H */

