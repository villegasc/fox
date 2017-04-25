#include <sys/queue.h>
#include <liblightnvm.h>

#define NBLK_PER_VBLK 1;

#ifndef NVM_PROVISIONING_H
#define	NVM_PROVISIONING_H

    
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

    struct v_dev virt_dev;

    int prov_init_fblk_list(struct nvm_dev* dev, const struct nvm_geo* geo);

    int prov_exit_fblk_list();

    int prov_gen_list_per_lun(int ch, int l);
    
    struct nvm_vblk* prov_alloc_vblk(struct nvm_dev* dev, 
                                struct nvm_addr* addr);

    const struct nvm_geo* prov_get_geo(struct nvm_dev* dev);
    
    const struct nvm_bbt* prov_get_bbt(struct nvm_dev *dev, 
                              struct nvm_addr addr, struct nvm_ret *ret);
    
    int prov_get_vblock(size_t ch, size_t lun, struct nvm_vblk* vblk);
    
    int prov_put_vblock(struct nvm_vblk* vblock);

    ssize_t prov_vblock_pread(struct nvm_vblk* vblk, void* buf, size_t count, 
                size_t offset);
    
    ssize_t prov_vblock_pwrite(struct nvm_vblk* vblk, const void* buf, 
                            size_t count, size_t offset);
    
    ssize_t prov_vblock_pwrite_next(struct nvm_vblk* vblk, const void* buf, 
                            size_t count);

    ssize_t prov_vblock_erase(struct nvm_vblk* vblk);

    void prov_fblk_pr();

    void prov_lun_pr(struct nvm_lun lun);

    void prov_dev_pr();

#endif	/* NVM_PROVISIONING_H */

