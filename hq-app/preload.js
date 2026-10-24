const { contextBridge, ipcRenderer } = require('electron');
contextBridge.exposeInMainWorld('hq', {
  getSettings: () => ipcRenderer.invoke('settings:get'),
  setSettings: p => ipcRenderer.invoke('settings:set', p),
  setToken: t => ipcRenderer.invoke('token:set', t),
  connect: () => ipcRenderer.invoke('discord:connect'),
  state: () => ipcRenderer.invoke('discord:state'),
  messages: id => ipcRenderer.invoke('discord:messages', id),
  send: (id, text) => ipcRenderer.invoke('discord:send', id, text),
  openExternal: u => ipcRenderer.invoke('open:external', u),
  onState: fn => ipcRenderer.on('discord:state', (_e, s) => fn(s))
});
