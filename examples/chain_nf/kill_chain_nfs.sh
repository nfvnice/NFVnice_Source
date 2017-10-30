!/bin/bash 

types="basic_nf monitor onvm"

for type in $types
do
num_lines=`pgrep -a $type | awk '{print $1}' | wc -l`
echo "Type: $type has: $num_lines"
pids=`pgrep -a $type | awk '{print $1}'`
echo "pids of $type: $pids"
for pid in $pids 
do
        echo "Killing PID ( $type): $pid"
        sudo kill -9 $pid
done
done

