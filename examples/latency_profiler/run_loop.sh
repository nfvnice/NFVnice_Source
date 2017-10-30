if [ -z $1 ]; then
  num=10
else
  num=$1
fi
for i in {1..$num}; do ./build/latency_profiler ; done
