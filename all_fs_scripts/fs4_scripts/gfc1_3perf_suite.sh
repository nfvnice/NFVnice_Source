# $1=Test_case_name
mkdir results
mkdir results/iperf3_results/
sub_dir=`date |  awk '{ print $2"_"$3}'`
mkdir results/iperf3_results/$sub_dir
rm -rf results/iperf3_results/$sub_dir/$1
mkdir results/iperf3_results/$sub_dir/$1
#$1=test_name
if [ -z $1 ]; then
 rm -rf results/iperf3_results/$sub_dir/default
 mkdir results/iperf3_results/$sub_dir/default
 out_data=results/iperf3_results/$sub_dir/default/iperf3_default_test_log.txt
else
 out_data=results/iperf3_results/$sub_dir/$1/iperf3_${1}_test_log.txt 
fi

duration=60
num_streams=6

echo "==================BEGIN IPERF3 TEST SERIES========================" | tee -a $out_data
num_streams=6
echo "running test with streams=$num_streams" | tee -a $out_data
./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
sleep 2

num_streams=12
echo "running test with streams=$num_streams" | tee -a $out_data
./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
sleep 2

#num_streams=4
#echo "running test with streams=$num_streams" | tee -a $out_data
#./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
#sleep 2

#num_streams=8
#echo "running test with streams=$num_streams" | tee -a $out_data
#./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
#sleep 2

num_streams=18
echo "running test with streams=$num_streams" | tee -a $out_data
./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
sleep 2

#num_streams=16
#echo "running test with streams=$num_streams" | tee -a $out_data
#./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
#sleep 2

num_streams=24
echo "running test with streams=$num_streams" | tee -a $out_data
./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
sleep 2

#num_streams=32
#echo "running test with streams=$num_streams" | tee -a $out_data
#./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
#sleep 2

sleep 2
num_streams=30
echo "running test with streams=$num_streams"
./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
sleep 2

#num_streams=64
#echo "running test with streams=$num_streams"
#./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
#sleep 2

num_streams=36
echo "running test with streams=$num_streams" | tee -a $out_data
./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
sleep 2

num_streams=72
echo "running test with streams=$num_streams" | tee -a $out_data
./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
sleep 2

num_streams=120
echo "running test with streams=$num_streams" | tee -a $out_data
./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
sleep 2

#num_streams=128
#echo "running test with streams=$num_streams" | tee -a $out_data
#./3iperf_tcp_client.sh $duration $num_streams | tee -a $out_data
#sleep 2

echo "==================END IPERF3 TEST SERIES========================"  | tee -a $out_data

do_copy_results () {
	if [ -z $result_output ] ; then
	cp -r abtest_* results/
	echo "all results are copied to folder `pwd`/results/"
	else
	cp -r abtest_* $result_output/
	rm -r abtest_*.txt
	rm -r abtest_*.csv
	echo "all results are moved to folder `pwd`/$result_output/"
	fi
}
