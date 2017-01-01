#!/bin/bash

#Default Path as set in $RTE_SDK
if [  [ -n $RTE_SDK]  -a [ -n $ONVMD ] ]; then
  echo $ONVMD
  ONVMT=$ONVMD/onvm/onvm_mgr
elif [ ! -z $RTE_SDK ]; then
  ONVMT=$RTE_SDK/../onvm/onvm_mgr
else
  ONVMT="."
fi

#Default to use ./go.sh 0,1,2,3,4,5,6 3 10
if [ ! -z $1 ]; then
  cpu=$1
else
  cpu="0,1,2,3,4,5,6"
fi

if [ ! -z $2 ]; then
  ports=$2
else
  ports=3
fi

if [ ! -z $3 ]; then
  num_clients=$3
else
  num_clients=10
fi

#cpu=$1
#ports=$2
#num_clients=$3

if [ -z $ports ]
then
        echo "$0 [cpu-list] [port-bitmask]"
        # this works well on our 2x6-core nodes
        echo "$0 0,1,2,6 3 1 --> cores 0, 1, 2 and 6 with ports 0 and 1, num_clients 1"
        echo "Cores will be used as follows in numerical order:"
        echo "  RX thread, ..., TX thread, ..., WAKEUP thread, stat thread"
        exit 1
fi

sudo rm -rf /mnt/huge/rtemap_*
sudo $ONVMT/onvm_mgr/$RTE_TARGET/onvm_mgr -l $cpu -n 4 --proc-type=primary  -- -p${ports} -n${num_clients}
#sudo ./onvm_mgr/onvm_mgr/$RTE_TARGET/onvm_mgr -l $cpu -n 4 --proc-type=primary  -- -p${ports} -n${num_clients}
