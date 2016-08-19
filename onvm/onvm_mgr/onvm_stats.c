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
                          onvm_stats.c

   This file contain the implementation of all functions related to
   statistics display in the manager.

******************************************************************************/


#include "onvm_mgr.h"
#include "onvm_stats.h"
#include "onvm_nf.h"


/****************************Interfaces***************************************/


void
onvm_stats_display_all(unsigned difftime) {
        onvm_stats_clear_terminal();
        onvm_stats_display_ports(difftime);
        onvm_stats_display_clients(difftime);
}


void
onvm_stats_clear_all_clients(void) {
        unsigned i;

        for (i = 0; i < MAX_CLIENTS; i++) {
                clients[i].stats.rx = clients[i].stats.rx_drop = 0;
                clients[i].stats.act_drop = clients[i].stats.act_tonf = 0;
                clients[i].stats.act_next = clients[i].stats.act_out = 0;
        }
}

void
onvm_stats_clear_client(uint16_t id) {
        clients[id].stats.rx = clients[id].stats.rx_drop = 0;
        clients[id].stats.act_drop = clients[id].stats.act_tonf = 0;
        clients[id].stats.act_next = clients[id].stats.act_out = 0;
}


/****************************Internal functions*******************************/


void
onvm_stats_display_ports(unsigned difftime) {
        unsigned i;
        /* Arrays to store last TX/RX count to calculate rate */
        static uint64_t tx_last[RTE_MAX_ETHPORTS];
        static uint64_t rx_last[RTE_MAX_ETHPORTS];

        printf("PORTS\n");
        printf("-----\n");
        for (i = 0; i < ports->num_ports; i++)
                printf("Port %u: '%s'\t", (unsigned)ports->id[i],
                                onvm_stats_print_MAC(ports->id[i]));
        printf("\n\n");
        for (i = 0; i < ports->num_ports; i++) {
                printf("Port %u - rx: %9"PRIu64"  (%9"PRIu64" pps)\t"
                                "tx: %9"PRIu64"  (%9"PRIu64" pps)\n",
                                (unsigned)ports->id[i],
                                ports->rx_stats.rx[ports->id[i]],
                                (ports->rx_stats.rx[ports->id[i]] - rx_last[i])
                                        /difftime,
                                ports->tx_stats.tx[ports->id[i]],
                                (ports->tx_stats.tx[ports->id[i]] - tx_last[i])
                                        /difftime);

                rx_last[i] = ports->rx_stats.rx[ports->id[i]];
                tx_last[i] = ports->tx_stats.tx[ports->id[i]];
        }
}


void
onvm_stats_display_clients(unsigned difftime) {
        unsigned i;

        #ifdef INTERRUPT_SEM
        uint64_t vol_rate;
        uint64_t rx_drop_rate;
        uint64_t serv_rate;
        uint64_t serv_drop_rate;
        uint64_t rx_qlen;
        uint64_t tx_qlen;
        uint64_t comp_cost;
        uint64_t num_wakeups = 0;
        uint64_t prev_num_wakeups = 0;
        uint64_t wakeup_rate;
        /* unsigned sleep_time = 1; // This info is not availble anymore: 
                // must move entire wakeup to separate new function  onvm_stats_display_client_wakeup_info(difftime)
        */
        #else
        printf ("diff_time= 0x%4x", difftime);       
        #endif

        #ifdef INTERRUPT_SEM
        for (i = 0; i < ONVM_NUM_WAKEUP_THREADS; i++) {
                num_wakeups += wakeup_infos[i].num_wakeups;
                prev_num_wakeups += wakeup_infos[i].prev_num_wakeups;
                wakeup_infos[i].prev_num_wakeups = wakeup_infos[i].num_wakeups;
        }

        wakeup_rate = (num_wakeups - prev_num_wakeups) / difftime; 
        printf("num_wakeups=%"PRIu64", wakeup_rate=%"PRIu64"\n", num_wakeups, wakeup_rate);
        #endif

        printf("\nCLIENTS\n");
        printf("-------\n");
        for (i = 0; i < MAX_CLIENTS; i++) {
                if (!onvm_nf_is_valid(&clients[i]))
                        continue;
                const uint64_t rx = clients[i].stats.rx;
                const uint64_t rx_drop = clients[i].stats.rx_drop;
                const uint64_t tx = clients_stats->tx[i];
                const uint64_t tx_drop = clients_stats->tx_drop[i];
                const uint64_t act_drop = clients[i].stats.act_drop;
                const uint64_t act_next = clients[i].stats.act_next;
                const uint64_t act_out = clients[i].stats.act_out;
                const uint64_t act_tonf = clients[i].stats.act_tonf;
                const uint64_t act_buffer = clients_stats->tx_buffer[i];
                const uint64_t act_returned = clients_stats->tx_returned[i];

                printf("Client %2u - rx: %9"PRIu64" rx_drop: %9"PRIu64" next: %9"PRIu64" drop: %9"PRIu64" ret: %9"PRIu64"\n"
                                    "tx: %9"PRIu64" tx_drop: %9"PRIu64" out:  %9"PRIu64" tonf: %9"PRIu64" buf: %9"PRIu64"\n",
                                clients[i].info->instance_id,
                                rx, rx_drop, act_next, act_drop, act_returned,
                                tx, tx_drop, act_out, act_tonf, act_buffer);
        
          
                #ifdef INTERRUPT_SEM
                /* periodic/rate specific statistics of NF instance */ 
                const uint64_t prev_rx = clients[i].stats.prev_rx;
                const uint64_t prev_rx_drop = clients[i].stats.prev_rx_drop;
                const uint64_t prev_tx = clients_stats->prev_tx[i];
                const uint64_t prev_tx_drop = clients_stats->prev_tx_drop[i];

                vol_rate = (rx - prev_rx) / difftime;
                rx_drop_rate = (rx_drop - prev_rx_drop) / difftime;
                serv_rate = (tx - prev_tx) / difftime;
                serv_drop_rate = (tx_drop - prev_tx_drop) / difftime;
                rx_qlen = rte_ring_count(clients[i].rx_q);
                tx_qlen = rte_ring_count(clients[i].tx_q);
                comp_cost = clients_stats->comp_cost[i];

                printf("instance_id=%d, vol_rate=%"PRIu64", rx_drop_rate=%"PRIu64","
                " comp_cost=%"PRIu64", serv_rate=%"PRIu64","
                "serv_drop_rate=%"PRIu64", rx_qlen=%"PRIu64", tx_qlen=%"PRIu64", msg_flag=%d\n",
                clients[i].info->instance_id, vol_rate, rx_drop_rate, comp_cost, 
                serv_rate, serv_drop_rate, rx_qlen, tx_qlen, rte_atomic16_read(clients[i].shm_server));

                //clients[i].stats.prev_rx = clients[i].stats.rx;
                //clients[i].stats.prev_rx_drop = clients[i].stats.rx_drop;
                //clients_stats->prev_tx[i] = clients_stats->tx[i];
                //clients_stats->prev_tx_drop[i] = clients_stats->tx_drop[i];                
                #endif



        }

        printf("\n");
}


/***************************Helper functions**********************************/


void
onvm_stats_clear_terminal(void) {
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

        printf("%s%s", clr, topLeft);
}


const char *
onvm_stats_print_MAC(uint8_t port) {
        static const char err_address[] = "00:00:00:00:00:00";
        static char addresses[RTE_MAX_ETHPORTS][sizeof(err_address)];

        if (unlikely(port >= RTE_MAX_ETHPORTS))
                return err_address;
        if (unlikely(addresses[port][0] == '\0')) {
                struct ether_addr mac;
                rte_eth_macaddr_get(port, &mac);
                snprintf(addresses[port], sizeof(addresses[port]),
                                "%02x:%02x:%02x:%02x:%02x:%02x\n",
                                mac.addr_bytes[0], mac.addr_bytes[1],
                                mac.addr_bytes[2], mac.addr_bytes[3],
                                mac.addr_bytes[4], mac.addr_bytes[5]);
        }
        return addresses[port];
}
