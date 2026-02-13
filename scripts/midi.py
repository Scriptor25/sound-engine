import sys
import mido
import math
import re

def midi_note_to_freq(note):
    return 440.0 * (2.0 ** ((note - 69) / 12.0))

def ticks_to_ms(ticks, ticks_per_beat, tempo):
    # tempo is microseconds per beat
    return (ticks * tempo) / (ticks_per_beat * 1000.0)

def process_track(track, ticks_per_beat, tempo):
    current_ticks = 0

    active_note_ticks = {}
    last_sound_end_ticks = 0   # last time we were completely silent

    events = []
    track_name = None

    for msg in track:
        current_ticks += msg.time

        if msg.type == 'set_tempo':
            tempo = msg.tempo

        if msg.type == 'track_name':
            track_name = msg.name

        # NOTE ON
        if msg.type == 'note_on' and msg.velocity > 0:
            # If we are currently silent, insert rest
            if len(active_note_ticks) == 0 and current_ticks > last_sound_end_ticks:
                rest_ticks = current_ticks - last_sound_end_ticks
                rest_ms = ticks_to_ms(rest_ticks, ticks_per_beat, tempo)
                if int(rest_ms) > 0:
                    events.append((0.0, rest_ms))

            active_note_ticks[msg.note] = current_ticks

        # NOTE OFF (or note_on with velocity 0)
        elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0):
            if msg.note not in active_note_ticks:
                continue

            start_ticks = active_note_ticks.pop(msg.note)
            duration_ticks = current_ticks - start_ticks
            duration_ms = ticks_to_ms(duration_ticks, ticks_per_beat, tempo)

            if int(duration_ms) > 0:
                freq = midi_note_to_freq(msg.note)
                events.append((freq, duration_ms))

            # If that was the last active note, we are now silent
            if len(active_note_ticks) == 0:
                last_sound_end_ticks = current_ticks

    return track_name, events

def generate_c(tracks, name):
    print("#pragma once\n")
    print("#include <engine.h>\n")

    for track_name, events in tracks:
        print(f"event_t {name}_{track_name}[] = {{")
        for freq, duration in events:
            print(f"  {{{int(freq)}, {int(duration)}}},")
        print("};\n")

    print(f"track_t {name}_data[] = {{")
    for i, (track_name, events) in enumerate(tracks):
        print(f"  {{ .pin = {i}, .events = {name}_{track_name}, .event_count = sizeof({name}_{track_name}) / sizeof(event_t) }},")
    print("};\n")

def sanitize_name(name, fallback):
    if not name:
        return fallback

    # lower case
    name = name.lower()

    # replace non-alphanumeric with underscore
    name = re.sub(r'[^a-z0-9]+', '_', name)

    # remove leading/trailing underscores
    name = name.strip('_')

    # must not start with digit
    if not name or name[0].isdigit():
        name = fallback

    return name

def main(filename, name):
    mid = mido.MidiFile(filename)
    tempo = 750000  # default 80 BPM
    tracks_data = []
    used_names = set()

    for i, track in enumerate(mid.tracks):
        raw_name, events = process_track(track, mid.ticks_per_beat, tempo)

        if not events:
            continue

        fallback = f"track{i}"
        track_name = sanitize_name(raw_name, fallback)

        # ensure uniqueness
        base = track_name
        counter = 1
        while track_name in used_names:
            track_name = f"{base}_{counter}"
            counter += 1

        used_names.add(track_name)
        tracks_data.append((track_name, events))

    generate_c(tracks_data, name)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python midi_to_c.py <file>.mid <name>")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
