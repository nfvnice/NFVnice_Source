/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * onvm_flow_dir.c - flow director APIs
 ********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include "common.h"
#include "onvm_flow_table.h"
#include "onvm_flow_dir.h"

#define NO_FLAGS 0
#define SDN_FT_ENTRIES 1024

struct onvm_ft *sdn_ft;
struct onvm_ft **sdn_ft_p;

int
onvm_flow_dir_init(void)
{
	const struct rte_memzone *mz_ftp;

	sdn_ft = onvm_ft_create(SDN_FT_ENTRIES, sizeof(struct onvm_flow_entry));
        if(sdn_ft == NULL) {
                rte_exit(EXIT_FAILURE, "Unable to create flow table\n");
        }
        mz_ftp = rte_memzone_reserve(MZ_FTP_INFO, sizeof(struct onvm_ft *),
                                  rte_socket_id(), NO_FLAGS);
        if (mz_ftp == NULL) {
                rte_exit(EXIT_FAILURE, "Canot reserve memory zone for flow table pointer\n");
        }
        memset(mz_ftp->addr, 0, sizeof(struct onvm_ft *));
        sdn_ft_p = mz_ftp->addr;
        *sdn_ft_p = sdn_ft;

	return 0;
}

int
onvm_flow_dir_nf_init(void)
{
	const struct rte_memzone *mz_ftp;
        struct onvm_ft **ftp;

        mz_ftp = rte_memzone_lookup(MZ_FTP_INFO);
        if (mz_ftp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get table pointer\n");
        ftp = mz_ftp->addr;
        sdn_ft = *ftp;

	return 0;
}

int
onvm_flow_dir_get_pkt( struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry){
	int ret;
	ret = onvm_ft_lookup_pkt(sdn_ft, pkt, (char **)flow_entry);

	return ret;
}

int
onvm_flow_dir_add_pkt(struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry){
	int ret;
       	ret = onvm_ft_add_pkt(sdn_ft, pkt, (char**)flow_entry);

	return ret;
}

int
onvm_flow_dir_del_pkt(struct rte_mbuf* pkt){
	int ret;
	struct onvm_flow_entry *flow_entry;
	int ref_cnt;

        ret = onvm_flow_dir_get_pkt(pkt, &flow_entry);
	if (ret >= 0) {
		ref_cnt = flow_entry->sc->ref_cnt--;
		if (ref_cnt <= 0) {
			ret = onvm_flow_dir_del_and_free_pkt(pkt);
		}
	}

	return ret;
}

int
onvm_flow_dir_del_and_free_pkt(struct rte_mbuf *pkt){
	int ret;
	struct onvm_flow_entry *flow_entry;

	ret = onvm_flow_dir_get_pkt(pkt, &flow_entry);
	if (ret >= 0) {
		rte_free(flow_entry->sc);
		rte_free(flow_entry->key);
		ret = onvm_ft_remove_pkt(sdn_ft, pkt);
	}

	return ret;
}

int
onvm_flow_dir_get_key(struct onvm_ft_ipv4_5tuple *key, struct onvm_flow_entry **flow_entry){
	int ret;
        ret = onvm_ft_lookup_key(sdn_ft, key, (char **)flow_entry);

        return ret;
}

int
onvm_flow_dir_add_key(struct onvm_ft_ipv4_5tuple *key, struct onvm_flow_entry **flow_entry){
        int ret;
        ret = onvm_ft_add_key(sdn_ft, key, (char**)flow_entry);

        return ret;
}

int
onvm_flow_dir_del_key(struct onvm_ft_ipv4_5tuple *key){
        int ret;
        struct onvm_flow_entry *flow_entry;
        int ref_cnt;

        ret = onvm_flow_dir_get_key(key, &flow_entry);
        if (ret >= 0) {
                ref_cnt = flow_entry->sc->ref_cnt--;
                if (ref_cnt <= 0) {
                        ret = onvm_flow_dir_del_and_free_key(key);
                }
        }

        return ret;
}

int
onvm_flow_dir_del_and_free_key(struct onvm_ft_ipv4_5tuple *key){
        int ret;
        struct onvm_flow_entry *flow_entry;

        ret = onvm_flow_dir_get_key(key, &flow_entry);
        if (ret >= 0) {
                //ret = onvm_ft_remove_key(sdn_ft, key); // This function keeps crashing
                rte_free(flow_entry->sc);
                flow_entry->sc=NULL;
                rte_free(flow_entry->key);
                flow_entry->key=NULL;
        }

        return ret;
}
void
onvm_flow_dir_print_stats(void) {

        if(sdn_ft) {
                int32_t tbl_index = 0;
                uint32_t active_chains = 0;
                uint32_t mapped_chains = 0;
                for (; tbl_index < SDN_FT_ENTRIES; tbl_index++)
                {
                        struct onvm_flow_entry *flow_entry = (struct onvm_flow_entry *)&sdn_ft->data[tbl_index*sdn_ft->entry_size];
                        if (flow_entry && flow_entry->sc && flow_entry->sc->chain_length) {
                                active_chains+=1;
#ifdef ENABLE_NF_BACKPRESSURE
#ifdef NF_BACKPRESSURE_APPROACH_2
                                if(flow_entry->sc->nf_instance_id[1]) {
                                        mapped_chains+=1;
                                }
#endif
#endif
                        }
                        else continue;
#ifdef ENABLE_NF_BACKPRESSURE
                        if (flow_entry->sc->highest_downstream_nf_index_id) {
                                //printf ("FT_Key[%d], OverflowStatus [%d], \t", (int)flow_entry->key->src_addr, (int)flow_entry->sc->highest_downstream_nf_index_id);
                                printf ("OverflowStatus [%d], \t", flow_entry->sc->highest_downstream_nf_index_id);
#ifdef NF_BACKPRESSURE_APPROACH_2
                                uint8_t nf_idx = 0;
                                for (; nf_idx < ONVM_MAX_CHAIN_LENGTH; nf_idx++) {
                                        printf("[%d: %d] \t", nf_idx, flow_entry->sc->nf_instance_id[nf_idx]);
                                }
#endif //NF_BACKPRESSURE_APPROACH_2
                                printf("\n");
                        }
#endif  //ENABLE_NF_BACKPRESSURE
                }
                printf("Total chains: [%d], mapped chains: [%d]  \n", active_chains, mapped_chains);
        }

        return ;
}

int
onvm_flow_dir_clear_all_entries(void) {
        if(sdn_ft) {
                int32_t tbl_index = 0;
                uint32_t active_chains = 0;
                uint32_t cleared_chains = 0;
                for (; tbl_index < SDN_FT_ENTRIES; tbl_index++) {
                        struct onvm_flow_entry *flow_entry = (struct onvm_flow_entry *)&sdn_ft->data[tbl_index*sdn_ft->entry_size];
                        if (flow_entry && flow_entry->key) { //flow_entry->sc && flow_entry->sc->chain_length) {
                                active_chains+=1;
                                if (onvm_flow_dir_del_key(flow_entry->key) >=0) cleared_chains++;
                        }
                        //else continue;
                }
                printf("Total chains: [%d], cleared chains: [%d]  \n", active_chains, cleared_chains);
                return (int)(active_chains-cleared_chains);
        }
        return 0;
}
