// Selbst-Update über die Cloud: neue Versionen liegen als "BugzHQ-x.y.z.exe" im Ordner "_HQ-App" der Brain Bugz Cloud.
// Google Drive verteilt sie an alle. Die App tauscht sich nach Klick selbst aus und startet neu.
const fs = require('fs');
const path = require('path');
const os = require('os');
const { spawn } = require('child_process');

const ORDNER = '_HQ-App';
const MUSTER = /^BugzHQ-(\d+)\.(\d+)\.(\d+)\.exe$/i;

function parse(v) { return String(v).split('.').map(n => parseInt(n, 10) || 0); }
function newer(a, b) { const x = parse(a), y = parse(b); for (let i = 0; i < 3; i++) { if ((x[i] || 0) !== (y[i] || 0)) return (x[i] || 0) > (y[i] || 0); } return false; }

// Neueste Version im Update-Ordner finden, die neuer ist als die laufende
function find(cloudRoot, current) {
  if (!cloudRoot) return null;
  const dir = path.join(cloudRoot, ORDNER);
  let names = []; try { names = fs.readdirSync(dir); } catch { return null; }
  let best = null;
  for (const n of names) {
    const m = n.match(MUSTER); if (!m) continue;
    const v = m.slice(1, 4).join('.');
    if (newer(v, current) && (!best || newer(v, best.version))) best = { version: v, file: path.join(dir, n), name: n };
  }
  return best;
}

// Nur in der fertigen Exe möglich: electron-builder setzt dann den Pfad der Exe
function canApply() { return !!process.env.PORTABLE_EXECUTABLE_FILE; }

function apply(upd, quit) {
  const target = process.env.PORTABLE_EXECUTABLE_FILE;
  if (!target) throw new Error('Selbst-Update geht nur in der fertigen BugzHQ.exe.');
  // Erst lokal kopieren (lädt die Datei auch aus Google Drive herunter, falls sie nur online liegt)
  const tmp = path.join(os.tmpdir(), 'BugzHQ-update-' + upd.version + '.exe');
  fs.copyFileSync(upd.file, tmp);
  const bat = path.join(os.tmpdir(), 'BugzHQ-update.cmd');
  // Wartet, bis die alte Exe frei ist, ersetzt sie und startet die neue
  fs.writeFileSync(bat, [
    '@echo off',
    'set n=0',
    ':retry',
    'timeout /t 1 /nobreak >nul',
    `copy /y "${tmp}" "${target}" >nul 2>&1 && goto ok`,
    'set /a n+=1',
    'if %n% lss 90 goto retry',
    'exit /b 1',
    ':ok',
    `start "" "${target}"`,
    `del "${tmp}" >nul 2>&1`,
    '(goto) 2>nul & del "%~f0"'
  ].join('\r\n'));
  spawn('cmd.exe', ['/c', bat], { detached: true, stdio: 'ignore', windowsHide: true }).unref();
  quit();
}

module.exports = { ORDNER, find, newer, canApply, apply };
