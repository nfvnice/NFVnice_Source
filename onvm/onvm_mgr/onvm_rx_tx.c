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
 * onvm_rx_tx.c - for all packet functions aside from master
 ********************************************************************/

#include "onvm_includes.h"
#include "onvm_rx_tx.h"
#include "onvm_mgr_nf.h"

/**
 Helper function that drop a packet and uses the success return convention
 (a 0).
 */
int onvm_rx_tx_drop_packet(struct rte_mbuf *pkt) {
        rte_pktmbuf_free(pkt);
        if (pkt != NULL) {
                return 1;
        }
        return 0;
}

/*
 * Send a burst of traffic to a client, assuming there are packets
 * available to be sent to this client
 */
void onvm_rx_tx_flush_nf_queue(struct thread_info *thread, uint16_t client) {
        uint16_t i;
        struct client *cl;

        if (thread->nf_rx_buf[client].count == 0)
                return;

        cl = &clients[client];

        // Ensure destination NF is running and ready to receive packets
        if (!onvm_mgr_nf_is_valid_nf(cl))
                return;

        if (rte_ring_enqueue_bulk(cl->rx_q, (void **)thread->nf_rx_buf[client].buffer,
                        thread->nf_rx_buf[client].count) != 0) {
                for (i = 0; i < thread->nf_rx_buf[client].count; i++) {
                        onvm_rx_tx_drop_packet(thread->nf_rx_buf[client].buffer[i]);
                }
                cl->stats.rx_drop += thread->nf_rx_buf[client].count;
        } else {
                cl->stats.rx += thread->nf_rx_buf[client].count;
        }
        thread->nf_rx_buf[client].count = 0;
}

/**
 * Send a burst of packets out a NIC port.
 */
void onvm_rx_tx_flush_port_queue(struct thread_info *tx, uint16_t port) {
        uint16_t i, sent;
        volatile struct tx_stats *tx_stats;

        if (tx->port_tx_buf[port].count == 0)
                return;

        tx_stats = &(ports->tx_stats);
        sent = rte_eth_tx_burst(port, tx->queue_id, tx->port_tx_buf[port].buffer, tx->port_tx_buf[port].count);
        if (unlikely(sent < tx->port_tx_buf[port].count)) {
                for (i = sent; i < tx->port_tx_buf[port].count; i++) {
                        onvm_rx_tx_drop_packet(tx->port_tx_buf[port].buffer[i]);
                }
                tx_stats->tx_drop[port] += (tx->port_tx_buf[port].count - sent);
        }
        tx_stats->tx[port] += sent;

        tx->port_tx_buf[port].count = 0;
}

/**
 * Add a packet to a buffer destined for an NF's RX queue.
 */
inline void onvm_rx_tx_enqueue_nf_packet(struct thread_info *thread, uint16_t dst_service_id, struct rte_mbuf *pkt) {
        struct client *cl;
        uint16_t dst_instance_id;

        // map service to instance and check one exists
        dst_instance_id = onvm_mgr_nf_service_to_nf_map(dst_service_id, pkt);
        if (dst_instance_id == 0) {
                onvm_rx_tx_drop_packet(pkt);
                return;
        }

        // Ensure destination NF is running and ready to receive packets
        cl = &clients[dst_instance_id];
        if (!onvm_mgr_nf_is_valid_nf(cl)) {
                onvm_rx_tx_drop_packet(pkt);
                return;
        }

        thread->nf_rx_buf[dst_instance_id].buffer[thread->nf_rx_buf[dst_instance_id].count++] = pkt;
        if (thread->nf_rx_buf[dst_instance_id].count == PACKET_READ_SIZE) {
                onvm_rx_tx_flush_nf_queue(thread, dst_instance_id);
        }
}

/**
 * Add a packet to a buffer destined for a port's TX queue.
 */
inline void onvm_rx_tx_enqueue_port_packet(struct thread_info *tx, uint16_t port, struct rte_mbuf *buf) {
        tx->port_tx_buf[port].buffer[tx->port_tx_buf[port].count++] = buf;
        if (tx->port_tx_buf[port].count == PACKET_READ_SIZE) {
                onvm_rx_tx_flush_port_queue(tx, port);
        }
}

/*
 * Process a packet with action NEXT
 */
inline void onvm_rx_tx_process_next_action_packet(struct thread_info *tx, struct rte_mbuf *pkt, struct client *cl) {
	struct onvm_flow_entry *flow_entry;
	struct onvm_service_chain *sc;
	struct onvm_pkt_meta *meta = onvm_get_pkt_meta(pkt);
	int ret;

	ret = onvm_flow_dir_get_pkt(pkt, &flow_entry);
	if (ret >= 0) {
		sc = flow_entry->sc;
		meta->action = onvm_sc_next_action(sc, pkt);
		meta->destination = onvm_sc_next_destination(sc, pkt);
	}
	else {
		meta->action = onvm_sc_next_action(default_chain, pkt);
		meta->destination = onvm_sc_next_destination(default_chain, pkt);
	}

	switch(meta->action) {
		case ONVM_NF_ACTION_DROP:
                        // if the packet is drop, then <return value> is 0
                        // and !<return value> is 1.
                        cl->stats.act_drop += !onvm_rx_tx_drop_packet(pkt);
			break;
		case ONVM_NF_ACTION_TONF:
                        cl->stats.act_tonf++;
			onvm_rx_tx_enqueue_nf_packet(tx, meta->destination, pkt);
			break;
		case ONVM_NF_ACTION_OUT:
                        cl->stats.act_out++;
			onvm_rx_tx_enqueue_port_packet(tx, meta->destination, pkt);
			break;
		default:
			break;
	}
	(meta->chain_index)++;
}

/*
 * This function takes a group of packets and routes them
 * to the first client process. Simply forwarding the packets
 * without checking any of the packet contents.
 */
void onvm_rx_process_rx_packet_batch(struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t i;
        struct onvm_pkt_meta *meta;
	struct onvm_flow_entry *flow_entry;
	struct onvm_service_chain *sc;
	int ret;

        for (i = 0; i < rx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
		(void)meta;
                meta->src = 0;
                meta->chain_index = 0;
		ret = onvm_flow_dir_get_pkt(pkts[i], &flow_entry);
		if (ret >= 0) {
			sc = flow_entry->sc;
			meta->action = onvm_sc_next_action(sc, pkts[i]);
			meta->destination = onvm_sc_next_destination(sc, pkts[i]);
		}
		else {
			meta->action = onvm_sc_next_action(default_chain, pkts[i]);
			meta->destination = onvm_sc_next_destination(default_chain, pkts[i]);
		}
                /* PERF: this might hurt performance since it will cause cache
                 * invalidations. Ideally the data modified by the NF manager
                 * would be a different line than that modified/read by NFs.
                 * That may not be possible.
                 */

                (meta->chain_index)++;
		onvm_rx_tx_enqueue_nf_packet(rx, meta->destination, pkts[i]);
        }

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (rx->nf_rx_buf[i].count != 0) {
			onvm_rx_tx_flush_nf_queue(rx, i);
		}
	}
}

/*
 * Handle the packets from a client. Check what the next action is
 * and forward the packet either to the NIC or to another NF Client.
 */
void onvm_tx_process_tx_packet_batch(struct thread_info *tx, struct rte_mbuf *pkts[], uint16_t tx_count, struct client *cl) {
        uint16_t i;
        struct onvm_pkt_meta *meta;

        for (i = 0; i < tx_count; i++) {
                meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
                meta->src = cl->instance_id;
                if (meta->action == ONVM_NF_ACTION_DROP) {
                        // if the packet is drop, then <return value> is 0
                        // and !<return value> is 1.
                        cl->stats.act_drop += !onvm_rx_tx_drop_packet(pkts[i]);
                } else if (meta->action == ONVM_NF_ACTION_NEXT) {
                        /* TODO: Here we drop the packet : there will be a flow table
                        in the future to know what to do with the packet next */
                        cl->stats.act_next++;
			onvm_rx_tx_process_next_action_packet(tx, pkts[i], cl);
                } else if (meta->action == ONVM_NF_ACTION_TONF) {
                        cl->stats.act_tonf++;
                        onvm_rx_tx_enqueue_nf_packet(tx, meta->destination, pkts[i]);
                } else if (meta->action == ONVM_NF_ACTION_OUT) {
                        cl->stats.act_out++;
                        onvm_rx_tx_enqueue_port_packet(tx, meta->destination, pkts[i]);
                } else {
                        printf("ERROR invalid action : this shouldn't happen.\n");
                        onvm_rx_tx_drop_packet(pkts[i]);
                        return;
                }
        }
}
