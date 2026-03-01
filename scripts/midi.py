import sys
import pretty_midi
import math
import re

def midi_note_to_freq(note):
    return 440.0 * (2.0 ** ((note - 69) / 12.0))

def process_instrument(instrument):
    events = []
    
    for note in instrument.notes:
        freq = midi_note_to_freq(note.pitch)

        time = note.start * 1000.0
        duration = (note.end - note.start) * 1000.0
        velocity = note.velocity
        
        if freq > 0 and duration > 0:
            events.append((freq, time, duration, velocity))

    return instrument.name, instrument.program, events

def generate_c(tracks, name):
    print("#pragma once\n")
    print("#include <engine.h>\n")

    for track_name, program, events in tracks:
        print(f"const event_data_t {name}_{track_name}[] = {{")
        for freq, time, duration, velocity in events:
            print(f"  {{{int(freq)}, {int(time)}, {int(duration)}, {int(velocity)}}},")
        print("};\n")

    print(f"const track_data_t {name}_data[] = {{")
    for track_name, program, events in tracks:
        print(f"  {{ {int(program)}, {name}_{track_name}, sizeof({name}_{track_name}) / sizeof(event_data_t), 0 }},")
    print("};")

def sanitize_name(name, fallback):
    if not name:
        return fallback

    name = name.lower()
    name = re.sub(r'[^a-z0-9]+', '_', name)
    name = name.strip('_')

    if not name or name[0].isdigit():
        name = fallback

    return name

def main(filename, name):
    mid = pretty_midi.PrettyMIDI(filename)
    
    tracks = []
    used_names = set()

    for i, instrument in enumerate(mid.instruments):
        if instrument.is_drum:
            continue

        raw_name, program, events = process_instrument(instrument)

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
        tracks.append((track_name, program, events))

    generate_c(tracks, name)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python midi.py <file>.mid <name>")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
