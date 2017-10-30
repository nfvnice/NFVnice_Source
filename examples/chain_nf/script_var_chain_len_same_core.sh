chain_len=$1
cost=$2

if [ -z $chain_len ]
then
        chain_len=1
fi
echo "chain_len is set to $chain_len"

if [ -z $cost ]
then
        cost=0
fi
echo "cost is set to $cost"

#inst_id_list=(1 2 5 9 13 3 6 10 14 4 7)
inst_id_list=(1 2 8 15 22 29 4 10 17 24 31)
#cost_id_list=(0 1 2 3 1 1 1 1 1 1 1 1)
#cost_id_list=(0 1 1 1 1 1 2 1 1 3 1 1)
#cost_id_list=(0 1 1 1 2 2 2 3 3 3 1 1)
#cost_id_list=(0 2 1 1 1 2 3 1 2 3 1)
#cost_id_list=(0 1 1 1 1 1 1 1 1 1 1)
cost_id_list=(0 1 1 1 1 2 3 1 1 1 1)
#cost_id_list=(0 2 1 3 2 1 3 2 1 3 2)
core_id=8
svc_id=2
dst_id=3
inst_id=1
nfs_per_core=3
nf=1
bm_svc_id=$((chain_len+2))
while [ $nf -le $chain_len ]
do

        inst_id=${inst_id_list[$nf]}
        cost=${cost_id_list[$nf]}
        echo "Starting ./og.sh on core: $core_id svcid: $svc_id dstid: $dst_id instid: $inst_id cost: $cost"
        ./og.sh $core_id $svc_id $dst_id $inst_id $cost &        
        sleep 4
        nf=$((nf+1))
        #inst_id=${inst_id_list[$nf]}
	#inst_id=$((inst_id+1))
        svc_id=$((svc_id+1))
        dst_id=$((dst_id+1))
#        core_id=$((core_id+1))
#        if [ $core_id -ge 11 ]
#        then
#                core_id=8
#        fi
#        if [ $inst_id -eq 4 ] || [ $inst_id -eq 8 ]
#        then
#                inst_id=$((inst_id+1))
#        fi
done
chain_len=$((chain_len+1))
cd ../basic_monitor
echo "Starting Basic Monitor with Service Id: $bm_svc_id, $chain_len, $nf"
./lsc_x.sh $bm_svc_id
sleep 3
echo "pwd=$PWD"
cd ../../onvm                                                                                                                       
echo "pwd=$PWD"
echo "settin BATCH Scheduler"                                                                                                       
./set_sched_type.sh b 
