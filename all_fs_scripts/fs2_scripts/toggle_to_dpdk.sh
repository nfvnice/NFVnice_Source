#!/bin/bash

mode=$1
if [ -z $1 ]; then
 mode=0
fi

#export RTE_SDK=/home/skulk901/dev/openNetVM/dpdk
cur_dir=`cwd`
cd $RTE_SDK/tools
DPDKT=$RTE_SDK/tools

#p1p1 = 08:00.0
#p2p1 = 05:00.0

get_dpdk_status() {
	python $DPDKT/dpdk_nic_bind.py --status
	ifconfig
}

set_to_dpdk_mode() {
	ifdown p2p1
	python $DPDKT/dpdk_nic_bind.py -b igb_uio 05:00.0
}


reset_to_dpdk_mode() {
	python $DPDKT/dpdk_nic_bind.py -u 05:00.0
	ifup p2p1
}

if [ $mode -eq 0 ]; then
	reset_to_dpdk_mode
else
	set_to_dpdk_mode
fi

get_dpdk_status

cd $cur_dir
