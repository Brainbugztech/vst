// Einstellungen lokal speichern. Der Bot-Token wird mit Windows verschlüsselt (safeStorage), nie im Klartext.
const { app, safeStorage } = require('electron');
const fs = require('fs');
const path = require('path');

const file = () => path.join(app.getPath('userData'), 'settings.json');
const DEFAULTS = { boardUrl: '', guildId: '', channelId: '', tokenEnc: '', tokenPlain: '', overlayOpen: true };

function load() {
  try { return { ...DEFAULTS, ...JSON.parse(fs.readFileSync(file(), 'utf8')) }; }
  catch { return { ...DEFAULTS }; }
}
function save(s) {
  fs.mkdirSync(path.dirname(file()), { recursive: true });
  fs.writeFileSync(file(), JSON.stringify(s, null, 2));
}
function setToken(token) {
  const s = load();
  if (!token) { s.tokenEnc = ''; s.tokenPlain = ''; save(s); return true; }
  if (safeStorage.isEncryptionAvailable()) { s.tokenEnc = safeStorage.encryptString(token).toString('base64'); s.tokenPlain = ''; }
  else { s.tokenPlain = token; s.tokenEnc = ''; }
  save(s); return true;
}
function getToken() {
  const s = load();
  try {
    if (s.tokenEnc) return safeStorage.decryptString(Buffer.from(s.tokenEnc, 'base64'));
  } catch { return ''; }
  return s.tokenPlain || '';
}
// Für das Fenster nie den Token selbst herausgeben, nur ob einer da ist.
function publicView() { const s = load(); const { tokenEnc, tokenPlain, ...rest } = s; return { ...rest, hasToken: !!(tokenEnc || tokenPlain) }; }
function patch(p) { const s = load(); const allowed = ['boardUrl', 'guildId', 'channelId', 'overlayOpen']; for (const k of allowed) if (k in p) s[k] = p[k]; save(s); }

module.exports = { load, save, setToken, getToken, publicView, patch };
