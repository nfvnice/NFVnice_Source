#!/bin/bash

#on Flashstack-3
#P1P1 = 08.00.0 ( 10.0.0.3)  -- Connected with Flashstack-2
#P1P2 = 08.00.1 ( 10.10.0.3) -- Connected with Flashstack-4
#P2P1 = 05.00.0 ( 10.0.2.3)  -- Connected with Flashstack-2
#P2P2 = 05.00.1 ( 10.10.2.3) -- Connected with Flashstack-4

#export RTE_SDK=/home/skulk901/dev/openNetVM/dpdk
cur_dir=`pwd`
cd $RTE_SDK/tools
DPDKT=$RTE_SDK/tools

#p1p1 = 08:00.0
#p2p1 = 05:00.0

get_dpdk_status() {
	python $DPDKT/dpdk_nic_bind.py --status
	ifconfig
}

get_dpdk_status

cd $cur_dir
