#!/bin/bash

cmp --silent LeFilze/1KiB LeFilze/1KiB1 && echo '### SUCCESS Files Are Identical! ###' || echo '### WARNING: Files Are Different! ###'

cmp --silent LeFilze/1MiB LeFilze/1MiB1 && echo '### SUCCESS Files Are Identical! ###' || echo '### WARNING: Files Are Different! ###'

cmp --silent LeFilze/200MiB LeFilze/200MiB1 && echo '### SUCCESS Files Are Identical! ###' || echo '### WARNING: Files Are Different! ###'