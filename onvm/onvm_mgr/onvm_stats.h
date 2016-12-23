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
                                 onvm_stats.h

            This file contains all function prototypes related to
            statistics display.

******************************************************************************/


#ifndef _ONVM_STATS_H_
#define _ONVM_STATS_H_


/*********************************Interfaces**********************************/
typedef struct onvm_stats_snapshot {
        uint64_t rx_delta;          // rx packets in sampled interval
        uint64_t tx_delta;          // tx packets in sampled interval
        uint64_t rx_drop_delta;     // rx drops in sampled interval
        uint64_t tx_drop_delta;     // tx drops in sampled interval
        uint32_t arrival_rate;      // (rx_delta+rx_drops_delta)/interval
        uint32_t rx_rate;           // (rx_delta)/interval
        uint32_t serv_rate;         // (tx_rate)/interval)
        uint32_t tx_rate;           // (tx_rate+tx_drops_delta)/interval)
        uint32_t rx_drop_rate;      // (rx_drops_delta)/interval)
        uint32_t tx_drop_rate;      // (tx_drops_delta)/interval)
}onvm_stats_snapshot_t;

/* Interace to retieve nf stats
 * difftime: if 0 : only read but do not update params and rate else update
 */
int get_onvm_nf_stats_snapshot(unsigned nf_index, onvm_stats_snapshot_t *snapshot, unsigned difftime);

/* Interace to retieve nf stats
 * difftime: if 0 : read and update params and cache params locally; else return cached params.
 */
int get_onvm_nf_stats_snapshot_v2(unsigned nf_index, onvm_stats_snapshot_t *snapshot, unsigned difftime);

/*
 * Interface called by the ONVM Manager to display all statistics
 * available.
 *
 * Input : time passed since last display (to compute packet rate)
 *
 */
void onvm_stats_display_all(unsigned difftime);


/*
 * Interface called by the ONVM Manager to clear all clients statistics
 * available.
 *
 * Note : this function doesn't use onvm_stats_clear_client for each client,
 * since with a huge number of clients, the additional functions calls would
 * incur a visible slowdown.
 *
 */
void onvm_stats_clear_all_clients(void);


/*
 * Interface called by the ONVM Manager to clear one client's statistics.
 *
 * Input : the client id
 *
 */
void onvm_stats_clear_client(uint16_t id);


/******************************Main functions*********************************/


/*
 * Function displaying statistics for all ports
 *
 * Input : time passed since last display (to compute packet rate)
 *
 */
void onvm_stats_display_ports(unsigned difftime);


/*
 * Function displaying statistics for all clients
 *
 * Input : time passed since last display (to compute packet rate)
 */
void onvm_stats_display_clients(unsigned difftime);

/*
 * Function displaying statistics for all active service chains (flow_entry*)
 *
 * Input : time passed since last display (to compute packet rate)
 */
void onvm_stats_display_chains(unsigned difftime);
/******************************Helper functions*******************************/


/*
 * Function clearing the terminal and moving back the cursor to the top left.
 * 
 */
void onvm_stats_clear_terminal(void);


/*
 * Function giving the MAC address of a port in string format.
 *
 * Input  : port
 * Output : its MAC address
 * 
 */
const char * onvm_stats_print_MAC(uint8_t port);


#endif  // _ONVM_STATS_H_
