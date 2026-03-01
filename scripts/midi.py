import sys
import pretty_midi
import math
import re

def midi_note_to_freq(note):
    return 440.0 * (2.0 ** ((note - 69) / 12.0))

def split_voices(notes, overlap = 0.005):
    notes = sorted(notes, key=lambda n: n.start)

    voices = []

    for note in notes:
        placed = False

        for voice in voices:
            last = voice[-1]

            if note.start >= last.end - overlap:
                if note.start < last.end:
                    last.end = note.start

                voice.append(note)
                placed = True
                break
        
        if not placed:
            voices.append([note])
    
    return voices

def process_instrument(instrument):
    voices = split_voices(instrument.notes)

    processed = []

    for index, notes in enumerate(voices):
        events = []
        
        for note in notes:
            freq = midi_note_to_freq(note.pitch)

            time = note.start * 1000.0
            duration = (note.end - note.start) * 1000.0
            velocity = note.velocity
            
            if freq > 0 and duration > 0:
                events.append((freq, time, duration, velocity))
        
        if events:
            processed.append((index, events))

    return instrument.name, instrument.program, processed

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

    for index, instrument in enumerate(mid.instruments):
        if instrument.is_drum:
            continue

        instrument_name, program, voices = process_instrument(instrument)

        if not voices:
            continue

        base_fallback = f"track{index}"
        base_name = sanitize_name(instrument_name, base_fallback)

        for voice_index, events in voices:
            voice_name = base_name if voice_index == 0 else f"{base_name}_{voice_index}"

            # ensure uniqueness
            base = voice_name
            counter = 1
            while voice_name in used_names:
                voice_name = f"{base}_{counter}"
                counter += 1

            used_names.add(voice_name)
            tracks.append((voice_name, program, events))

    generate_c(tracks, name)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python midi.py <file>.mid <name>")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
