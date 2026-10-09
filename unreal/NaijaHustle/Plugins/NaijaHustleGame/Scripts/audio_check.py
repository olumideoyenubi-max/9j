"""Measures the recording ANHAudioTest makes, and says whether the audio mix did what it should.

    Scripts/mac.sh city -NHAudioTest
    python3 Plugins/NaijaHustleGame/Scripts/audio_check.py Saved/NHAudio

The test plays placeholder tones through the game's mix, one situation after another, and writes
nh_audio_test.wav (what came out of the speakers) and nh_audio_test.json (when each situation began and ended).
This reads both, measures the level of each tone in each situation, and checks:

    a line of dialogue ducks the music, and the music comes back after
    a phone call ducks it further
    the Music and Master sliders scale it
    the muffled radio loses its top end
    a sound to the right is louder in the right ear, and the same in both with Mono on
    a wall between the listener and a sound cuts it
    a burst rings longer in the tunnel than in the street
    forty horns at once leave no more sounding than the Horn kind allows

Plain Python, no packages. Exit code 0 if every check passes.
"""
import json
import math
import struct
import sys
from pathlib import Path


def read_wav(path):
    """Returns (rate, [channel sample lists as floats -1..1]) for 16/24/32-bit PCM or 32-bit float."""
    data = Path(path).read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path} is not a WAV file")
    pos, fmt, pcm = 12, None, None
    while pos + 8 <= len(data):
        tag, size = data[pos:pos + 4], struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if tag == b"fmt ":
            fmt = struct.unpack("<HHIIHH", body[:16])
        elif tag == b"data":
            pcm = body
        pos += 8 + size + (size & 1)
    if fmt is None or pcm is None:
        raise ValueError(f"{path} has no fmt or data chunk")
    kind, channels, rate, _, _, bits = fmt
    width = bits // 8
    count = len(pcm) // (width * channels)
    if kind == 3 and bits == 32:
        flat = struct.unpack(f"<{count * channels}f", pcm[:count * channels * 4])
    elif bits == 16:
        flat = [v / 32768.0 for v in struct.unpack(f"<{count * channels}h", pcm[:count * channels * 2])]
    elif bits == 32:
        flat = [v / 2147483648.0 for v in struct.unpack(f"<{count * channels}i", pcm[:count * channels * 4])]
    elif bits == 24:
        flat = [int.from_bytes(pcm[i:i + 3], "little", signed=True) / 8388608.0 for i in range(0, count * channels * 3, 3)]
    else:
        raise ValueError(f"{path}: {bits}-bit format {kind} is not handled")
    return rate, [list(flat[c::channels]) for c in range(channels)]


def tone(samples, rate, hz):
    """Amplitude of one frequency in a stretch of samples (Goertzel)."""
    n = len(samples)
    if n == 0:
        return 0.0
    w = 2.0 * math.pi * hz / rate
    coeff, s1, s2 = 2.0 * math.cos(w), 0.0, 0.0
    for x in samples:
        s1, s2 = x + coeff * s1 - s2, s1
    power = s1 * s1 + s2 * s2 - coeff * s1 * s2
    return 2.0 * math.sqrt(max(power, 0.0)) / n


def rms(samples):
    return math.sqrt(sum(x * x for x in samples) / len(samples)) if samples else 0.0


def band(samples, rate, freqs, block=2048):
    """Mean level of hiss at a handful of frequencies, block by block."""
    total, blocks = 0.0, 0
    for start in range(0, len(samples) - block, block):
        chunk = samples[start:start + block]
        total += sum(tone(chunk, rate, f) ** 2 for f in freqs) / len(freqs)
        blocks += 1
    return math.sqrt(total / blocks) if blocks else 0.0


def main(folder):
    folder = Path(folder)
    rate, channels = read_wav(folder / "nh_audio_test.wav")
    info = json.loads((folder / "nh_audio_test.json").read_text())
    phases = {p["name"]: p for p in info["phases"]}
    left, right = channels[0], channels[1] if len(channels) > 1 else channels[0]
    mid = [(a + b) * 0.5 for a, b in zip(left, right)]
    seconds = len(mid) / rate
    print(f"recording: {seconds:.1f} s, {rate} Hz, {len(channels)} channels; started in the {info['startSpace']} space")

    def cut(samples, name, lead=1.2, tail=0.1):
        """The settled part of a situation: fades take up to 0.8 s."""
        p = phases[name]
        return samples[int((p["start"] + lead) * rate):int((p["end"] - tail) * rate)]

    results = []

    def check(what, ok, detail):
        results.append(ok)
        print(f"  {'ok  ' if ok else 'FAIL'}  {what}: {detail}")

    def ratio(a, b):
        return a / b if b > 1e-9 else float("inf")

    print("the music tone (220 Hz) by situation:")
    music = {name: tone(cut(mid, name), rate, 220.0) for name in
             ("silence", "music", "music_dialogue", "music_after_dialogue", "music_call", "music_after_call", "music_slider_50", "music_master_50")
             if name != "silence"}
    for name, level in music.items():
        print(f"    {name:24s} {level:.4f}  ({ratio(level, music['music']):.2f} of its level alone)")
    base = music["music"]
    check("the music plays", base > 0.01, f"level {base:.4f}")
    check("silence before it is silent", rms(cut(mid, "silence", 0.2, 0.2)) < 0.002, f"rms {rms(cut(mid, 'silence', 0.2, 0.2)):.5f}")
    r = ratio(music["music_dialogue"], base)
    check("a line of dialogue ducks the music (mix says 0.45)", 0.35 <= r <= 0.58, f"{r:.2f}")
    check("the line itself is heard", tone(cut(mid, "music_dialogue"), rate, 660.0) > 0.01, f"660 Hz at {tone(cut(mid, 'music_dialogue'), rate, 660.0):.4f}")
    r = ratio(music["music_after_dialogue"], base)
    check("the music comes back after the line", r > 0.9, f"{r:.2f}")
    r = ratio(music["music_call"], base)
    check("a phone call ducks the music further (mix says 0.2)", 0.12 <= r <= 0.3, f"{r:.2f}")
    check("the call itself is heard", tone(cut(mid, "music_call"), rate, 1320.0) > 0.01, f"1320 Hz at {tone(cut(mid, 'music_call'), rate, 1320.0):.4f}")
    r = ratio(music["music_after_call"], base)
    check("the music comes back after the call", r > 0.9, f"{r:.2f}")
    r = ratio(music["music_slider_50"], base)
    check("Music slider at 50%", 0.42 <= r <= 0.58, f"{r:.2f}")
    r = ratio(music["music_master_50"], base)
    check("Music 50% and Master 50%", 0.19 <= r <= 0.31, f"{r:.2f}")

    low, high = (250.0, 400.0, 550.0, 700.0), (4000.0, 6000.0, 8000.0, 10000.0)
    clear_low, clear_high = band(cut(mid, "radio"), rate, low), band(cut(mid, "radio"), rate, high)
    muff_low, muff_high = band(cut(mid, "radio_muffled"), rate, low), band(cut(mid, "radio_muffled"), rate, high)
    print(f"the radio hiss: clear low {clear_low:.5f} high {clear_high:.5f}; muffled low {muff_low:.5f} high {muff_high:.5f}")
    check("the radio plays", clear_high > 0.0005, f"high band {clear_high:.5f}")
    check("muffled, the radio loses its top end", ratio(muff_high, clear_high) < 0.15, f"high band at {ratio(muff_high, clear_high):.3f} of clear")
    check("muffled, it is quieter but still there (mix says 0.45)", 0.25 <= ratio(muff_low, clear_low) <= 0.6, f"low band at {ratio(muff_low, clear_low):.2f} of clear")

    side_l, side_r = tone(cut(left, "side"), rate, 500.0), tone(cut(right, "side"), rate, 500.0)
    mono_l, mono_r = tone(cut(left, "side_mono"), rate, 500.0), tone(cut(right, "side_mono"), rate, 500.0)
    print(f"a tone 4 m to the right: left {side_l:.4f} right {side_r:.4f}; with Mono on: left {mono_l:.4f} right {mono_r:.4f}")
    check("a sound to the right is louder in the right ear", ratio(side_r, side_l) > 1.5, f"right is {ratio(side_r, side_l):.1f}x left")
    check("with Mono on both ears get the same", 0.95 <= ratio(mono_r, mono_l) <= 1.05 and mono_l > 0.001, f"right is {ratio(mono_r, mono_l):.2f}x left")

    clear, walled = tone(cut(mid, "front"), rate, 3000.0), tone(cut(mid, "front_wall", 1.6), rate, 3000.0)
    print(f"a 3 kHz tone 8.5 m ahead: in the clear {clear:.4f}, behind a wall {walled:.4f}")
    check("the tone is heard in the clear", clear > 0.002, f"{clear:.4f}")
    check("a wall in the way cuts it", ratio(walled, clear) < 0.5, f"{ratio(walled, clear):.2f} of clear")

    def ring(name):
        # the burst ends 0.4 s in; what is left from 0.25 s after that is the space ringing
        p = phases[name]
        return rms(mid[int((p["start"] + 0.65) * rate):int((p["start"] + 2.4) * rate)])

    def bang(name):
        p = phases[name]
        return rms(mid[int((p["start"] + 0.33) * rate):int((p["start"] + 0.45) * rate)])

    street, tunnel = ring("burst_street"), ring("burst_tunnel")
    print(f"a burst of hiss: the burst itself {bang('burst_street'):.4f} (street) {bang('burst_tunnel'):.4f} (tunnel); "
          f"ringing after it {street:.5f} (street) {tunnel:.5f} (tunnel)")
    check("the burst is heard", bang("burst_street") > 0.005 and bang("burst_tunnel") > 0.005, "both spaces")
    check("it rings longer in the tunnel than in the street", ratio(tunnel, max(street, 1e-6)) > 3.0, f"{ratio(tunnel, max(street, 1e-6)):.1f}x")

    flood = info["flood"]
    check("forty horns at once: no more sound than the kind allows", 0 < flood["playing"] <= flood["limit"],
          f"asked {flood['asked']}, sounding {flood['playing']}, allowed {flood['limit']}")
    check("voices stay inside the budget", info["voicesPeak"] <= info["voiceBudget"], f"most at once {info['voicesPeak']} of {info['voiceBudget']}")

    passed = sum(results)
    print(f"RESULT: {passed} of {len(results)} checks passed; frame {info['frameMs']} ms, process memory {info['usedPhysicalMB']:.0f} MB")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "Saved/NHAudio"))
