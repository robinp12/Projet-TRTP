# My Wonderful LINFO1341 Project


Pour lancer le receiver :
``` ./receiver ::1 2000 ```

Pour lancer le sender :
``` ./sender -f README.md ::1 2000 ```


### Notes
Compile sender :
``` gcc sender.c read-write/*.c packet_implem.c -o sender -lz ```

Compile receiver :
``` gcc receiver.c read-write/*.c packet_implem.c -o receiver -lz ```

The Makefile contains all the required targets, but you might want to extend their behavior.

Very basic skelettons of receiver and sender source files are present, have a look to understand how you can enable logging or not.

A very simple test case is present, you probably want to update it.

You might be interested in the link simulator that can be found at https://github.com/cnp3/Linksimulator

And finally, if this message is still there at your final submission, it looks like you forgot to provide a proper README.