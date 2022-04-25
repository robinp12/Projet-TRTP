# Note that this assumes to be called from the Makefile, you may want to adapt it.
echo "Test with simlink"
./tests/test.sh tests/small.txt 5 5 5 5 5
# Run the same test, but this time with valgrind
echo "A very simple test, with Valgrind"
VALGRIND=1 ./tests/test.sh tests/small.txt 10 10 10 10 10