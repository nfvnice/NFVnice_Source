#!/bin/bash

mode=$1
if [ -z $1 ]; then
 mode=0
fi
#mode=0 => Single port connection with Flashstack-2 (P1P1=08:00.0) <---> Flashstack-3 (P1P1=08:00.0)
#mode=1 => Single port connection with Flashstack-2 (P2P1=05:00.0) <---> Flashstack-3 (P2P1=05:00.0)
#mode=2 => Single Node two port connection with Flashstack-2 (P2P1=05:00.0) <---> Flashstack-3 (P2P1=05:00.0) and Flashstack-2 (P1P1=08:00.0) <---> Flashstack-3 (P1P1=08:00.0)

#mode=3 => Two node Two ports connection   Flashstack-2 (P1P1=08:00.0) <---> Flashstack-3 (P1P1=08:00.0) and Flashstack-3(P1P2=08:00.1) <---> Flashstack-4(P1P2=08:00.1) 
#mode=4 => Two node Two ports connection   Flashstack-2 (P2P1=05:00.0) <---> Flashstack-3 (P2P1=05:00.0) and Flashstack-3(P2P2=05:00.1) <---> Flashstack-4(P2P2=05:00.1) 

#on Flashstack-3
#P1P1 = 08.00.0 ( 10.0.0.3)  -- Connected with Flashstack-2
#P1P2 = 08.00.1 ( 10.10.0.3) -- Connected with Flashstack-4
#P2P1 = 05.00.0 ( 10.0.2.3)  -- Connected with Flashstack-2
#P2P2 = 05.00.1 ( 10.10.2.3) -- Connected with Flashstack-4

#export RTE_SDK=/home/skulk901/dev/openNetVM/dpdk
cur_dir=`pwd`
cd $RTE_SDK/tools
DPDKT=$RTE_SDK/tools

get_dpdk_status() {
	python $DPDKT/dpdk_nic_bind.py --status
	ifconfig
}

bind_to_dpdk_p1p1() {
	ifdown p1p1
	python $DPDKT/dpdk_nic_bind.py -b igb_uio 08:00.0
}

bind_to_dpdk_p1p2() {
	ifdown p1p2
	python $DPDKT/dpdk_nic_bind.py -b igb_uio 08:00.1
}

bind_to_dpdk_p2p1() {
	ifdown p2p1
	python $DPDKT/dpdk_nic_bind.py -b igb_uio 05:00.0
}

bind_to_dpdk_p2p2() {
	ifdown p2p2
	python $DPDKT/dpdk_nic_bind.py -b igb_uio 05:00.1
}

unbind_p1p1() {
	python $DPDKT/dpdk_nic_bind.py -b ixgbe 08:00.0
	ifup p1p1
}

unbind_p1p2() {
	python $DPDKT/dpdk_nic_bind.py -b ixgbe 08:00.1
	ifup p1p2
}

unbind_p2p1() {
	python $DPDKT/dpdk_nic_bind.py -b ixgbe 05:00.0
	ifup p2p1
}

unbind_p2p2() {
	python $DPDKT/dpdk_nic_bind.py -b ixgbe 05:00.1
	ifup p2p2
}

reset_dpdk_mode_all() {
	unbind_p1p1
	unbind_p1p2
	unbind_p2p1
	unbind_p2p2
}

set_to_dpdk_mode0() {
	bind_to_dpdk_p1p1
}

set_to_dpdk_mode1() {
	bind_to_dpdk_p2p1
}

set_to_dpdk_mode2() {
	set_to_dpdk_mode0 
	set_to_dpdk_mode1
}

set_to_dpdk_mode3() {
	bind_to_dpdk_p1p1
	bind_to_dpdk_p1p2

}

set_to_dpdk_mode4() {
	bind_to_dpdk_p2p1
	bind_to_dpdk_p2p2
}

if [ $mode -eq "-2" ]; then
      get_dpdk_status
      exit 0
fi

echo "Resetting current DPDK NIC Configurations!"
reset_dpdk_mode_all

echo "Setting DPDK NICS to MODE: $mode"
if [ $mode -eq 0 ]; then
    set_to_dpdk_mode0
elif [ $mode -eq 1 ]; then
    set_to_dpdk_mode1
elif [ $mode -eq 2 ]; then
    set_to_dpdk_mode2
elif [ $mode -eq 3 ]; then
    set_to_dpdk_mode3
elif [ $mode -eq 4 ]; then 
    set_to_dpdk_mode4
elif [ $mode -eq "-1" ]; then 
    reset_dpdk_mode_all
else
    set_to_dpdk_mode0
fi

echo "New DPDK NIC Configuration:"
get_dpdk_status

cd $cur_dir
