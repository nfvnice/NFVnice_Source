ifdown p2p1
ifdown p1p1
$RTE_SDK/tools/dpdk_nic_bind.py -b igb_uio 05:00.0
$RTE_SDK/tools/dpdk_nic_bind.py -b igb_uio 08:00.0

