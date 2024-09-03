#!/bin/bash
#SBATCH --job-name=MPI_WEAKSCALING         # Job name
#SBATCH --output=JOB_MPI_%j.txt    # OUTPUT
#SBATCH --nodes=8            # Number of nodes
#SBATCH --ntasks-per-node=1        # Number of tasks
#SBATCH --time=02:00:00            # Time

#Executables info
GENERATE_TXT=../generateTxt
EXE_PATH=../MPI_minizip
DELETE_SCRIPT=./deleteMiniz.sh
CHECK_FILES=./checkFilze.sh

#Files Info
ScratchDir="/tmp/myjob"  # create a name for the TEMP directory (The TXT file will be in the temp directory of the NODE)
rm -rf ${ScratchDir}     # Making sure the folder hasn't file in it
mkdir -p ${ScratchDir}   # make the directory
ScratchDir="/tmp/myjob/"
NAME_OF_FILE=FILE
DIMENSION_FILE=200

for l in 2 3 4 5 6 7 8; do
    echo FILE: $NAME_OF_FILE DIMENSION: $((DIMENSION_FILE*(l))) MB
    $GENERATE_TXT $((DIMENSION_FILE*(l))) $ScratchDir$NAME_OF_FILE
    for r in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32; do
        echo "NODES: $l RWORKERS: $r"
        echo "COMPRESSION:"
        mpirun -np $l $EXE_PATH c $ScratchDir $r
        echo "DECOMPRESSION:"
        mpirun -np $l $EXE_PATH d $ScratchDir $r
        #Check if uncompressed and the original file are equal
        $CHECK_FILES $ScratchDir
        $DELETE_SCRIPT $ScratchDir
        echo "-----------------------"
    done
    find $ScratchDir -name $NAME_OF_FILE -type f -delete
done
