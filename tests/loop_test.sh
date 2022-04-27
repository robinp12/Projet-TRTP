

# Tests pour observer l'impact des différents paramètres du linksim sur le transfert

# Reference
./tests/test.sh tests/ressources/test.txt 0 0 0 0 0 loop -nolog

# Delai
for i in {1..10}
do
    ./tests/test.sh tests/ressources/test.txt $(($i*5)) 0 0 0 0 loop -nolog
done

# Delay + jitter
for i in {1..10}
do
    ./tests/test.sh tests/ressources/test.txt 50 $(($i*2)) 0 0 0 loop -nolog
done

# Error rate
for i in {1..10}
do
    ./tests/test.sh tests/ressources/test.txt 0 0 $(($i*5)) 0 0 loop -nolog
done

# Cut
for i in {1..10}
do
    ./tests/test.sh tests/ressources/test.txt 0 0 0 $(($i*5)) 0 loop -nolog
done

# Loss
for i in {1..10}
do
    ./tests/test.sh tests/ressources/test.txt 0 0 0 0 $(($i*5)) loop -nolog
done