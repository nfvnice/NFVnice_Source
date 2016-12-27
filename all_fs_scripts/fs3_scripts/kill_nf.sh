#!/bin/bash 

types="basic_nf bridge"

for type in $types
do
num_lines=`ps aux | grep $type | grep -v "color" | wc -l`
echo $num_lines

for i in `seq $num_lines`
do
        echo "i=$i"
        pid=`ps aux|grep $type | grep -v "color" | head -n 1 | awk '{print $2}'`
        echo $pid
        echo "19840115" | sudo kill -9 $pid
done
done
umount -f /mnt/huge
