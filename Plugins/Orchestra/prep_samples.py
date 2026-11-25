# Bereitet VSCO-2-CE-Samples fuer Brainbugz Orchestra v2 auf:
# trimmen, Pegel pro Grundton angleichen, Loops einbacken, OGG, Manifest (C++-Header)
import sys, os, re, glob, subprocess
import numpy as np, soundfile as sf

V = sys.argv[1]; SFZ = sys.argv[2]; OUT = sys.argv[3]; HDR = sys.argv[4]
os.makedirs(OUT, exist_ok=True)
SR = 44100
PCS = {'C':0,'C#':1,'Db':1,'D':2,'D#':3,'Eb':3,'E':4,'F':5,'F#':6,'Gb':6,'G':7,'G#':8,'Ab':8,'A':9,'A#':10,'Bb':10,'B':11}
DYN = ['pppp','ppp','pp','p','mp','mf','f','ff','fff']

def G(*pats):
    out = []
    for p in pats: out += sorted(glob.glob(os.path.join(V, p)))
    return out

def note_of(b):
    m = re.search(r'_([A-G][#b]?)(-?\d)_', b)
    return (int(m.group(2)) + 2) * 12 + PCS[m.group(1)]

def sfz_roots(fname):
    t = open(os.path.join(SFZ, fname), errors='ignore').read()
    d = {}
    for reg in re.split(r'<region>', t)[1:]:
        s = re.search(r'sample=(\S+)', reg); k = re.search(r'pitch_keycenter=(\d+)', reg)
        if s and k: d[s.group(1).split('\\')[-1]] = int(k.group(1))
    return d

def load(path):
    x, sr = sf.read(path, always_2d=True)
    if sr != SR:
        raise RuntimeError(path + ' sr ' + str(sr))
    if x.shape[1] == 1: x = np.repeat(x, 2, 1)
    return x[:, :2].astype(np.float64)

def trim_start(x, thr_db=-50):
    env = np.abs(x).max(1)
    thr = env.max() * 10 ** (thr_db / 20)
    idx = int(np.argmax(env > thr))
    return x[max(0, idx - int(0.004 * SR)):]

def fade_out(x, sec):
    n = min(len(x), int(sec * SR))
    x[-n:] *= np.linspace(1, 0, n)[:, None] ** 2
    return x

def bake_loop(x, a_sec=1.4, b_max=4.6, xf=0.45):
    n = len(x); a = int(a_sec * SR); b = min(int(b_max * SR), n - int(0.25 * SR))
    if b - a < int(1.2 * SR):
        a = int(n * 0.3); b = int(n * 0.85)
    X = min(int(xf * SR), a - 1, (b - a) // 2)
    t = np.linspace(0, 1, X)[:, None]
    fo, fi = np.cos(t * np.pi / 2), np.sin(t * np.pi / 2)
    y = x[:b].copy()
    y[b - X:b] = x[b - X:b] * fo + x[a - X:a] * fi
    return y, a, b

def layer_key(b):
    m = re.search(r'_v(\d+)', b)
    if m: return int(m.group(1))
    m = re.findall(r'(?:^|_)(pppp|ppp|pp|p|mp|mf|f|ff|fff)(?=_|\.|\d|$)', b)
    if m: return DYN.index(m[-1])
    return 0

entries = []
def emit(group, root, layer, rr, y, loop=None):
    name = f"{group}_{root}_{layer}_{rr}"
    tmp = f"/tmp/{name}.wav"
    sf.write(tmp, np.clip(y, -1, 1).astype(np.float32), SR, subtype='PCM_16')
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', tmp, '-c:a', 'libvorbis', '-q:a', '3', f"{OUT}/{name}.ogg"], check=True)
    os.remove(tmp)
    entries.append(dict(name=name, group=group, root=root, layer=layer, rr=rr,
                        ls=loop[0] if loop else -1, le=loop[1] if loop else -1, len=len(y)))

def process(group, files, kind, rootfn=note_of, maxlen=3.0, maxlayers=4, maxrr=2, target_db=None, maxgain=4.0):
    """kind: 'loop' (Sustain mit Loop) | 'short' (One-Shot)"""
    assert files, group
    # Layer/Round-Robin automatisch bestimmen
    keyed = {}
    for f in files:
        b = os.path.basename(f)
        keyed.setdefault((rootfn(b), layer_key(b)), []).append(f)
    allkeys = sorted(set(k for _, k in keyed))
    if len(allkeys) > maxlayers:
        allkeys = sorted(set(allkeys[round(i * (len(allkeys) - 1) / (maxlayers - 1))] for i in range(maxlayers))) if maxlayers > 1 else [allkeys[-1]]
    byroot = {}
    for (root, lk), fl in keyed.items():
        if lk not in allkeys: continue
        for rr, f in enumerate(fl[:maxrr]):
            byroot.setdefault(root, []).append((allkeys.index(lk), rr, trim_start(load(f))))
    for root, lst in byroot.items():
        top = max(l for l, _, _ in lst)
        refs = [x for l, _, x in lst if l == top]
        if kind == 'loop':
            rms = np.median([np.sqrt((x[int(SR*0.5):int(SR*3.5)]**2).mean()) for x in refs])
            gk = 10 ** ((target_db or -17) / 20) / max(rms, 1e-6)
        else:
            pk = np.median([np.abs(x[:int(SR*1.5)]).max() for x in refs])
            gk = 10 ** ((target_db or -2) / 20) / max(pk, 1e-6)
        gk = min(gk, 0.97 / max(np.abs(x).max() for _, _, x in lst), maxgain)
        # Layer neu durchnummerieren (pro Grundton lueckenlos)
        lays = sorted(set(l for l, _, _ in lst))
        for l, rr, x in lst:
            x = x * gk
            if kind == 'loop':
                y, a, e = bake_loop(x); emit(group, root, lays.index(l), rr, y, (a, e))
            else:
                y = x[:int(maxlen * SR)].copy(); fade_out(y, min(0.3, maxlen * 0.2)); emit(group, root, lays.index(l), rr, y)

S = 'Strings/'
# ---------------- Streicher (Ensemble) ----------------
process('VLN_SUS',  G(S+'Violin Section/susVib/*.wav'), 'loop')
process('VLN_SPIC', G(S+'Violin Section/Spic/*.wav'), 'short', maxlen=1.6)
process('VLN_PIZZ', G(S+'Violin Section/Pizz/*.wav'), 'short', maxlen=2.0)
process('VLN_TREM', G(S+'Violin Section/Trem/*.wav'), 'loop')
process('VLA_SUS',  G(S+'Viola Section/susvib/*_1.wav'), 'loop')
process('VLA_SPIC', G(S+'Viola Section/spic/*.wav'), 'short', maxlen=1.6)
process('VLA_PIZZ', G(S+'Viola Section/pizz/*.wav'), 'short', maxlen=2.0)
process('VLA_TREM', [f for f in G(S+'Viola Section/trem/*.wav') if not re.search(r'_[2-9]\.wav$', f)], 'loop')
process('VC_SUS',   G(S+'Cello Section/susvib/*_1.wav'), 'loop')
process('VC_SPIC',  G(S+'Cello Section/spic/*.wav'), 'short', maxlen=1.6)
process('VC_PIZZ',  G(S+'Cello Section/pizzT/*.wav'), 'short', maxlen=2.2)
process('VC_TREM',  G(S+'Cello Section/trem/*_1.wav'), 'loop')
process('CB_SUS',   G(S+'Solo Contrabass/SusVib/*_rr1.wav'), 'loop', maxlayers=2)
process('CB_SPIC',  G(S+'Solo Contrabass/Spic/*.wav'), 'short', maxlen=1.6, maxlayers=2)
process('CB_PIZZ',  G(S+'Solo Contrabass/Pizz/*.wav'), 'short', maxlen=2.4, maxlayers=2)
process('CB_TREM',  G(S+'Solo Contrabass/Trem/*.wav'), 'loop', maxlayers=2)

# ---------------- Pad-Quellen (Sustains, geloopt, 2 Layer) ----------------
process('HRN_SUS',  G('Brass/F Horn/sus/*_1.wav'), 'loop', maxlayers=2)
process('TBN_SUS',  G('Brass/Tenor Trombone/sus/*.wav'), 'loop', maxlayers=2, maxrr=1)
process('TUBA_SUS', G('Brass/Tuba/sus/*_rr1_Mid.wav'), 'loop', maxlayers=2)
process('TPT_SUS',  G('Brass/Trumpet/sus/*_rr1.wav'), 'loop', maxlayers=2)
process('FLT_SUS',  G('Woodwinds/Flute/susNV/*.wav'), 'loop', maxlayers=2, maxrr=1)
process('CLA_SUS',  G('Woodwinds/Clarinet/susLong/*_rr1_sum.wav'), 'loop', maxlayers=2)
process('OBO_SUS',  G('Woodwinds/Oboe/Sus/*.wav'), 'loop', maxlayers=2)
process('BSN_SUS',  G('Woodwinds/Bassoon/sus/*.wav'), 'loop', maxlayers=2, maxrr=1)
org = sfz_roots('OrganLoud.sfz')
process('ORG_SUS',  [f for f in G('Keys/Organ/Loud/*.wav') if os.path.basename(f) in org], 'loop',
        rootfn=lambda b: org[b], maxlayers=1)

# ---------------- Timpani chromatisch ----------------
tim = sfz_roots('Timpani.sfz'); tro = sfz_roots('TimpaniRolls.sfz')
process('TIMP_HIT',  [f for f in G('Percussion/Timpani/*.wav') if os.path.basename(f) in tim], 'short',
        rootfn=lambda b: tim[b], maxlen=4.0, maxlayers=3)
process('TIMP_ROLL', [f for f in G('Percussion/Timpani/Rolls/*.wav') if os.path.basename(f) in tro], 'short',
        rootfn=lambda b: tro[b], maxlen=6.0, maxlayers=2)

# ---------------- Drum-Kits ----------------
P = 'Percussion/'; P1 = 'VSCO 1 Percussion/'
kits = {
 0: [ # EPIC
  (36, 'Bass Drum', [P+'BDrumNewhit_*'], 4.0, 1), (37, 'Taiko Hand', [P1+'drums/other/ethnic/giant/hand/EthnicLargeHand_hit_*'], 3.5, 1),
  (38, 'Snare', [P+'Snare2-HitSN_*'], 2.0, 0), (39, 'Taiko Sticks', [P1+'drums/other/ethnic/giant/sticks/*'], 3.5, 1),
  (40, 'Snare Roll', [P+'Snare2-rollSN_*'], 5.0, 0), (41, 'Timpani 1', [P+'Timpani/Timpani1_Hit_*'], 4.0, 1),
  (42, 'Taiko Mallet', [P1+'drums/other/ethnic/giant/mallet/*'], 4.0, 1), (43, 'Timpani 2', [P+'Timpani/Timpani2_Hit_*'], 4.0, 1),
  (44, 'Tenor Low', [P1+'drums/tenor/tenor_lower/*'], 2.5, 1), (45, 'Timpani 3', [P+'Timpani/Timpani3_Hit_*'], 4.0, 1),
  (46, 'Tenor High', [P1+'drums/tenor/tenor_higher/*'], 2.5, 0), (47, 'Timpani 4', [P+'Timpani/Timpani4_Hit_*'], 4.0, 1),
  (48, 'Timpani 5', [P+'Timpani/Timpani5_Hit_*'], 4.0, 1), (49, 'Crash', [P+'cymbal-crash1_*'], 5.0, 0),
  (50, 'Timpani Roll', [P+'Timpani/Rolls/Timpani3_Roll_*'], 6.0, 1), (51, 'Cymbal Hit', [P+'susCymb1-hit_*'], 5.0, 0),
  (52, 'Cymbal Swell', [P+'susCymb1-cresc-Median_v1.wav'], 8.0, 0), (53, 'Gong', [P+'gongHit_*'], 6.0, 1),
  (54, 'Tambourine', [P+'Tamb1-Hit_*'], 1.5, 0), (55, 'Big Boom', [P1+'drums/bass/bdrum_f*', P1+'drums/bass/bdrum_mp_*', P1+'drums/bass/bdrum_p_*'], 5.0, 1),
  (56, 'Anvil', [P+'Anvil_Hit1_*'], 2.5, 0), (57, 'Taiko Rim', [P1+'drums/other/ethnic/giant/EthnicLargeStick_rimshot_*'], 2.0, 0),
  (58, 'Triangle', [P+'Triangle6-Hit_*'], 4.0, 0), (59, 'Log Drum', [P+'LogDrumLo_*'], 2.0, 0),
  (60, 'BD Roll', [P1+'drums/bass/bdrum_roll1.wav'], 7.0, 1), (61, 'Brake Drum', [P+'BrakeDrum1_Hammer_*'], 2.5, 0),
  (62, 'Gong Scrape', [P+'gongscrape_mf.wav'], 7.0, 0), (63, 'Timpani Roll 2', [P+'Timpani/Rolls/Timpani1_Roll_*'], 6.0, 1),
  (64, 'Clash Cymbals', [P1+'varMetal/Cymbals/clash/crash_hit_*loose*'], 5.0, 0), (65, 'Gong Roll', [P1+'varMetal/Gong/gong_roll_long_*'], 7.0, 1),
  (66, 'Cymbal Roll', [P1+'varMetal/Cymbals/susp/susp_hit_softmall_roll3_ff.wav', P1+'varMetal/Cymbals/susp/susp_hit_softmall_roll4_fff_long.wav'], 7.0, 0),
  (67, 'Timp Snare', [P+'timpsnare_*', P+'timp-snare_f.wav'], 2.5, 0),
  (68, 'March Snare', [P1+'drums/snare/drum3_marching/snare3_f*', P1+'drums/snare/drum3_marching/snare3_p*', P1+'drums/snare/drum3_marching/snare3_mp*'], 2.0, 0),
  (69, 'Muted BD', [P1+'drums/bass/bdrum_muted_*'], 2.5, 1), (70, 'Gong Smack', [P1+'varMetal/Gong/gong_smack_[0-9]*'], 4.0, 1),
  (71, 'Snare Rimshot', [P1+'drums/snare/drum1/snare1_rimshot*'], 2.0, 0),
 ],
 1: [ # PERCUSSION
  (36, 'Conga Hit', [P+'Conga-HitN_*'], 2.0, 0), (37, 'Conga Tap', [P+'Conga-Tap1_*'], 1.5, 0),
  (38, 'Quinto Hit', [P+'Quinto-HitN_*'], 2.0, 0), (39, 'Quinto Tap', [P+'Quinto-Tap1_*'], 1.5, 0),
  (40, 'Tumba Hit', [P+'Tumba-HitN_*'], 2.0, 0), (41, 'Tumba Tap', [P+'Tumba-Tap1_*'], 1.5, 0),
  (42, 'Bongo High', [P1+'drums/other/Bongos/HighBongo*'], 1.5, 0), (43, 'Bongo Low', [P1+'drums/other/Bongos/LowBongo*'], 1.5, 0),
  (44, 'Djembe High', [P1+'drums/other/ethnic/congo/muted/ethnicHigh_hit_*'], 1.8, 0), (45, 'Djembe Low', [P1+'drums/other/ethnic/congo/muted/ethnicLow_hit_*'], 1.8, 0),
  (46, 'Djembe Open', [P1+'drums/other/ethnic/congo/open/ethnicLowOpen_hit_*'], 2.0, 0), (47, 'Log Drum High', [P+'LogDrumHi_*'], 2.0, 0),
  (48, 'Log Drum Low', [P+'LogDrumLo_*'], 2.0, 0), (49, 'Slit Drum 1', [P1+'varWood/log_drum/slitdrum0_*'], 2.0, 0),
  (50, 'Slit Drum 2', [P1+'varWood/log_drum/slitdrum1_*'], 2.0, 0), (51, 'Slit Drum 3', [P1+'varWood/log_drum/slitdrum2_*'], 2.0, 0),
  (52, 'Slit Drum 4', [P1+'varWood/log_drum/slitdrum3_*'], 2.0, 0), (53, 'Claves', [P+'Claves1_Hit_*'], 1.2, 0),
  (54, 'Wood Click', [P1+'varWood/wood_click*'], 1.0, 0), (55, 'Cowbell', [P+'Cowbell1_Hit_*'], 1.5, 0),
  (56, 'Agogo', [P1+'varMetal/various/agogoBell*'], 1.8, 0), (57, 'Guiro Hit', [P+'Guiro-Hit_*'], 1.2, 0),
  (58, 'Guiro Fast', [P+'Guiro-Fast_*'], 2.0, 0), (59, 'Guiro Slow', [P+'Guiro-Slow_*', P+'Guiro-M_*'], 3.0, 0),
  (60, 'Tamb Hit', [P+'Tamb1-Hit_*'], 1.5, 0), (61, 'Tamb Shake', [P+'Tamb1-Shake_*'], 1.5, 0),
  (62, 'Tamb Roll', [P+'Tamb1-Roll_*'], 3.0, 0), (63, 'Maraca', [P1+'varWood/maraca[0-9]*'], 1.0, 0),
  (64, 'Cabasa', [P1+'varMetal/various/cabasa_*', P1+'varWood/cabasa_*'], 1.0, 0), (65, 'Shaker', [P1+"varWood/Camo's Shaker/*.wav"], 1.2, 0),
  (66, 'Slapstick', [P1+'varWood/slapstick[0-9]*'], 1.2, 0), (67, 'Vibraslap', [P1+'varWood/vibraslap*'], 2.0, 0),
  (68, 'Ratchet', [P+'Ratchet1-Crank_*'], 2.0, 0), (69, 'Sleigh Bells', [P+'Sleighbells_Hit_*'], 2.0, 0),
  (70, 'Triangle', [P+'Triangle3-Hit_*'], 3.5, 0), (71, 'Triangle Muted', [P1+'varMetal/triangle/1/triangle1_hit_mp_muted*'], 1.0, 0),
 ],
 2: [ # METAL & FX
  (36, 'Anvil', [P+'Anvil_Hit1_*'], 2.5, 0), (37, 'Brake Drum', [P1+'varMetal/various/brake_*'], 2.5, 0),
  (38, 'Gong Soft', [P+'gongHit_p.wav', P+'gongHit_mf.wav'], 6.0, 1), (39, 'Gong Epic', [P+'gongHit_f.wav', P+'gongHit_fff.wav'], 7.0, 1),
  (40, 'Gong Hard', [P1+'varMetal/Gong/gong_hit_ff*', P1+'varMetal/Gong/gong_hit_mp_hard*', P1+'varMetal/Gong/gong_hit_p_hard*'], 6.0, 1),
  (41, 'Gong Roll', [P1+'varMetal/Gong/gong_roll_medium_*'], 6.0, 1), (42, 'Crash Short', [P+'cymbal-crashshort_v1.wav'], 3.0, 0),
  (43, 'Clash Tight', [P1+'varMetal/Cymbals/clash/crash_hit_ff_*tight*'], 4.0, 0), (44, 'Cymbal Hard', [P1+'varMetal/Cymbals/susp/susp_hit_hardmall_*'], 5.0, 0),
  (45, 'Cymbal Wood', [P1+'varMetal/Cymbals/susp/susp_hit_woodmall_[fmp]*'], 4.0, 0), (46, 'Cymbal Metal', [P1+'varMetal/Cymbals/susp/susp_hit_metal_2_*'], 5.0, 0),
  (47, 'Cymbal Bell', [P+'susCymb1-hit-bell_*'], 4.0, 0), (48, 'Cymbal Stick', [P+'susCymb1-hitstick_*'], 4.0, 0),
  (49, 'Bowed Cymbal', [P+'susCymb1-bow-*'], 6.0, 0), (50, 'Swell Short', [P+'susCymb1-cresc-Short_v1.wav'], 5.0, 0),
  (51, 'Swell Long', [P+'susCymb1-cresc-Long_v1.wav'], 10.0, 0), (52, 'Cymbal Scrape', [P+'susCymb1-scrape*'], 5.0, 0),
  (53, 'Scrape Rapid', [P1+'varMetal/Cymbals/susp/susp_FX_scrape_rapid.wav'], 5.0, 0), (54, 'Gong Scrape', [P1+'varMetal/Gong/gong_scrape_*'], 6.0, 0),
  (55, 'Bell Tree', [P+'BellTree_Stroke*'], 4.0, 0), (56, 'Tree Scrape', [P1+'varMetal/various/bell_tree_*scrape*'], 4.0, 0),
  (57, 'Triangle Roll', [P+'Triangle6-Roll_*'], 5.0, 0), (58, 'Finger Cymbal', [P1+'varMetal/various/Fing_Cymb.wav'], 4.0, 0),
  (59, 'Vibra Ring', [P+'vibraring_*'], 4.0, 0), (60, 'Zap 1', [P+'zap1_v1.wav'], 3.0, 0),
  (61, 'Zap 2', [P+'zap2_v1.wav'], 3.0, 0), (62, 'Zap 3', [P+'zap3_v1.wav'], 3.0, 0), (63, 'Zap 4', [P+'zap4_v1.wav'], 3.0, 0),
  (64, 'Alien 1', [P+'alien1_*'], 4.0, 0), (65, 'Alien 2', [P+'alien2_f.wav'], 4.0, 0), (66, 'Alien 3', [P+'alien3_f.wav'], 4.0, 0),
  (67, 'Tenor Snare', [P+'timpsnaretenor_*'], 2.5, 0), (68, 'BD Friction', [P+'bassdrum_rub*'], 4.0, 1),
  (69, 'Cymbal FX Roll', [P1+'varMetal/Cymbals/susp/susp_hit_woodmall_roll_fx.wav'], 6.0, 0), (70, 'Gong Long Roll', [P1+'varMetal/Gong/gong_roll_long_ff.wav'], 8.0, 1),
  (71, 'Gong Smack Low', [P1+'varMetal/Gong/gong_smack_low_1.wav', P1+'varMetal/Gong/gong_smack_light1.wav'], 4.0, 1),
 ],
}
kitnames = {}
for kit, pieces in kits.items():
    for note, nm, pats, ml, low in pieces:
        files = G(*pats)
        if not files: print('FEHLT', kit, note, nm, pats); continue
        process(f'K{kit}_{note}', files, 'short', rootfn=lambda b, n=note: n, maxlen=ml, maxgain=30.0)
        kitnames[(kit, note)] = (nm, low)

groups = []
for e in entries:
    if e['group'] not in groups: groups.append(e['group'])
with open(HDR, 'w') as h:
    h.write('// Automatisch erzeugt von prep_samples.py (VSCO 2 CE, CC0)\n#pragma once\n\nnamespace orch\n{\n')
    h.write('enum Group\n{\n' + ''.join(f'    G_{g},\n' for g in groups) + '    G_NUM\n};\n\n')
    h.write('struct SampleDef { const char* res; int group, root, layer, rr, loopStart, loopEnd, length; };\n\n')
    h.write('inline const SampleDef sampleDefs[] =\n{\n')
    for e in entries:
        res = e['name'].replace('-', '_') + '_ogg'
        h.write(f"    {{ \"{res}\", G_{e['group']}, {e['root']}, {e['layer']}, {e['rr']}, {e['ls']}, {e['le']}, {e['len']} }},\n")
    h.write('};\n\n// Kit-Instrumente mit Sample-Gruppe (low = bekommt den Sub-Boom)\n')
    h.write('struct KitPiece { int kit, note; const char* name; int group; bool low; };\ninline const KitPiece kitPieces[] =\n{\n')
    for (k, n), (nm, low) in sorted(kitnames.items()):
        h.write(f'    {{ {k}, {n}, "{nm}", G_K{k}_{n}, {"true" if low else "false"} }},\n')
    h.write('};\n} // namespace orch\n')
tot = sum(e['len'] for e in entries)
print(len(entries), 'samples', round(tot / SR / 60, 1), 'min,', len(groups), 'groups')
