#!/bin/bash

cpu=$1
service=$2
dst=$3
cost=$4
print=$5

if [ -z $service ]
then
        echo "$0 [cpu-list] [Service ID] [DST] [COST] [PRINT]"
        echo "$0 3 0 --> core 3, Service ID 0"
        echo "$0 3,7,9 1 2 1 --> cores 3,7, and 9 with Service ID 1, Destination SvcId 2 and comput_cost_level 1 [0,1=Min,2=Med,3=Max]!"
        echo "$0 3,7,9 1 2 2 1000 --> cores 3,7, and 9 with Service ID 1, Destination SvcId 2, comput_cost_level 2 [0,1=Min,2=Med,3=Max] and Print Rate of 1000"
        exit 1
fi

if [ -z $dst ]
then
        dst=0
fi

if [ -z $cost ]
then
        cost=0
fi
echo "cost is set to $cost"

if [ -z $print ]
then
        sudo ./build/basic_nf -l $cpu -n 3 --proc-type=secondary -- -r $service -- -d $dst -c $cost
else
        sudo ./build/basic_nf -l $cpu -n 3 --proc-type=secondary -- -r $service -- -d $dst -c $cost -p $print
fi
