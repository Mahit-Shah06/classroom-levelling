#!/usr/bin/env python3
"""Synthesize Classroom Levelling game audio (stdlib only)."""

from __future__ import annotations

import math
import os
import random
import struct
import wave

SR = 22050
TWOPI = 2.0 * math.pi
OUT_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "Content",
    "Audio",
)

# ---------------------------------------------------------------------------
# Core I/O / DSP helpers
# ---------------------------------------------------------------------------


def clamp(x: float, lo: float = -1.0, hi: float = 1.0) -> float:
    return lo if x < lo else hi if x > hi else x


def write_wav(path: str, samples: list[float]) -> None:
    with wave.open(path, "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        frames = bytearray()
        for s in samples:
            v = int(clamp(s) * 32767.0)
            frames += struct.pack("<h", v)
        w.writeframes(frames)
    print(path)


def normalize(samples: list[float], peak_db: float) -> list[float]:
    peak = 0.0
    for s in samples:
        a = abs(s)
        if a > peak:
            peak = a
    if peak < 1e-12:
        return samples
    target = 10.0 ** (peak_db / 20.0)
    g = target / peak
    return [s * g for s in samples]


def crossfade_loop(samples: list[float], fade_sec: float = 0.05) -> list[float]:
    """Crossfade loop tail into head; force start/end near zero."""
    n = len(samples)
    fade = max(1, int(fade_sec * SR))
    fade = min(fade, n // 4)
    out = list(samples)
    for i in range(fade):
        t = i / fade
        # equal-power-ish linear blend of head and tail
        head = samples[i]
        tail = samples[n - fade + i]
        out[i] = head * t + tail * (1.0 - t)
    # zero-ish ends after blend
    out[0] = 0.0
    out[-1] = 0.0
    # soft approach to zero at both ends (tiny)
    edge = min(8, fade)
    for i in range(edge):
        w = i / edge
        out[i] *= w
        out[n - 1 - i] *= w
    return out


def one_pole_lp(samples: list[float], cutoff: float) -> list[float]:
    """Simple one-pole lowpass. cutoff in Hz."""
    x = math.exp(-TWOPI * cutoff / SR)
    a0 = 1.0 - x
    y = 0.0
    out = [0.0] * len(samples)
    for i, s in enumerate(samples):
        y = a0 * s + x * y
        out[i] = y
    return out


def one_pole_hp(samples: list[float], cutoff: float) -> list[float]:
    lp = one_pole_lp(samples, cutoff)
    return [samples[i] - lp[i] for i in range(len(samples))]


def biquad_bp(samples: list[float], freq: float, q: float = 1.0) -> list[float]:
    """Direct-form I bandpass (constant skirt gain)."""
    w0 = TWOPI * freq / SR
    alpha = math.sin(w0) / (2.0 * q)
    cosw = math.cos(w0)
    b0 = alpha
    b1 = 0.0
    b2 = -alpha
    a0 = 1.0 + alpha
    a1 = -2.0 * cosw
    a2 = 1.0 - alpha
    inv = 1.0 / a0
    b0, b1, b2 = b0 * inv, b1 * inv, b2 * inv
    a1, a2 = a1 * inv, a2 * inv
    x1 = x2 = y1 = y2 = 0.0
    out = [0.0] * len(samples)
    for i, x0 in enumerate(samples):
        y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        out[i] = y0
        x2, x1 = x1, x0
        y2, y1 = y1, y0
    return out


def biquad_lp(samples: list[float], freq: float, q: float = 0.707) -> list[float]:
    w0 = TWOPI * freq / SR
    alpha = math.sin(w0) / (2.0 * q)
    cosw = math.cos(w0)
    b0 = (1.0 - cosw) * 0.5
    b1 = 1.0 - cosw
    b2 = (1.0 - cosw) * 0.5
    a0 = 1.0 + alpha
    a1 = -2.0 * cosw
    a2 = 1.0 - alpha
    inv = 1.0 / a0
    b0, b1, b2 = b0 * inv, b1 * inv, b2 * inv
    a1, a2 = a1 * inv, a2 * inv
    x1 = x2 = y1 = y2 = 0.0
    out = [0.0] * len(samples)
    for i, x0 in enumerate(samples):
        y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        out[i] = y0
        x2, x1 = x1, x0
        y2, y1 = y1, y0
    return out


def mix_into(dst: list[float], src: list[float], gain: float = 1.0, offset: int = 0) -> None:
    n = len(dst)
    for i, s in enumerate(src):
        j = offset + i
        if 0 <= j < n:
            dst[j] += s * gain


def env_exp(n: int, attack: float, decay: float) -> list[float]:
    """Per-sample exponential envelope. attack/decay in seconds."""
    atk = max(1, int(attack * SR))
    out = [0.0] * n
    # decay tau so ~e^-5 at end of decay portion
    tau = max(decay, 1e-4) / 5.0
    for i in range(n):
        if i < atk:
            a = i / atk
        else:
            a = 1.0
        t = i / SR
        out[i] = a * math.exp(-t / tau)
    return out


def midi_hz(m: float) -> float:
    return 440.0 * (2.0 ** ((m - 69.0) / 12.0))


# Note names in D minor context
NOTE = {
    "A2": 45, "Bb2": 46, "B2": 47, "C3": 48, "D3": 50, "E3": 52, "F3": 53, "G3": 55,
    "A3": 57, "Bb3": 58, "C4": 60, "D4": 62, "E4": 64, "F4": 65, "G4": 67,
    "A4": 69, "Bb4": 70, "C5": 72, "D5": 74, "E5": 76, "F5": 77, "A5": 81,
}


def add_piano_note(
    buf: list[float],
    start: int,
    midi: float,
    dur: float,
    amp: float,
    partials: tuple[tuple[float, float], ...] = (
        (1.0, 1.0),
        (2.01, 0.45),
        (3.02, 0.22),
        (4.04, 0.12),
        (5.05, 0.06),
    ),
) -> None:
    """Additive sine partials with shared exponential decay envelope."""
    n = int(dur * SR)
    if n <= 0:
        return
    f0 = midi_hz(midi)
    env = env_exp(n, 0.008, dur * 0.85)
    for i in range(n):
        t = i / SR
        s = 0.0
        for mul, g in partials:
            s += g * math.sin(TWOPI * f0 * mul * t)
        j = start + i
        if 0 <= j < len(buf):
            buf[j] += amp * env[i] * s


def soft_noise(n: int, rng: random.Random) -> list[float]:
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def brown_noise(n: int, rng: random.Random) -> list[float]:
    out = [0.0] * n
    x = 0.0
    for i in range(n):
        x += rng.uniform(-1.0, 1.0)
        x *= 0.996  # leak to avoid drift
        out[i] = x
    # rough normalize
    peak = max(abs(v) for v in out) or 1.0
    return [v / peak for v in out]


# ---------------------------------------------------------------------------
# Generators
# ---------------------------------------------------------------------------


def gen_music_menu(rng: random.Random) -> list[float]:
    dur = 32.0
    n = int(dur * SR)
    buf = [0.0] * n

    # Soft low drone pad (D2 + A2 sine + quiet fifth)
    f_d = midi_hz(38)  # D2
    f_a = midi_hz(45)  # A2
    f_f = midi_hz(41)  # F2
    for i in range(n):
        t = i / SR
        # slow amplitude swell for pad
        swell = 0.55 + 0.45 * math.sin(TWOPI * t / dur)
        pad = (
            0.55 * math.sin(TWOPI * f_d * t)
            + 0.28 * math.sin(TWOPI * f_a * t)
            + 0.18 * math.sin(TWOPI * f_f * t)
        )
        # gentle beating
        pad += 0.08 * math.sin(TWOPI * (f_d + 0.15) * t)
        buf[i] += 0.12 * swell * pad

    # Sparse D minor piano-like notes
    # Phrase tones (midi): D4 A4 F4 E4 D4 Bb3 C4 A3 ...
    phrase = [
        (0.0, "D4", 2.4, 0.22),
        (2.5, "A4", 2.0, 0.14),
        (5.0, "F4", 2.2, 0.16),
        (7.5, "E4", 1.8, 0.12),
        (10.0, "D4", 2.5, 0.18),
        (12.5, "Bb3", 2.0, 0.15),
        (15.0, "C4", 1.6, 0.11),
        (17.0, "A3", 2.4, 0.14),
        (20.0, "F4", 2.0, 0.13),
        (22.5, "D4", 3.0, 0.20),
        (26.0, "A4", 2.5, 0.10),
        (28.5, "E4", 2.0, 0.09),
        # wrap-friendly near end
        (30.5, "D4", 1.4, 0.08),
    ]
    for t0, name, d, amp in phrase:
        add_piano_note(buf, int(t0 * SR), NOTE[name], d, amp)

    # Quiet ticking clock @ 60 bpm (one tick per second)
    tick_len = int(0.012 * SR)
    for sec in range(32):
        start = int(sec * SR)
        for i in range(tick_len):
            t = i / SR
            env = math.exp(-t * 280.0)
            # soft click: high sine + tiny noise
            s = 0.7 * math.sin(TWOPI * 1800.0 * t) + 0.3 * rng.uniform(-1, 1)
            j = start + i
            if j < n:
                buf[j] += 0.035 * env * s

    buf = one_pole_lp(buf, 4200.0)
    buf = crossfade_loop(buf, 0.08)
    return normalize(buf, -6.0)


def gen_music_hub(rng: random.Random) -> list[float]:
    dur = 48.0
    n = int(dur * SR)
    buf = [0.0] * n

    # Chord voicings (midi) — lo-fi electric piano feel
    # Dm9: D F A C E
    # Bbmaj7: Bb D F A
    # Gm7: G Bb D F
    # A7sus: A D E G
    chords = [
        [50, 53, 57, 60, 64],   # Dm9
        [46, 50, 53, 57],       # Bbmaj7
        [55, 58, 62, 65],       # Gm7
        [57, 62, 64, 67],       # A7sus
    ]
    # 4 bars of 4s each = 16s cycle, 3 cycles = 48s
    bar = 4.0
    for cyc in range(3):
        for ci, chord in enumerate(chords):
            t0 = (cyc * 4 + ci) * bar
            start = int(t0 * SR)
            # soft attack rolled chord
            for k, m in enumerate(chord):
                delay = int(k * 0.018 * SR)
                add_piano_note(
                    buf,
                    start + delay,
                    m,
                    bar * 1.05,
                    0.09 if k < 3 else 0.055,
                    partials=(
                        (1.0, 1.0),
                        (2.0, 0.35),
                        (3.0, 0.15),
                        (4.0, 0.08),
                    ),
                )

    # Gentle sub bass on roots
    roots = [38, 34, 43, 45]  # D2 Bb1 G2 A2
    for cyc in range(3):
        for ci, root in enumerate(roots):
            t0 = (cyc * 4 + ci) * bar
            start = int(t0 * SR)
            length = int(bar * 0.95 * SR)
            f = midi_hz(root)
            for i in range(length):
                t = i / SR
                env = min(1.0, t / 0.05) * math.exp(-t / (bar * 0.55))
                s = math.sin(TWOPI * f * t) * (1.0 + 0.15 * math.sin(TWOPI * f * 2 * t))
                j = start + i
                if j < n:
                    buf[j] += 0.16 * env * s

    # Light brushed noise hats — soft, sparse
    # ~80 bpm feel: hat every 0.375s within bars, quiet
    hat_n = int(0.035 * SR)
    step = 0.375
    t = 0.0
    while t < dur:
        start = int(t * SR)
        noise = soft_noise(hat_n, rng)
        noise = one_pole_hp(noise, 4000.0)
        noise = one_pole_lp(noise, 10000.0)
        for i in range(hat_n):
            env = math.exp(-i / SR * 90.0)
            j = start + i
            if j < n:
                buf[j] += 0.028 * env * noise[i]
        t += step

    buf = one_pole_lp(buf, 5500.0)
    buf = crossfade_loop(buf, 0.1)
    return normalize(buf, -6.0)


def gen_music_tension(rng: random.Random) -> list[float]:
    dur = 24.0
    n = int(dur * SR)
    buf = [0.0] * n
    bpm = 100.0
    beat = 60.0 / bpm  # 0.6s

    # Pulsing low ostinato (D1/D2 alternating pattern)
    pattern = [26, 26, 33, 26, 29, 26, 33, 31]  # midi, dark
    beat_i = 0
    t = 0.0
    while t < dur:
        m = pattern[beat_i % len(pattern)]
        start = int(t * SR)
        length = int(beat * 0.92 * SR)
        f = midi_hz(m)
        for i in range(length):
            tt = i / SR
            env = min(1.0, tt / 0.01) * math.exp(-tt / 0.22)
            # slightly square-ish via odd harmonics, kept soft
            s = (
                math.sin(TWOPI * f * tt)
                + 0.35 * math.sin(TWOPI * f * 2 * tt)
                + 0.12 * math.sin(TWOPI * f * 3 * tt)
            )
            j = start + i
            if j < n:
                buf[j] += 0.28 * env * s
        t += beat
        beat_i += 1

    # Dissonant high sustained tone with slow tremolo (E5 vs F5 minor second cluster)
    f1 = midi_hz(76)  # E5
    f2 = midi_hz(77)  # F5
    for i in range(n):
        tsec = i / SR
        trem = 0.55 + 0.45 * math.sin(TWOPI * 0.35 * tsec)
        tone = 0.55 * math.sin(TWOPI * f1 * tsec) + 0.45 * math.sin(TWOPI * f2 * tsec)
        # slow in/out so loop edges are calm
        gate = 0.7 + 0.3 * math.sin(TWOPI * tsec / dur)
        buf[i] += 0.07 * trem * gate * tone

    buf = one_pole_lp(buf, 3800.0)
    buf = crossfade_loop(buf, 0.06)
    return normalize(buf, -6.0)


def gen_amb_campus(rng: random.Random) -> list[float]:
    dur = 30.0
    n = int(dur * SR)
    buf = [0.0] * n

    # Soft wind: lowpassed brown noise with slow swell
    wind = brown_noise(n, rng)
    wind = biquad_lp(wind, 380.0, 0.7)
    for i in range(n):
        t = i / SR
        swell = 0.45 + 0.55 * (0.5 + 0.5 * math.sin(TWOPI * t / 11.0 + 0.3))
        swell *= 0.7 + 0.3 * math.sin(TWOPI * t / 7.0)
        buf[i] += 0.22 * swell * wind[i]

    # Distant birds: short chirps every few seconds
    t = 1.5 + rng.uniform(0, 1.5)
    while t < dur - 0.5:
        start = int(t * SR)
        chirp_n = int(rng.uniform(0.06, 0.14) * SR)
        f0 = rng.uniform(2200.0, 3800.0)
        f1 = f0 + rng.uniform(200.0, 900.0)
        for i in range(chirp_n):
            tt = i / SR
            frac = i / max(1, chirp_n - 1)
            f = f0 + (f1 - f0) * frac
            env = math.sin(math.pi * frac) ** 1.5
            s = math.sin(TWOPI * f * tt)
            j = start + i
            if j < n:
                buf[j] += 0.045 * env * s
        t += rng.uniform(2.5, 5.5)

    # Very faint crowd murmur: bandpassed noise + syllable-rate AM
    murmur = soft_noise(n, rng)
    murmur = biquad_bp(murmur, 450.0, 0.8)
    murmur = one_pole_lp(murmur, 1200.0)
    for i in range(n):
        tsec = i / SR
        # ~4–6 Hz syllable envelope, irregular
        am = 0.35 + 0.65 * abs(math.sin(TWOPI * 5.2 * tsec + 0.4 * math.sin(TWOPI * 0.7 * tsec)))
        buf[i] += 0.04 * am * murmur[i]

    buf = crossfade_loop(buf, 0.12)
    return normalize(buf, -6.0)


def gen_ui_move(rng: random.Random) -> list[float]:
    n = int(0.030 * SR)
    buf = [0.0] * n
    for i in range(n):
        t = i / SR
        env = math.exp(-t * 180.0)
        s = math.sin(TWOPI * 2400.0 * t) + 0.25 * rng.uniform(-1, 1)
        buf[i] = 0.5 * env * s
    return normalize(buf, -3.0)


def gen_ui_select() -> list[float]:
    n = int(0.120 * SR)
    buf = [0.0] * n
    # two-tone up blip
    parts = [(0.0, 880.0, 0.055), (0.05, 1320.0, 0.065)]
    for t0, f, d in parts:
        start = int(t0 * SR)
        length = int(d * SR)
        for i in range(length):
            t = i / SR
            env = min(1.0, t / 0.004) * math.exp(-t / 0.035)
            j = start + i
            if j < n:
                buf[j] += env * math.sin(TWOPI * f * t)
    return normalize(buf, -3.0)


def gen_ui_back() -> list[float]:
    n = int(0.100 * SR)
    buf = [0.0] * n
    parts = [(0.0, 990.0, 0.045), (0.04, 660.0, 0.055)]
    for t0, f, d in parts:
        start = int(t0 * SR)
        length = int(d * SR)
        for i in range(length):
            t = i / SR
            env = min(1.0, t / 0.003) * math.exp(-t / 0.03)
            j = start + i
            if j < n:
                buf[j] += env * math.sin(TWOPI * f * t)
    return normalize(buf, -3.0)


def gen_notify() -> list[float]:
    """EVAL app notification: glassy two-note chime, 400 ms."""
    n = int(0.400 * SR)
    buf = [0.0] * n
    # glassy: inharmonic partials
    notes = [
        (0.00, 1046.5, [(1.0, 1.0), (2.76, 0.35), (5.4, 0.12)]),
        (0.12, 1568.0, [(1.0, 1.0), (2.76, 0.3), (5.4, 0.1)]),
    ]
    for t0, f0, partials in notes:
        start = int(t0 * SR)
        length = n - start
        for i in range(length):
            t = i / SR
            env = min(1.0, t / 0.008) * math.exp(-t / 0.14)
            s = 0.0
            for mul, g in partials:
                s += g * math.sin(TWOPI * f0 * mul * t)
            buf[start + i] += 0.55 * env * s
    return normalize(buf, -3.0)


def gen_hesitation() -> list[float]:
    n = int(0.600 * SR)
    buf = [0.0] * n
    # low unsettling sting: dissonant cluster + slow swell/decay
    freqs = [110.0, 116.5, 165.0, 233.0]
    for i in range(n):
        t = i / SR
        # swell then unease
        env = math.sin(math.pi * t / 0.6) ** 1.2
        s = 0.0
        for k, f in enumerate(freqs):
            s += (0.4 / (k + 1)) * math.sin(TWOPI * f * t)
        # slow AM tremor
        s *= 0.7 + 0.3 * math.sin(TWOPI * 7.0 * t)
        buf[i] = env * s
    buf = one_pole_lp(buf, 900.0)
    return normalize(buf, -3.0)


def gen_choice_confirm(rng: random.Random) -> list[float]:
    n = int(0.200 * SR)
    buf = [0.0] * n
    # soft thump
    thump_n = int(0.08 * SR)
    for i in range(thump_n):
        t = i / SR
        env = math.exp(-t * 55.0)
        s = math.sin(TWOPI * 90.0 * t) + 0.4 * math.sin(TWOPI * 55.0 * t)
        buf[i] += 0.7 * env * s
    # click
    click_start = int(0.05 * SR)
    click_n = int(0.02 * SR)
    for i in range(click_n):
        t = i / SR
        env = math.exp(-t * 220.0)
        s = math.sin(TWOPI * 2100.0 * t) + 0.4 * rng.uniform(-1, 1)
        j = click_start + i
        if j < n:
            buf[j] += 0.35 * env * s
    buf = one_pole_lp(buf, 6000.0)
    return normalize(buf, -3.0)


def gen_footstep(rng: random.Random, seed_extra: int) -> list[float]:
    local = random.Random(rng.randint(0, 1_000_000) + seed_extra * 97)
    n = int(0.080 * SR)
    noise = soft_noise(n, local)
    # slight spectral variation
    hp = 400.0 + local.uniform(-80, 120)
    lp = 2800.0 + local.uniform(-400, 500)
    noise = one_pole_hp(noise, hp)
    noise = one_pole_lp(noise, lp)
    buf = [0.0] * n
    decay = 70.0 + local.uniform(-15, 20)
    for i in range(n):
        t = i / SR
        env = math.exp(-t * decay)
        # tiny body thud under noise
        body = 0.25 * math.sin(TWOPI * (140.0 + local.uniform(-20, 20)) * t) * math.exp(-t * 90.0)
        buf[i] = env * (0.85 * noise[i] + body)
    return normalize(buf, -3.0)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    rng = random.Random(20261113)

    jobs = [
        ("music_menu.wav", lambda: gen_music_menu(rng)),
        ("music_hub.wav", lambda: gen_music_hub(rng)),
        ("music_tension.wav", lambda: gen_music_tension(rng)),
        ("amb_campus.wav", lambda: gen_amb_campus(rng)),
        ("ui_move.wav", lambda: gen_ui_move(rng)),
        ("ui_select.wav", gen_ui_select),
        ("ui_back.wav", gen_ui_back),
        ("notify.wav", gen_notify),
        ("hesitation.wav", gen_hesitation),
        ("choice_confirm.wav", lambda: gen_choice_confirm(rng)),
        ("footstep_1.wav", lambda: gen_footstep(rng, 1)),
        ("footstep_2.wav", lambda: gen_footstep(rng, 2)),
        ("footstep_3.wav", lambda: gen_footstep(rng, 3)),
        ("footstep_4.wav", lambda: gen_footstep(rng, 4)),
    ]

    for name, fn in jobs:
        samples = fn()
        write_wav(os.path.join(OUT_DIR, name), samples)


if __name__ == "__main__":
    main()
