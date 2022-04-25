#!/bin/bash

# cleanup d'un test précédent
rm -f received_file input_file

input_file=$1

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
./link_sim -p 2000 -P 2456 -l $loss -d $delay -j $jitter -e $error -c $cut -R &
link_pid=$!

# On lance le receiver et capture sa sortie standard
echo "receiver démarré"
./receiver :: 2456 1>received_file &
receiver_pid=$!

cleanup()
{
    kill -9 $receiver_pid
    kill -9 $link_pid
    exit 0
}
trap cleanup SIGINT  # Kill les process en arrière plan en cas de ^-C

# On démarre le transfert
echo "sender démarré"


if ! time_sender=$(/usr/bin/time --format="%e" ./sender -f $input_file ::1 2000 2>&1|tail -n 1) ; then
  echo "Crash du sender!"
  cat sender.log
  err=1  # On enregistre l'erreur
fi

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
  echo "Le transfert a corrompu le fichier!"
  echo "Diff binaire des deux fichiers: (attendu vs produit)"
  diff -C 9 <(od -Ax -t x1z input_file) <(od -Ax -t x1z received_file)
  corrompu=1
else
  corrompu=0
  echo "Le transfert est réussi!"
fi

if [ -s time_to_sender.csv ]
then
  echo "$extension ,$mysize, $time_sender ,$loss ,$delay ,$jitter ,$error ,$cut, $corrompu," >> time_to_sender.csv
else
  echo "extension ,size ,time_sender ,loss ,delay ,jitter ,error ,cut ,pkt_corrupted," >> time_to_sender.csv
  echo "$extension ,$mysize, $time_sender ,$loss ,$delay ,$jitter ,$error ,$cut, $corrompu," >> time_to_sender.csv
fi

exit 1
