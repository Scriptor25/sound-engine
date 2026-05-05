#!/usr/bin/env sh

names="all_night_long aria_math axelf bohemian_rhapsody dont_look_back_in_anger fade final_countdown free_bird golden highway_to_hell hotel_california its_my_life jump mayday megalovania mission_impossible never_gonna_give_you_up sharp_dressed_man stairway_to_heaven still_alive super_mario_bros sweden take_on_me tetris the_real_slim_shady through_the_fire_and_flames toccata_and_fugue what_ive_done where_is_my_mind"

for name in $names; do
    echo "processing $name..."
    python "scripts/midi.py" "songs/$name.mid" "$name" > "main/include/songs/$name.h"
done
