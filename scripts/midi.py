import sys
import mido
import math
import re

def midi_note_to_freq(note):
    return 440.0 * (2.0 ** ((note - 69) / 12.0))

def ticks_to_ms(ticks, ticks_per_beat, tempo):
    return (ticks * tempo) / (ticks_per_beat * 1000.0)

def process_track(track, ticks_per_beat, tempo):
    current_time = 0
    active_note_time = {}

    events = []
    track_name = None

    for msg in track:
        current_time += msg.time

        if msg.type == 'set_tempo':
            tempo = msg.tempo

        if msg.type == 'track_name':
            track_name = msg.name

        if msg.type == 'note_on' and msg.velocity > 0:
            active_note_time[msg.note] = current_time

        elif msg.type == 'note_off' or (msg.type == 'note_on' and msg.velocity == 0) and (msg.note in active_note_time):
            freq = midi_note_to_freq(msg.note)

            start_time = active_note_time.pop(msg.note)
            delta_time = current_time - start_time

            time = ticks_to_ms(start_time, ticks_per_beat, tempo)
            duration = ticks_to_ms(delta_time, ticks_per_beat, tempo)
            
            if freq > 0 and duration > 0:
                events.append((freq, time, duration))

    return track_name, events

def generate_c(tracks, name):
    print("#pragma once\n")
    print("#include <engine.h>\n")

    for track_name, events in tracks:
        print(f"const event_data_t {name}_{track_name}[] = {{")
        for freq, time, duration in events:
            print(f"  {{{int(freq)}, {int(time)}, {int(duration)}}},")
        print("};\n")

    print(f"const track_data_t {name}_data[] = {{")
    for i, (track_name, events) in enumerate(tracks):
        print(f"  {{ .events = {name}_{track_name}, .event_count = sizeof({name}_{track_name}) / sizeof(event_data_t), 0 }},")
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
    mid = mido.MidiFile(filename)
    bpm = 120
    tempo = 60000000 / bpm
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
        print("Usage: python midi.py <file>.mid <name>")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])
