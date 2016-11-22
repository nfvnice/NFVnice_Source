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

#define NF_TAG "schain_preload"

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

static void
parse_services(int services[][ONVM_MAX_CHAIN_LENGTH]) {
        FILE *fp = fopen(globals.servicechain_file, "rb");
        int i=0;
        
        printf("parsing services in file services.txt\n\n");
        if (fp == NULL) {
                fprintf(stderr, "Can not open services.txt file\n");
                exit(1);
        }

        while (fp && !feof(fp)) {
                char line[64] = "";
                char svc[64] = "";
                if (fgets(line, 63, fp) == NULL) {
                        continue;
                }
                printf("parsing services in line[%s]", line);

                int llen = strlen(line);
                int slen = 0;
                int svc_index = 0;
                int added = 0;
                for(i=0; i< llen; i++) {
                        if(( (',' == line[i])  || ('\0' == line[i]) || ('\n' == line[i]) ) && (slen > 0)) {
                                svc[slen++] = '\0';
                                int svc_id = atoi(svc);
                                if (svc_id > 0) {
                                        services[max_service_chains][svc_index++] = svc_id;
                                }
                                slen = 0;
                                added+=1;
                                
                        }              
                        if (line[i] != '\0' && line[i] != ',') {
                                svc[slen++] = line[i];
                        }
                        if(ONVM_MAX_CHAIN_LENGTH <= svc_index) {
                                printf("service chain reached maximum length: %d",(int)svc_index);    
                                svc_index = 0;
                                break;
                        }
                }
                if (added) {
                        max_service_chains++;
                        added = 0;
                }
                if (MAX_SERVICE_CHAINS <= max_service_chains ) {
                        printf("Reached the limit on allowed num of service chains [%d]!", max_service_chains);
                        break;
                }
        }
        if (fp) {
                fclose(fp);
        }
#define DEBUG
#ifdef DEBUG
        int j = 0;
        printf("Read Service List!\n");
        for(i=0; i< max_service_chains; i++){
                for(j=0; j< ONVM_MAX_CHAIN_LENGTH; j++){
                        printf("%d ", services[i][j]);
                }
                printf("\n");
        }
        printf("\n\n************\n\n");
#else
        printf("Total Service Chains = [%d]", max_service_chains);
#endif
        return;
}

static uint32_t get_ipv4_value(const char *ip_addr){


        if (NULL == ip_addr) {
                return 0;
        }
        struct sockaddr_in antelope;
        inet_aton(ip_addr, &antelope.sin_addr);
        #ifdef DEBUG_0
        printf("For IP:[%s] the IPv4 value is: [%x]\n", ip_addr, antelope.sin_addr.s_addr);
        #endif
        return rte_be_to_cpu_32(antelope.sin_addr.s_addr);

        /*
         * Alternate logic to generate internally:
         * "10.0.0.1"  => 10*2^24 + 0*2^16 + 0*2^8 + 1;
         */
        #ifdef DEBUG_0
        uint32_t ipv4_value = 0;
        in_addr_t ipv4 = inet_addr(ip_addr);
        ipv4_value = ipv4;

        int a=0,b=0,c=0,d=0;
        sscanf(ip_addr, "%d.%d.%d.%d", &a,&b,&c,&d);
        uint32_t ipv4_val2 = (a << 24) + (b << 16) + (c << 8) + d;
        uint32_t ipv4_val3 = (d << 24) + (c << 16) + (b << 8) + a;
        printf("For IP:[%s] the IPv4 values are: [%x: %x], [%x : %x] \n", ip_addr, ipv4_value, antelope.sin_addr.s_addr, ipv4_val2,  ipv4_val3);
        //exit(1);
        return rte_be_to_cpu_32(ipv4_val2);
        #endif
}
static void
parse_ipv4_5t_rules(void) {
        FILE *fp = fopen(globals.ipv4rules_file, "rb");
        uint32_t i=0;

        printf("parsing services in file ipv4rules_file.txt\n\n");
        if (fp == NULL) {
                fprintf(stderr, "Can not open ipv4rules_file.txt file\n");
                exit(1);
        }
        while (fp && !feof(fp)) {
                char line[256] = "";
                if (fgets(line, 255, fp) == NULL) {
                        continue;
                }
                //printf("parsing ipv4rule in line[%s]", line);
                char *s = line, *in[6], *sp=NULL;
                uint32_t num_cols = 5;
                static const char *dlm = ",";
                for (i = 0; i != num_cols; i++, s = NULL) {
                        in[i] = strtok_r(s, dlm, &sp);
                        if (in[i] == NULL) {
                                break;
                        }
                }
                if (i >=5) {
                        //onvm_ft_ipv4_5tuple read_tuple = {0,0,0,0,0};
                        //read_tuple.src_addr = ;
                        ipv4_5tRules[max_ft_entries].src_addr = get_ipv4_value(in[0]);
                        ipv4_5tRules[max_ft_entries].dst_addr = get_ipv4_value(in[1]);
                        ipv4_5tRules[max_ft_entries].src_port =(uint16_t)(strtoul(in[2],NULL,10));
                        ipv4_5tRules[max_ft_entries].dst_port =(uint16_t)(strtoul(in[3],NULL,10));
                        ipv4_5tRules[max_ft_entries].proto = (uint8_t) strtoul(in[4],NULL,10);
                        if( (ipv4_5tRules[max_ft_entries].src_addr && ipv4_5tRules[max_ft_entries].dst_addr) &&
                            (IP_PROTOCOL_TCP == ipv4_5tRules[max_ft_entries].proto  || IP_PROTOCOL_UDP == ipv4_5tRules[max_ft_entries].proto)) {
                                max_ft_entries++;
                        }

                }
                //if (MAX_FLOW_TABLE_ENTRIES <= max_ft_entries ) {
                if (globals.max_ft_rules <= max_ft_entries ) {
                        printf("Reached the limit on allowed num of flow entries [%d]!", max_ft_entries);
                        break;
                }
        }
        if (fp) {
                fclose(fp);
        }
#define DEBUG
#ifdef DEBUG
        printf("Read IPv4-5T List!\n");
        for(i=0; i< max_ft_entries; i++){
                printf("%u %u %u %u %u \n", ipv4_5tRules[i].src_addr, ipv4_5tRules[i].dst_addr,
                        ipv4_5tRules[i].src_port, ipv4_5tRules[i].dst_port, ipv4_5tRules[i].proto);
        }
        printf("\n\n************\n\n");
#else
        printf("Total FT Entries = [%d]", max_ft_entries);
#endif
        return;
}

static int 
setup_service_chain_for_flow_entry(struct onvm_service_chain *sc, int sc_index, int rev_order) {
        static uint32_t next_sc = 0;
        int index = 0, service_id=0, chain_len = 0;
        if (0 == max_service_chains) {
                return -1;
        }


        //Get the sc_index based on either valid passed value or static incremental
        //sc_index = (sc_index >= 0 && sc_index < max_service_chains)? (sc_index):((int)next_sc);
        uint8_t use_input_sc_index = 0;
        if(sc_index >= 0 && sc_index < max_service_chains) {
                use_input_sc_index = 1;
        }
        else {
                sc_index =  (int)next_sc;
        }
        /* Setup the chain in reverse order makes sense only when use_input_sc_index=1 */
        if (1 == use_input_sc_index && 1 == rev_order) {
                int last_index = 0;
                while(services[sc_index][last_index++] != -1 || last_index < ONVM_MAX_CHAIN_LENGTH);
                printf("\n Adding Flip Service chain of Length [%d]: ", last_index-1);
                for(index =last_index-1; index >=0; index--) {
                        service_id = services[sc_index][index]; //service_id = services[next_sc][index];
                        if (service_id > 0){
                                /* if(chain_len == 0){
                                        onvm_sc_set_entry(sc, 0, ONVM_NF_ACTION_TONF, service_id);
                                }
                                else */{
                                        onvm_sc_append_entry(sc, ONVM_NF_ACTION_TONF, service_id);
                                        printf(" [%d] \t ", service_id);
                                }
                                chain_len++;
                        }
                }
        }
        else {
                for(index =0; index < ONVM_MAX_CHAIN_LENGTH; index++) {
                        service_id = services[sc_index][index]; //service_id = services[next_sc][index];
                        if (service_id > 0){
                                /* if(chain_len == 0){
                                        onvm_sc_set_entry(sc, 0, ONVM_NF_ACTION_TONF, service_id);
                                }
                                else */{
                                        onvm_sc_append_entry(sc, ONVM_NF_ACTION_TONF, service_id);
                                        printf(" [%d] \t ", service_id);
                                }
                                chain_len++;
                        }
                }
        }
        if(chain_len){
                printf("setup the service chain of length: %d\n ",chain_len);
                //next_sc = (next_sc+1)%max_service_chains;
        }
        else {
                printf("Didn't setup the service chain of length: %d\n ",chain_len);
        }
        if(!use_input_sc_index) {
                next_sc = (next_sc == ((uint32_t)(max_service_chains-1))?(0):(next_sc+1));
        }

        //return chain_len;
        return sc_index;
}
static int
setup_schain_and_flow_entry_for_flip_key(struct onvm_ft_ipv4_5tuple *fk_in, int sc_index);

static int
setup_schain_and_flow_entry_for_flip_key(struct onvm_ft_ipv4_5tuple *fk_in, int sc_index) {
        int ret = 0;
        if (NULL == fk_in) {
                ret = -1;
                return ret;
        }
        struct onvm_ft_ipv4_5tuple *fk = NULL;
        fk = rte_calloc("flow_key",1, sizeof(struct onvm_ft_ipv4_5tuple), 0); //RTE_CACHE_LINE_SIZE
        if (fk == NULL) {
                printf("failed in rte_calloc \n");
                return -ENOMEM;
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for flow key\n");
                exit(1);
        }
        //swap ip_addr info (Does it also need LE_to_BE_swap?? it should not)
        fk->src_addr = fk_in->dst_addr;
        fk->dst_addr = fk_in->src_addr;
        fk->src_port = fk_in->dst_port;
        fk->dst_port = fk_in->src_port;
        fk->proto    = fk_in->proto;

        printf("\nAdding Flip rule for FlowKey [%x:%u:, %x:%u, %u]", fk->src_addr, fk->src_port, fk->dst_addr, fk->dst_port, fk->proto);

        struct onvm_flow_entry *flow_entry = NULL;
        ret = onvm_flow_dir_get_key(fk, &flow_entry);

        #ifdef DEBUG_0
        printf("\n Starting to assign sc for flow_entries with src_ip [%x] \n", fk->src_addr);
        #endif

        if (ret == -ENOENT) {
                flow_entry = NULL;
                ret = onvm_flow_dir_add_key(fk, &flow_entry);

                #ifdef DEBUG_0
                printf("Adding fresh Key [%x] for flow_entry\n", fk->src_addr);
                #endif
        }
        /* Entry already exists  */
        else if (ret >= 0) {
                //rte_free(flow_entry->key);
                //rte_free(flow_entry->sc);
                //printf("Flow Entry already exits for Key [%x] for flow_entry [%x] \n", fk->src_addr, flow_entry->key->src_addr);
                  printf("Flow Entry in Table already exits at index [%d]\n\n", ret);
                return -EEXIST;
        }
        else {
                printf("\n Existing due to unknown Failure in get_key()! \n");
                return ret;
                rte_exit(EXIT_FAILURE, "onvm_flow_dir_get parameters are invalid");
                exit(2);
        }

        if(NULL == flow_entry) {
                printf("Failed flow_entry Allocations!!" ); return -ENOMEM;
        }

        memset(flow_entry, 0, sizeof(struct onvm_flow_entry));
        flow_entry->key = fk;

        #ifdef DEBUG_0
        //printf("\n Enter in sc_create()! \n");
        #endif

        flow_entry->sc = onvm_sc_create();
        if (NULL ==  flow_entry->sc) {
                rte_exit(EXIT_FAILURE, "onvm_sc_create() Failed!!");
        }

        sc_index = setup_service_chain_for_flow_entry(flow_entry->sc, sc_index,1);

        /* Setup the properties of Flow Entry */
        flow_entry->idle_timeout = 0; //OFP_FLOW_PERMANENT;
        flow_entry->hard_timeout = 0; //OFP_FLOW_PERMANENT;

        #ifdef DEBUG_0
        onvm_sc_print(flow_entry->sc);
        #endif

        return 0;

        return ret;
}

//#define DEBUG_0
static int setup_flow_rule_and_sc_entries(void);
static int
add_flow_key_to_sc_flow_table(struct onvm_ft_ipv4_5tuple *ft)
{
        int ret = 0;
        struct onvm_ft_ipv4_5tuple *fk = NULL;
        static struct onvm_flow_entry *flow_entry = NULL;

        if (NULL == ft){
                return -EINVAL;
        }

        fk = rte_calloc("flow_key",1, sizeof(struct onvm_ft_ipv4_5tuple), 0); //RTE_CACHE_LINE_SIZE
        if (fk == NULL) {
                printf("failed in rte_calloc \n");
                return -ENOMEM;
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for flow key\n");
                exit(1);
        }

        #ifdef DEBUG_0
        printf("\n Adding New Rule.\n");
        #endif

        fk->proto = ft->proto;
        fk->src_addr = rte_cpu_to_be_32(ft->src_addr); //ft->src_addr;
        fk->src_port = rte_cpu_to_be_16(ft->src_port); //ft->src_port;
        fk->dst_addr = rte_cpu_to_be_32(ft->dst_addr); //ft->dst_addr;
        fk->dst_port = rte_cpu_to_be_16(ft->dst_port); //ft->dst_port;

        printf("\nAdding [%x:%u:, %x:%u, %u]", fk->src_addr, fk->src_port, fk->dst_addr, fk->dst_port, fk->proto);

        ret = onvm_flow_dir_get_key(fk, &flow_entry);

        #ifdef DEBUG_0
        printf("\n Starting to assign sc for flow_entries with src_ip [%x] \n", fk->src_addr);
        #endif

        if (ret == -ENOENT) {
                flow_entry = NULL;
                ret = onvm_flow_dir_add_key(fk, &flow_entry);

                #ifdef DEBUG_0
                printf("Adding fresh Key [%x] for flow_entry\n", fk->src_addr);
                #endif
        }
        /* Entry already exists  */
        else if (ret >= 0) {
                //rte_free(flow_entry->key);
                //rte_free(flow_entry->sc);
                //printf("Flow Entry already exits for Key [%x] for flow_entry [%x] \n", fk->src_addr, flow_entry->key->src_addr);
                  printf("Flow Entry in Table already exits at index [%d]\n\n", ret);
                return -EEXIST;
        }
        else {
                printf("\n Existing due to unknown Failure in get_key()! \n");
                return ret;
                rte_exit(EXIT_FAILURE, "onvm_flow_dir_get parameters are invalid");
                exit(2);
        }

        if(NULL == flow_entry) {
                printf("Failed flow_entry Allocations!!" ); return -ENOMEM;
        }

        memset(flow_entry, 0, sizeof(struct onvm_flow_entry));
        flow_entry->key = fk;

        #ifdef DEBUG_0
        //printf("\n Enter in sc_create()! \n");
        #endif

        flow_entry->sc = onvm_sc_create();
        if (NULL ==  flow_entry->sc) {
                rte_exit(EXIT_FAILURE, "onvm_sc_create() Failed!!");
        }

        int sc_index = setup_service_chain_for_flow_entry(flow_entry->sc, -1,0);
        sc_index = setup_schain_and_flow_entry_for_flip_key(fk, sc_index);

        /* Setup the properties of Flow Entry */
        flow_entry->idle_timeout = 0; //OFP_FLOW_PERMANENT;
        flow_entry->hard_timeout = 0; //OFP_FLOW_PERMANENT;

        #ifdef DEBUG_0
        onvm_sc_print(flow_entry->sc);
        #endif

        return 0;
}
#if 0
static int populate_random_flow_rules_with_pkts(uint32_t max_rules) {
        int ret = 0;

//#define DEBUG_ALLOC
#ifdef DEBUG_ALLOC
        for(ret = 0; ret < 1024; ret++) {
                base_ip++;
                fk = rte_calloc("flow_key",1, sizeof(struct onvm_ft_ipv4_5tuple), 0);
                if (fk == NULL) {
                        printf("failed in rte_calloc [%u] \n", base_ip);
                        rte_exit(EXIT_FAILURE, "Cannot allocate memory for flow key\n");
                        exit(1);
                }
                sc = onvm_sc_create();
                setup_service_chain_for_flow_entry(sc);
                printf("[%d] \n",ret);
        }
#endif
#define NUM_PKTS 10
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
        
        /* Generate dummy packets and setup the keys
        struct onvm_ft_ipv4_5tuple *fk = NULL;
        struct onvm_service_chain *sc = NULL;
        struct onvm_flow_entry *flow_entry = NULL;
        struct rte_mbuf* pkts[2];
        int i;

        pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if(pktmbuf_pool == NULL) {
                rte_exit(EXIT_FAILURE, "Cannot find mbuf pool!\n");
        }
        printf("Creating %d packets to prepopulate \n", NUM_PKTS);
        for (i=0; i < 2; i++) {
                struct onvm_pkt_meta* pmeta;
                pkts[i] = rte_pktmbuf_alloc(pktmbuf_pool);
                pmeta = onvm_get_pkt_meta(pkts[i]);
                pmeta->destination = globals.destination;
                pmeta->action = ONVM_NF_ACTION_TONF;
                pkts[i]->port = 3;
                pkts[i]->hash.rss = i;
                
                setup_rule_for_packet(pkts[i]);
                
                onvm_nflib_return_pkt(pkts[i]);
        }
        //printf("%x", max_rules);
        */
        return (ret=max_rules);
}
#endif

uint32_t populate_random_flow_rules(uint32_t max_rules);
uint32_t
populate_random_flow_rules(uint32_t max_rules) {
        uint32_t ret=0;

        /* Any better logic to generate the Table Entries?? */
        uint32_t base_ip = get_ipv4_value(globals.base_ip_addr);//must be input arg
        uint32_t max_ips = globals.max_ip_addrs;                //must be input arg
        uint16_t bs_port = 5001;                                //must be input arg
        uint16_t bd_port = 57304;                               //must be input arg
        uint32_t rules_added = max_ft_entries;

        /* Can make TCP/UDP as input arg and cut down on the rules: instead
         * setup bidirectional rules and replicate mirror of NF chain (reverse order)
         */
        while(rules_added < max_rules) {
                uint32_t sip_inc = 0;
                for (; sip_inc < max_ips; sip_inc++) {
                        uint32_t dip_inc = 0;
                        for(;dip_inc < max_ips; dip_inc++ ) {
                                #ifdef DEBUG_0
                                printf("\n Adding [TCP and UDP] Rules!");
                                #endif

                                //IP_PROTOCOL_UDP (6=TCP, 17=UDP, 1=ICMP)
                                ipv4_5tRules[rules_added].proto = IP_PROTOCOL_TCP;
                                ipv4_5tRules[rules_added].src_addr = (base_ip + sip_inc);
                                ipv4_5tRules[rules_added].src_port = bs_port;
                                ipv4_5tRules[rules_added].dst_addr = (base_ip + (sip_inc+1+dip_inc)%max_ips);
                                ipv4_5tRules[rules_added].dst_port = bd_port;
                                #ifdef DEBUG_0
                                struct onvm_ft_ipv4_5tuple *fk = &ipv4_5tRules[rules_added];
                                printf("\nAdding to ipv4FT [TCP, %x:%x:, %x:%x]", fk->src_addr,
                                                fk->src_port, fk->dst_addr, fk->dst_port);
                                #endif
                                rules_added++;
                                if(rules_added >= max_rules) break;

                                //IP_PROTOCOL_UDP (6=TCP, 17=UDP, 1=ICMP)
                                ipv4_5tRules[rules_added].proto = IP_PROTOCOL_UDP;
                                ipv4_5tRules[rules_added].src_addr = (base_ip + sip_inc);
                                ipv4_5tRules[rules_added].src_port = bs_port;
                                ipv4_5tRules[rules_added].dst_addr = (base_ip + (sip_inc+1+dip_inc)%max_ips);
                                ipv4_5tRules[rules_added].dst_port = bd_port;
                                #ifdef DEBUG_0
                                fk = &ipv4_5tRules[rules_added];
                                printf("\nAdding to ipv4FT [UDP, %x:%x:, %x:%x]", fk->src_addr,
                                                fk->src_port, fk->dst_addr, fk->dst_port);
                                #endif
                                rules_added++;
                                if(rules_added >= max_rules) break;
                        }
                }
                if(rules_added >= max_rules) break;
                bs_port+=10;
                bd_port+=10;
        }
        ret = (rules_added - max_ft_entries);
        //#ifdef DEBUG_0
        printf("Total random_rules_added: %u", ret);
        //#endif
        return ret;
}

static int
setup_flow_rule_and_sc_entries(void) {

        int ret = 0;
        uint32_t random_flows = 0; //populate_random_flow_rules(MIN(globals.max_ft_rules/2,MAX_FLOW_TABLE_ENTRIES));
        max_ft_entries+= random_flows;

        /* Now add the Flow Tuples to the Global Flow Table with appropriate Service chains */
        uint32_t i = 0;
        for (;i < max_ft_entries;i++) {
                ret = add_flow_key_to_sc_flow_table(&ipv4_5tRules[i]);
        }
        printf("\n\n Populated %d flow table rules with service chains!\n", max_ft_entries);
        return ret;
}

static int setup_rule_for_packet(struct rte_mbuf *pkt, struct onvm_pkt_meta* meta) {
        int ret = 0;
        //struct onvm_pkt_meta* meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkt)->udata64);
        meta->src = 0;
        meta->chain_index = 0;
        struct onvm_flow_entry *flow_entry = NULL;

        if (pkt->hash.rss == 0) {
                //meta->action = ONVM_NF_ACTION_DROP;
                //meta->destination = 0;
                //printf("Setting to drop packet \n");

                printf("Setting to redirect on alternate port\n ");
                meta->destination = (pkt->port == 0)? (1):(0);
                meta->action = ONVM_NF_ACTION_OUT;
                return 0;
        }

        struct onvm_ft_ipv4_5tuple fk;
        uint8_t ipv4_pkt = 0;
        /* Check if it is IPv4 packet and get Ipv4 tuple Key*/
        if (onvm_ft_fill_key(&fk, pkt)) {
                // Not and Ipv4 pkt
                printf("No IP4 header found\n");
        }
        else {
                onvm_pkt_print(pkt);
                ipv4_pkt = 1;
        }

        /* Get the Flow Entry for this packet:: Must fail if there is no entry in flow_table */
        ret = onvm_flow_dir_get_pkt(pkt, &flow_entry);
        // Success case: Duplicate packet requesting entry, make it point to first index and return to make pkt proceed with already setup chain
        if (ret >= 0 && flow_entry != NULL) {
                #ifdef DEBUG_0
                printf("Exisitng_S:[%x] \n", pkt->hash.rss);
                #endif
                meta->action = flow_entry->sc->sc[meta->chain_index].action;//ONVM_NF_ACTION_NEXT;
                meta->destination = flow_entry->sc->sc[meta->chain_index].destination;  //globals.destination;
                meta->chain_index -=1; //  (meta->chain_index)--;
                #ifdef DEBUG_0
                printf("Exisitng_E:\n"); //onvm_sc_print(flow_entry->sc);
                #endif
        }
        // Expected Failure case: setup the Flow table entry with appropriate service chain
        else {
                #ifdef DEBUG_0
                printf("Setting new_S for [%x]:\n", pkt->hash.rss);
                #endif
                ret = onvm_flow_dir_add_pkt(pkt, &flow_entry);
                if (NULL == flow_entry) {
                        printf("Couldnt! get the flow entry!!");
                        return 0;
                }
                memset(flow_entry, 0, sizeof(struct onvm_flow_entry));
                flow_entry->sc = onvm_sc_create();
                int sc_index = setup_service_chain_for_flow_entry(flow_entry->sc, -1,0);

                if(ipv4_pkt) {
                        //set the same schain for flow_entry with flipped ipv4 % Tuple rule
                        sc_index = setup_schain_and_flow_entry_for_flip_key(&fk, sc_index);
                } else {
                        printf("Skipped adding Flip rule \n");
                }

                //onvm_sc_append_entry(flow_entry->sc, ONVM_NF_ACTION_TONF, globals.destination);
                #ifdef DEBUG_0
                printf("Setting new_E:\n");//onvm_sc_print(flow_entry->sc);
                #endif
                meta->action = ONVM_NF_ACTION_NEXT;//ONVM_NF_ACTION_NEXT;
                //meta->action = ONVM_NF_ACTION_DROP;
                //meta->destination = 0;
                //sleep(1);
        }
               return 0;
}

static int
packet_handler(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta) { // __attribute__((unused))
        int ret=0;
        
        //reactive rule setup: check/lookup if packet already has entry 
        printf("inside flow_rule_instaler packet handler\n");
        ret = setup_rule_for_packet(pkt, meta);

        return ret;
}

static int
clear_flow_rule_and_sc_entries(void) {

        int ret = 0;

        ret = onvm_flow_dir_clear_all_entries();
        return ret;
}
int clear_thread_start(void *pdata);
int clear_thread_start(void *pdata) {
        printf("Waiting on clear signal: enter 999 to clear and 0 to exit \n:");
        int ret = 0;
        int input = 1;
        if(pdata){};
        while(input) {
                ret = scanf("%d",&input);
                if(input == 999) {
                       ret= clear_flow_rule_and_sc_entries();
                       printf("clear flow_rule_return_Status [%d]\n",ret);
                       printf("Waiting on clear signal: enter 999 to clear and 0 to exit \n:");
                }
        }
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
        
        memset(services, -1, sizeof(int) * ONVM_MAX_CHAIN_LENGTH*MAX_SERVICE_CHAINS );

        /* get service type */
        parse_services(services);

        /* Get Flow rule entries */
        parse_ipv4_5t_rules();

        /* Setup pre-defined set of FT entries UDP protocol: Extend optargs as necessary */ 
        setup_flow_rule_and_sc_entries();

        int pid = fork();
        if (pid == 0) {
                return clear_thread_start(&pid);
        }

        if (0 == globals.destination || globals.destination == nf_info->service_id) {
                        globals.destination = nf_info->service_id + 1;
        }

        onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending");
        
        return 0;
}
