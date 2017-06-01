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
inst_id_list=(1 2 5 9 13 3 6 10 14 4 7)
#cost_id_list=(0 1 2 3 1 1 1 1 1 1 1 1)
#cost_id_list=(0 1 1 1 1 1 2 1 1 3 1 1)
cost_id_list=(0 1 1 1 1 1 1 1 1 1 1)
#cost_id_list=(0 2 1 1 1 2 3 1 2 3 1)

core_id_list=(0 8 8 8 9 9 9 10 10 10 11 11)
core_id=8
svc_id=2
dst_id=3
inst_id=1
nfs_per_core=3
nf=1
while [ $nf -le $chain_len ]
do
        inst_id=${inst_id_list[$nf]}
        cost=${cost_id_list[$nf]}
        core_id=${core_id_list[$nf]}
	echo "Starting ./og.sh $core_id $svc_id $dst_id $inst_id $cost"
        ./og.sh $core_id $svc_id $dst_id $inst_id $cost &       
        sleep 4
        nf=$((nf+1))
        inst_id=$((inst_id+1))
        svc_id=$((svc_id+1))
        dst_id=$((dst_id+1))
        core_id=$((core_id+1))
        if [ $core_id -ge 11 ]
        then
                core_id=8
        fi
        if [ $inst_id -eq 4 ] || [ $inst_id -eq 8 ]
        then
                inst_id=$((inst_id+1))
        fi
done
cd ../basic_monitor
echo "Starting Basic Monitor at: $nf"
./lsc_x.sh $nf

#./og.sh 8 2 3 1 $1 $2
#./og.sh 8 3 4 2 $1 $2
#./og.sh 8 4 5 3 $1 $2
#./og.sh 8 5 6 5 $1 $2
#./og.sh 8 6 7 6 $1 $2
#./og.sh 8 7 8 7 $1 $2
#./og.sh 8 8 9 9 $1 $2
#./og.sh 8 9 10 10 $1 $2
#./og.sh 8 10 11 11 $1 $2


#ls -l a9* | awk '{ print "./"$9 "\ &"}'  > run_9schain.sh
#./a9NF_1.sh 1 &
#  sleep 2
#./a9NF_2.sh 2 &
#  sleep 2
#./a9NF_3.sh 3 &
#  sleep 2
#./a9NF_4.sh 1 &
#  sleep 2
#./a9NF_5.sh 2 &
#  sleep 2
#./a9NF_6.sh 3 &
#  sleep 2
#./a9NF_7.sh 1 &
#  sleep 2
#./a9NF_8.sh 2 &
#  sleep 2
#./a9NF_9.sh 3 &
#  sleep 2
