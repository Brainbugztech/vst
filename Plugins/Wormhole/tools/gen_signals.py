import numpy as np, scipy.io.wavfile as wf, os
SR = 44100
rng = np.random.default_rng(1)
os.makedirs('sig', exist_ok=True)

def save(name, x):
    x = np.asarray(x, np.float32)
    if x.ndim == 1: x = np.stack([x, x], 1)
    x = x / max(1e-9, np.abs(x).max()) * 0.8
    wf.write(f'sig/{name}.wav', SR, x.astype(np.float32))
    return x

def env(n, a, d):
    t = np.arange(n) / SR
    return np.minimum(1, t / max(a, 1e-4)) * np.exp(-t / d)

# --- sines: steady partials for pitch and purity
T = 6; t = np.arange(int(T * SR)) / SR
sine = 0.5 * np.sin(2*np.pi*440*t) + 0.3 * np.sin(2*np.pi*1000*t) + 0.2*np.sin(2*np.pi*110*t)
save('sines', sine)

# --- steady harmonic tone (purity / phasiness)
f0 = 196.0
tone = sum((1/k) * np.sin(2*np.pi*f0*k*t + rng.uniform(0, 6.28)) for k in range(1, 25) if f0*k < 15000)
save('tone', tone)

# --- drums 120 bpm, 8 bars of 16ths
bpm = 120; beat = 60 / bpm; L = int(8 * beat * SR)
dr = np.zeros((L, 2))
def add(pos, sig, pan=0.0):
    i = int(pos * SR); n = min(len(sig), L - i)
    if n <= 0: return
    dr[i:i+n, 0] += sig[:n] * (1 - pan) ; dr[i:i+n, 1] += sig[:n] * (1 + pan)
kn = int(0.4*SR); tk = np.arange(kn)/SR
kick = np.sin(2*np.pi*np.cumsum(45 + 90*np.exp(-tk/0.03))/SR) * env(kn, 0.001, 0.15)
sn = int(0.25*SR)
snare = (rng.standard_normal(sn) * env(sn, 0.0005, 0.06) + 0.6*np.sin(2*np.pi*185*np.arange(sn)/SR)*env(sn, 0.0005, 0.05))
hn = int(0.05*SR)
hat = np.diff(rng.standard_normal(hn+1)) * env(hn, 0.0002, 0.012) * 0.5
onsets = []
for b in range(16):
    add(b*beat, kick); onsets.append(b*beat)
    if b % 2 == 1: add(b*beat, snare*0.8)
    for s in range(4):
        add(b*beat + s*beat/4, hat, pan=0.5 if s % 2 else -0.5)
drums = save('drums', dr)
np.save('sig/drum_onsets.npy', np.array(onsets))

# --- piano-ish chords
ch = np.zeros(int(8*SR))
chords = [[261.6, 329.6, 392.0], [220.0, 261.6, 329.6], [174.6, 220.0, 261.6], [196.0, 246.9, 293.7]]
for i, c in enumerate(chords * 2):
    n = int(2.0*SR); st = int(i*1.0*SR)
    seg = np.zeros(n); tt = np.arange(n)/SR
    for f in c:
        for k in range(1, 12):
            seg += (0.6**k) * np.sin(2*np.pi*f*k*tt*(1+0.0004*k*k)) * np.exp(-tt*(0.8+0.5*k))
    seg *= np.minimum(1, tt/0.003)
    e = min(len(ch), st+n); ch[st:e] += seg[:e-st]
save('chords', ch)

# --- synthetic voice: glottal pulses + formants, vibrato, vowel changes
from scipy.signal import lfilter
Tv = 6; tv = np.arange(int(Tv*SR))/SR
f0v = 150 + 40*np.sin(2*np.pi*0.25*tv) + 4*np.sin(2*np.pi*5.5*tv)
ph = np.cumsum(f0v/SR)
glott = np.zeros_like(tv)
idx = np.where(np.diff(np.floor(ph)) > 0)[0]
glott[idx] = 1.0
glott = lfilter([1], [1, -0.97], glott)  # spectral tilt
glott += 0.02 * rng.standard_normal(len(tv))
vowels = {'a': [(730, 90), (1090, 110), (2440, 170)], 'i': [(270, 60), (2290, 120), (3010, 170)],
          'u': [(300, 60), (870, 90), (2240, 150)], 'e': [(530, 70), (1840, 120), (2480, 160)]}
seq = ['a', 'i', 'u', 'e', 'a', 'i']
voice = np.zeros_like(tv)
for i, v in enumerate(seq):
    a, b = int(i*SR), int((i+1)*SR)
    seg = glott[a:b].copy(); y = np.zeros_like(seg)
    for fc, bw in vowels[v]:
        r = np.exp(-np.pi*bw/SR); th = 2*np.pi*fc/SR
        y += lfilter([1-r], [1, -2*r*np.cos(th), r*r], seg)
    fade = np.minimum(1, np.minimum(np.arange(len(y)), len(y)-1-np.arange(len(y))) / 800)
    voice[a:b] = y * fade
save('voice', voice)
np.save('sig/voice_f0.npy', f0v)

# --- full mix (stereo)
n = min(len(drums), len(ch))
mix = drums[:n]*0.7 + np.stack([ch[:n], ch[:n]*0.8], 1)*0.6
vv = np.zeros(n); vv[:min(n, len(voice))] = voice[:n]
mix[:, 0] += vv*0.6; mix[:, 1] += vv*0.6
save('mix', mix)
print('signals written')
