# Note that this assumes to be called from the Makefile, you may want to adapt it.
# To add a test, create a directory with the name of the test (to store the log and stats files)
# then run test.sh 
total=0
failed=0
log=$1


echo ""
echo "==== Suite de tests, avec et sans linksim ==="


if [ $1 ]
then
echo "             (logs désactivés)"
else
echo "              (logs activés)"
fi

echo ""
echo "[basic] Fichier text avec simlink"
./tests/test.sh tests/ressources/test.txt 0 0 0 0 0 basic $log
failed=$(($failed + $?))
((total++))

echo "[smallSimlink] Fichier text avec simlink"
./tests/test.sh tests/ressources/small.txt 10 5 5 5 5 smallSimlink $log
failed=$(($failed + $?))
((total++))

echo "[random] Fichier text aléatoire sur réseau parfait"
# Fichier au contenu aléatoire de 1MO
dd if=/dev/urandom of=./tests/ressources/random.txt bs=1 count=1000000 &> /dev/null
./tests/test.sh tests/ressources/random.txt 0 0 0 0 0 random $log
failed=$(($failed + $?))
((total++))
rm ./tests/ressources/random.txt

echo "[smallImage] Petit fichier jpg avec simlink"
./tests/test.sh tests/ressources/smallImage.jpg 50 5 5 2 10 smallImage $log
failed=$(($failed + $?))
((total++))

echo "[image] Fichier jpg sur réseau parfait"
./tests/test.sh tests/ressources/image.jpg 0 0 0 0 0 image $log
failed=$(($failed + $?))
((total++))

echo "[imageSimlink] Fichier jpg avec simlink"
./tests/test.sh tests/ressources/image.jpg 10 2 2 2 3 imageSimlink $log
failed=$(($failed + $?))
((total++))

echo "[pdf] Fichier pdf sur réseau parfait"
./tests/test.sh tests/ressources/report_template_a3.pdf 0 0 0 0 0 pdf $log
failed=$(($failed + $?))
((total++))

echo "[pdfSimlink] Fichier pdf avec simlink"
./tests/test.sh tests/ressources/report_template_a3.pdf 10 3 3 3 10 pdfSimlink $log
failed=$(($failed + $?))
((total++))

echo ""
echo "==== ${failed} transfert(s) raté(s) sur ${total} tests ===="
echo ""
