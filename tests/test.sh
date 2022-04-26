#!/bin/bash

# cleanup d'un test précédent
rm -f received_file

input_file=$1
test_name=$7


echo "=== Test with file ${input_file} ==="

filesize=$(ls -lh $input_file | awk '{print  $5}')
echo "Size of the file : ${filesize}"

receiver_log="./tests/${test_name}/receiver.log"
receiver_csv="./tests/${test_name}/receiver.csv"

sender_log="./tests/${test_name}/sender.log"
sender_csv="./tests/${test_name}/sender.csv"

linksim_log="./tests/${test_name}/linksim.log"

rm -f receiver_csv
rm -f sender_csv
rm -f receiver_log
rm -f sender_log
rm -f linksim_log

if [ $8 ]
then
linksim_log="/dev/null"
fi


# get size in bytes
mysize=$(find "$input_file" -printf "%s")
# get extension 
extension="${input_file##*.}"

delay=$2
jitter=$3
error=$4
cut=$5
loss=$6

# On lance le simulateur de lien avec 10% de pertes et un délais de 50ms
echo "link_sim démarré"

./link_sim -p 2000 -P 2456 -l $loss -d $delay -j $jitter -e $error -c $cut -R 2> $linksim_log &

link_pid=$!

# On lance le receiver et capture sa sortie standard
echo "Receiver démarré"
./receiver -s $receiver_csv :: 2456 1>received_file 2>$receiver_log &
receiver_pid=$!

cleanup()
{
    kill -9 $receiver_pid
    kill -9 $link_pid
    exit 1
}
trap cleanup SIGINT  # Kill les process en arrière plan en cas de ^-C

# On démarre le transfert

echo "Sender démarré"

# La durée de transmission est ajoutée à la fin des stats du sender (pas moyen de les séparer sans perdre les stats)
/usr/bin/time --format="%e" ./sender -f $input_file -s $sender_csv ::1 2000 2>$sender_log

if ! time_sender=$(tail $sender_log -n 1) ; then
  echo "Crash du sender!"
  err=1  # On enregistre l'erreur
fi

echo "Execution time of the sender : ${time_sender}"
echo "En attente que le receiver finisse"
sleep 5 # On attend 5 seconde que le receiver finisse


if kill -0 $receiver_pid &> /dev/null ; then
  echo "Le receiver ne s'est pas arreté à la fin du transfert!"
  kill -9 $receiver_pid
  err=1
else  # On teste la valeur de retour du receiver
  if ! wait $receiver_pid ; then
    echo "Crash du receiver!"
    cat receiver.log
    err=1
  fi
fi

# On arrête le simulateur de lien
kill -9 $link_pid &> /dev/null

sleep 1

# On vérifie que le transfert s'est bien déroulé
if [[ "$(sha256sum $input_file | awk '{print $1}')" != "$(sha256sum received_file | awk '{print $1}')" ]]; then
  echo " --> Le transfert a corrompu le fichier!"
  #echo "Diff binaire des deux fichiers: (attendu vs produit)"
  #diff -C 9 <(od -Ax -t x1z input_file) <(od -Ax -t x1z received_file)
  corrompu=1
else
  corrompu=0
  echo " --> Le transfert est réussi!"
fi

if [ -s time_to_sender.csv ]
then
  echo "$extension ,$mysize, $time_sender ,$loss ,$delay ,$jitter ,$error ,$cut, $corrompu," >> time_to_sender.csv
else
  echo "extension ,size ,time_sender ,loss ,delay ,jitter ,error ,cut ,pkt_corrupted," >> time_to_sender.csv
  echo "$extension ,$mysize, $time_sender ,$loss ,$delay ,$jitter ,$error ,$cut, $corrompu," >> time_to_sender.csv
fi


echo ""
exit $corrompu

