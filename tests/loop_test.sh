for i in {1..9}
do
    ./tests/test.sh tests/small.txt $(($i*10)) 0 0 0 0
done

for i in {1..9}
do
    ./tests/test.sh tests/small.txt 10 $(($i*10)) 0 0 0
done

for i in {1..9}
do
    ./tests/test.sh tests/small.txt 0 0 $(($i*10)) 0 0
done

for i in {1..9}
do
    ./tests/test.sh tests/small.txt 0 0 0 $(($i*10)) 0
done

for i in {1..9}
do
    ./tests/test.sh tests/small.txt 0 0 0 0 $(($i*10)) 
done