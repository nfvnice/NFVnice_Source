#!/bin/bash

#Execution Mode:
#./new_launch_script.sh /home/mininet/drench incast_conf.csv list4.csv   svc_chain_dict.csv  inc_flows1.py 1 1 1
#./new_launch_script.sh /home/mininet/drench incast_conf.csv list4.csv   svc_chain_dict.csv
#DemoTopo
#./new_launch_script.sh /home/mininet/drench demotopo_conf.csv list3.csv svc_chain_dict.csv 172.23.0.33 6633
#./new_launch_script.sh /home/mininet/drench demotopo_conf.csv list3.csv svc_chain_dict.csv 192.168.56.101 6633

#4K-FatTree 
#./new_launch_script.sh /home/mininet/drench ftreetopo_conf.csv list_ftree.csv svc_chain_dict.csv 172.23.0.33 6633

#SVC_SWAP TOPO
#./new_launch_script.sh /home/mininet/drench svc_swap_topo_conf.csv list_svcswap.csv svc_chain_dict.csv 172.23.0.33 6633
#./new_launch_script.sh /home/mininet/drench svc_swap_topo_conf.csv list_svcswap.csv svc_chain_dict.csv 127.0.0.1 6633

#Arg1= Base Directory [Full Path]
#Arg2= Topology Configuration File [ relative path (only file name) assumed under base_dir/topologies/incast_conf.csv]
#Arg3= NFV ResourceFile [relative path (only file name) assumed under base_dir/nfv_rsrc_files/list4.csv]
#Arg4=SVC Chain Mapper File [relative path (only file name) assumed under base_dir/svc_chains/svc_chain_dict.csv]
#Arg5= Test Script File  [ relative path (only file name) assumed under base_dir/scripts/inc_flows1.py ]
#Arg6= SWITCH_ECN_MODE [ON=1, OFF=0]
#Arg7= CONTRL_ECN_MODE [ON=1, OFF=0]
#Arg8= CONTRL_SEL_ECN  [ON=1, OFF=0]

# Exit on any failure
#set -e

cleanup() {
    sudo mn -c
    sudo fuser -k 6633/tcp
    #sudo killall -9 python
    #sudo rm -rf /home/mininet/mptcp/results/* 
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
base_dir="/home/mininet/drench"
conf_file="demotopo_conf.csv"
rsrc_file="list4.csv"
svcch_file="svc_chain_dict.csv"
#rsrc_file=$base_dir"/nfv_rsrc_files/list4.csv"
#svcch_file=$base_dir"/svc_chains/svc_chain_dict.csv"
controller_ip="172.23.0.33"
controller_port=6633
cluster_list=""
test_file="inc_flows1.py"
ecn_sw_flag=0
ecn_ctrl_flag=0
sel_ecn_ctrl_flag=0
cur_dir=`pwd`
rootdir=$cur_dir/../$exptid

if [ $# -ge 1 ];
then
  base_dir=$1
  echo "updated base_dir: $base_dir"
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


#Launch controller
launch_controller() {
    contr_dir=$base_dir"/pox/"
    contr_launch_script="drench_controller.sh" #"rpox_def.sh"
    cur_dir = `pwd`
    #cd $contr_dir
    #sudo chmod a+x $contr_launch_script
    #ccmd="$contr_dir$contr_launch_script
    #echo "Controller sub command: $ccmd"
    #cmd="$ccmd $conf_file"
    
    cmd="./$contr_launch_script $base_dir $rsrc_file $svcch_file $ecn_ctrl_flag $sel_ecn_ctrl_flag"
    #cmd="./$contr_launch_script $base_dir"
    
    echo "*** Launching Controller: $cmd ****"
    #xterm -hold -e " cd $contr_dir && $cmd"
    #gnome-terminal --title="Controller" --window-with-profile="DUMMY" --working-directory=$contr_dir -x $cmd
    gnome-terminal --title="Controller" --working-directory=$contr_dir -x $cmd &
    cd $cur_dir
    echo "*** Launch Controller completed!: $cmd, $? ****"
}

wait_for_completion() {
    sleep 2
    a=`sudo lsof -i:6633`; while [ ! -z "$a" ]; do sleep 5; a=`sudo lsof -i:6633` ; done
}

merge_pcap_files() {
    contr_dir=$base_dir"/results/"
    out_dir=$base_dir"/dresults/"
    contr_logs=$base_dir"/pox_eel/pox/controller_logs.txt"
    #cur_dir=`pwd`
    #cd contr_dir
    output=merged.pcap
    outlog=merge.log
    #cmd="mergecap -F libpcap -w merged.pcap `find . -type f ! -empty | grep pcap` "
    cmd="mergecap -F libpcap -w $contr_dir$output `find $contr_dir -type f ! -empty -name '*.pcap'`"
    #gnome-terminal --title="Merger" --working-directory=$contr_dir -x $cmd
    $cmd
    #Move the merged file to the output location
    sudo mkdir $out_dir
    out_d=$out_dir$exptid$conf_file$test_file$ecn_sw_flag$ecn_ctrl_flag$sel_ecn_ctrl_flag
    sudo mkdir $out_d 
    outpath=$out_d/
    mv $contr_dir$output $outpath
    echo "Merge PCAP command is : $cmd" > $outpath$outlog 
    cp $contr_logs $outpath
    #cd $cur_dir
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
#( D=$(pwd); cd "$contr_dir"; xterm -e "$D/contr_launch_script" )
#( D=$(pwd); cd "$contr_dir"; xterm -hold -e "$cmd" )
launch_mininet() {
    contr_dir=$base_dir"/mn_scripts/"
    contr_launch_script="net_v2.py"
    #cur_dir = `pwd`
    #cd $contr_dir
    cmd="sudo python $contr_dir$contr_launch_script -b $base_dir -c $conf_file -s $svcch_file  -cip $controller_ip -cpt $controller_port -t $test_file -e $ecn_sw_flag"
    echo "*** Launching Mininet: $cmd ****"
    
    #xterm -hold -e " cd $contr_dir && $cmd"
    #gnome-terminal --title="Mininet"--window-with-profile="DUMMY" --working-directory=$contr_dir -x $cmd
    gnome-terminal --title="Mininet" --working-directory=$contr_dir -x $cmd &
    echo "*** Launch Mininet completed!: $cmd, $? ****"
    #pid=$!
    #wait $pid
    wait_for_completion
    #pid=`sudo fuser 6633/tcp`
    #wait $pid
    echo "*** Exit Mininet completed!: $cmd, $? ****"
}

start_work() {
    #cleanup
    cleanup

    #Launch Contoller
    launch_controller

    #Launch Mininet
    launch_mininet
    
    #process current outcome
    process_results
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
