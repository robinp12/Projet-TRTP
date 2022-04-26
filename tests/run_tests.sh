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
echo "Basic test with simlink inactive and text file"
./tests/test.sh tests/test.txt 0 0 0 0 0 basic $log
failed=$(($failed + $?))
((total++))

echo "Test with simlink and text file"
./tests/test.sh tests/small.txt 10 5 5 5 5 smallSimlink $log
failed=$(($failed + $?))
((total++))

echo "Test with random text file on perfect network"
./tests/test.sh tests/random.txt 0 0 0 0 0 random $log
failed=$(($failed + $?))
((total++))

echo "Test with small jpg file on bad network"
./tests/test.sh tests/smallImage.jpg 50 5 5 2 10 smallImage $log
failed=$(($failed + $?))
((total++))

echo "Test with jpg file on perfect network"
./tests/test.sh tests/image.jpg 0 0 0 0 0 image $log
failed=$(($failed + $?))
((total++))

echo "Test with simlink and jpg file"
./tests/test.sh tests/image.jpg 10 2 2 2 3 imageSimlink $log
failed=$(($failed + $?))
((total++))

echo "Test with pdf file on perfect network"
./tests/test.sh tests/report_template_a3.pdf 0 0 0 0 0 pdf $log
failed=$(($failed + $?))
((total++))

echo "Test with simlink and pdf file"
./tests/test.sh tests/report_template_a3.pdf 10 3 3 3 10 pdfSimlink $log
failed=$(($failed + $?))
((total++))

echo "==== ${failed} transfert(s) raté(s) sur ${total} tests ===="
echo ""
