#!/bin/bash
#SBATCH --job-name=SEQ_WEAKSCALING_200  # Job name
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
DIMENSION_FILE=200

for l in 2 3 4 5 6 7 8; do
        echo FILE: $NAME_OF_FILE DIMENSION: $((DIMENSION_FILE*(l))) MB
        $GENERATE_TXT $((DIMENSION_FILE*(l))) $ScratchDir$NAME_OF_FILE
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
