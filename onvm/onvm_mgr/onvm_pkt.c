/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
 *            2010-2014 Intel Corporation. All rights reserved.
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
 ********************************************************************/


/******************************************************************************
                                 onvm_pkt.c

            This file contains all functions related to receiving or
            transmitting packets.

******************************************************************************/


#include "onvm_mgr.h"
#include "onvm_pkt.h"
#include "onvm_nf.h"


/**********************************Interfaces*********************************/


void
onvm_pkt_process_rx_batch(struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t i;
        struct onvm_pkt_meta *meta;
        struct onvm_flow_entry *flow_entry;
        struct onvm_service_chain *sc;
        int ret;

        if (rx == NULL || pkts == NULL)
                return;

        for (i = 0; i < rx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                meta->src = 0;
                meta->chain_index = 0;

                ret = onvm_flow_dir_get_pkt(pkts[i], &flow_entry);

                if (ret >= 0) {
                        sc = flow_entry->sc;
                        meta->action = onvm_sc_next_action(sc, pkts[i]);
                        meta->destination = onvm_sc_next_destination(sc, pkts[i]);
                } else {
                        meta->action = onvm_sc_next_action(default_chain, pkts[i]);
                        meta->destination = onvm_sc_next_destination(default_chain, pkts[i]);
                }
                /* PERF: this might hurt performance since it will cause cache
                 * invalidations. Ideally the data modified by the NF manager
                 * would be a different line than that modified/read by NFs.
                 * That may not be possible.
                 */

                (meta->chain_index)++;
                onvm_pkt_enqueue_nf(rx, meta->destination, pkts[i]);
        }

        onvm_pkt_flush_all_nfs(rx);
}


void
onvm_pkt_process_tx_batch(struct thread_info *tx, struct rte_mbuf *pkts[], uint16_t tx_count, struct client *cl) {
        uint16_t i;
        struct onvm_pkt_meta *meta;

        if (tx == NULL || pkts == NULL || cl == NULL)
                return;

        for (i = 0; i < tx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                meta->src = cl->instance_id;
                if (meta->action == ONVM_NF_ACTION_DROP) {
                        // if the packet is drop, then <return value> is 0
                        // and !<return value> is 1.
                        //cl->stats.act_drop += !onvm_pkt_drop(pkts[i]);
                        onvm_pkt_drop(pkts[i]);
                        cl->stats.act_drop += 1;
                } else if (meta->action == ONVM_NF_ACTION_NEXT) {
                        /* TODO: Here we drop the packet : there will be a flow table
                        in the future to know what to do with the packet next */
                        cl->stats.act_next++;
                        onvm_pkt_process_next_action(tx, pkts[i], cl);
                } else if (meta->action == ONVM_NF_ACTION_TONF) {
                        cl->stats.act_tonf++;
                        (meta->chain_index)++;
                        onvm_pkt_enqueue_nf(tx, meta->destination, pkts[i]);
                } else if (meta->action == ONVM_NF_ACTION_OUT) {
                        cl->stats.act_out++;
                        onvm_pkt_enqueue_port(tx, meta->destination, pkts[i]);
                } else {
                        printf("ERROR invalid action : this shouldn't happen.\n");
                        onvm_pkt_drop(pkts[i]);
                        return;
                }
        }
}


void
onvm_pkt_flush_all_ports(struct thread_info *tx) {
        uint16_t i;

        if (tx == NULL)
                return;

        for (i = 0; i < ports->num_ports; i++)
                onvm_pkt_flush_port_queue(tx, i);
}


void
onvm_pkt_flush_all_nfs(struct thread_info *tx) {
        uint16_t i;

        if (tx == NULL)
                return;

        for (i = 0; i < MAX_CLIENTS; i++)
                onvm_pkt_flush_nf_queue(tx, i);
}

void
onvm_pkt_drop_batch(struct rte_mbuf **pkts, uint16_t size) {
        uint16_t i;

        if (pkts == NULL)
                return;

        for (i = 0; i < size; i++)
                rte_pktmbuf_free(pkts[i]);
}


/****************************Internal functions*******************************/


void
onvm_pkt_flush_port_queue(struct thread_info *tx, uint16_t port) {
        uint16_t i, sent;
        volatile struct tx_stats *tx_stats;

        if (tx == NULL)
                return;

        if (tx->port_tx_buf[port].count == 0)
                return;

        tx_stats = &(ports->tx_stats);
        sent = rte_eth_tx_burst(port,
                                tx->queue_id,
                                tx->port_tx_buf[port].buffer,
                                tx->port_tx_buf[port].count);
        if (unlikely(sent < tx->port_tx_buf[port].count)) {
                for (i = sent; i < tx->port_tx_buf[port].count; i++) {
                        onvm_pkt_drop(tx->port_tx_buf[port].buffer[i]);
                }
                tx_stats->tx_drop[port] += (tx->port_tx_buf[port].count - sent);
        }
        tx_stats->tx[port] += sent;

        tx->port_tx_buf[port].count = 0;
}


void
onvm_pkt_flush_nf_queue(struct thread_info *thread, uint16_t client) {
        uint16_t i;
        struct client *cl;

        if (thread == NULL)
                return;

        if (thread->nf_rx_buf[client].count == 0)
                return;

        cl = &clients[client];

        // Ensure destination NF is running and ready to receive packets
        if (!onvm_nf_is_valid(cl))
                return;

        /* Note: Adding check here might have impact on cases where NF is transferring packets from its Tx queue to Rx queue
         * Possible situation where the service Id is repeated in the chain and Instance is same for processing.
         */
        //#define PRE_PROCESS_DROP_ON_RX
        #ifdef PRE_PROCESS_DROP_ON_RX
        #ifdef DROP_APPROACH_2
        //#define MAX_RING_QUEUE_SIZE (CLIENT_QUEUE_RINGSIZE - PACKET_READ_SIZE)
        /* check here for the Tx Ring size to drop apriori to pushing to NFs Rx Ring */
        if(rte_ring_count(cl->tx_q) >= (CLIENT_QUEUE_RINGSIZE-rte_ring_count(cl->rx_q) - thread->nf_rx_buf[client].count - PACKET_READ_SIZE) ) {
        //if(rte_ring_count(cl->tx_q) >= (CLIENT_QUEUE_RINGSIZE-rte_ring_count(cl->rx_q)) ) {
        //if(rte_ring_count(cl->tx_q) >= MAX_RING_QUEUE_SIZE) {
        //if (rte_ring_full(cl->tx_q)) {
                for (i = 0; i < thread->nf_rx_buf[client].count; i++) {
                        onvm_pkt_drop(thread->nf_rx_buf[client].buffer[i]);
                }
                cl->stats.rx_drop += thread->nf_rx_buf[client].count;
                thread->nf_rx_buf[client].count = 0;
                //cl->stats.rx_drop += !onvm_pkt_drop(pkt); //onvm_pkt_drop(pkt); -- This call doesnt always ensure that freed packet is set to null; hence not a good way; revert others as well.
                return;
        }
        #endif // DROP_APPROACH_2
        #endif //PRE_PROCESS_DROP_ON_RX

        int enq_status = rte_ring_enqueue_bulk(cl->rx_q, (void **)thread->nf_rx_buf[client].buffer,
                                thread->nf_rx_buf[client].count);


#ifdef ENABLE_NF_BACKPRESSURE
        if ( 0 != enq_status) {
                onvm_detect_and_set_back_pressure(thread->nf_rx_buf[client].buffer, thread->nf_rx_buf[client].count, cl);
        }
#endif  //ENABLE_NF_BACKPRESSURE

        if ( -ENOBUFS == enq_status) {
                for (i = 0; i < thread->nf_rx_buf[client].count; i++) {
                        onvm_pkt_drop(thread->nf_rx_buf[client].buffer[i]);
                }
                cl->stats.rx_drop += thread->nf_rx_buf[client].count;
        }
        else {
                cl->stats.rx += thread->nf_rx_buf[client].count;
        }
        thread->nf_rx_buf[client].count = 0;
}


inline void
onvm_pkt_enqueue_port(struct thread_info *tx, uint16_t port, struct rte_mbuf *buf) {

        if (tx == NULL || buf == NULL)
                return;


        tx->port_tx_buf[port].buffer[tx->port_tx_buf[port].count++] = buf;
        if (tx->port_tx_buf[port].count == PACKET_READ_SIZE) {
                onvm_pkt_flush_port_queue(tx, port);
        }
}


inline void
onvm_pkt_enqueue_nf(struct thread_info *thread, uint16_t dst_service_id, struct rte_mbuf *pkt) {
        struct client *cl;
        uint16_t dst_instance_id;


        if (thread == NULL || pkt == NULL)
                return;

        // map service to instance and check one exists
        dst_instance_id = onvm_nf_service_to_nf_map(dst_service_id, pkt);
        if (dst_instance_id == 0) {
                onvm_pkt_drop(pkt);
                return;
        }

        // Ensure destination NF is running and ready to receive packets
        cl = &clients[dst_instance_id];
        if (!onvm_nf_is_valid(cl)) {
                onvm_pkt_drop(pkt);
                return;
        }

        #ifdef ENABLE_NF_BACKPRESSURE
        // Update the Instance ID values in the Service Chain:: in-order to optimize this add a flag field and set it, check every time ( but where/how to indicate for entire chain to be setup completely)
        struct onvm_flow_entry *flow_entry = NULL;
        if ((onvm_flow_dir_get_pkt(pkt, &flow_entry) >= 0) && flow_entry != NULL && flow_entry->sc != NULL) {
                struct onvm_pkt_meta *meta = onvm_get_pkt_meta(pkt);
                if(meta) {
                        flow_entry->sc->nf_instance_id[meta->chain_index] = (uint8_t)cl->instance_id;
                }
        }
        /* Option: Throttle NF/Drop the packets till the flag is cleared */
        #ifdef NF_BACKPRESSURE_APPROACH_1
        struct onvm_pkt_meta *meta = onvm_get_pkt_meta(pkt);
        if ((flow_entry) && (flow_entry->sc) && (flow_entry->sc->downstream_nf_overflow) && (flow_entry->sc->highest_downstream_nf_index_id < meta->chain_index)) {
                onvm_pkt_drop(pkt);
                cl->stats.rx_drop+=1;
                return;
        }
        #endif //NF_BACKPRESSURE_APPROACH_1
        #endif // ENABLE_NF_BACKPRESSURE

        /* For Drop: Earlier the better, but this part is not only expensive,
         * but can lead to drop of intermittent packets and not batch of packets, and can still result in Tx drops.
         */
        //#define PRE_PROCESS_DROP_ON_RX_0
        #ifdef PRE_PROCESS_DROP_ON_RX_0
        #define MAX_RING_QUEUE_SIZE (CLIENT_QUEUE_RINGSIZE - PACKET_READ_SIZE)
        /* check here for the Tx Ring size to drop apriori to pushing to NF */
        //if(rte_ring_count(cl->tx_q) >= (CLIENT_QUEUE_RINGSIZE-rte_ring_count(cl->rx_q) - thread->nf_rx_buf[dst_instance_id].count) ) {
        if(rte_ring_count(cl->tx_q) >= (CLIENT_QUEUE_RINGSIZE-rte_ring_count(cl->rx_q)) ) {
        //if(rte_ring_count(cl->tx_q) >= MAX_RING_QUEUE_SIZE) {
        //if (rte_ring_full(cl->tx_q)) {
                onvm_pkt_drop(pkt);
                cl->stats.rx_drop+=1;
                //cl->stats.rx_drop += !onvm_pkt_drop(pkt); //onvm_pkt_drop(pkt); -- This call doesnt always ensure that freed packet is set to null; hence not a good way; revert others as well.
                return;
        }
        #endif //PRE_PROCESS_DROP_ON_RX_0

        thread->nf_rx_buf[dst_instance_id].buffer[thread->nf_rx_buf[dst_instance_id].count++] = pkt;
        if (thread->nf_rx_buf[dst_instance_id].count == PACKET_READ_SIZE) {
                onvm_pkt_flush_nf_queue(thread, dst_instance_id);
        }
}


inline void
onvm_pkt_process_next_action(struct thread_info *tx, struct rte_mbuf *pkt, struct client *cl) {

        if (tx == NULL || pkt == NULL || cl == NULL)
                return;

        struct onvm_flow_entry *flow_entry;
        struct onvm_service_chain *sc;
        struct onvm_pkt_meta *meta = onvm_get_pkt_meta(pkt);
        int ret;

        ret = onvm_flow_dir_get_pkt(pkt, &flow_entry);
        if (ret >= 0) {
                sc = flow_entry->sc;
                meta->action = onvm_sc_next_action(sc, pkt);
                meta->destination = onvm_sc_next_destination(sc, pkt);
        } else {
                meta->action = onvm_sc_next_action(default_chain, pkt);
                meta->destination = onvm_sc_next_destination(default_chain, pkt);
        }

        switch (meta->action) {
                case ONVM_NF_ACTION_DROP:
                        onvm_pkt_drop(pkt);
                        cl->stats.act_drop++;
                        // if the packet is drop, then <return value> is 0
                        // and !<return value> is 1.
                        //cl->stats.act_drop += !onvm_pkt_drop(pkt);
                        break;
                case ONVM_NF_ACTION_TONF:
                        cl->stats.act_tonf++;
                        (meta->chain_index)++;
                        onvm_pkt_enqueue_nf(tx, meta->destination, pkt);
                        break;
                case ONVM_NF_ACTION_OUT:
                        cl->stats.act_out++;
                        (meta->chain_index)++;
                        onvm_pkt_enqueue_port(tx, meta->destination, pkt);
                        break;
                default:
                        break;
        }
        //(meta->chain_index)++;
}


/*******************************Helper function*******************************/


int
onvm_pkt_drop(struct rte_mbuf *pkt) {
        rte_pktmbuf_free(pkt);
        if (pkt != NULL) {
                return 1;
        }
        return 0;
}

#ifdef ENABLE_NF_BACKPRESSURE
void
onvm_detect_and_set_back_pressure(struct rte_mbuf *pkts[], uint16_t count, struct client *cl) {
        /*** Make sure this function is called only on error status on rx_enqueue() ***/
        /*** Detect the NF Rx Buffer overflow and signal this NF instance in the service chain as bottlenecked -- source of back-pressure -- all NFs prior to this in chain must throttle (either not scheduler or drop packets). ***/

        #ifdef ENABLE_NF_BACKPRESSURE
        struct onvm_pkt_meta *meta = NULL;
        uint16_t i;
        //unsigned rx_q_count = rte_ring_count(cl->rx_q);
        struct onvm_flow_entry *flow_entry = NULL;

        cl->rx_buffer_overflow = 1;

        for(i = 0; i < count; i++) {
                int ret = onvm_flow_dir_get_pkt(pkts[i], &flow_entry);
                if (ret >= 0 && flow_entry && flow_entry->sc) {
                        meta = onvm_get_pkt_meta(pkts[i]);
                        if (meta->chain_index > 1) {
                                // case where sc overflow is detected for first time in any of the downstream components
                                if ((flow_entry->sc->downstream_nf_overflow == 0)) {
                                        flow_entry->sc->downstream_nf_overflow = 1;
                                        flow_entry->sc->highest_downstream_nf_index_id = meta->chain_index;
                                        uint8_t index = 1;
                                        for(; index < meta->chain_index; index++ ) {
                                                clients[flow_entry->sc->nf_instance_id[index]].throttle_this_upstream_nf = 1;
                                                if (meta->chain_index > clients[flow_entry->sc->nf_instance_id[index]].highest_downstream_nf_index_id) { //if(clients[flow_entry->sc->nf_instance_id[index]].highest_downstream_nf_index_id == 0) {
                                                        clients[flow_entry->sc->nf_instance_id[index]].highest_downstream_nf_index_id = meta->chain_index;
                                                }
                                        }
                                }
                                //case where sc overflow is detected in further downstream components
                                else if ((flow_entry->sc->downstream_nf_overflow) && (flow_entry->sc->highest_downstream_nf_index_id < meta->chain_index)) {
                                        flow_entry->sc->highest_downstream_nf_index_id = meta->chain_index;
                                        uint8_t index = 1;
                                        for(; index < meta->chain_index; index++ ) {
                                                clients[flow_entry->sc->nf_instance_id[index]].throttle_this_upstream_nf = 1;
                                                if (meta->chain_index > clients[flow_entry->sc->nf_instance_id[index]].highest_downstream_nf_index_id) {
                                                        clients[flow_entry->sc->nf_instance_id[index]].highest_downstream_nf_index_id = meta->chain_index;
                                                }
                                        }
                                }
                                //approach: extend the service chain to keep track of client_nf_ids that service the chain, in-order to know which NFs to throttle in the wakeup thread..?
                                //Test and Set
                        }
                        flow_entry = NULL;
                        meta = NULL;
                }
                //global single chain scenario
                /** Note: This only works for the default chain case where service ID of chain is always in increasing order **/
                else {
                        if (cl->info->service_id > 1) {
                                downstream_nf_overflow = 1;
                                if (cl->info->service_id > highest_downstream_nf_service_id) {
                                        highest_downstream_nf_service_id = cl->info->service_id;
                                }
                        }
                        break; // just do once
                }
        }
        #endif //ENABLE_NF_BACKPRESSURE
}

void
onvm_check_and_reset_back_pressure(struct rte_mbuf *pkts[], uint16_t count, struct client *cl) {

        #ifdef ENABLE_NF_BACKPRESSURE
        struct onvm_pkt_meta *meta = NULL;
        uint16_t i;
        unsigned rx_q_count = rte_ring_count(cl->rx_q);
        struct onvm_flow_entry *flow_entry = NULL;
        // check if rx_q_size has decreased to acceptable level
        if (rx_q_count >= CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE) {
                return;
        }
        cl->rx_buffer_overflow = 0;
        //  if acceptable range then for all service chains, if marked to be overflow, then reset the overflow status
        for(i = 0; i < count; i++) {
                int ret = onvm_flow_dir_get_pkt(pkts[i], &flow_entry);
                if (ret >= 0 && flow_entry && flow_entry->sc) {
                                if(flow_entry->sc->downstream_nf_overflow ) {
                                        meta = onvm_get_pkt_meta(pkts[i]);
                                        if(meta->chain_index == flow_entry->sc->highest_downstream_nf_index_id) { //if(meta->chain_index >= flow_entry->sc->highest_downstream_nf_index_id) {
                                                // also reset the chain's downstream NFs cl->downstream_nf_overflow and cl->highest_downstream_nf_index_id=0. But How?? <track the nf_instance_id in the service chain.
                                                unsigned nf_index=0;
                                                for(; nf_index < meta->chain_index; nf_index++) {
                                                        clients[flow_entry->sc->nf_instance_id[nf_index]].throttle_this_upstream_nf = 0;
                                                        clients[flow_entry->sc->nf_instance_id[nf_index]].highest_downstream_nf_index_id = 0;
                                                }
                                                flow_entry->sc->downstream_nf_overflow = 0;
                                                flow_entry->sc->highest_downstream_nf_index_id=0;
                                        }
                                }
                        }
                        //global single chain scenario
                        /** Note: This only works for the default chain case where service ID of chain is always in increasing order **/
                        else {
                                if (downstream_nf_overflow) {
                                        // If service id is of any downstream that is/are bottlenecked then "move the lowest literally to next higher number" and when it is same as highsest reset bottlenext flag to zero
                                        //  if(rte_ring_count(cl->rx_q) < CLIENT_QUEUE_RING_WATER_MARK_SIZE) {
                                        if(rte_ring_count(cl->rx_q) < CLIENT_QUEUE_RING_LOW_WATER_MARK_SIZE) {
                                                if (cl->info->service_id == highest_downstream_nf_service_id) {
                                                        downstream_nf_overflow = 0;
                                                        highest_downstream_nf_service_id = 0;
                                                        lowest_upstream_to_throttle = 0;
                                                }
                                        }
                                }
                                break; // just do once
                        }
        }
        #endif //ENABLE_NF_BACKPRESSURE
}
#endif // ENABLE_NF_BACKPRESSURE
