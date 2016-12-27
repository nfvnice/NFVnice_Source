#Perform series of tests with ab and copy output files to specified folder
# usage: ./do_ab_test <file_size> <test_name> <result_output_dir> <port>
# ex. ./do_ab_test 32k bkpr_1 results 60

if [ -z $1 ] ; then
  #file_name=32k
  file_name=10240k
else 
  file_name=$1
fi

if [ -z $2 ] ; then
  test_name="default"
else
  test_name=$2
fi

result_output=$3


if [ -z $4 ] ; then
  port=60
else
  port=$4
fi

#Record terminal print of entire test
out_data=abtest_series_out_$2.txt
touch $out_data
echo "==================BEGIN ABENCH TEST SERIES========================" | tee -a $out_data
num_sessions=1
concurrent_sessions=1
echo "running test with nSession=$num_sessions and nConcurrent=$concurrent_sessions"
./ab_launch.sh $num_sessions $concurrent_sessions $file_name $port $test_name | tee -a $out_data
sleep 2

num_sessions=10
concurrent_sessions=2
echo "running test with nSession=$num_sessions and nConcurrent=$concurrent_sessions"
./ab_launch.sh $num_sessions $concurrent_sessions $file_name $port $test_name | tee -a $out_data
sleep 2

num_sessions=20
concurrent_sessions=5
echo "running test with nSession=$num_sessions and nConcurrent=$concurrent_sessions"
./ab_launch.sh $num_sessions $concurrent_sessions $file_name $port $test_name | tee -a $out_data
sleep 2

num_sessions=50
concurrent_sessions=10
echo "running test with nSession=$num_sessions and nConcurrent=$concurrent_sessions"
./ab_launch.sh $num_sessions $concurrent_sessions $file_name $port $test_name
sleep 2

num_sessions=100
concurrent_sessions=20
echo "running test with nSession=$num_sessions and nConcurrent=$concurrent_sessions"
./ab_launch.sh $num_sessions $concurrent_sessions $file_name $port $test_name | tee -a $out_data

sleep 2
num_sessions=200
concurrent_sessions=25
echo "running test with nSession=$num_sessions and nConcurrent=$concurrent_sessions"
./ab_launch.sh $num_sessions $concurrent_sessions $file_name $port $test_name | tee -a $out_data
sleep 2

echo "==================END ABENCH TEST SERIES========================"  | tee -a $out_data

if [ -z $result_output ] ; then
cp -r abtest_* results/
echo "all results are copied to folder `pwd`/results/"
else
cp -r abtest_* $result_output/
rm -r abtest_*.txt
rm -r abtest_*.csv
echo "all results are moved to folder `pwd`/$result_output/"
fi

