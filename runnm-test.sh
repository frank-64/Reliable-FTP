#!/bin/bash
port=20333
srvr=127.0.0.1
out=out
mode=nm
client=rft_client
server=rft_server

if [ ! -f "$client" ] || [ ! -f "$server" ]
then
     make
fi


if [ ! -d "$out" ]; then
    mkdir $out
fi

rm -rf $out/$mode
mkdir $out/$mode

declare -a testf=(0 1 17 34 35 36 350 660)

echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
"
echo "Running tests ..."
echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
"
    
for tf in ${testf[@]} 
do
    pkill -I $client
    pkill -I $server

    rm -f out/out.txt
    ./$server $port &> $out/$mode/s$tf-out.txt &
    
    ./$client in_${tf}_pay.txt $out/out.txt $srvr $port $mode  &> $out/$mode/c$tf-out.txt
    
    diff -sq in_${tf}_pay.txt $out/$out.txt
    
    sleep 3
done

echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
"

rm -f $out/out.txt

for tf in ${testf[@]} 
do
    cat $out/$mode/c$tf-out.txt
    echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
    echo "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
    cat $out/$mode/s$tf-out.txt
    echo "*******************************************************************************"
    echo "*******************************************************************************"
done
