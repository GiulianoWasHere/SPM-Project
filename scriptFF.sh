#!/bin/bash
#SBATCH --job-name=fastFlow      # Job name
#SBATCH --output=output_ff_%j.txt  # Standard output and error log
#SBATCH --nodes=1                  # Run on a single node
#SBATCH --ntasks=1                 # Number of tasks (processes)
#SBATCH --time=01:00:00      

# Define the dataset path
FILES_PATH="/home/g.difranco/zipProject/LeFilze"
EXE_PATH=/home/g.difranco/zipProject/ffa2a
DELETE_SCRIPT=/home/g.difranco/zipProject/deleteMiniz.sh
# Loop over different values of -l and -w
for l in 1 2 4 8; do
    for r in 4 8 16 20 22 24 26 28 29 30; do
        echo "LWORKERS: $l RWORKERS: $r COMPRESSION"
        $EXE_PATH c $FILES_PATH $l $r
        $DELETE_SCRIPT 
    done
done