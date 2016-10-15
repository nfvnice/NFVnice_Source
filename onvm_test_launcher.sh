#!/bin/bash

#Execution Mode:1
#./onvm_test_launcher.sh 1
#./onvm_test_launcher.sh 0

#Arg1= CLEAN AND EXIT=1 or RUN=Any other value 
#Arg2= SAME CORE OR DIFFERENT CORE FOR NFs
#Arg3= NFV ResourceFile [relative path (only file name) assumed under base_dir/nfv_rsrc_files/list4.csv]
#Arg4=SVC Chain Mapper File [relative path (only file name) assumed under base_dir/svc_chains/svc_chain_dict.csv]
#Arg5= Test Script File  [ relative path (only file name) assumed under base_dir/scripts/inc_flows1.py ]
#Arg6= SWITCH_ECN_MODE [ON=1, OFF=0]
#Arg7= CONTRL_ECN_MODE [ON=1, OFF=0]
#Arg8= CONTRL_SEL_ECN  [ON=1, OFF=0]

# Exit on any failure
#set -e

cleanup() {
    onvm_dir=$base_dir/onvm
    sudo $onvm_dir/kill_all.sh
    echo "*** cleanup completed ****"
}
ctrlc() {
    cleanup
    echo "*** Exitting!! ****"
    exit
}
trap ctrlc SIGINT

start=`date`
exptid=`date +%b%d-%H:%M`
exptid=`date +%b%d-%H-%M`
clean_make=0
base_dir="/home/skulk901"
cur_dir=`pwd`
rootdir=$cur_dir/../$exptid
if [ $# -ge 1 ];
then
  clean_make=$1
  echo "updated clean_make: $clean_make"
fi

if [ $# -ge 2 ];
then
  conf_file=$2
  echo "updated conf_file: $conf_file"
fi

if [ $# -ge 3 ];
then
  rsrc_file=$3
  echo "updated rsrc_file: $rsrc_file"
fi

if [ $# -ge 4 ];
then
  svcch_file=$4
  echo "updated svcch_file: $svcch_file"
fi

if [ $# -ge 5 ];
then
  controller_ip=$5
  echo "updated controller_ip: $controller_ip"
fi

if [ $# -ge 6 ];
then
  controller_port=$6
  echo "updated controller_port: $controller_port"
fi

if [ $# -ge 7 ];
then
  cluster_list=$7
  echo "updated cluster_list: $cluster_list"
fi

if [ $# -ge 8 ];
then
  test_file=$8
  echo "updated test_file: $test_file"
fi

if [ $# -ge 9 ];
then
  ecn_sw_flag=$9
  echo "updated ecn_sw_flag: $ecn_sw_flag"
fi

if [ $# -ge 10 ];
then
  ecn_ctrl_flag=$10
  echo "updated ecn_ctrl_flag: $ecn_ctrl_flag"
fi

if [ $# -ge 11 ];
then
  sel_ecn_ctrl_flag=$11
  echo "updated ecn_ctrl_flag: $sel_ecn_ctrl_flag"
fi

wait_for_completion() {
    sleep 2
    #a=`sudo lsof -i:6633`; while [ ! -z "$a" ]; do sleep 5; a=`sudo lsof -i:6633` ; done
    a=`sudo fuser -m /mnt/huge`; while [ ! -z "$a" ]; do sleep 5; a=`sudo fuser -m /mnt/huge` ; done
}


prepare_data_for_plot() {
    sleep 1
}

plot_graphs(){
    sleep 1
}


process_results() {
    #merge the pcap files
    #merge_pcap_files

    #prepare data for plot
    prepare_data_for_plot
    
    #plot the charts
    plot_graphs
    
}

base_dir=$(pwd)
#Launch ONVM_MGR
launch_onvm_mgr() {
    onvm_dir=$base_dir/onvm
    launch_script="launch.sh" #"rpox_def.sh"
    cur_dir = `pwd`

    cmd="./$launch_script"
    #cmd="./$contr_launch_script $base_dir $rsrc_file $svcch_file $ecn_ctrl_flag $sel_ecn_ctrl_flag"
    #cmd="./$contr_launch_script $base_dir"
    
    echo "*** Launching ONVM MANAGER: $cmd ****"
    #xterm -hold -e " cd $contr_dir && $cmd"
    #gnome-terminal --title="Controller" --window-with-profile="DUMMY" --working-directory=$contr_dir -x $cmd
    gnome-terminal --title="ONVM_MANAGER" --working-directory=$onvm_dir -x $cmd &
    cd $cur_dir
    echo "*** Launch ONVM MANAGER completed!: $cmd, $? ****"
}

max_clients=3
launch_nf_clients() {
    nf_base_dir=$base_dir"/examples"
    nf_arr=("dummy" "simple_forward" "simple_forward" "bridge")
        
    for i in `seq 1 $max_clients`; do
        #echo "\n Enter $i nf_name:"
        #read nf_name
        nf_name=${nf_arr[$i]}        
        launch_script="${i}.sh"
        echo "$nf_name $launch_script"
    
        launch_script="${max_clients}NF_${i}.sh"
        if [ $i -gt 1 ] ; then
                launch_script="s${max_clients}NF_${i}.sh"
        fi
        cur_dir=`pwd`
        nf_dir=$nf_base_dir/$nf_name        
        cd $nf_dir
        cmd="./$launch_script"
        #cmd="sudo python $contr_dir$contr_launch_script -b $base_dir -c $conf_file -s $svcch_file  -cip $controller_ip -cpt $controller_port -t $test_file -e $ecn_sw_flag"
        echo "*** Launching NF_${i}_${nf_name} with command: $cmd in dir: $nf_dir ****"
        gnome-terminal --title="NF_${i}_${nf_name}" --working-directory=$nf_dir -x $cmd &
        echo "*** Launch NF_${i}_${nf_name} completed!: $cmd, `pwd` $? ****"
        cd $cur_dir
        sleep 0.5
    done

    #pid=$!
    #wait $pid
    wait_for_completion
    #pid=`sudo fuser 6633/tcp`
    #wait $pid
    echo "*** Exit ONVM_TEST completed!: $cmd, $? ****"
}


clean_build_all() {
if  [ $clean_make -gt 1 ] ; then
        return 0
fi
CDIR=$(pwd)
ODIR=$(pwd)/onvm
        cd $ODIR
        ./kill_all.sh
        make clean 
        if  [ $clean_make -eq 1 ] ; then
            make
        fi
        cd $CDIR

NDIR=$(pwd)/examples
cd $NDIR

#for i in $(find . -maxdepth 1 -type d -exec basename {}\; ); do
CWD=$(pwd)
for i in $(find . -maxdepth 1 -type d ); do
            echo $(basename ${i})
            cd $(basename $i)
            make clean 
            #make
            if  [ $clean_make -eq 1 ] ; then
                make
            fi
            cd $CWD
        done

if  [ $clean_make -le 1 ] ; then
        cd $ODIR
        ./kill_all.sh
        exit 0
fi

#CDIR=$(pwd)/examples
#        for i in $(ls -R | grep :); do
#            DIR=${i%:}                    # Strip ':'
#            cd $DIR
#            make clean
#            make                            # Your command
#            cd $CDIR
#        done
}

start_work() {
    
    clean_build_all
    
    launch_onvm_mgr

    sleep 5

    launch_nf_clients

    #cleanup
    #cleanup

    #Launch Contoller
    #launch_controller

    #Launch Mininet
    #launch_mininet
    
    #process current outcome
    #process_results
}

end_work() {
    #cleanup
    cleanup
}

#for n in 1 2 3 4 5; do
#    dir=$rootdir/n$n
#    sudo python parkinglot.py --bw $bw \
#        --dir $dir \
#        -t 60 \
#        -n $n
#    sudo python ../util/plot_rate.py --rx \
#        --maxy $bw \
#        --xlabel 'Time (s)' \
#        --ylabel 'Rate (Mbps)' \
#        -i 's.*-eth2' \
#        -f $dir/bwm.txt \
#        -o $dir/rate.png
#    sudo python ../util/plot_tcpprobe.py \
#        -f $dir/tcp_probe.txt \
#        -o $dir/cwnd.png
#    sudo python ../util/plot_queue.py \
#    -f $dir/qlen_s1-eth1.txt \
#    -o $dir/qlen.png
#done
 
echo "Started at" $start
start_work
echo "Ended at" `date`
end_work
echo "Output saved to $rootdir"
