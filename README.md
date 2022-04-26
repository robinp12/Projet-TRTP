# LINFO1341 Projet TRTP

## Cibles du Make file

**make** : créé les exécutables `sender` et `receiver` (cible par défaut)

**make debug** : créé les exécutables `sender` et `receiver` en affichant les informations de debug (macro `DEBUG` désactivé)

**make nolog** : créé les exécutables `sender` et `receiver`, sans afficher les logs (macros `LOG_SENDER` et `LOG_RECEIVER` désactivés). La sortie d'erreur contient alors uniquement les messages d'erreurs

**make tests** : lance une petite suite de tests, pour différents fichiers avec différents paramètres du simulateur de lien [link_sim](https://github.com/cnp3/Linksimulator). Les logs des différents tests sont disponibles dans le sous-dossier de tests correspondant

**make tests-nolog** : idem que make tests, mais désactive les logs

**make valgrind** : lance un test avec le simulateur de lien avec valgrind sur le sender et le receiver. La fin des logs sont affichés afin d'avoir le rapport de Valgrind sur le sender et receiver. L'entièreté des logs est sauvegardé dans tests/valgrind

**make clean** : supprime les fichiers objets

**make mrproper** : supprime les exécutables `sender` et `receiver`

**make cleanTests** : supprimes les logs créés par les tests

**make zip** : créer un fichier zip pour la soumission

## Lancer le programme : 

./receiver :: 1234 

./sender -f *input_file* :: 1234 

## Lancer les tests


**make tests**  pour lancer la suite de tests

**make valgrind** pour lancer un test avec valgrind

Pour lancer un test particulier

./tests/test.sh *input_file* *delay* *jitter* *error_rate* *cut* *loss* 

## Organisations des fichiers

```
TRTP
 |------src
 |       |-----linkedList
 |       |         |------linkedList.c (.h)
 |       |         |------Makefile
 |       |         |------test_linkedList.c (.h)
 |       |-----read-write
 |       |         |------create_socket.c (.h) 
 |       |         |------read_write_receiver.c (.h) 
 |       |         |------read_write_sender.c (.h) 
 |       |         |------real_address.c (.h) 
 |       |         |------wait_for_client.c (.h) 
 |       |------fec.c (.h) 
 |       |------log.c (.h) 
 |       |------packet_implem.c (.h) 
 |       |------receiver.c 
 |       |------sender.c
 |------tests                   _
 |       |------basic/           |
 |       |------image/           |
 |       |------imageSimlink/    |
 |       |------pdf/             |
 |       |------pdfSimlink/      |--> Dossiers pour stocker les logs
 |       |------random/          |    des différents tests
 |       |------smallImage/      |
 |       |------smallSimlink/    |
 |       |------valgrind/       _|
 |       |
 |       | [Divers fichiers pour les tests]
 |       |
 |       |------loop_test.sh
 |       |------run_tests.sh
 |       |------simple_test.sh
 |       |------valgrind.s
 |------Assignment.pdf
 |------link_sim
 |------Makefile
 |------rapport.pdf
 |------README.md
```

