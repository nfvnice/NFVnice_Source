chain_len=$1
cost=$2

mypwd=`pwd`
echo "mypwd=$mypwd"
cd ../../onvm
onvm_dir=`pwd`
onvm_cmd="./launch.sh"
#gnome-terminal --title="ONVM_MANAGER" --working-directory=$onvm_dir -x $onvm_cmd --window-with-profile="small_font" &
xterm -e $onvm_cmd  &
cd $mypwd
sleep 5
cd ../flow_rule_installer
rm gfc_out.txt
#./gfc1.sh &
./gfc1.sh 2>&1 >> gfc_out.txt &
sleep 5
pid=`pgrep -a flow_rule_inst | awk '{print $1}'`
sudo kill -9 $pid

cd $mypwd

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

#inst_id_list=(1 2 5 9 13 3 6 10 14 4 7)        #cl=16
inst_id_list=(1 2 8 15 22 29 4 10 17 24 31)   #cl=32
#cost_id_list=(0 1 2 3 1 1 1 1 1 1 1 1)
#cost_id_list=(0 1 1 1 1 1 2 1 1 3 1 1)
#cost_id_list=(0 1 1 1 2 2 2 3 3 3 1 1)
#cost_id_list=(0 2 1 1 1 2 3 1 2 3 1)

#cost_id_list=(0 1 1 1 1 1 1 1 1 1 1)
#cost_id_list=(0 2 1 3 1 3 2 3 3 2 1)
cost_id_list=(0 0 2 3 2 3 0 3 0 2 1)

#cost_id_list=(0 1 1 1 1 2 3 1 1 1 1)
#cost_id_list=(0 2 1 3 2 1 3 2 1 3 2)
core_id=8
svc_id=2
dst_id=3
inst_id=1
nfs_per_core=3
nf=1
rm chain_nfs_out.txt
while [ $nf -lt $chain_len ]
#while [ $nf -le $chain_len ]
do
        inst_id=${inst_id_list[$nf]}
        cost=${cost_id_list[$nf]}
        echo "Starting ./og.sh on core: $core_id svcid: $svc_id dstid: $dst_id instid: $inst_id cost: $cost"
        ./og.sh $core_id $svc_id $dst_id $inst_id $cost 2>&1 >> chain_nfs_out.txt &        
        sleep 4
        nf=$((nf+1))
        svc_id=$((svc_id+1))
        dst_id=$((dst_id+1))
done
if [ $chain_len -eq 1 ]
then
   bm_svc_id=2
   core_id=8
else 
   bm_svc_id=$((chain_len+1))
   core_id=8
fi
cd ../basic_monitor
rm monitor_out.txt
echo "Starting Basic Monitor on core: $core_id Service Id: $bm_svc_id, $chain_len, $nf"
./lsc_x.sh $core_id $bm_svc_id 2>&1 >> monitor_out.txt &
sleep 3
echo "pwd=$PWD"
cd ../../onvm                                                                                                                       
echo "pwd=$PWD"
echo "settin BATCH Scheduler"                                                                                                       
./only_pidstat.sh > tmp_res/same_core_${chain_len}_pidstat.txt 2>&1
#./set_sched_type.sh b
sleep 15
read inp
#sleep 30
cd $mypwd
#cd ../examples/chain_nf
./kill_chain_nfs.sh

