c_dir=`pwd`
echo "Launching Pkt-gen"
cd /home/skulk901/dev/openNetVM/dpdk/pktgen-dpdk 
ls
sh run-pktgen.sh
echo " Terminated Pkt-gen"
cd $c_dir
