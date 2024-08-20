#!/bin/bash

cmp --silent LeFilze/1Kb LeFilze/1Kb && echo '### SUCCESS Files Are Identical! ###' || echo '### WARNING: Files Are Different! ###'

cmp --silent LeFilze/1Mb LeFilze/1Mb1 && echo '### SUCCESS Files Are Identical! ###' || echo '### WARNING: Files Are Different! ###'

cmp --silent LeFilze/200Mb LeFilze/200Mb1 && echo '### SUCCESS Files Are Identical! ###' || echo '### WARNING: Files Are Different! ###'