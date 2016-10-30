#ls -l a9* | awk '{ print "./"$9 "\ &"}'  > run_9schain.sh
./a9NF_1.sh 1 &
  sleep 2
./a9NF_2.sh 2 &
  sleep 2
./a9NF_3.sh 3 &
  sleep 2
./a9NF_4.sh 1 &
  sleep 2
./a9NF_5.sh 2 &
  sleep 2
./a9NF_6.sh 3 &
  sleep 2
./a9NF_7.sh 1 &
  sleep 2
./a9NF_8.sh 2 &
  sleep 2
./a9NF_9.sh 3 &
  sleep 2
