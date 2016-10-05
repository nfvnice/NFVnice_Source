#!/bin/bash

cpu=$1
service=$2
dst=$3
inst_id=$4
print=$5

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
if [ -z $print ]
then
        sudo ./build/vlantrunc -l $cpu -n 3 --proc-type=secondary -- -r $service -n $inst_id -- -d $dst
else
        sudo ./build/vlantrunc -l $cpu -n 3 --proc-type=secondary -- -r $service -n $inst_id -- -d $dst -p $print
fi
