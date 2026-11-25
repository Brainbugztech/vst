# Bereitet VSCO-2-CE-Samples fuer Brainbugz Orchestra auf: trimmen, Loops einbacken, OGG, Manifest
import sys, os, re, glob, subprocess, json
import numpy as np, soundfile as sf

V = sys.argv[1]; OUT = sys.argv[2]; HDR = sys.argv[3]
os.makedirs(OUT, exist_ok=True)
SR = 44100
PCS = {'C':0,'C#':1,'Db':1,'D':2,'D#':3,'Eb':3,'E':4,'F':5,'F#':6,'Gb':6,'G':7,'G#':8,'Ab':8,'A':9,'A#':10,'Bb':10,'B':11}
DYN = ['pppp','ppp','pp','p','mp','mf','f','ff','fff']

def note_of(name):
    m = re.search(r'_([A-G][#b]?)(-?\d)_', name)
    return (int(m.group(2)) + 2) * 12 + PCS[m.group(1)]

def load(path):
    x, sr = sf.read(path, always_2d=True)
    assert sr == SR, (path, sr)
    if x.shape[1] == 1: x = np.repeat(x, 2, 1)
    return x[:, :2].astype(np.float64)

def trim_start(x, thr_db=-50):
    env = np.abs(x).max(1)
    thr = env.max() * 10 ** (thr_db / 20)
    idx = np.argmax(env > thr)
    return x[max(0, idx - int(0.004 * SR)):]

def fade_out(x, sec):
    n = min(len(x), int(sec * SR))
    x[-n:] *= np.linspace(1, 0, n)[:, None] ** 2
    return x

def bake_loop(x, a_sec=1.4, b_max=4.6, xf=0.45):
    # Loop [a,b): Ende in Anfang ueberblenden, damit Sprung b->a nahtlos ist
    n = len(x); a = int(a_sec * SR); b = min(int(b_max * SR), n - int(0.25 * SR))
    if b - a < int(1.2 * SR):
        a = int(n * 0.3); b = int(n * 0.85)
    X = min(int(xf * SR), a - 1, (b - a) // 2)
    t = np.linspace(0, 1, X)[:, None]
    fo, fi = np.cos(t * np.pi / 2), np.sin(t * np.pi / 2)
    y = x[:b].copy()
    y[b - X:b] = x[b - X:b] * fo + x[a - X:a] * fi
    return y, a, b

entries = []
def emit(group, root, layer, rr, y, loop=None):
    name = f"{group}_{root}_{layer}_{rr}"
    tmp = f"/tmp/{name}.wav"
    sf.write(tmp, np.clip(y, -1, 1).astype(np.float32), SR, subtype='PCM_16')
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', tmp, '-c:a', 'libvorbis', '-q:a', '4', f"{OUT}/{name}.ogg"], check=True)
    os.remove(tmp)
    entries.append(dict(name=name, group=group, root=root, layer=layer, rr=rr,
                        ls=loop[0] if loop else -1, le=loop[1] if loop else -1, len=len(y)))

def process_group(group, files, kind, rootfn, layerfn, rrfn, maxlen=3.0, gain_norm=True):
    data = []
    for f in files:
        x = trim_start(load(f))
        data.append((f, x))
    layers = sorted(set(layerfn(os.path.basename(f)) for f, _ in data))
    # Pegel pro Grundton angleichen (oberster Layer = Referenz), Dynamik zwischen Layern bleibt
    byroot = {}
    for f, x in data:
        b = os.path.basename(f)
        byroot.setdefault(rootfn(b), []).append((layers.index(layerfn(b)), x))
    gains = {}
    for root, lst in byroot.items():
        top = max(l for l, _ in lst)
        refs = [x for l, x in lst if l == top]
        if kind == 'loop':
            rms = np.median([np.sqrt((x[int(SR*0.5):int(SR*3.5)]**2).mean()) for x in refs])
            gk = 10 ** (-17 / 20) / max(rms, 1e-6)
        else:
            pk = np.median([np.abs(x[:int(SR*1.5)]).max() for x in refs])
            gk = 10 ** (-2 / 20) / max(pk, 1e-6)
        allpk = max(np.abs(x).max() for _, x in lst)
        gains[root] = min(gk, 0.97 / allpk, 4.0)
    for f, x in data:
        b = os.path.basename(f); x = x * gains[rootfn(b)]
        root = rootfn(b); layer = layers.index(layerfn(b)); rr = rrfn(b)
        if kind == 'loop':
            y, a, e = bake_loop(x)
            emit(group, root, layer, rr, y, (a, e))
        else:
            y = x[:int(maxlen * SR)].copy()
            fade_out(y, min(0.25, maxlen * 0.2))
            emit(group, root, layer, rr, y)

def vnum(b):
    m = re.search(r'_v(\d+)', b); return int(m.group(1)) if m else 1
def rrnum(b):
    m = re.search(r'_(?:rr|RR)(\d+)', b); return int(m.group(1)) - 1 if m else 0
def dyn(b):
    m = re.findall(r'_(pppp|ppp|pp|p|mp|mf|f|ff|fff)(?:_|\.|$)', b)
    return DYN.index(m[0]) if m else 4
def numrr(b):
    m = re.search(r'_(\d+)\.wav$', b); return int(m.group(1)) - 1 if m else 0

S = V + '/Strings/'
G = lambda p: sorted(glob.glob(p))
# Streicher: Sustain (geloopt), Spiccato, Pizzicato, Tremolo (geloopt)
process_group('VLN_SUS',  G(S+'Violin Section/susVib/*.wav'), 'loop', note_of, vnum, lambda b: 0)
process_group('VLN_SPIC', G(S+'Violin Section/Spic/*.wav'), 'short', note_of, vnum, rrnum, 1.6)
process_group('VLN_PIZZ', G(S+'Violin Section/Pizz/*.wav'), 'short', note_of, vnum, rrnum, 2.0)
process_group('VLN_TREM', G(S+'Violin Section/Trem/*.wav'), 'loop', note_of, vnum, lambda b: 0)
process_group('VLA_SUS',  [f for f in G(S+'Viola Section/susvib/*.wav') if f.endswith('_1.wav')], 'loop', note_of, vnum, lambda b: 0)
process_group('VLA_SPIC', G(S+'Viola Section/spic/*.wav'), 'short', note_of, vnum, rrnum, 1.6)
process_group('VLA_PIZZ', G(S+'Viola Section/pizz/*.wav'), 'short', note_of, vnum, rrnum, 2.0)
process_group('VLA_TREM', [f for f in G(S+'Viola Section/trem/*.wav') if not re.search(r'_[2-9]\.wav$', f)], 'loop', note_of, vnum, lambda b: 0)
process_group('VC_SUS',   [f for f in G(S+'Cello Section/susvib/*.wav') if f.endswith('_1.wav')], 'loop', note_of, vnum, lambda b: 0)
process_group('VC_SPIC',  G(S+'Cello Section/spic/*.wav'), 'short', note_of, vnum, rrnum, 1.6)
process_group('VC_PIZZ',  G(S+'Cello Section/pizzT/*.wav'), 'short', note_of, vnum, rrnum, 2.2)
process_group('VC_TREM',  [f for f in G(S+'Cello Section/trem/*.wav') if f.endswith('_1.wav')], 'loop', note_of, vnum, lambda b: 0)

# Drums: eine Taste pro Instrument
P = V + '/Percussion/'; P1 = V + '/VSCO 1 Percussion/drums/'
kit = [
 (36, 'Bass Drum',     G(P+'BDrumNewhit_*.wav'), vnum, rrnum, 4.0),
 (37, 'Taiko Hand',    G(P1+'other/ethnic/giant/hand/EthnicLargeHand_hit_*.wav'), dyn, numrr, 3.5),
 (38, 'Snare',         G(P+'Snare2-HitSN_*.wav'), vnum, rrnum, 2.0),
 (39, 'Taiko Sticks',  G(P1+'other/ethnic/giant/sticks/*.wav'), dyn, numrr, 3.5),
 (40, 'Snare Roll',    G(P+'Snare2-rollSN_*.wav'), vnum, rrnum, 6.0),
 (41, 'Timpani 1',     G(P+'Timpani/Timpani1_Hit_*.wav'), vnum, rrnum, 4.5),
 (42, 'Taiko Mallet',  G(P1+'other/ethnic/giant/mallet/*.wav'), dyn, numrr, 4.0),
 (43, 'Timpani 2',     G(P+'Timpani/Timpani2_Hit_*.wav'), vnum, rrnum, 4.5),
 (44, 'Tenor Low',     G(P1+'tenor/tenor_lower/*.wav'), dyn, numrr, 2.5),
 (45, 'Timpani 3',     G(P+'Timpani/Timpani3_Hit_*.wav'), vnum, rrnum, 4.5),
 (46, 'Tenor High',    G(P1+'tenor/tenor_higher/*.wav'), dyn, numrr, 2.5),
 (47, 'Timpani 4',     G(P+'Timpani/Timpani4_Hit_*.wav'), vnum, rrnum, 4.5),
 (48, 'Timpani 5',     G(P+'Timpani/Timpani5_Hit_*.wav'), vnum, rrnum, 4.5),
 (49, 'Crash',         G(P+'cymbal-crash1_*.wav'), dyn, rrnum, 6.0),
 (50, 'Timpani Roll',  G(P+'Timpani/Rolls/Timpani3_Roll_*.wav'), vnum, rrnum, 8.0),
 (51, 'Cymbal Hit',    [f for f in G(P+'susCymb1-hit_*.wav')], dyn, rrnum, 6.0),
 (52, 'Cymbal Swell',  G(P+'susCymb1-cresc-Median_v1.wav'), lambda b: 0, lambda b: 0, 12.0),
 (53, 'Gong',          [f for f in G(P+'gongHit_*.wav')], dyn, lambda b: 0, 9.0),
 (54, 'Tambourine',    G(P+'Tamb1-Hit_*.wav'), vnum, rrnum, 1.5),
 (55, 'Big Boom',      G(P1+'bass/bdrum_f*.wav') + G(P1+'bass/bdrum_mp_*.wav') + G(P1+'bass/bdrum_p_*.wav'), dyn, numrr, 5.0),
 (56, 'Anvil',         G(P+'Anvil_Hit1_*.wav'), vnum, lambda b: 0, 2.5),
 (57, 'Taiko Rim',     G(P1+'other/ethnic/giant/EthnicLargeStick_rimshot_*.wav'), lambda b: 0, numrr, 2.0),
 (58, 'Triangle',      G(P+'Triangle6-Hit_*.wav'), vnum, rrnum, 4.0),
 (59, 'Log Drum',      G(P+'LogDrumLo_*.wav'), vnum, rrnum, 2.0),
 (60, 'Bass Drum Roll',G(P1+'bass/bdrum_roll1.wav'), lambda b: 0, lambda b: 0, 8.0),
 (61, 'Brake Drum',    G(P+'BrakeDrum1_Hammer_*.wav'), vnum, lambda b: 0, 2.5),
 (62, 'Gong Scrape',   G(P+'gongscrape_mf.wav'), lambda b: 0, lambda b: 0, 8.0),
 (63, 'Timpani Roll 2',G(P+'Timpani/Rolls/Timpani1_Roll_*.wav'), vnum, rrnum, 8.0),
]
kitnames = []
for note, nm, files, lf, rf, ml in kit:
    assert files, nm
    # Speicher sparen: max. 4 Dynamikstufen x 2 Round-Robins pro Instrument
    lv = sorted(set(lf(os.path.basename(f)) for f in files))
    if len(lv) > 4: lv = [lv[round(i * (len(lv) - 1) / 3)] for i in range(4)]
    files = [f for f in files if lf(os.path.basename(f)) in lv and rf(os.path.basename(f)) < 2]
    ml = min(ml, 6.0 if ml > 6 else ml * 0.8)
    process_group(f'DRUM{note}', files, 'short', lambda b, n=note: n, lf, rf, ml)
    kitnames.append((note, nm))

groups = sorted(set(e['group'] for e in entries))
with open(HDR, 'w') as h:
    h.write('// Automatisch erzeugt von prep.py (VSCO 2 CE, CC0)\n#pragma once\n\nnamespace orch\n{\n')
    h.write('enum Group\n{\n' + ''.join(f'    G_{g},\n' for g in groups) + '    G_NUM\n};\n\n')
    h.write('struct SampleDef { const char* res; int group, root, layer, rr, loopStart, loopEnd, length; };\n\n')
    h.write('inline const SampleDef sampleDefs[] =\n{\n')
    for e in entries:
        res = e['name'].replace('-', '_') + '_ogg'
        h.write(f"    {{ \"{res}\", G_{e['group']}, {e['root']}, {e['layer']}, {e['rr']}, {e['ls']}, {e['le']}, {e['len']} }},\n")
    h.write('};\n\nstruct KitPiece { int note; const char* name; };\ninline const KitPiece kitPieces[] =\n{\n')
    for n, nm in kitnames: h.write(f'    {{ {n}, "{nm}" }},\n')
    h.write('};\n} // namespace orch\n')
tot = sum(e['len'] for e in entries)
print(len(entries), 'samples', round(tot / SR / 60, 1), 'min')
