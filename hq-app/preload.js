const { contextBridge, ipcRenderer, webUtils } = require('electron');
contextBridge.exposeInMainWorld('hq', {
  getSettings: () => ipcRenderer.invoke('settings:get'),
  setSettings: p => ipcRenderer.invoke('settings:set', p),
  setToken: t => ipcRenderer.invoke('token:set', t),
  connect: () => ipcRenderer.invoke('discord:connect'),
  state: () => ipcRenderer.invoke('discord:state'),
  messages: id => ipcRenderer.invoke('discord:messages', id),
  send: (id, text) => ipcRenderer.invoke('discord:send', id, text),
  openExternal: u => ipcRenderer.invoke('open:external', u),
  onState: fn => ipcRenderer.on('discord:state', (_e, s) => fn(s)),
  // Selbst-Update über die Cloud
  updateState: () => ipcRenderer.invoke('update:get'),
  applyUpdate: () => ipcRenderer.invoke('update:apply'),
  onUpdate: fn => ipcRenderer.on('update:state', (_e, s) => fn(s))
});
// Cloud-Ordner (Brain Bugz Cloud)
contextBridge.exposeInMainWorld('cloud', {
  status: () => ipcRenderer.invoke('cloud:status'),
  detect: () => ipcRenderer.invoke('cloud:detect'),
  choose: () => ipcRenderer.invoke('cloud:choose'),
  ensure: () => ipcRenderer.invoke('cloud:ensure'),
  list: rel => ipcRenderer.invoke('cloud:list', rel),
  subdirs: rel => ipcRenderer.invoke('cloud:subdirs', rel),
  stats: rel => ipcRenderer.invoke('cloud:stats', rel),
  mkdir: (rel, name) => ipcRenderer.invoke('cloud:mkdir', rel, name),
  // Beim Ablegen kommen File-Objekte an; den echten Pfad gibt es nur hier im Preload
  importFiles: (files, rel) => ipcRenderer.invoke('cloud:import', [...files].map(f => webUtils.getPathForFile(f)).filter(Boolean), rel),
  move: (rels, rel) => ipcRenderer.invoke('cloud:move', rels, rel),
  open: rel => ipcRenderer.invoke('cloud:open', rel),
  reveal: rel => ipcRenderer.invoke('cloud:reveal', rel),
  fileUrl: rel => ipcRenderer.invoke('cloud:fileurl', rel),
  startDrag: rel => ipcRenderer.send('cloud:drag', rel),
  onChanged: fn => ipcRenderer.on('cloud:changed', () => fn()),
  onProgress: fn => ipcRenderer.on('cloud:progress', (_e, p) => fn(p))
});
