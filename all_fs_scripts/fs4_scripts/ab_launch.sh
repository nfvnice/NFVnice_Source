#usage: ./ab_launch.sh [num_requests] [concurrent_requests] [file_size=8k, 16k, 32k, ... 10240k] [port=80 or 1111] [gnu_plot_file] [csv_file]
max_requests=$1
concurrent_requests=$2
file_size=$3
port=$4
if [ -z $1 ] ; then
max_requests=500
fi
if [ -z $2 ] ; then
concurrent_requests=100
fi
if [ -z $3 ] ; then
#file_size=32k
file_size=10240k
fi
file_name=file_${file_size}.txt
if [ -z $4 ] ; then
port=60
fi

if [ -z $5 ] ; then
gnu_log="abtest_gnu_data_${max_requests}_${concurrent_requests}_default.txt"
csv_log="abtest_csv_data_${max_requests}_${concurrent_requests}_default.csv"
out_log="abtest_std_data_${max_requests}_${concurrent_requests}_default.txt"
else
gnu_log="abtest_gnu_data_${max_requests}_${concurrent_requests}_$5.txt"
csv_log="abtest_csv_data_${max_requests}_${concurrent_requests}_$5.csv"
out_log="abtest_std_data_${max_requests}_${concurrent_requests}_$5.txt"
fi

#csv_log=$6
#if [ -z $6 ] ; then
#csv_log="abtest_csv_data_${max_requests}_${concurrent_requests}_default.csv"
#fi

echo "$gnu_log and $csv_log"
echo "********************** STARTING AB TEST ************************"
echo "max_requests = $max_requests concurrent_requests = $concurrent_requests, file_name = $file_name"

#-k == enable keep_alive on server


#ab -n $max_requests -c $concurrent_requests http://10.0.0.2:60/$file_name
ab -n $max_requests -c $concurrent_requests  -g $gnu_log -e $csv_log http://10.0.0.2:$port/file_${file_size}.txt | tee $out_log
#ab -n 500 -c 100 http://10.0.0.2:60/file_10240k.txt


echo "********************** END OF AB TEST ************************"
