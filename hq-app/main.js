const { app, BrowserWindow, ipcMain, shell, dialog, nativeImage } = require('electron');
const { pathToFileURL } = require('url');
const path = require('path');
const store = require('./store');
const dc = require('./discord');
const cloud = require('./cloud');
const updater = require('./updater');

let win;
function createWindow() {
  win = new BrowserWindow({
    width: 1500, height: 900, minWidth: 900, minHeight: 600, backgroundColor: '#090b0d', title: 'Bugz HQ',
    icon: path.join(__dirname, 'icon.png'),
    // sandbox aus, damit das Preload-Skript sicher geladen wird; die Seite selbst bleibt isoliert (contextIsolation, kein nodeIntegration)
    webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false, sandbox: false, webviewTag: true }
  });
  win.setMenuBarVisibility(false);
  win.webContents.on('preload-error', (_e, p, err) => dialog.showErrorBox('Bugz HQ: Start-Fehler', 'Das Preload-Skript ließ sich nicht laden:\n' + p + '\n\n' + (err && err.stack || err)));
  win.webContents.on('before-input-event', (e, input) => { if (input.type === 'keyDown' && (input.key === 'F12' || (input.control && input.shift && input.key.toUpperCase() === 'I'))) win.webContents.toggleDevTools(); });
  win.loadFile(path.join(__dirname, 'renderer', 'index.html'));
  win.webContents.setWindowOpenHandler(({ url }) => { if (/^https:\/\//.test(url)) shell.openExternal(url); return { action: 'deny' }; });
  // Das Board läuft in einem eigenen Webview: nur claude.ai darf dort rein, alles andere geht in den Browser.
  win.webContents.on('will-attach-webview', (e, prefs) => { delete prefs.preload; prefs.nodeIntegration = false; prefs.contextIsolation = true; });
  win.webContents.on('did-attach-webview', (_e, wc) => {
    wc.setWindowOpenHandler(({ url }) => { if (/^https:\/\//.test(url)) shell.openExternal(url); return { action: 'deny' }; });
  });
}

function push(snap) { if (win && !win.isDestroyed()) win.webContents.send('discord:state', snap); }

async function connectFromSettings() {
  const token = store.getToken();
  if (!token) return { ok: false, error: 'Noch kein Bot-Token eingetragen.' };
  try {
    const snap = await dc.connect(token, push);
    const s = store.load();
    if (!s.guildId && snap.guild) store.patch({ guildId: snap.guild.id });
    return { ok: true, state: dc.snapshot(store.load().guildId || undefined) };
  } catch (e) { return { ok: false, error: dc.describeError(e) }; }
}

// Cloud-Ordner: gespeicherten Pfad nehmen, sonst automatisch suchen
// Updates aus der Cloud (Ordner _HQ-App) prüfen und dem Fenster melden
let upd = null;
function updState() { return { current: app.getVersion(), available: upd ? { version: upd.version } : null, canApply: updater.canApply() }; }
function checkUpdate() {
  try { upd = updater.find(cloud.getRoot(), app.getVersion()); } catch { upd = null; }
  if (win && !win.isDestroyed()) win.webContents.send('update:state', updState());
}

function cloudStart() {
  let p = store.load().cloudPath;
  if (!p || !require('fs').existsSync(p)) { const d = cloud.detect(); if (d) { p = d; store.patch({ cloudPath: d }); } }
  cloud.setRoot(p || '');
  cloud.watch(() => { if (win && !win.isDestroyed()) win.webContents.send('cloud:changed'); checkUpdate(); });
  checkUpdate();
  return cloud.status();
}
// ipcMain.handle übergibt zuerst das Event, das wird hier weggelassen
const safe = fn => async (_e, ...a) => { try { return { ok: true, data: await fn(...a) } } catch (e) { return { ok: false, error: String(e && e.message || e) } } };
let dragIcon = null;

app.whenReady().then(() => {
  createWindow();
  cloudStart();
  ipcMain.handle('update:get', () => { checkUpdate(); return updState(); });
  ipcMain.handle('update:apply', () => {
    checkUpdate();
    if (!upd) return { ok: false, error: 'Kein Update gefunden.' };
    try { updater.apply(upd, () => setTimeout(() => app.quit(), 300)); return { ok: true }; }
    catch (e) { return { ok: false, error: String(e && e.message || e) }; }
  });
  setInterval(checkUpdate, 15 * 60e3);
  ipcMain.handle('cloud:status', () => cloud.status());
  ipcMain.handle('cloud:detect', () => { store.patch({ cloudPath: '' }); return cloudStart(); });
  ipcMain.handle('cloud:choose', async () => {
    const r = await dialog.showOpenDialog(win, { title: 'Cloud-Ordner "Brain Bugz Cloud" wählen', properties: ['openDirectory'] });
    if (r.canceled || !r.filePaths[0]) return cloud.status();
    store.patch({ cloudPath: r.filePaths[0] }); return cloudStart();
  });
  ipcMain.handle('cloud:ensure', safe(() => cloud.ensureStructure()));
  ipcMain.handle('cloud:list', safe(rel => cloud.list(rel)));
  ipcMain.handle('cloud:subdirs', safe(rel => cloud.subdirs(rel)));
  ipcMain.handle('cloud:stats', safe(rel => cloud.stats(rel)));
  ipcMain.handle('cloud:mkdir', safe((rel, name) => cloud.mkdir(rel, name)));
  ipcMain.handle('cloud:import', safe((paths, rel) => cloud.importPaths(Array.isArray(paths) ? paths : [], rel,
    p => { if (win && !win.isDestroyed()) win.webContents.send('cloud:progress', p); })));
  ipcMain.handle('cloud:move', safe((rels, rel) => cloud.importPaths((Array.isArray(rels) ? rels : []).map(r => cloud.abs(r)), rel)));
  ipcMain.handle('cloud:open', safe(async rel => { const e = await shell.openPath(cloud.abs(rel)); if (e) throw new Error(e); }));
  ipcMain.handle('cloud:reveal', safe(rel => { const p = cloud.abs(rel); if (require('fs').statSync(p).isDirectory()) return shell.openPath(p); shell.showItemInFolder(p); }));
  ipcMain.handle('cloud:fileurl', safe(rel => pathToFileURL(cloud.abs(rel)).href));
  // Datei aus der App herausziehen, z.B. ein Sample direkt in Ableton
  ipcMain.on('cloud:drag', (e, rel) => {
    try {
      if (!dragIcon) dragIcon = nativeImage.createFromPath(path.join(__dirname, 'icon.png')).resize({ width: 48, height: 48 });
      e.sender.startDrag({ file: cloud.abs(rel), icon: dragIcon });
    } catch {}
  });
  ipcMain.handle('settings:get', () => store.publicView());
  ipcMain.handle('settings:set', (_e, p) => { store.patch(p || {}); return store.publicView(); });
  ipcMain.handle('token:set', async (_e, t) => { store.setToken(String(t || '').trim()); return connectFromSettings(); });
  ipcMain.handle('discord:connect', () => connectFromSettings());
  ipcMain.handle('discord:state', () => dc.snapshot(store.load().guildId || undefined));
  ipcMain.handle('discord:messages', async (_e, id) => { try { return { ok: true, list: await dc.messages(id) }; } catch (e) { return { ok: false, error: dc.describeError(e) }; } });
  ipcMain.handle('discord:send', async (_e, id, text) => { try { await dc.send(id, text); return { ok: true }; } catch (e) { return { ok: false, error: dc.describeError(e) }; } });
  ipcMain.handle('open:external', (_e, url) => { if (/^https:\/\//.test(url)) shell.openExternal(url); });
  if (store.getToken()) connectFromSettings().then(r => { if (r.ok) push(r.state); });
  app.on('activate', () => { if (BrowserWindow.getAllWindows().length === 0) createWindow(); });
});
app.on('window-all-closed', async () => { cloud.unwatch(); await dc.disconnect(); if (process.platform !== 'darwin') app.quit(); });
