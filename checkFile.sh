#!/bin/bash

cmp --silent LeFilze/1Kb LeFilze/1Kb1 && echo '### SUCCESS Files Are Identical! ### (1Kb)' || echo '### WARNING: Files Are Different! ### (1Kb)'

cmp --silent LeFilze/1Mb LeFilze/1Mb1 && echo '### SUCCESS Files Are Identical! ###(1Mb)' || echo '### WARNING: Files Are Different!### (1Mb) '

cmp --silent LeFilze/200Mb LeFilze/200Mb1 && echo '### SUCCESS Files Are Identical! ### (200Mb)' || echo '### WARNING: Files Are Different! (200Mb)###'

cmp --silent LeFilze/4097kb LeFilze/4097kb1 && echo '### SUCCESS Files Are Identical! ### (4097Kb)' || echo '### WARNING: Files Are Different! (4097Kb)###'