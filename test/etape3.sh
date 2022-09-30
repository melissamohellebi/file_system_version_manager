#!/bin/bash


if [ $1 -eq 0 ]
then
	cd ../partition/partition_ouichefs
	echo 1 > toto
	echo 2 > toto
	echo 3 > toto
	echo 4 > toto
	echo 5 > toto
	echo 6 > toto
	echo 7 > toto
	echo 8 > toto
	echo 9 > toto
fi

if [ $1 -eq 1 ]
then
	./change_version ../partition/partition_ouichefs/toto $2
fi


if [ $1 -eq 2 ]
then
	echo toto > ../partition/partition_ouichefs/toto
fi


if [ $1 -eq 3 ]
then
	./release_version ../partition/partition_ouichefs/toto 2
fi




