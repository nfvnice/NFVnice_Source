if [ -z $1 ]; 
then
  core_id=12
else 
  core_id=$1
fi

if [ -z $2 ]; 
then
  svc_id=2
else
 svc_id=$2
fi

echo "Starting Basic Monitor on core:$core_id with service id:$svc_id"

#in=$1
#./sc${in}NF_${in}.sh
#./og.sh 12 5 14
#./og.sh $1 $2 30
#./og.sh $core_id $svc_id 14     #cl=16
./og.sh $core_id $svc_id 30    #cl=32
