// Zugriff auf den gemeinsamen Cloud-Ordner "Brain Bugz Cloud".
// Google Drive für Desktop synct den Ordner lokal, die App arbeitet nur mit diesem lokalen Ordner.
// Alle Pfade vom Fenster sind relativ zum Cloud-Ordner und werden hier geprüft, damit nichts außerhalb angefasst wird.
const fs = require('fs');
const fsp = fs.promises;
const path = require('path');
const os = require('os');

const ORDNER_NAME = 'Brain Bugz Cloud';

// Gleiche Struktur wie im Setup-Skript CLOUD_EINRICHTEN.bat
const STRUKTUR = [
  '_Inbox (zum Einsortieren)',
  '01_Projekte', '01_Projekte/_Vorlagen', '01_Projekte/_Archiv',
  '02_Presets', '02_Presets/Ableton Racks', '02_Presets/Synth Presets', '02_Presets/Brain Bugz Plugin Presets', '02_Presets/Presetpacks',
  '03_Samples', '03_Samples/Drums/Kicks', '03_Samples/Drums/Snares und Claps', '03_Samples/Drums/HiHats und Cymbals',
  '03_Samples/Drums/Percussion', '03_Samples/Drums/Drum Loops', '03_Samples/Bass', '03_Samples/Melodie und Chords',
  '03_Samples/Vocals', '03_Samples/FX', '03_Samples/Eigene Aufnahmen', '03_Samples/Samplepacks',
  '04_Plugins', '04_Plugins/Brain Bugz Builds', '04_Plugins/Brain Bugz Builds/Brainbugz Haas', '04_Plugins/Brain Bugz Builds/Phazer',
  '04_Plugins/Brain Bugz Builds/Bleep Designer', '04_Plugins/Brain Bugz Builds/Bugz Align', '04_Plugins/Installer (Fremd-Plugins)',
  '05_Tracks', '05_Tracks/Fertig (Master)', '05_Tracks/Stems', '05_Tracks/Demos und Previews', '05_Tracks/Releases (Artwork, Infos)'
];
const VERSTECKT = new Set(['desktop.ini', 'thumbs.db', '.ds_store', 'icon\r']);

let root = '';
function setRoot(p) { root = p ? path.resolve(p) : ''; }
function getRoot() { return root; }

// Sucht "Meine Ablage\Brain Bugz Cloud" auf allen Laufwerken (Drive für Desktop hängt sich als eigenes Laufwerk ein).
function detect() {
  const kandidaten = [];
  if (process.platform === 'win32') {
    for (const l of 'CDEFGHIJKLMNOPQRSTUVWXYZ') for (const n of ['Meine Ablage', 'My Drive']) kandidaten.push(path.join(l + ':\\', n, ORDNER_NAME));
  }
  const home = os.homedir();
  for (const n of ['Google Drive', 'GoogleDrive', 'Google Drive/Meine Ablage', 'Google Drive/My Drive']) kandidaten.push(path.join(home, n, ORDNER_NAME));
  for (const k of kandidaten) { try { if (fs.statSync(k).isDirectory()) return k; } catch {} }
  return '';
}

// Relativen Pfad sicher auflösen. Wirft, wenn er aus dem Cloud-Ordner herausführt.
function abs(rel) {
  if (!root) throw new Error('Kein Cloud-Ordner eingestellt.');
  const p = path.resolve(root, String(rel || '').replace(/^[\\/]+/, ''));
  const r = path.relative(root, p);
  if (r.startsWith('..') || path.isAbsolute(r)) throw new Error('Pfad liegt außerhalb der Cloud.');
  return p;
}
function relOf(p) { return path.relative(root, p).split(path.sep).join('/'); }
function inside(p) { if (!root) return false; const r = path.relative(root, path.resolve(p)); return r !== '' && !r.startsWith('..') && !path.isAbsolute(r); }

function status() {
  let ok = false; try { ok = !!root && fs.statSync(root).isDirectory(); } catch {}
  const fehlend = ok ? STRUKTUR.filter(s => !fs.existsSync(abs(s))) : STRUKTUR.slice();
  return { root, ok, fehlend: fehlend.length };
}

async function ensureStructure() {
  if (!root) throw new Error('Kein Cloud-Ordner eingestellt.');
  let neu = 0;
  for (const s of STRUKTUR) { const p = abs(s); if (!fs.existsSync(p)) { await fsp.mkdir(p, { recursive: true }); neu++; } }
  return neu;
}

async function list(rel) {
  const dir = abs(rel);
  const ents = await fsp.readdir(dir, { withFileTypes: true });
  const out = [];
  for (const e of ents) {
    if (VERSTECKT.has(e.name.toLowerCase()) || e.name.startsWith('~$')) continue;
    const p = path.join(dir, e.name);
    try {
      const st = await fsp.stat(p);
      const isDir = st.isDirectory();
      let count = 0;
      if (isDir) { try { count = (await fsp.readdir(p)).filter(n => !VERSTECKT.has(n.toLowerCase())).length; } catch {} }
      out.push({ name: e.name, rel: relOf(p), dir: isDir, size: isDir ? 0 : st.size, mtime: st.mtimeMs, count, ext: isDir ? '' : path.extname(e.name).slice(1).toLowerCase() });
    } catch {}
  }
  out.sort((a, b) => (b.dir - a.dir) || a.name.localeCompare(b.name, 'de', { numeric: true, sensitivity: 'base' }));
  return out;
}

// Unterordner für die Seitenleiste
async function subdirs(rel) {
  const dir = abs(rel);
  const ents = await fsp.readdir(dir, { withFileTypes: true });
  return ents.filter(e => e.isDirectory() && !e.name.startsWith('.')).map(e => ({ name: e.name, rel: relOf(path.join(dir, e.name)) }))
    .sort((a, b) => a.name.localeCompare(b.name, 'de', { numeric: true, sensitivity: 'base' }));
}

// Dateien und Gesamtgröße eines Ordners (mit Obergrenze, damit riesige Sample-Ordner nicht hängen)
async function stats(rel) {
  let files = 0, bytes = 0, neu = 0, gezählt = 0;
  const grenze = Date.now() - 24 * 3600e3;
  async function walk(d) {
    if (gezählt > 30000) return;
    let ents; try { ents = await fsp.readdir(d, { withFileTypes: true }); } catch { return; }
    for (const e of ents) {
      if (VERSTECKT.has(e.name.toLowerCase())) continue;
      const p = path.join(d, e.name); gezählt++;
      if (e.isDirectory()) await walk(p);
      else { try { const st = await fsp.stat(p); files++; bytes += st.size; if (st.mtimeMs > grenze) neu++; } catch {} }
    }
  }
  await walk(abs(rel));
  return { files, bytes, neu, capped: gezählt > 30000 };
}

function cleanName(n) { return String(n || '').replace(/[\\/:*?"<>|]/g, '').replace(/[. ]+$/, '').trim(); }
async function freeName(dir, name) {
  const ext = path.extname(name), base = name.slice(0, name.length - ext.length);
  let cand = name, i = 2;
  while (fs.existsSync(path.join(dir, cand))) cand = `${base} (${i++})${ext}`;
  return cand;
}

async function mkdir(rel, name) {
  const n = cleanName(name); if (!n) throw new Error('Ungültiger Ordnername.');
  const dir = abs(rel); const p = path.join(dir, await freeName(dir, n));
  await fsp.mkdir(p); return relOf(p);
}

// Dateien/Ordner von außen in die Cloud kopieren, oder innerhalb der Cloud verschieben.
async function importPaths(srcs, destRel, onProgress) {
  const dest = abs(destRel);
  if (!fs.statSync(dest).isDirectory()) throw new Error('Ziel ist kein Ordner.');
  const res = { copied: 0, moved: 0, skipped: 0, errors: [] };
  let i = 0;
  for (const src0 of srcs) {
    i++;
    const src = path.resolve(String(src0));
    const name = path.basename(src);
    try {
      if (onProgress) onProgress({ i, n: srcs.length, name });
      if (path.dirname(src) === dest) { res.skipped++; continue; }
      const rs = path.relative(src, dest);
      if (!rs.startsWith('..') && !path.isAbsolute(rs)) throw new Error('Ein Ordner kann nicht in sich selbst gelegt werden.');
      const target = path.join(dest, await freeName(dest, name));
      if (inside(src)) { await fsp.rename(src, target); res.moved++; }
      else { await fsp.cp(src, target, { recursive: true, errorOnExist: true, force: false }); res.copied++; }
    } catch (e) { res.errors.push(name + ': ' + (e && e.message || e)); }
  }
  return res;
}

let watcher = null, timer = null;
function watch(onChange) {
  unwatch();
  if (!root) return;
  try {
    watcher = fs.watch(root, { recursive: true }, () => { clearTimeout(timer); timer = setTimeout(onChange, 700); });
    watcher.on('error', () => unwatch());
  } catch { watcher = null; }
}
function unwatch() { if (watcher) { try { watcher.close(); } catch {} watcher = null; } }

module.exports = { ORDNER_NAME, STRUKTUR, setRoot, getRoot, detect, abs, status, ensureStructure, list, subdirs, stats, mkdir, importPaths, watch, unwatch };
