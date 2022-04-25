# My Wonderful LINFO1341 Project

### Compiler le programme (aussi DEBUG) :
> make (debug)

### Lancer le programme : 

> ./receiver :: 1234 

> ./sender -f *input_file* :: 1234 

### Lancer les tests (automatique ou avec paramètres symlink): 

> make tests 

> ./tests/test.sh *input_file* *delay* *jitter* *error_rate* *cut* *loss* 

### Nettoyer les fichiers binaire : 

> make .PHONY 

## ``` Notes ```

The Makefile contains all the required targets, but you might want to extend their behavior.

Very basic skelettons of receiver and sender source files are present, have a look to understand how you can enable logging or not.

A very simple test case is present, you probably want to update it.

You might be interested in the link simulator that can be found at https://github.com/cnp3/Linksimulator

And finally, if this message is still there at your final submission, it looks like you forgot to provide a proper README.