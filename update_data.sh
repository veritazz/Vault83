#!/bin/sh

./create_data.sh 
cp data-output/Vault83.bin ../../Arduino/Arduboy_FX/Arduboy-Python-Utilities/example-flashcart/FPS/Vault83Data.bin
cd ../../Arduino/Arduboy_FX/Arduboy-Python-Utilities
python flashcart-writer.py -d example-flashcart/FPS/Vault83Data.bin 
cd -
