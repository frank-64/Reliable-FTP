#!/bin/bash
port=20333
srvr=127.0.0.1
out=out
mode=wt
loss_prob=0.0
client=rft_client
server=rft_server

tf=660

pkill -I $client
pkill -I $server

if [ ! -f "$client" ] || [ ! -f "$server" ]
then
     make
fi

if [ ! -d "$out" ]
then
    mkdir $out
fi

if [ $# == 1 ]
then
    loss_prob=$1
fi

if [ $# == 2 ]
then
    loss_prob=$1
    tf=$2
fi

test_file=in_${tf}_pay.txt

echo "using $loss_prob loss probability ..."

rm -rf $out/$mode
mkdir $out/$mode

rm -f out/out.txt

./$server $port &> $out/$mode/s-out.txt &
    
./$client $test_file $out/$out.txt $srvr $port $mode $loss_prob

sleep 2

diff -sq $test_file $out/$out.txt

