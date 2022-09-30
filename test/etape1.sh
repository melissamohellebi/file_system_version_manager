#!/bin/bash

etape1(){

        #Création de répertoire 
        cd  ../partition/partition_ouichefs      
        mkdir d1
        mkdir d2

        #Création de fichiers
        echo "ceci est un test" > f1
        echo "ceci est un test" > f2
        cd d1
        echo "ceci est un test" > f3
	cd ../d2
        touch f4
        cd ..
        touch f5

        #Ajouter des versions 

        echo "ceci est la suite du test" > f2
        cd d1
        echo "ceci est la suite du test" >  f3
        cd ..

        echo "ceci est la suite du test" >> f1
        echo "ceci est la suite du test" >> f2
	cd d2
        echo "ceci est la suite du test" > f4
        cd ..

        # 5 fichiers respectivement f1,f2,d1/f3,d2/f4,f5
        # 2 dossiers d1 et d2 à la racine
        # f1: 2 versions
        # f2: 3 versions
        # f3: 2 versions
        # f4: 1 versions
        # f5: 1 version
        
        cat /sys/kernel/debug/ouichefs_debug_file
        
}

if [[ $1 = "clean" ]];
then
	
        rm f*
        rm -r d*
        cat /sys/kernel/debug/ouichefs_debug_file
else
        etape1
fi
