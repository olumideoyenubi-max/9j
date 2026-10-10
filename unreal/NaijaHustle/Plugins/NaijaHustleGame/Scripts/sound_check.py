"""Measures the recording -NHRun=sounds makes: do the voices, the horn and the engine come out of the speakers,
and do they do what they should?

    Scripts/mac.sh play -NHRun=sounds -NHNoSave
    python3 Plugins/NaijaHustleGame/Scripts/sound_check.py Saved/NHAudio/nh_sounds.wav ~/Library/Logs/MyProject/MyProject.log

The test logs "mark <name> <seconds>" as each sound begins (seconds from the start of the recording). This reads
those and the wav, and checks:

    each sound is there (louder than the quiet before the first one)
    the same syllable on Yoruba's three tones comes out at three pitches, high above mid above low
    the engine's note is higher while it pulls away than at idle

Needs numpy. Exit code 0 if every check passes.
"""
import re
import sys
import wave

import numpy as np


def read(path):
    with wave.open(path, "rb") as w:
        rate, channels, width, frames = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
        raw = w.readframes(frames)
    kind = {2: "<i2", 3: None, 4: "<i4"}[width]
    if kind is None:  # 24-bit
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3).astype(np.int32)
        x = (b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16))
        x = np.where(x >= 1 << 23, x - (1 << 24), x) / float(1 << 23)
    else:
        x = np.frombuffer(raw, dtype=kind) / float(1 << (8 * width - 1))
    return rate, x.reshape(-1, channels).mean(axis=1)


def level(x):
    return 20 * np.log10(np.sqrt(np.mean(x * x)) + 1e-9)


def pitch(x, rate, lo=60.0, hi=400.0):
    """The strongest repeat in the sound, by autocorrelation, Hz"""
    x = x - np.mean(x)
    n = len(x)
    spec = np.fft.rfft(x * np.hanning(n), 2 * n)
    ac = np.fft.irfft(np.abs(spec) ** 2)[:n]
    a, b = int(rate / hi), int(rate / lo)
    return rate / (a + int(np.argmax(ac[a:b])))


def main():
    rate, x = read(sys.argv[1])
    marks = {}
    for line in open(sys.argv[2], errors="ignore"):
        m = re.search(r"\[sounds\] mark (\w+) ([0-9.]+)", line)
        if m:
            marks[m.group(1)] = float(m.group(2))
    cut = lambda t0, t1: x[int(max(t0, 0) * rate):int(t1 * rate)]
    ok = True

    def check(what, passed, detail):
        nonlocal ok
        ok &= bool(passed)
        print(("PASS  " if passed else "FAIL  ") + what + "  : " + detail)

    print(f"{sys.argv[1]}: {len(x) / rate:.1f} s at {rate} Hz; marks {marks}")
    quiet = level(cut(0.1, marks["high"] - 0.1))
    for name, length in (("high", 1.2), ("mid", 1.2), ("low", 1.2), ("iya", 2.5), ("bark", 1.2), ("horn", 0.4), ("idle", 2.0), ("pull", 4.0)):
        loud = level(cut(marks[name] + 0.1, marks[name] + 0.1 + length))
        check(f"'{name}' is heard", loud > quiet + 12, f"{loud:.0f} dB against {quiet:.0f} dB of quiet")
    tones = [pitch(cut(marks[n] + 0.15, marks[n] + 1.2), rate) for n in ("high", "mid", "low")]
    check("the three tones are three pitches, falling", tones[0] > tones[1] * 1.08 > tones[2] * 1.08 * 1.08, "%.0f, %.0f, %.0f Hz" % tuple(tones))
    # the engine changes gear on the way, so its note is not one pitch: what is measured is where the weight of the
    # sound sits (its spectral centroid, 20 Hz to 2 kHz) and how loud it is, idling and pulling
    def centre(seg):
        spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg))))
        freq = np.fft.rfftfreq(len(seg), 1.0 / rate)
        band = (freq > 20) & (freq < 2000)
        return float(np.sum(freq[band] * spec[band]) / (np.sum(spec[band]) + 1e-12))
    idle_seg, pull_seg = cut(marks["idle"] + 1.0, marks["idle"] + 2.4), cut(marks["pull"] + 1.0, marks["end"] - 0.2)
    check("the engine sounds higher pulling away than at idle", centre(pull_seg) > centre(idle_seg) * 1.15, f"its weight sits at {centre(idle_seg):.0f} Hz idling, {centre(pull_seg):.0f} Hz pulling")
    check("and louder", level(pull_seg) > level(idle_seg) + 2, f"{level(idle_seg):.0f} dB idling, {level(pull_seg):.0f} dB pulling")
    check("the engine can be heard beside a voice", level(pull_seg) > level(cut(marks["mid"] + 0.1, marks["mid"] + 1.3)) - 16, f"{level(pull_seg):.0f} dB against a line at {level(cut(marks['mid'] + 0.1, marks['mid'] + 1.3)):.0f} dB")
    print("RESULT: " + ("ALL PASS" if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
