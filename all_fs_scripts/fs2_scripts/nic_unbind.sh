python $RTE_SDK/tools/dpdk_nic_bind.py -u 05:00.0
python $RTE_SDK/tools/dpdk_nic_bind.py -u 08:00.0
ifup p2p1
ifup p1p1
