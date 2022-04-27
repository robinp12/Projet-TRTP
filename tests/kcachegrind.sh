#!/bin/bash

# Pour générer un callgraph

# cleanup d'un test précédent
rm -f received_file

input_file="./tests/ressources/image.jpg"
test_name="valgrind"

echo "=== Valgrind with file ${input_file} ==="

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

# get size in bytes
mysize=$(find "$input_file" -printf "%s")
# get extension 
extension="${input_file##*.}"

delay=50
jitter=5
error=3
cut=2
loss=10

# On lance le simulateur de lien avec 10% de pertes et un délais de 50ms
echo "link_sim démarré"
./link_sim -p 2000 -P 2456 -l $loss -d $delay -j $jitter -e $error -c $cut -R 2> $linksim_log &
link_pid=$!

# On lance le receiver et capture sa sortie standard
echo "Receiver démarré"
valgrind --tool=callgrind ./receiver -s $receiver_csv :: 2456 1>received_file 2>$receiver_log &
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

valgrind --tool=callgrind ./sender -f $input_file -s $sender_csv ::1 2000 2>$sender_log


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

echo ""
echo ""
echo "VALGRIND SENDER"
tail $sender_log
echo ""
echo ""
echo "VALGRIND RECEIVER"
tail $receiver_log
echo ""
echo ""

