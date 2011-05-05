#!/bin/ksh

main(){
     filename=$RANDOM
     if [  -n "$1" ]; then
        cd $1
     fi
     while :
     do
         echo $filename > $filename
         if [ "$?" -ne 0 ]; then
             echo "do_create_and_delete: cannot create $filenme."
             continue
         fi
         echo " "
         rm $filename
         if [ "$?" -ne 0 ]; then
             echo "do_create_and_delete: cannot remove $filenme."
             continue
         fi
         echo .
     done
}

main $1
