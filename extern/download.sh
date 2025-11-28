#!/bin/bash

wget https://github.com/VeriFIT/mata/archive/refs/tags/1.25.0.zip
unzip devel.zip
mv mata-devel mata
rm devel.zip

wget https://github.com/cs-au-dk/MONA/archive/refs/heads/master.zip
unzip master.zip
mv MONA-master MONA
rm master.zip
