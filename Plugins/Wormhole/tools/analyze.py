import numpy as np, scipy.io.wavfile as wf, subprocess, sys, os
from scipy.signal import stft
SR = 44100
OURS = './cli'; REF = '/home/claude/ref/refcli'
os.makedirs('out', exist_ok=True)

def run(exe, sig, ratio, semis, formant=0, rt=0, tag=''):
    o = f'out/{tag}_{sig}_{ratio}_{semis}_{formant}{"_rt" if rt else ""}.wav'
    args = [exe, f'sig/{sig}.wav', o, str(ratio), str(semis), str(formant)]
    if rt: args.append('1')
    subprocess.run(args, check=True, capture_output=True)
    return wf.read(o)[1].astype(np.float64)

def mono(x): return x.mean(1) if x.ndim > 1 else x

def peak_freq(x, f_guess, sr=SR):
    x = mono(x); n = len(x); seg = x[n//4: 3*n//4] * np.hanning(n//2)
    X = np.abs(np.fft.rfft(seg, 1 << 20)); f = np.fft.rfftfreq(1 << 20, 1/sr)
    m = (f > f_guess*0.8) & (f < f_guess*1.2)
    i = np.argmax(X * m)
    return f[i]

def purity(x, f0, nharm, sr=SR):
    # energy at expected harmonics (+-6 Hz) vs total, steady middle segment, in dB
    x = mono(x); n = len(x); seg = x[n//4: 3*n//4] * np.hanning(n//2)
    X = np.abs(np.fft.rfft(seg))**2; f = np.fft.rfftfreq(len(seg), 1/sr)
    mask = np.zeros_like(f, bool)
    for k in range(1, nharm+1):
        if f0*k < 15000: mask |= np.abs(f - f0*k) < 6
    sig = X[mask].sum(); noise = X[~mask & (f > 40) & (f < 15000)].sum()
    return 10*np.log10(sig / max(noise, 1e-20))

def partial_wobble(x, f0, nharm=10, sr=SR):
    # amplitude fluctuation (phasiness/roughness) of partials over time, in dB std
    x = mono(x)
    f, t, Z = stft(x, sr, nperseg=4096, noverlap=3584)
    A = np.abs(Z); n = A.shape[1]; A = A[:, n//5: 4*n//5]
    vals = []
    for k in range(1, nharm+1):
        b = np.argmin(np.abs(f - f0*k))
        e = A[b-1:b+2].max(0)
        vals.append(np.std(20*np.log10(e + 1e-9)))
    return float(np.mean(vals))

_ORIG = None
def attack_lag(y, c, o, sr=SR):
    global _ORIG
    if _ORIG is None: _ORIG = mono(wf.read('sig/drums.wav')[1].astype(float))
    hp = lambda v: np.convolve(np.diff(v)**2, np.ones(32)/32, 'same')
    a = hp(_ORIG[int(o*sr) - 441: int(o*sr) + 1323])
    best, bl = -1e30, 0
    for lag in range(-441, 442, 4):
        s0 = c + lag - 441
        b = hp(y[s0: s0 + 1764])
        if len(b) != len(a): continue
        v = np.dot(a, b) / (np.linalg.norm(b) + 1e-12)
        if v > best: best, bl = v, lag
    return bl / sr

def transient_score(x, onsets, ratio, sr=SR):
    # pre-echo: energy 30..3 ms before expected onset vs 0..30 ms after (dB, lower=better)
    # sharpness: max of 1 ms RMS rise within +-5 ms
    x = mono(x); res_pre = []; res_time = []
    for o in onsets[1:-1]:
        c = int(o * ratio * sr)
        pre = x[c - int(0.030*sr): c - int(0.003*sr)]; post = x[c: c + int(0.030*sr)]
        if len(post) < 10: continue
        res_pre.append(10*np.log10((pre**2).mean() / max((post**2).mean(), 1e-20)))
        # timing: lag of best match between original attack (high-passed envelope) and output
        res_time.append(attack_lag(x, c, o) * 1000)
    return float(np.mean(res_pre)), float(np.mean(np.abs(res_time)))

def cep_env(x, sr=SR, lifter=50):
    x = mono(x)
    f, t, Z = stft(x, sr, nperseg=2048, noverlap=1536)
    L = np.log(np.abs(Z) + 1e-7)
    C = np.fft.irfft(L, axis=0); C[lifter:-lifter] = 0
    E = np.fft.rfft(C, axis=0).real
    E = E[: L.shape[0]]
    return 20/np.log(10) * E, np.abs(Z)

def formant_dist(orig, proc, sr=SR):
    # mean abs difference of spectral envelope, 200..5000 Hz, voiced frames (dB)
    n = min(len(orig), len(proc))
    Eo, Ao = cep_env(orig[:n]); Ep, Ap = cep_env(proc[:n])
    f = np.fft.rfftfreq(2048, 1/sr); m = (f > 200) & (f < 5000)
    energy = (Ao**2).sum(0); fr = energy > energy.max() * 0.01
    D = np.abs((Eo - Eo[m].mean(0)) - (Ep - Ep[m].mean(0)))[m][:, fr]
    return float(D.mean())

def stereo_corr(x):
    return float(np.corrcoef(x[:, 0], x[:, 1])[0, 1])

def report():
    onsets = np.load('sig/drum_onsets.npy')
    orig_voice = wf.read('sig/voice.wav')[1].astype(np.float64)
    rows = []
    for name, exe in [('Wormhole', OURS), ('Referenz', REF)]:
        r = {}
        # pitch accuracy
        y = run(exe, 'sines', 1.0, 3, tag=name)
        r['Tonhoehe +3 HT Fehler (cent)'] = 1200*np.log2(peak_freq(y, 440*2**(3/12)) / (440*2**(3/12)))
        y = run(exe, 'sines', 1.3, -7, tag=name)
        r['Tonhoehe -7 HT Fehler (cent)'] = 1200*np.log2(peak_freq(y, 440*2**(-7/12)) / (440*2**(-7/12)))
        r['Laenge 1.3x Fehler (Samples)'] = len(y) - round(len(wf.read('sig/sines.wav')[1]) * 1.3)
        # purity & wobble
        for ratio, semis in [(1.0, 4), (1.5, 0), (0.75, 0), (1.25, -5)]:
            y = run(exe, 'tone', ratio, semis, tag=name)
            f0 = 196*2**(semis/12)
            r[f'Reinheit T{ratio} P{semis:+d} (dB)'] = purity(y, f0, 24)
            r[f'Schwankung T{ratio} P{semis:+d} (dB)'] = partial_wobble(y, f0)
        # transients
        for ratio in [1.5, 0.8]:
            y = run(exe, 'drums', ratio, 0, tag=name)
            pre, tim = transient_score(y, onsets, ratio)
            r[f'Vorecho Drums T{ratio} (dB)'] = pre
            r[f'Timing Drums T{ratio} (ms)'] = tim
        y = run(exe, 'drums', 1.0, 4, tag=name)
        pre, tim = transient_score(y, onsets, 1.0)
        r['Vorecho Drums P+4 (dB)'] = pre
        # formant preservation
        y = run(exe, 'voice', 1.0, 5, 1, tag=name)
        r['Formant-Abweichung P+5 (dB)'] = formant_dist(orig_voice, y)
        y = run(exe, 'voice', 1.0, -5, 1, tag=name)
        r['Formant-Abweichung P-5 (dB)'] = formant_dist(orig_voice, y)
        # stereo
        y = run(exe, 'drums', 1.3, 2, tag=name)
        r['Stereo-Korrelation (orig %.2f)' % stereo_corr(wf.read('sig/drums.wav')[1].astype(float))] = stereo_corr(y)
        rows.append((name, r))
    keys = list(rows[0][1].keys())
    print(f'{"Messung":40s} {"Wormhole":>10s} {"Referenz":>10s}')
    for k in keys:
        print(f'{k:40s} {rows[0][1][k]:10.2f} {rows[1][1][k]:10.2f}')

if __name__ == '__main__':
    report()

def tonality(x, sr=SR):
    # spectral peakiness of a noise signal: mean over frames of log(max/median) in 1-8 kHz (dB)
    x = mono(x); f, t, Z = stft(x, sr, nperseg=2048, noverlap=1536)
    A = np.abs(Z)[(f > 1000) & (f < 8000)]
    return float(np.mean(20*np.log10(A.max(0) / np.median(A, 0))))

def roughness(x, sr=SR):
    # temporal modulation of noise band energy (1-8 kHz), std of frame energy in dB
    x = mono(x); f, t, Z = stft(x, sr, nperseg=512, noverlap=384)
    e = (np.abs(Z)[(f > 1000) & (f < 8000)]**2).sum(0); e = e[len(e)//5: 4*len(e)//5]
    return float(np.std(10*np.log10(e)))
