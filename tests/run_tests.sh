# Note that this assumes to be called from the Makefile, you may want to adapt it.
# To add a test, create a directory with the name of the test (to store the log and stats files)
# then run test.sh 
total=0
failed=0

echo "Basic test with simlink inactive and text file"
./tests/test.sh tests/test.txt 0 0 0 0 0 basic
failed=$(($failed + $?))
((total++))

echo "Test with simlink and text file"
./tests/test.sh tests/small.txt 10 5 5 5 5 smallSimlink
failed=$(($failed + $?))
((total++))

echo "Test with random text file"
./tests/test.sh tests/random.txt 10 5 5 5 5 random
failed=$(($failed + $?))
((total++))

echo "Test with small jpg file on bad network"
./tests/test.sh tests/smallImage.jpg 50 5 5 2 10 smallImage
failed=$(($failed + $?))
((total++))

echo "Test with jpg file on perfect network"
./tests/test.sh tests/image.jpg 0 0 0 0 0 image
failed=$(($failed + $?))
((total++))

echo "Test with simlink and jpg file"
./tests/test.sh tests/image.jpg 10 2 2 2 3 imageSimlink
failed=$(($failed + $?))
((total++))

echo "Test with pdf file on perfect network"
./tests/test.sh tests/report_template_a3.pdf 0 0 0 0 0 pdf
failed=$(($failed + $?))
((total++))

echo "Test with simlink and pdf file"
./tests/test.sh tests/report_template_a3.pdf 10 3 3 3 10 pdfSimlink
failed=$(($failed + $?))
((total++))

echo "==== ${failed} transfert(s) raté(s) sur ${total} tests ===="
echo ""
