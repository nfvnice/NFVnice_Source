#!/bin/bash

function start_nf {
        while true
        do
	echo "sudo examples/$1/build/$1 -l $2 -n 3 --proc-type=secondary -- -r $3"
	echo "19840115" |  sudo -S examples/$1/build/$1 -l $2 -n 3 --proc-type=secondary -- -r $3 > tmp.txt &
        nf_pid=$!
        echo $nf_pid
        sleep 0.02
        if ps -p $nf_pid > /dev/null
        then
                break
        fi
        done
}

num_nfs=$1
cpu=$2

if [ -z $cpu ]
then
        echo "usage: $0 [num_nfs][cpu]"
        exit 1
fi

echo "num_nfs=$num_nfs"
nf=1
while [ $nf -le $num_nfs ]
do
        #start_nf bridge $cpu 1
        start_nf bridge $cpu 2

        nf=$((nf+1))
done


