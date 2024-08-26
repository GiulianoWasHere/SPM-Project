#!/bin/bash
#SBATCH --job-name=fastFlow      # Job name
#SBATCH --output=output_ff_%j.txt  # Standard output and error log
#SBATCH --nodes=1                  # Run on a single node
#SBATCH --ntasks=1                 # Number of tasks (processes)
#SBATCH --time=01:00:00      

FILES_PATH=/home/g.difranco/zipProject/LeFilze/
EXE_PATH=/home/g.difranco/zipProject/ffa2a
DELETE_SCRIPT=/home/g.difranco/zipProject/scripts/deleteMiniz.sh
GENERATE_TXT=/home/g.difranco/zipProject/generateTxt
NAME_OF_FILE=1GB
$GENERATE_TXT 1024 $FILES_PATH$NAME_OF_FILE
# Loop over different values of -l and -w
for l in 1 2 4; do
    for r in 4 8 16 20 22 24 26 28 29 30 31 32 33 34 35 36; do
        echo "LWORKERS: $l RWORKERS: $r COMPRESSION"
        $EXE_PATH c $FILES_PATH $l $r
        $DELETE_SCRIPT 
    done
done

find $FILES_PATH -name $NAME_OF_FILE -type f -delete