#!/bin/bash

cpu=$1
service=$2
dst=$3
sclen=$4
print=$5

if [ -z $sclen ] 
then
  sclen=3
fi

if [ -z $dst ]
then
        echo "$0 [cpu-list] [Service ID] [DST] [SCHAIN_LEN_LIMIT] [PRINT]"
        echo "$0 3,7,9 1 2 --> cores 3,7, and 9, with Service ID 1, and forwards to service ID 2 and default chain_len_limit=3"
        echo "$0 3,7,9 1 2 4 1000 --> cores 3,7, and 9, with Service ID 1 forwards to service ID 2 and chain_len_limit=4 Print Rate of 1000"
        exit 1
fi

if [ -z $print ]
then
        sudo ./build/flow_rule_deleter -l $cpu -n 3 --proc-type=secondary -- -r $service -- -d $dst
else
        sudo ./build/flow_rule_deleter -l $cpu -n 3 --proc-type=secondary -- -r $service -- -d $dst  -s $sclen -p $print
fi
