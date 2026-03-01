#!/usr/bin/env sh

names="all_night_long axelf fade free_bird golden hotel_california mayday megalovania mission_impossible tetris the_real_slim_shady what_ive_done"

for name in $names; do
    echo "processing $name..."
    python "scripts/midi.py" "/home/felix/Music/$name.mid" "$name" > "main/include/songs/$name.h"
done
