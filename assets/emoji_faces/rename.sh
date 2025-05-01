#!/bin/bash -x
filenumber=$1 #file number from where you want to rename. 
lastfile=$2 #Last filenumber in the folder
extension=$3
count=-1


for ((i=filenumber; i<=$lastfile; i++))
do
     mv $i.$extension `expr $i + $count`.$extension
done
