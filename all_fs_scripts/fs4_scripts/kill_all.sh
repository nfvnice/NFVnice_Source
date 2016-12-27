#!/bin/bash 

#types="basic_nf bridge"

#for type in $types
#do
#num_lines=`ps aux | grep $type | grep -v "color" | wc -l`
#echo $num_lines

#for i in `seq $num_lines`
#do
#        echo "i=$i"
#        pid=`ps aux|grep $type | grep -v "color" | head -n 1 | awk '{print $2}'`
#        echo $pid
#        echo "19840115" | sudo kill -9 $pid
#done
#done

#re_n=^-?[0-9]+([.][0-9]+)?$
#re1=^[0-9]+([.][0-9]+)?$
re='^[0-9]+$'
pids=`fuser -v -m /mnt/huge | head -n 1 | awk '{print }'`
for pid in $pids
do
  if [ $pid == "kernel" ]; then
    echo "Not killing Kernel Process: $pid"
    continue
  elif ! [[ $pid =~ $re ]]; then
    echo "Not killing Unknown Process: $pid"
    continue
  else
    echo "killing Process: $pid"
    sudo kill -9 $pid
  fi
   
done

