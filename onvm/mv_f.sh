#usage: ./mv_f POLL 1NF or ./mv_f YLD 3NF
mv pidstat2_log_${1}.csv run_1/$1/pidstat2_log_${1}_${2}.csv
mv pidstat_log_${1}.csv run_1/$1/pidstat_log_${1}_${2}.csv
mv onvm_stat_${2}_${1}.txt run_1/$1/onvm_stat_log_${1}_${2}.csv
