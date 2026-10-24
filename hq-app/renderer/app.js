const $ = s => document.querySelector(s);
const hq = window.hq;
let state = { ready: false };
let cfg = {};
let curChannel = '';

function el(tag, cls, text) { const e = document.createElement(tag); if (cls) e.className = cls; if (text != null) e.textContent = text; return e; }
function notice(msg) { const n = $('#notice'); n.hidden = !msg; n.textContent = msg || ''; }
function setBoard() {
  const url = (cfg.boardUrl || '').trim();
  const ok = /^https:\/\/claude\.ai\//.test(url);
  $('#noBoard').hidden = ok;
  $('#board').style.display = ok ? '' : 'none';
  if (ok && $('#board').getAttribute('src') !== url) $('#board').setAttribute('src', url);
}

function renderLed() {
  const led = $('#led');
  led.className = 'led' + (state.ready ? ' on' : '');
  $('#gname').textContent = state.guild ? state.guild.name : 'Discord';
  $('#gstat').textContent = state.ready ? (state.members ? state.members.filter(m => m.status !== 'offline').length + ' online' : '') : 'nicht verbunden';
}
function renderMembers() {
  const ul = $('#members'); ul.replaceChildren();
  if (!state.members) { ul.append(el('li', 'muted', cfg.hasToken ? 'Verbinde mit Discord …' : 'Trag in den Einstellungen den Bot-Token ein, dann siehst du hier, wer auf eurem Server online ist.')); return; }
  const on = state.members.filter(m => m.status !== 'offline'), off = state.members.filter(m => m.status === 'offline');
  const add = (m) => {
    const li = el('li', 'mem' + (m.status === 'offline' ? ' off' : ''));
    if (m.avatar) { const i = el('img'); i.src = m.avatar; i.alt = ''; li.append(i); }
    const box = el('div'); box.append(el('div', 'n', m.name));
    const sub = m.voice ? '🔊 ' + ((state.voice || []).find(v => v.id === m.voice)?.name || 'Voice') : (m.game ? 'spielt ' + m.game : '');
    if (sub) box.append(el('div', 'g', sub));
    li.append(box, el('span', 'dot ' + m.status)); ul.append(li);
  };
  ul.append(el('li', 'sec', 'Online (' + on.length + ')')); on.forEach(add);
  if (!on.length) ul.append(el('li', 'muted', 'Gerade ist niemand online.'));
  ul.append(el('li', 'sec', 'Offline (' + off.length + ')')); off.forEach(add);
}
function renderVoice() {
  const ul = $('#voice'); ul.replaceChildren();
  if (!state.voice) { ul.append(el('li', 'muted', 'Noch nicht verbunden.')); return; }
  if (!state.voice.length) ul.append(el('li', 'muted', 'Keine Voice-Channels auf dem Server.'));
  state.voice.forEach(v => {
    const li = el('li', 'vch' + (v.members.length ? ' live' : '')); li.append(el('b', null, '🔊 ' + v.name));
    if (v.members.length) { const u = el('ul'); v.members.forEach(n => u.append(el('li', null, n))); li.append(u); }
    else li.append(el('div', 'muted', 'leer'));
    ul.append(li);
  });
}
function renderChannels() {
  const sel = $('#chSel'); const keep = curChannel || cfg.channelId;
  sel.replaceChildren();
  (state.text || []).forEach(c => { const o = el('option', null, '# ' + c.name); o.value = c.id; sel.append(o); });
  if (keep && [...sel.options].some(o => o.value === keep)) sel.value = keep;
  curChannel = sel.value || '';
  updateSend();
}
function updateSend() {
  const c = (state.text || []).find(x => x.id === curChannel);
  const can = !!(c && c.canSend);
  $('#sendBtn').disabled = !can; $('#sendText').disabled = !can;
  $('#sendHint').textContent = !c ? '' : can ? '' : 'Dem Bot fehlt hier die Schreibberechtigung.';
}
async function loadMessages() {
  const ul = $('#msgs'); ul.replaceChildren();
  if (!curChannel) return;
  const r = await hq.messages(curChannel);
  if (!r.ok) { ul.append(el('li', 'muted', r.error)); return; }
  if (!r.list.length) ul.append(el('li', 'muted', 'Noch keine Nachrichten.'));
  r.list.forEach(m => {
    const li = el('li', 'msg' + (m.bot ? ' bot' : ''));
    const h = el('div', 'h'); h.append(el('b', null, m.author), document.createTextNode('  ' + new Date(m.at).toLocaleString('de-DE', { day: '2-digit', month: '2-digit', hour: '2-digit', minute: '2-digit' })));
    li.append(h, el('p', null, m.text)); ul.append(li);
  });
  ul.scrollTop = ul.scrollHeight;
}
function applyState(s) {
  const fresh = s.newMessage;
  state = s.ready ? s : { ready: false };
  renderLed(); renderMembers(); renderVoice();
  if (state.ready) {
    const prev = $('#chSel').options.length;
    renderChannels();
    if (!prev || (fresh && fresh.channelId === curChannel)) loadMessages();
  }
}

document.querySelectorAll('.tab').forEach(t => t.addEventListener('click', () => {
  document.querySelectorAll('.tab').forEach(x => x.classList.toggle('on', x === t));
  for (const id of ['who', 'voice', 'chat']) $('#p-' + id).hidden = id !== t.dataset.tab;
  if (t.dataset.tab === 'chat') loadMessages();
}));
$('#chSel').addEventListener('change', e => { curChannel = e.target.value; hq.setSettings({ channelId: curChannel }); updateSend(); loadMessages(); });
$('#sendForm').addEventListener('submit', async e => {
  e.preventDefault(); const t = $('#sendText').value.trim(); if (!t || !curChannel) return;
  $('#sendBtn').disabled = true;
  const r = await hq.send(curChannel, t);
  $('#sendBtn').disabled = false;
  if (r.ok) { $('#sendText').value = ''; loadMessages(); } else $('#sendHint').textContent = r.error;
});
$('#sendText').addEventListener('keydown', e => { if (e.key === 'Enter' && (e.ctrlKey || e.metaKey)) $('#sendForm').requestSubmit(); });
$('#bReload').addEventListener('click', () => { const b = $('#board'); try { b.reload(); } catch { setBoard(); } });
$('#bOverlay').addEventListener('click', async e => {
  const open = $('#overlay').classList.toggle('hidden') === false;
  e.currentTarget.setAttribute('aria-pressed', String(open)); await hq.setSettings({ overlayOpen: open });
});

const dlg = $('#dlg');
function fillGuilds() {
  const sel = $('#sGuild'); sel.replaceChildren();
  const gs = state.guilds || [];
  if (!gs.length) { const o = el('option', null, 'Erst Token speichern'); o.value = ''; sel.append(o); return; }
  gs.forEach(g => { const o = el('option', null, g.name); o.value = g.id; sel.append(o); });
  if (cfg.guildId) sel.value = cfg.guildId;
}
async function openSettings() {
  cfg = await hq.getSettings();
  $('#sBoard').value = cfg.boardUrl || ''; $('#sToken').value = '';
  $('#tokenState').textContent = cfg.hasToken ? 'Ein Token ist gespeichert. Leer lassen, um ihn zu behalten.' : 'Noch kein Token gespeichert. Den Token findest du im Discord-Developer-Portal unter Bot. Gib ihn niemandem weiter.';
  fillGuilds(); dlg.showModal();
}
$('#bSettings').addEventListener('click', openSettings);
$('#bFirst').addEventListener('click', openSettings);
$('#sCancel').addEventListener('click', () => dlg.close());
$('#setForm').addEventListener('submit', async e => {
  e.preventDefault();
  const url = $('#sBoard').value.trim();
  if (url && !/^https:\/\/claude\.ai\//.test(url)) { $('#tokenState').textContent = 'Der Board-Link muss mit https://claude.ai/ anfangen.'; return; }
  const guild = $('#sGuild').value;
  cfg = await hq.setSettings({ boardUrl: url, ...(guild ? { guildId: guild } : {}) });
  setBoard();
  const tok = $('#sToken').value.trim();
  dlg.close();
  if (tok) { notice('Verbinde mit Discord …'); const r = await hq.setToken(tok); cfg = await hq.getSettings(); r.ok ? (notice(''), applyState(r.state)) : notice(r.error); }
  else if (guild) { const s = await hq.state(); applyState(s); }
});

(async () => {
  cfg = await hq.getSettings();
  if (!cfg.overlayOpen) { $('#overlay').classList.add('hidden'); $('#bOverlay').setAttribute('aria-pressed', 'false'); }
  setBoard(); renderLed(); renderMembers(); renderVoice();
  hq.onState(applyState);
  const s = await hq.state(); if (s.ready) applyState(s);
  else if (cfg.hasToken) { notice('Verbinde mit Discord …'); setTimeout(async () => { const t = await hq.state(); if (t.ready) { notice(''); applyState(t); } else { const r = await hq.connect(); r.ok ? (notice(''), applyState(r.state)) : notice(r.error); } }, 2500); }
})();
