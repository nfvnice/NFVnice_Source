/*********************************************************************
 *                     openNetVM
 *       https://github.com/sdnfv/openNetVM
 *
 *  Copyright 2015 George Washington University
 *            2015 University of California Riverside
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the Li¡cense for the specific language governing permissions and
 *  limitations under the License.
 *
 *  schain_preload.c - NF that preloads FlowTable entries with service chains.
 *              -- List of service chains are parsed from "services.txt"
 *              Service chains are randomly assigned < Round Robin >
 *              -- List of IPv4 % Tuple Rules in the order
 *              <SRC_IP,DST_IP,SRC_PORT,DST_PORT,IP_PROTO>
 *              are parsed from ipv4rules.txt
 *              -- In addition the baseIP, MaxIPs and ports can be specified to
 *              pre-populate FlowTable rules.
 *              -- Can also dynamically set the SC on the missing FT entries.
 *              -- NF can be registered with any serviceID (preferably 1)
 *
 ********************************************************************/

#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_memory.h>
#include <rte_cycles.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_flow_table.h"
#include "onvm_sc_common.h"
#include "onvm_flow_dir.h"
#include "onvm_sc_mgr.h"

#define NF_TAG "schain_clear"

#define MIN(a,b) ((a) < (b)? (a):(b))
#define MAX(a,b) ((a) > (b)? (a):(b))

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

/* Service Chain Related entries */
#define MAX_SERVICE_CHAINS 32
int services[MAX_SERVICE_CHAINS][ONVM_MAX_CHAIN_LENGTH];
int max_service_chains=0;

/* IPv4 5Tuple Rules for Flow Table Entries */
#define MAX_FLOW_TABLE_ENTRIES 1024
struct onvm_ft_ipv4_5tuple ipv4_5tRules[MAX_FLOW_TABLE_ENTRIES];
uint32_t max_ft_entries=0;
//static const char *base_ip_addr = "10.0.0.1";

/* List of Global Command Line Arguments */
typedef struct globalArgs {
        uint32_t destination;               /* -d <destination_service ID> */
        uint32_t print_delay;               /* -p <print delay in num_pakets> */
        const char* servicechain_file;      /* -s <service chain listings> */
        const char* ipv4rules_file;         /* -r <IPv45Tuple Flow Table entries> */
        const char* base_ip_addr;           /* -b <IPv45Tuple Base Ip Address> */
        uint32_t max_ip_addrs;              /* -m <Maximum number of IP Addresses> */
        uint32_t max_ft_rules;              /* -M <Maximum number of FT entries> */
}globalArgs_t;
static const char *optString = "d:p:s:r:b:m:M";

static globalArgs_t globals = {
        .destination = 0,
        .print_delay = 1, //1000000,
        .servicechain_file = "services.txt",
        .ipv4rules_file = "ipv4rules.txt",
        .base_ip_addr   = "10.0.0.1",
        .max_ip_addrs   = 10,
        .max_ft_rules   = MAX_FLOW_TABLE_ENTRIES,
};


static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -d <destination> -p <print_delay>"
                "-s <service_chain_file> -r <IPv4_5tuple Rules file>"
                "-b <base_ip_address> -m <max_num_ips>        \n\n", progname);
}

static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;

        while ((c = getopt(argc, argv, optString)) != -1) {
                switch (c) {
                case 'd':
                        globals.destination = strtoul(optarg, NULL, 10);
                        break;
                case 'p':
                        globals.print_delay = strtoul(optarg, NULL, 10);
                        break;
                case 's':
                        globals.servicechain_file = optarg;
                        break;
                case 'r':
                        globals.ipv4rules_file = optarg;
                        break;
                case 'b':
                        globals.base_ip_addr = optarg;
                        break;
                case 'm':
                        globals.max_ip_addrs = strtoul(optarg, NULL, 10);
                        break;
                case 'M':
                        globals.max_ft_rules = strtoul(optarg, NULL, 10);
                        break;
                case '?':
                        usage(progname);
                        if (optopt == 'd')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (optopt == 'p')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (isprint(optopt))
                                RTE_LOG(INFO, APP, "Unknown option `-%c'.\n", optopt);
                        else
                                RTE_LOG(INFO, APP, "Unknown option character `\\x%x'.\n", optopt);
                        return -1;
                default:
                        usage(progname);
                        return -1;
                }
        }
        return optind;
}

static int
clear_flow_rule_and_sc_entries(void) {

        int ret = 0;

        ret = onvm_flow_dir_clear_all_entries();
        return ret;
}

int main(int argc, char *argv[]) {
        int arg_offset;

        const char *progname = argv[0];

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG)) < 0)
                return -1;
        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        
        /* Map the sdn_ft table */
        onvm_flow_dir_nf_init();
        
        /* Setup pre-defined set of FT entries UDP protocol: Extend optargs as necessary */ 
        int ret = 1;
        while(ret > 0) {
                ret = clear_flow_rule_and_sc_entries();
                sleep(3);
        }


        if (0 == globals.destination || globals.destination == nf_info->service_id) {
                        globals.destination = nf_info->service_id + 1;
        }

        //onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending");
        
        return 0;
}
