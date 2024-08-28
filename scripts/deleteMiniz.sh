#!/bin/bash

#Delete all the files with the .miniz at the end
find $1 -name "*.miniz" -type f -delete

#Delete all the files with the 1 or 2 at the end
find $1 -name "*1" -type f -delete
find $1 -name "*2" -type f -delete