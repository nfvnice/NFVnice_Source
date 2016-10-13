#!/bin/bash

cpu=$1
service=$2
dst=$3
inst_id=$4
cost=$5
print=$6

if [ -z $dst ]
then
        echo "$0 [cpu-list] [Service ID] [DST] [PRINT]"
        echo "$0 3,7,9 1 2 --> cores 3,7, and 9, with Service ID 1, and forwards to service ID 2"
        echo "$0 3,7,9 1 2 1000 --> cores 3,7, and 9, with Service ID 1, forwards to service ID 2,  and Print Rate of 1000"
        exit 1
fi

if [ -z $inst_id ]
then
inst_id=7
fi


if [ -z $cost ]
then
        cost=0
fi
echo "cost is set to $cost"

if [ -z $print ]
then
        sudo ./build/basic_nf -l $cpu -n 3 --proc-type=secondary -- -r $service -n $inst_id -- -d $dst -c $cost
else
        sudo ./build/basic_nf -l $cpu -n 3 --proc-type=secondary -- -r $service -n $inst_id -- -d $dst -c $cost -p $print
fi
