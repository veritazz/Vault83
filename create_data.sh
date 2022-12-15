#!/bin/bash

asset_dir=../assets
output_dir=data-output

mkdir ${output_dir}
cd ${output_dir}

rm -f *

../scripts/generate_map.py ${asset_dir} .
if [ $? -ne 0 ]
then
	echo "map generation failed..."
	exit 1
fi

../scripts/generate_tables.py
if [ $? -ne 0 ]
then
	echo "table generation failed..."
	exit 1
fi



ase=${asset_dir}
o=.


# assets that have a mask layer
# TODO sort them!
assets="
enemy-1
enemy-2
enemy-3
enemy-4
item-key-1
item-weapon-2
item-weapon-3
item-weapon-4
item-health-1
item-ammo-1
item-7
item-8
item-9
item-10
item-11
item-12
item-13
item-14
sprite-barrel
sprite-spider
projectile-1
projectile-2
projectile-3
projectile-4
projectile-5
projectile-6
projectile-7
projectile-8
projectile-9
projectile-10
projectile-11
projectile-12
projectile-13
projectile-14
projectile-15
projectile-16
characters_3x4
hud
icons-hud-weapon
item-weapon-1
mainscreen
menu_arrow
player-weapons
texture-wall-1
texture-wall-2
texture-wall-3
texture-wall-4
texture-wall-5
texture-wall-6
texture-wall-7
texture-wall-8
texture-wall-9
texture-door-frame
texture-door-1
dialog-00
dialog-01
background
hud_special
"

png_assets="
wall32x32
wall16x16
wall8x8
"

# batch process all aseprite files to png/json files
for asset in $assets
do
	aseprite --batch $ase/${asset}.ase --sheet-type=vertical --sheet $o/${asset}.png --data $o/${asset}.json
done

for asset in $png_assets
do
	cp ${asset_dir}/${asset}.png $o/
	cp ${asset_dir}/${asset}.json $o/
done

# read all json files and convert them to code
../scripts/conpack.py $o

# clean up
#rm $o/*.json

# now copy all [str_|msg_]*.txt files from assets to output dir
cp $ase/str_* $o 
cp $ase/msg_* $o 

../scripts/generate_flashfile.py $o $ase/flashlayout.txt Vault83.bin flashoffsets.h

# copy code to source directory
cp leveldata.* ../src
cp tables.c ../src
cp tables.h ../src
cp flashoffsets.* ../src
