#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/queue.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include "nvm_provisioning.h"

struct v_dev virt_dev;

struct nvm_vblk* alloc_vblk(struct nvm_dev* dev, struct nvm_addr* addr){
    struct nvm_vblk* vblk;
    const struct nvm_geo *geo;
    int errno;
	
    geo = virt_dev.geo;
    vblk = malloc(sizeof(struct nvm_vblk));
    if (!vblk) {
        errno = ENOMEM;
	return NULL;
    }
    vblk->blks[0]= *addr;
    vblk->nblks = NBLK_PER_VBLK;
    vblk->dev = dev;
    vblk->pos_write = 0;
    vblk->pos_read = 0;
    vblk->nbytes = geo->page_nbytes * geo->npages * geo->nplanes;
   return vblk;
}

struct free_blk* init_free_blk(int ch, int lun, int blk){
    struct free_blk* fblk = malloc(sizeof(struct free_blk));
    fblk->addr = malloc(sizeof(struct nvm_addr));
    fblk->addr->g.ch = ch;
    fblk->addr->g.lun = lun;
    fblk->addr->g.blk = blk;
    fblk->blk = alloc_vblk(virt_dev.dev, fblk->addr);
    return fblk;
}

int get_free_from_bbt(const struct nvm_bbt* bbt, struct nvm_lun* lun ){
    int blk, idx;
    int64_t nblks = bbt->nblks;
    
    lun->nfree_blks = 0;
    idx = 0;
    for (blk=0; blk < nblks; blk++){
        if (!bbt->blks[blk]){
            lun->index[idx] = init_free_blk(lun->addr->g.ch, lun->addr->g.lun, blk);
            lun->nfree_blks++;
            LIST_INSERT_HEAD(&(lun->free_blk_head), lun->index[idx], entry); 
            idx++;        
        } else {
            lun->index[idx] = NULL;
        }
    }
    return 0;
}

struct nvm_lun init_lun(int lun_idx){
    struct nvm_lun lun;
    lun.addr = malloc(sizeof(struct nvm_addr));
    lun.addr->ppa = 0;
    lun.addr->g.ch = lun_idx / virt_dev.geo->nluns;
    lun.addr->g.lun = lun_idx % virt_dev.geo->nluns;
    lun.index = malloc(virt_dev.geo->nblocks *  virt_dev.geo->nplanes
                                * sizeof (struct free_blk*));
    pthread_mutex_init (&(lun.l_mutex), NULL);
    return lun;
}

int init_v_dev(struct nvm_dev* dev, const struct nvm_geo* geo){
    int l;
    virt_dev.dev = dev;
    virt_dev.geo = geo;
    int total_luns = virt_dev.geo->nchannels * virt_dev.geo->nluns;
    virt_dev.free_blks = malloc(total_luns * sizeof(struct nvm_lun));
    for (l=0; l<total_luns; l++){
        virt_dev.free_blks[l] = init_lun(l);	
    }
    return 0;
}

void exit_free_blk_lun(struct nvm_lun* lun){
    uint32_t i;
    pthread_mutex_lock(&(lun->l_mutex));
    for (i=0; i<lun->nfree_blks; i++){
        LIST_REMOVE(lun->index[i], entry);
        free(lun->index[i]);
    }
}

int exit_free_blk_list(){
    int total_luns, l;
    total_luns = virt_dev.geo->nchannels * virt_dev.geo->nluns;
    for (l=0; l<total_luns; l++){
        exit_free_blk_lun(&(virt_dev.free_blks[l]));
        free(virt_dev.free_blks[l].index);
        pthread_mutex_destroy (&(virt_dev.free_blks[l].l_mutex));
    }
    free(virt_dev.free_blks);
    return 0;
}

int init_free_blk_list(struct nvm_dev* dev, const struct nvm_geo* geo){
    size_t ch, l;
    size_t nchannels, nluns;
    
    srand(time (NULL));
    init_v_dev(dev, geo);
    nchannels = virt_dev.geo->nchannels;
    nluns = virt_dev.geo->nluns;

    for (ch=0; ch<nchannels; ch++){
        for (l=0; l<nluns; l++){
	    gen_list_per_lun(ch, l);
        }
    }
    return 0;
}

int gen_list_per_lun(int ch, int l){
    int curr_lun;
    const struct nvm_bbt* bbt;
    struct nvm_ret ret;
    struct nvm_addr* addr = malloc(sizeof(struct nvm_addr));
    curr_lun = ch*virt_dev.geo->nluns + l;
    addr->ppa = 0;
    addr->g.ch = ch;
    addr->g.lun = l;
    virt_dev.free_blks[curr_lun].addr = addr;
    bbt = get_bbt(virt_dev.dev, *addr, &ret);
    if (!bbt)
        return -1;
    LIST_INIT(&(virt_dev.free_blks[curr_lun].free_blk_head));
    get_free_from_bbt(bbt, &(virt_dev.free_blks[curr_lun]));
    free(addr);
    return 0;
}

int update_free_blk_list(struct nvm_lun* lun, uint32_t blk_idx){
    pthread_mutex_lock(&(lun->l_mutex));
    LIST_REMOVE(lun->index[blk_idx], entry);
    free(lun->index[blk_idx]);
    lun->index[blk_idx] = lun->index[lun->nfree_blks-1];
    lun->index[lun->nfree_blks-1] = NULL; 
    lun->nfree_blks--;
    pthread_mutex_unlock(&(lun->l_mutex));
    return 0;
}

const struct nvm_geo* get_geo(struct nvm_dev* dev){
    return nvm_dev_get_geo(dev);
}

const struct nvm_bbt* get_bbt(struct nvm_dev *dev, struct nvm_addr addr, 
            struct nvm_ret *ret){
    return nvm_bbt_get(dev, addr, ret);
}
    
int get_vblock(size_t ch, size_t lun, struct nvm_vblk* vblk){
    size_t curr_lun = ch*virt_dev.geo->nluns + lun;
    ssize_t ret;
    int blk_idx;
    
    if (virt_dev.free_blks[curr_lun].nfree_blks > 0){
        blk_idx = rand() % virt_dev.free_blks[curr_lun].nfree_blks;
        if (virt_dev.free_blks[curr_lun].index[blk_idx] == NULL){      
            return -1;
        }
        *vblk = *virt_dev.free_blks[curr_lun].index[blk_idx]->blk;
        ret = vblock_erase(vblk);
        if (ret<0){
            return -1;
        }
        update_free_blk_list(&(virt_dev.free_blks[curr_lun]), blk_idx);
        return 0;
    }
    return -1;
}
    
int put_vblock(struct nvm_vblk* vblk){
    size_t max_blocks;
    uint64_t ch = vblk->blks[0].g.ch;
    uint64_t l = vblk->blks[0].g.lun;
    size_t curr_lun = ch*virt_dev.geo->nluns + l;
    struct nvm_lun lun = virt_dev.free_blks[curr_lun];
    struct free_blk* f_blk = malloc(sizeof(struct free_blk));
    
    max_blocks = virt_dev.geo->nblocks * virt_dev.geo->nplanes;
    f_blk->blk = vblk;
    f_blk->addr = &(vblk->blks[0]);
    if(lun.nfree_blks <max_blocks && 
                        lun.index[lun.nfree_blks]==NULL){
        lun.index[lun.nfree_blks] = f_blk;
        lun.nfree_blks++;
        LIST_INSERT_HEAD(&(lun.free_blk_head), f_blk, entry);
        virt_dev.free_blks[curr_lun] = lun;
        return 0;
    }
    free(f_blk);
    return -1;  
}

ssize_t vblock_pread(struct nvm_vblk* vblk, void* buf, size_t count,
		       size_t offset){
    return nvm_vblk_pread(vblk, buf, count, offset);
}
    
ssize_t vblock_pwrite(struct nvm_vblk* vblk, const void* buf, size_t count, 
                size_t offset){
    ssize_t nbytes = nvm_vblk_pwrite(vblk, buf, count, offset);
    if (nbytes>0)
        vblk->pos_write += count;
    return nbytes;
}

ssize_t block_pwrite_next(struct nvm_vblk* vblk, const void *buf, size_t count){ 
    return vblock_pwrite(vblk, buf, count, vblk->pos_write);
}

ssize_t vblock_erase(struct nvm_vblk* vblk){
    int err;
    struct nvm_ret ret;
    err = nvm_vblk_erase(vblk);
    if (err<0){
        nvm_bbt_mark(vblk->dev, vblk->blks, 1, 1, &ret);
    }
    return err;
}

void free_blk_pr(){
    int l;
    int total_luns = virt_dev.geo->nchannels * virt_dev.geo->nluns;
    printf("n_luns: %d {\n",total_luns);        
    for (l=0; l< total_luns; l++){
        printf("printing LUN: %d ", l);
	nvm_lun_pr(virt_dev.free_blks[l]);
    }
    printf("}\n");
}

void nvm_lun_pr(struct nvm_lun lun){
    size_t blk;
    printf("ppa: CH:%d, LUN:%d: {", lun.addr->g.ch, lun.addr->g.lun);
    printf("free blocks: %u ", lun.nfree_blks);
    for (blk=0; blk<lun.nfree_blks; blk++){
        printf("idx %lu: ", blk);
        nvm_addr_pr(*(lun.index[blk]->addr));
    }
    printf("}\n");
}
