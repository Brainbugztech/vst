// Cloud-Ansicht: Ordnerstruktur von "Brain Bugz Cloud", Drag & Drop rein und raus, Vorhören von Audio.
(() => {
  const $ = s => document.querySelector(s);
  const api = window.cloud;
  const NEU_MS = 24 * 3600e3;

  // Symbole (inline SVG, damit nichts nachgeladen werden muss)
  const I = {
    inbox: '<path d="M4 13h4l2 3h4l2-3h4M5 5h14l1 8v6H4v-6z"/>',
    proj: '<path d="M9 18V6l11-2v12"/><circle cx="6" cy="18" r="3"/><circle cx="17" cy="16" r="3"/>',
    preset: '<path d="M4 7h10M18 7h2M4 17h4M12 17h8"/><circle cx="16" cy="7" r="2"/><circle cx="10" cy="17" r="2"/>',
    sample: '<path d="M3 12h2l2-6 3 12 3-9 2 6 2-3h4"/>',
    plugin: '<rect x="4" y="4" width="16" height="16" rx="3"/><circle cx="12" cy="12" r="3.5"/><path d="M12 8.5V6"/>',
    track: '<circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="2.5"/>',
    folder: '<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>',
    audio: '<path d="M9 18V6l10-2v12"/><circle cx="6" cy="18" r="3"/>',
    file: '<path d="M6 3h8l4 4v14H6z M14 3v4h4"/>',
    zip: '<path d="M6 3h12v18H6z M12 3v2m0 2v2m0 2v2m-1 2h2v3h-2z"/>',
    img: '<rect x="3" y="5" width="18" height="14" rx="2"/><circle cx="9" cy="10" r="2"/><path d="M21 16l-5-5-8 8"/>',
    midi: '<rect x="3" y="6" width="18" height="12" rx="2"/><path d="M8 6v7M12 6v7M16 6v7"/>'
  };
  const svg = k => `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">${I[k] || I.file}</svg>`;

  // Hauptordner: Farbe, Symbol, Beschreibung
  const HAUPT = [
    { m: /^_inbox/i, c: '#9aa8a1', ico: 'inbox', d: 'Neues erstmal hier rein, später einsortieren' },
    { m: /^_hq-app/i, c: '#5f6b66', ico: 'plugin', d: 'Updates für die HQ-App, kommen automatisch' },
    { m: /^01_/, c: '#ff2f8e', ico: 'proj', d: 'Ableton-Projekte, ein Ordner pro Track' },
    { m: /^02_/, c: '#ffaa28', ico: 'preset', d: 'Racks, Synth-Presets und Presetpacks' },
    { m: /^03_/, c: '#b6ff1f', ico: 'sample', d: 'Drums, Bass, Melodie, Vocals, FX' },
    { m: /^04_/, c: '#33d6ff', ico: 'plugin', d: 'Brain-Bugz-Builds und Installer' },
    { m: /^05_/, c: '#a77bff', ico: 'track', d: 'Master, Stems, Demos, Releases' }
  ];
  const haupt = name => HAUPT.find(h => h.m.test(name)) || { c: '#8c9a93', ico: 'folder', d: '' };
  const pretty = name => name.replace(/^\d+_/, '').replace(/^_(inbox)\b.*$/i, 'Inbox').replace(/^_hq-app$/i, 'HQ-App Updates').replace(/^_/, '');
  // Update-Ordner immer ans Ende
  const lastHq = list => [...list.filter(e => !/^_hq-app$/i.test(e.name)), ...list.filter(e => /^_hq-app$/i.test(e.name))];
  const colorOf = rel => haupt((rel || '').split('/')[0] || '').c;

  const TYP = [
    [/^(wav|aif|aiff|flac|mp3|ogg|m4a)$/, 'Audio', 'audio', '#b6ff1f'],
    [/^(als|alp|alc)$/, 'Ableton', 'proj', '#ff2f8e'],
    [/^(adg|adv|agr|ams|fxp|fxb|vital|vitalbank|h2p|nmsv|vstpreset|aupreset|xpf)$/, 'Preset', 'preset', '#ffaa28'],
    [/^(vst3|dll|exe|msi|component)$/, 'Plugin', 'plugin', '#33d6ff'],
    [/^(mid|midi)$/, 'MIDI', 'midi', '#a77bff'],
    [/^(zip|rar|7z)$/, 'Archiv', 'zip', '#9aa8a1'],
    [/^(png|jpg|jpeg|webp|gif|psd)$/, 'Bild', 'img', '#ff7ad1']
  ];
  const typ = e => { if (e.dir) return ['Ordner', 'folder', colorOf(e.rel)]; for (const [re, l, i, c] of TYP) if (re.test(e.ext)) return [l, i, c]; return [e.ext ? e.ext.toUpperCase() : 'Datei', 'file', '#8c9a93']; };
  const isAudio = e => !e.dir && /^(wav|aif|aiff|flac|mp3|ogg|m4a)$/.test(e.ext);

  const fmtSize = b => b < 1024 ? b + ' B' : b < 1048576 ? (b / 1024).toFixed(0) + ' KB' : b < 1073741824 ? (b / 1048576).toFixed(1) + ' MB' : (b / 1073741824).toFixed(2) + ' GB';
  const fmtDate = ms => { const d = new Date(ms), now = new Date(); const t = d.toLocaleTimeString('de-DE', { hour: '2-digit', minute: '2-digit' }); if (d.toDateString() === now.toDateString()) return 'heute ' + t; const y = new Date(now - 864e5); if (d.toDateString() === y.toDateString()) return 'gestern ' + t; return d.toLocaleDateString('de-DE', { day: '2-digit', month: '2-digit', year: '2-digit' }); };
  const esc = s => String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

  let st = { ok: false }, cur = '', open = new Set(), sel = '', top = [];
  const statCache = new Map();
  function statsOf(rel) { const c = statCache.get(rel); if (c && Date.now() - c.t < 8000) return c.p; const p = api.stats(rel); statCache.set(rel, { t: Date.now(), p }); return p; }
  let toastT;
  function toast(msg, err) { const t = $('#toast'); t.textContent = msg; t.className = 'toast' + (err ? ' err' : ''); t.hidden = false; clearTimeout(toastT); toastT = setTimeout(() => t.hidden = true, 3500); }

  // ---------- Ansicht umschalten ----------
  function show(view) {
    const cloud = view === 'cloud';
    $('#cloudpane').hidden = !cloud;
    $('#vBoard').classList.toggle('on', !cloud); $('#vCloud').classList.toggle('on', cloud);
    $('#vBoard').setAttribute('aria-selected', String(!cloud)); $('#vCloud').setAttribute('aria-selected', String(cloud));
    try { localStorage.setItem('hq-view', view); } catch {}
    if (cloud) refresh();
  }
  $('#vBoard').addEventListener('click', () => show('board'));
  $('#vCloud').addEventListener('click', () => show('cloud'));
  document.addEventListener('keydown', e => { if (e.ctrlKey && e.key === '1') show('board'); if (e.ctrlKey && e.key === '2') show('cloud'); });

  if (!api) { $('#cloudDot').className = 'vs-dot err'; return; }

  // ---------- Status ----------
  async function loadStatus() {
    st = await api.status();
    $('#cloudDot').className = 'vs-dot ' + (st.ok ? 'ok' : 'err');
    $('#cSetup').hidden = st.ok;
    $('#cPath').textContent = st.root || 'kein Ordner'; $('#cPath').title = st.root || '';
    $('#cFix').hidden = !st.ok || !st.fehlend;
    $('#cFixTxt').textContent = st.fehlend ? `${st.fehlend} Standard-Ordner fehlen noch.` : '';
  }

  // ---------- Seitenleiste ----------
  async function renderTree() {
    const nav = $('#cTree'); const r = await api.list('');
    if (!r.ok) { nav.textContent = r.error; return; }
    top = lastHq(r.data.filter(e => e.dir));
    nav.replaceChildren();
    const home = node({ name: 'Übersicht', rel: '', top: true, ico: 'folder', c: '#e7efe4' });
    nav.append(home);
    for (const e of top) {
      const h = haupt(e.name);
      const wrap = document.createElement('div');
      const n = node({ name: pretty(e.name), rel: e.rel, top: true, ico: h.ico, c: h.c, cnt: e.count, canOpen: e.count > 0 });
      wrap.append(n);
      statsOf(e.rel).then(s => { if (s.ok && s.data.neu) { const b = document.createElement('span'); b.className = 'new'; b.textContent = 'NEU'; b.title = s.data.neu + ' neue Dateien seit gestern'; n.querySelector('.lab').after(b); } });
      if (open.has(e.rel)) wrap.append(await subtree(e.rel, h.c));
      nav.append(wrap);
    }
  }
  async function subtree(rel, c) {
    const box = document.createElement('div'); box.className = 'tsub';
    const r = await api.subdirs(rel); if (!r.ok) return box;
    for (const d of r.data) {
      const w = document.createElement('div');
      w.append(node({ name: d.name, rel: d.rel, ico: 'folder', c, canOpen: true }));
      if (open.has(d.rel)) w.append(await subtree(d.rel, c));
      box.append(w);
    }
    return box;
  }
  function node(o) {
    const b = document.createElement('div');
    b.className = 'tn' + (o.top ? ' top' : '') + (cur === o.rel ? ' on' : '');
    b.style.setProperty('--c', o.c); b.dataset.rel = o.rel; b.dataset.drop = o.rel; b.tabIndex = 0; b.setAttribute('role', 'button');
    const car = o.canOpen ? `<button class="car${open.has(o.rel) ? ' open' : ''}" type="button" aria-label="Aufklappen">▶</button>` : '<span class="car"></span>';
    b.innerHTML = `${o.rel ? car : '<span class="car"></span>'}<span class="ico">${svg(o.ico)}</span><span class="lab">${esc(o.name)}</span>${o.cnt != null ? `<span class="cnt">${o.cnt}</span>` : ''}`;
    b.addEventListener('click', ev => {
      if (ev.target.closest('.car') && o.canOpen) { open.has(o.rel) ? open.delete(o.rel) : open.add(o.rel); renderTree(); return; }
      go(o.rel);
    });
    b.addEventListener('keydown', ev => { if (ev.key === 'Enter') go(o.rel); });
    return b;
  }

  // ---------- Inhalt ----------
  function crumbs() {
    const nav = $('#cCrumbs'); nav.replaceChildren();
    const parts = cur ? cur.split('/') : [];
    const add = (label, rel) => { const b = document.createElement('button'); b.type = 'button'; b.textContent = label; b.dataset.drop = rel; b.onclick = () => go(rel); nav.append(b); };
    add('Brain Bugz Cloud', '');
    parts.forEach((p, i) => { const s = document.createElement('span'); s.className = 'sep'; s.textContent = '/'; nav.append(s); add(i === 0 ? pretty(p) : p, parts.slice(0, i + 1).join('/')); });
  }

  async function renderBody() {
    const body = $('#cBody');
    crumbs();
    const r = await api.list(cur);
    if (!r.ok) { body.innerHTML = `<div class="empty"><b>Ordner nicht lesbar</b>${esc(r.error)}</div>`; return; }
    if (!cur) return renderTiles(body, r.data);
    const list = r.data;
    if (!list.length) { body.innerHTML = `<div class="empty"><div style="font-size:30px">⇣</div><b>Noch leer</b>Zieh Dateien oder Ordner hier rein. Google Drive lädt sie automatisch hoch.</div>`; return; }
    const wrap = document.createElement('div'); wrap.className = 'rows';
    wrap.innerHTML = '<div class="rh"><span></span><span>Name</span><span>Typ</span><span class="sz">Größe</span><span class="dt">Geändert</span></div>';
    for (const e of list) wrap.append(row(e));
    body.replaceChildren(wrap);
  }

  function renderTiles(body, list) {
    const grid = document.createElement('div'); grid.className = 'tiles';
    const dirs = lastHq(list.filter(e => e.dir));
    if (!dirs.length) { body.innerHTML = '<div class="empty"><b>Cloud ist leer</b>Klick oben auf „Struktur anlegen“.</div>'; return; }
    for (const e of dirs) {
      const h = haupt(e.name);
      const t = document.createElement('button'); t.type = 'button'; t.className = 'tile'; t.style.setProperty('--c', h.c); t.dataset.drop = e.rel;
      t.innerHTML = `<span class="ico">${svg(h.ico)}</span><div><h3>${esc(pretty(e.name))}</h3><p>${esc(h.d)}</p></div><div class="meta"><span data-f>… Dateien</span><span data-b></span></div>`;
      t.onclick = () => go(e.rel);
      grid.append(t);
      statsOf(e.rel).then(s => {
        if (!s.ok) return;
        t.querySelector('[data-f]').textContent = s.data.files + (s.data.capped ? '+' : '') + ' Dateien';
        t.querySelector('[data-b]').textContent = fmtSize(s.data.bytes);
        if (s.data.neu) { const b = document.createElement('span'); b.className = 'badge'; b.textContent = s.data.neu + ' NEU'; t.append(b); }
      });
    }
    body.replaceChildren(grid);
  }

  function row(e) {
    const [label, ico, c] = typ(e);
    const r = document.createElement('div');
    r.className = 'row' + (e.dir ? ' dir' : '') + (sel === e.rel ? ' sel' : '') + (playing === e.rel ? ' playing' : '');
    r.style.setProperty('--c', c); r.dataset.rel = e.rel;
    if (e.dir) r.dataset.drop = e.rel;
    const neu = !e.dir && Date.now() - e.mtime < NEU_MS ? '<span class="new">NEU</span>' : '';
    const play = isAudio(e) ? `<button class="play" type="button" aria-label="Vorhören">${playing === e.rel && !$('#pAudio').paused ? '❚❚' : '▶'}</button>` : '';
    r.innerHTML = `<span class="ti">${svg(ico)}${play}</span><span class="nm"><span title="${esc(e.name)}">${esc(e.name)}</span>${neu}</span><span class="ty">${esc(label)}</span><span class="sz">${e.dir ? e.count + ' Obj.' : fmtSize(e.size)}</span><span class="dt">${fmtDate(e.mtime)}</span>`;
    r.addEventListener('click', ev => {
      if (ev.target.closest('.play')) { ev.stopPropagation(); playFile(e); return; }
      sel = e.rel; document.querySelectorAll('.row.sel').forEach(x => x.classList.remove('sel')); r.classList.add('sel');
    });
    r.addEventListener('dblclick', async () => {
      if (e.dir) return go(e.rel);
      const o = await api.open(e.rel); if (!o.ok) toast(o.error, true);
    });
    // Rausziehen: Dateien als echte Datei (geht in Ableton, Explorer und zurück in die App), Ordner nur innerhalb der App
    r.draggable = true;
    r.addEventListener('dragstart', ev => {
      if (e.dir) { ev.dataTransfer.setData('application/x-bugz-rel', e.rel); ev.dataTransfer.effectAllowed = 'move'; return; }
      ev.preventDefault(); api.startDrag(e.rel);
    });
    return r;
  }

  // ---------- Navigation ----------
  async function go(rel) { cur = rel || ''; sel = ''; if (cur) { const t = cur.split('/')[0]; open.add(t); } await renderTree(); await renderBody(); }
  async function refresh() { statCache.clear(); await loadStatus(); if (!st.ok) return; await renderTree(); await renderBody(); }
  let rT; api.onChanged(() => { clearTimeout(rT); rT = setTimeout(() => { if (!$('#cloudpane').hidden) refresh(); else loadStatus(); }, 300); });

  // ---------- Drag & Drop rein ----------
  const main = $('#cMain'), side = $('#cSide');
  let depth = 0, dropRel = null;
  const label = rel => rel ? rel.split('/').map((p, i) => i === 0 ? pretty(p) : p).join(' / ') : 'Brain Bugz Cloud';
  const accepts = ev => [...ev.dataTransfer.types].some(t => t === 'Files' || t === 'application/x-bugz-rel');
  function targetOf(ev) { const t = ev.target.closest('[data-drop]'); return t ? t.dataset.drop : (ev.target.closest('#cSide') ? null : cur); }
  function mark(ev) {
    document.querySelectorAll('.drop-on').forEach(x => x.classList.remove('drop-on'));
    const t = ev.target.closest('[data-drop]'); if (t) t.classList.add('drop-on');
    dropRel = targetOf(ev);
    const ov = $('#cDrop');
    // Über einer Kachel oder Zeile zeigen wir nur deren Hervorhebung, sonst das große Overlay
    ov.hidden = !(dropRel !== null && !t && ev.target.closest('#cMain'));
    $('#cDropTo').textContent = dropRel !== null ? '→ ' + label(dropRel) : '';
    if (dropRel === null || (!cur && !t)) { ov.hidden = true; }
  }
  function clear() { depth = 0; dropRel = null; $('#cDrop').hidden = true; document.querySelectorAll('.drop-on').forEach(x => x.classList.remove('drop-on')); }
  for (const zone of [main, side]) {
    zone.addEventListener('dragenter', ev => { if (!accepts(ev)) return; ev.preventDefault(); depth++; mark(ev); });
    zone.addEventListener('dragover', ev => {
      if (!accepts(ev)) return; ev.preventDefault();
      mark(ev);
      const ok = dropRel !== null && !(dropRel === '' && !ev.target.closest('[data-drop]'));
      ev.dataTransfer.dropEffect = ok ? (ev.dataTransfer.types.includes('application/x-bugz-rel') ? 'move' : 'copy') : 'none';
    });
    zone.addEventListener('dragleave', () => { if (--depth <= 0) clear(); });
    zone.addEventListener('drop', async ev => {
      if (!accepts(ev)) return; ev.preventDefault();
      const to = dropRel; const onTile = !!ev.target.closest('[data-drop]'); clear();
      if (to === null || (to === '' && !onTile)) { toast('Bitte in einen Ordner ziehen (z.B. auf eine Kachel).', true); return; }
      const inner = ev.dataTransfer.getData('application/x-bugz-rel');
      const files = [...ev.dataTransfer.files];
      if (!inner && !files.length) return;
      progress({ i: 0, n: inner ? 1 : files.length, name: '' });
      const r = inner ? await api.move([inner], to) : await api.importFiles(files, to);
      progress(null);
      if (!r.ok) { toast(r.error, true); return; }
      const d = r.data; const parts = [];
      if (d.copied) parts.push(`${d.copied} hochgeladen`); if (d.moved) parts.push(`${d.moved} verschoben`);
      if (d.errors.length) toast((parts.join(', ') ? parts.join(', ') + ' · ' : '') + 'Fehler: ' + d.errors[0], true);
      else if (parts.length) toast(parts.join(', ') + ' → ' + label(to) + '. Google Drive synct jetzt.');
      refresh();
    });
  }
  // Nichts außerhalb der Cloud-Zonen als Datei öffnen lassen
  document.addEventListener('dragover', ev => { if (!ev.target.closest('#cMain,#cSide')) ev.preventDefault(); });
  document.addEventListener('drop', ev => { if (!ev.target.closest('#cMain,#cSide')) ev.preventDefault(); });

  function progress(p) {
    const box = $('#cProg');
    if (!p) { box.hidden = true; return; }
    box.hidden = false;
    $('#cProgFill').style.width = (p.n ? Math.round((p.i / p.n) * 100) : 0) + '%';
    $('#cProgTxt').textContent = p.name ? `Kopiere ${p.i}/${p.n}: ${p.name}` : 'Kopiere …';
  }
  api.onProgress(progress);

  // ---------- Vorhören ----------
  const au = $('#pAudio'); let playing = '';
  async function playFile(e) {
    if (playing === e.rel) { au.paused ? au.play() : au.pause(); return; }
    const u = await api.fileUrl(e.rel); if (!u.ok) { toast(u.error, true); return; }
    playing = e.rel; au.src = u.data; $('#pName').textContent = e.name; $('#player').hidden = false;
    au.play().catch(() => toast('Dieses Format kann die App nicht abspielen. Doppelklick öffnet es.', true));
  }
  function syncPlay() {
    $('#pPlay').textContent = au.paused ? '▶' : '❚❚';
    document.querySelectorAll('.row').forEach(r => { const on = r.dataset.rel === playing; r.classList.toggle('playing', on); const b = r.querySelector('.play'); if (b) b.textContent = on && !au.paused ? '❚❚' : '▶'; });
  }
  au.addEventListener('play', syncPlay); au.addEventListener('pause', syncPlay); au.addEventListener('ended', syncPlay);
  au.addEventListener('timeupdate', () => {
    $('#pFill').style.width = (au.duration ? au.currentTime / au.duration * 100 : 0) + '%';
    const f = s => isFinite(s) ? Math.floor(s / 60) + ':' + String(Math.floor(s % 60)).padStart(2, '0') : '0:00';
    $('#pTime').textContent = f(au.currentTime) + ' / ' + f(au.duration);
  });
  $('#pPlay').onclick = () => au.src && (au.paused ? au.play() : au.pause());
  $('#pBar').onclick = ev => { if (!au.duration) return; const r = ev.currentTarget.getBoundingClientRect(); au.currentTime = (ev.clientX - r.left) / r.width * au.duration; };
  $('#pClose').onclick = () => { au.pause(); au.removeAttribute('src'); playing = ''; $('#player').hidden = true; syncPlay(); };
  document.addEventListener('keydown', ev => { if (ev.code === 'Space' && !$('#cloudpane').hidden && playing && !/INPUT|TEXTAREA/.test(document.activeElement.tagName)) { ev.preventDefault(); $('#pPlay').click(); } });

  // ---------- Knöpfe ----------
  $('#cDetect').onclick = async () => { await api.detect(); await refresh(); if (!st.ok) toast('Nicht gefunden. Läuft Google Drive für Desktop?', true); };
  $('#cChoose').onclick = async () => { await api.choose(); refresh(); };
  $('#cChange').onclick = async () => { await api.choose(); refresh(); };
  $('#cFixBtn').onclick = async () => { const r = await api.ensure(); r.ok ? toast(r.data + ' Ordner angelegt.') : toast(r.error, true); refresh(); };
  $('#cReveal').onclick = async () => { const r = await api.reveal(sel || cur); if (!r.ok) toast(r.error, true); };
  const dn = $('#dlgNew');
  $('#cNew').onclick = () => { $('#newName').value = ''; $('#newIn').textContent = 'in ' + label(cur); dn.showModal(); $('#newName').focus(); };
  $('#newCancel').onclick = () => dn.close();
  $('#newForm').addEventListener('submit', async ev => {
    ev.preventDefault(); const n = $('#newName').value.trim(); if (!n) return;
    const r = await api.mkdir(cur, n); dn.close();
    if (!r.ok) { toast(r.error, true); return; }
    toast('Ordner angelegt.'); if (cur) open.add(cur); refresh();
  });

  // ---------- Start ----------
  (async () => {
    await loadStatus();
    let v = 'board'; try { v = localStorage.getItem('hq-view') || 'board'; } catch {}
    if (v === 'cloud') show('cloud');
  })();
})();
