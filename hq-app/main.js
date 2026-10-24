const { app, BrowserWindow, ipcMain, shell, session } = require('electron');
const path = require('path');
const store = require('./store');
const dc = require('./discord');

let win;
function createWindow() {
  win = new BrowserWindow({
    width: 1500, height: 900, minWidth: 900, minHeight: 600, backgroundColor: '#090b0d', title: 'Bugz HQ',
    icon: path.join(__dirname, 'icon.png'),
    webPreferences: { preload: path.join(__dirname, 'preload.js'), contextIsolation: true, nodeIntegration: false, webviewTag: true }
  });
  win.setMenuBarVisibility(false);
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

app.whenReady().then(() => {
  createWindow();
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
app.on('window-all-closed', async () => { await dc.disconnect(); if (process.platform !== 'darwin') app.quit(); });
