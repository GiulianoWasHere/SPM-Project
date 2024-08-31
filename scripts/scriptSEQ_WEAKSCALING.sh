#!/bin/bash
#SBATCH --job-name=SEQ_WEAKSCALING  # Job name
#SBATCH --output=JOB_SEQ_%j.txt    # OUTPUT
#SBATCH --nodes=1                  # Number of nodes
#SBATCH --time=01:00:00            # Time

GENERATE_TXT=../generateTxt
EXE_PATH=../SEQ_minizip
DELETE_SCRIPT=./deleteMiniz.sh
CHECK_FILES=./checkFilze.sh
#Files Info
ScratchDir="/tmp/myjob"  # create a name for the TEMP directory (The TXT file will be in the temp directory of the NODE)
rm -rf ${ScratchDir}     # Making sure the folder hasn't file in it
mkdir -p ${ScratchDir}   # make the directory
ScratchDir="/tmp/myjob/"
NAME_OF_FILE=FILE
DIMENSION_FILE=50

for l in 1 2; do
    for r in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32; do
        echo FILE: $NAME_OF_FILE DIMENSION: $((DIMENSION_FILE*(l+r))) MB
        $GENERATE_TXT $((DIMENSION_FILE*(l+r))) $ScratchDir$NAME_OF_FILE
        echo "COMPRESSION:"
        $EXE_PATH c $ScratchDir
        echo "DECOMPRESSION:"
        $EXE_PATH d $ScratchDir
        #Check if uncompressed and the original file are equal
        $CHECK_FILES $ScratchDir
        $DELETE_SCRIPT $ScratchDir
        find $ScratchDir -name $NAME_OF_FILE -type f -delete
        echo "-----------------------"
    done
done
