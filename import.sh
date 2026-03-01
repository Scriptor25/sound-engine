#!/usr/bin/env sh

names="all_night_long free_bird golden megalovania tetris the_real_slim_shady"

for name in $names; do
    python "scripts/midi.py" "/home/felix/Music/$name.mid" "$name" > "main/include/songs/$name.h"
done
