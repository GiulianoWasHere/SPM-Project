#!/bin/bash
#SBATCH --job-name=FF_100FILES     # Job name
#SBATCH --output=JOB_FF_%j.txt     # OUTPUT
#SBATCH --nodes=1                  # Number of nodes
#SBATCH --ntasks-per-node=1        # Number of tasks
#SBATCH --time=02:00:00            # Time

GENERATE_TXT=../generateTxt
EXE_PATH=../FF_minizip
DELETE_SCRIPT=./deleteMiniz.sh
CHECK_FILES=./checkFilze.sh
#Files Info
ScratchDir="/tmp/myjob"  # create a name for the TEMP directory (The TXT file will be in the temp directory of the NODE)
rm -rf ${ScratchDir}     # Making sure the folder hasn't file in it
mkdir -p ${ScratchDir}   # make the directory
ScratchDir="/tmp/myjob/"
NAME_OF_FILE=20MB
DIMENSION_FILE=20
NUMBER_OF_FILES=100
echo NUMBER OF FILES: $NUMBER_OF_FILES EACH: $DIMENSION_FILE MB

#Generate Files
for i in $(seq $NUMBER_OF_FILES)
do
    FILENAME=$i"_"${NAME_OF_FILE}
    $GENERATE_TXT $DIMENSION_FILE $ScratchDir$FILENAME
done

for l in 2; do
    for r in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32; do
        echo "LWORKERS: $l RWORKERS: $r"
        echo "COMPRESSION:"
        $EXE_PATH c $ScratchDir $l $r
        echo "DECOMPRESSION:"
        $EXE_PATH d $ScratchDir $l $r
        #Check if uncompressed and the original file are equal
        #$CHECK_FILES $ScratchDir
        $DELETE_SCRIPT $ScratchDir
        echo "-----------------------"
    done
done

#Delete Files
for i in $(seq $NUMBER_OF_FILES)
do
    FILENAME=$i"_"${NAME_OF_FILE}
    find $ScratchDir -name $FILENAME -type f -delete
done