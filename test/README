Compilation:

dans le fichier projet/ouichefs
exécuter le make, il faut changer le path 

dans le fichier projet/ouichefs/mkfs 
générer l'image avec make img

Dans la vm projet/ouichefs/partition
un make est fournis pour mount et umount la partition, il faut changer le path


dans le fichier projet/ouichefs/test des tests sont fornis

etape 1:

bash etape1.sh 
afin de créer plusieurs répertoirs et voir avec le debugfs les différents changements des numéros de blocks


etape 2:

bash etape2.sh 
cat /sys/kernel/debug/ouichefs_debug_file

le lancer à chaque modification pour voir le comportement des blocks


etape 3:
un Makefile est fournis pour générer les fichier binair pour l'utilisation des requettes ioctl
lancer -> bash etape3.sh 0 pour créer plusieurs versions d'un même fichier
lancer -> bash etape2.sh    pour voir l'organisation des blocs
lancer -> bash etape3.sh 1 num_version pour changer de version
lancer -> bash etape3.sh 2 pour essayer d'ecrir dans le fichier, un message d'erreur est affiché disant que ce n'est pas possible d'ecrire sur ce fichier
lancer -> bash etape3.sh 3 pour se remettre sur le version initial et donc pouvoir réécrir sur le fichier avec bash etape3.sh 2


etape 4:

lancer ->  bash etape4.sh 0 pour pouvoir créer un fichier avec différentes versions
lancer -> bash etape2.sh    pour voir l'organisation des blocs
lancer ->  bash etape4.sh 1 num_version pour restorer une versions, le numéro de versiosn doit etre donné en paramètre

lancer -> bash etape4.sh 2 pour essayer d'ecrir dans le fichier, on vois bien qu'il réutilise les blocks deja libérés si l'on affiche l'organisation avec le debugfs
