#!/bin/bash

printf "\n\nRunning Tests for VMC\n"
printf "======================================================\n"

MPICOMMAND="mpirun -np 4"
VMCPATH="../../bin/VMC vmc.json"
CIPATH="../../bin/VMC ci.json"
here=`pwd`
tol=1.0e-7
clean=1

# VMC tests

cd $here/hubbard_1x10
../clean.sh
printf "...running hubbard_1x10\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/hubbard_1x10ghf
../clean.sh
printf "...running hubbard_1x10 ghf\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/hubbard_1x10agp
../clean.sh
printf "...running hubbard_1x10 agp\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/hubbard_1x14
../clean.sh
printf "...running hubbard_1x14\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
printf "...running hubbard_1x14 ci\n"
$MPICOMMAND $CIPATH > ci.out
python2 ../testEnergy.py 'ci' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/hubbard_1x22
../clean.sh
printf "...running hubbard_1x22\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/hubbard_1x50
../clean.sh
printf "...running hubbard_1x50\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/hubbard_1x6
../clean.sh
printf "...running hubbard_1x6\n"
$VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/hubbard_18_tilt/
../clean.sh
printf "...running hubbard_18_tilt uhf\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/h4_ghf_complex/
../clean.sh
printf "...running h4 ghf complex\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/h4_pfaffian_complex/
../clean.sh
printf "...running h4 pfaffian complex\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/h10pfaff/
../clean.sh
printf "...running h10 pfaffian\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/h20/
../clean.sh
printf "...running h20\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/h20ghf/
../clean.sh
printf "...running h20 ghf\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here/c2/
../clean.sh
printf "...running c2\n"
$MPICOMMAND $VMCPATH > vmc.out
python2 ../testEnergy.py 'vmc' $tol
if [ $clean == 1 ]
then    
    ../clean.sh
fi

cd $here
