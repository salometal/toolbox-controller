/* ==========================================================================
   ATOM COMPANION — script.js v3
   ========================================================================== */

// ==========================================================================
// 1. STATO GLOBALE
// ==========================================================================
const STATE = {
  mode: 0,
  masterHost: null,       // es. "regia.local"
  masterName: null,       // es. "regia"
  blackoutActive: false,

  // CueList
  allLists: [],
  activeListIndex: -1,
  currentCueIndex: 0,
  autogoTimer: null,
  autogoInterval: null,

  // Snap Recall
  snapList: [],
  currentSnapIndex: 0,

  // Override
  overrideLog: [],

  // Tally config
  tallyConfig: {
    params: ['running','mode','blackout','scene','artnet'],
    autoplay: false,
    interval: 5,
  },

  // Brightness
  brightness: 128,

  // Dati tally live
  tallyData: {},
};

function activeList() { return STATE.allLists[STATE.activeListIndex] || null; }
function activeCues()  { return activeList()?.cues || []; }
function activeCue()   { return activeCues()[STATE.currentCueIndex] || null; }
function nextCue()     { return activeCues()[STATE.currentCueIndex + 1] || null; }

// ==========================================================================
// 2. INIT
// ==========================================================================
document.addEventListener('DOMContentLoaded', () => {
  showTab('pane-network');
  loadConfig();
  updateStatus();
  setInterval(updateStatus, 3000);
  setInterval(() => { if (STATE.mode === 3) fetchTallyData(); }, 2000);

  // Scan WiFi al boot — aggiunto qui
  // Scan WiFi al boot — con delay
setTimeout(() => {
  fetch('/wifi_scan')
    .then(r => r.text())
    .then(data => {
      const sel = document.getElementById('wifi-dropdown');
      if (!sel || !data) return;
      sel.innerHTML = '<option value="">Seleziona rete...</option>';
      data.split(',').forEach(entry => {
        const [ssid, rssi] = entry.split('|');
        if (!ssid) return;
        const opt = document.createElement('option');
        opt.value = ssid;
        opt.text  = ssid + (rssi ? ' (' + rssi + ' dBm)' : '');
        sel.appendChild(opt);
      });
    })
    .catch(() => {});
}, 500);
});

// ==========================================================================
// 3. TAB
// ==========================================================================
function showTab(id) {
  document.querySelectorAll('.card').forEach(c => c.classList.remove('active'));
  document.querySelectorAll('nav button').forEach(b => b.classList.remove('active'));

  const card = document.getElementById(id);
  if (card) card.classList.add('active');

  const btnMap = {
    'pane-network': 'btn-network',
    'pane-live':    'btn-live',
    'pane-cuelist': 'btn-cuelist',
    'pane-setup':   'btn-setup',
  };
  const btn = document.getElementById(btnMap[id]);
  if (btn) btn.classList.add('active');

  if (id === 'pane-cuelist') renderCueListEditor();
  if (id === 'pane-live')    updateLivePanel();
}

// ==========================================================================
// 4. HEADER
// ==========================================================================
function toggleHeader() {
  document.getElementById('main-header').classList.toggle('expanded');
}

// ==========================================================================
// 5. STATUS POLLING
// ==========================================================================
function updateStatus() {
  fetch('/status')
    .then(r => r.json())
    .then(data => { if (data) applyStatus(data); })
    .catch(() => {}); // silenzioso se non raggiungibile
}

function applyStatus(data) {
  setBadge('wifi-badge',   'WIFI: '   + (data.wifi || '--'));
  setBadge('mode-badge',   'MODE: '   + modeLabel(STATE.mode));
  setBadge('master-badge', 'MASTER: ' + (data.masterName || STATE.masterName || '--'));
  setText('val-ip',     data.ip     || '--');
  setText('val-up',     formatUptime(data.uptime || 0));
  setText('val-heap',   (data.heap  || '--') + ' KB');
  setText('val-master', STATE.masterName || '--');
  setText('current-ip', data.ip     || '--');
}

function setBadge(id, text) { const el = document.getElementById(id); if (el) el.innerText = text; }
function setText(id, text)  { const el = document.getElementById(id); if (el) el.innerText = text; }

function formatUptime(s) {
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = s % 60;
  return [h, m, sec].map(v => String(v).padStart(2, '0')).join(':');
}

function modeLabel(m) {
  return ['--', 'CUE LIST', 'BLACKOUT', 'TALLY', 'SNAP', 'OVERRIDE'][m] || '--';
}

// ==========================================================================
// 6. NETWORK
// ==========================================================================
function toggleIPFields() {
  const dhcp = document.getElementById('dhcp-flag').checked;
  document.getElementById('static-fields').style.display = dhcp ? 'none' : 'block';
}

function selectNetwork(ssid) {
  const el = document.getElementById('ssid-input');
  if (el) el.value = ssid;
}

function validateHostname(input) {
  input.value = input.value.toLowerCase().replace(/[^a-z0-9-]/g, '');
  const preview = document.getElementById('hostname-preview');
  if (preview) preview.innerText = 'http://' + (input.value || 'atomcompanion') + '.local';
}

function saveNetwork() {
  const ssid = document.getElementById('ssid-input')?.value || '';
  const pass = document.getElementById('pwd-input')?.value  || '';
  const host = document.getElementById('hostname-input')?.value || 'atomcompanion';
  const dhcp = document.getElementById('dhcp-flag')?.checked;
  const ip   = document.getElementById('static-ip')?.value  || '';
  const gw   = document.getElementById('static-gw')?.value  || '';
  const sn   = document.getElementById('static-sn')?.value  || '';

  const url = `/connect?s=${encodeURIComponent(ssid)}&p=${encodeURIComponent(pass)}&h=${encodeURIComponent(host)}&dhcp=${dhcp ? 1 : 0}&ip=${ip}&gw=${gw}&sn=${sn}`;

  fetch(url)
    .then(() => alert('Configurazione salvata. Riavvio in corso...'))
    .catch(() => alert('Salvato (firmware non disponibile)'));
}



// ==========================================================================
// 7. MODALITÀ OPERATIVA
// ==========================================================================
let pendingMode = 0;

function selectMode(m) {
  pendingMode = m;
  document.querySelectorAll('.mode-btn').forEach(btn =>
    btn.classList.toggle('selected', parseInt(btn.dataset.mode) === m)
  );
}

function saveMode() {
  STATE.mode = pendingMode;
  applyMode(STATE.mode);
  persistConfig();
}

function applyMode(m) {
  STATE.mode = m;

  // Aggiorna badge nav
  setBadge('mode-badge', 'MODE: ' + modeLabel(m));

  // Aggiorna mode display nel live
  const icons  = ['●','📋','⬛','👁','🎬','📸'];
  const labels = ['NESSUNA MODALITÀ','CUE LIST','BLACKOUT','TALLY MONITOR','SNAP RECALL','SCENE OVERRIDE'];
  const subs   = [
    'Configura una modalità nel SETUP',
    'Controllo cue list sul Toolbox',
    'Toggle blackout su rete',
    'Monitoraggio stato Toolbox',
    'Scorrimento snapshot Toolbox',
    'Cattura stato DMX live',
  ];

  setText('live-mode-icon',  icons[m]  || '●');
  setText('live-mode-label', labels[m] || '--');
  setText('live-mode-sub',   subs[m]   || '');

  // Mostra pannello corretto
  ['ctx-cuelist','ctx-blackout','ctx-tally','ctx-snaprecall','ctx-override']
    .forEach(p => document.getElementById(p).style.display = 'none');
  const ctxMap = {1:'ctx-cuelist', 2:'ctx-blackout', 3:'ctx-tally', 4:'ctx-snaprecall', 5:'ctx-override'};
  if (ctxMap[m]) document.getElementById(ctxMap[m]).style.display = 'block';

  // Sync pulsanti setup
  document.querySelectorAll('.mode-btn').forEach(btn =>
    btn.classList.toggle('selected', parseInt(btn.dataset.mode) === m)
  );

  // Azioni specifiche
  if (m === 1) { refreshListSelects(); updateLiveCueUI(); }
  if (m === 3) { renderTallyGrid(); fetchTallyData(); }
  if (m === 4) { loadSnapList(); }
}

function updateLivePanel() {
  applyMode(STATE.mode);
  // Aggiorna master pill
  const pill = document.getElementById('live-master-pill');
  if (pill) pill.innerText = STATE.masterName || '--';
}

// ==========================================================================
// 8. MASTER DEVICE — DISCOVERY
// ==========================================================================
function scanToolbox() {
  const list = document.getElementById('master-discovery-list');
  if (list) list.innerHTML = '<div class="empty-msg" style="padding:8px;">Scansione...</div>';

  fetch('/scan')
    .then(r => r.json())
    .then(renderMasterList)
    .catch(() => {
      const list = document.getElementById('master-discovery-list');
      if (list) list.innerHTML = '<div class="empty-msg" style="padding:8px;">Errore scansione</div>';
    });
}

function renderMasterList(nodes) {
  const list = document.getElementById('master-discovery-list');
  if (!list) return;
  if (!nodes || nodes.length === 0) {
    list.innerHTML = '<div class="empty-msg" style="padding:8px;">Nessun Toolbox trovato</div>';
    return;
  }
  list.innerHTML = nodes.map(n => `
    <div class="master-item ${STATE.masterHost === n.ip ? 'selected' : ''}"
         onclick="selectMaster('${n.host}','${n.name}','${n.ip}')">
      <div style="flex:1; min-width:0;">
        <div class="master-name">${n.name}</div>
        <div class="master-host">${n.host} — ${n.ip}</div>
      </div>
      ${STATE.masterHost === n.ip ? '<span class="master-check">✓</span>' : ''}
    </div>
  `).join('');
}

function selectMaster(host, name, ip) {
  STATE.masterHost = ip || host;
  STATE.masterName = name;
  STATE.masterHostname = host;

  setBadge('master-badge', 'MASTER: ' + name);
  setText('val-master', name);

  const pill = document.getElementById('live-master-pill');
  if (pill) pill.innerText = name;

  persistConfig();
  renderMasterList([{ host, name, ip }]);
  loadSnapListSilent();
}

// ==========================================================================
// 9. CUE LIST — GESTIONE LISTE
// ==========================================================================
function refreshListSelects() {
  ['cl-list-select', 'live-cl-select'].forEach(selId => {
    const sel = document.getElementById(selId);
    if (!sel) return;
    sel.innerHTML = '<option value="">-- seleziona lista --</option>';
    STATE.allLists.forEach((l, i) => {
      const opt = document.createElement('option');
      opt.value = i;
      opt.text  = l.name || 'Lista ' + (i + 1);
      sel.appendChild(opt);
    });
    if (STATE.activeListIndex >= 0) sel.value = STATE.activeListIndex;
  });
}

function switchCueList(idx) {
  idx = parseInt(idx);
  if (isNaN(idx) || idx < 0) return;
  STATE.activeListIndex  = idx;
  STATE.currentCueIndex  = 0;
  cancelAutogo();
  refreshListSelects();
  renderCueListEditor();
  updateLiveCueUI();
}

function loadCueListByIndex(idx) { switchCueList(idx); }

function newCueList() {
  const name = prompt('Nome nuova lista (es. Atto 1):');
  if (!name) return;
  STATE.allLists.push({ name: name.trim(), cues: [] });
  STATE.activeListIndex = STATE.allLists.length - 1;
  refreshListSelects();
  renderCueListEditor();
  saveCueList();
}

function renameCueList() {
  const list = activeList();
  if (!list) { alert('Seleziona prima una lista.'); return; }
  const name = prompt('Nuovo nome:', list.name);
  if (!name) return;
  list.name = name.trim();
  refreshListSelects();
  saveCueList();
}

function deleteCueList() {
  const list = activeList();
  if (!list) return;
  if (!confirm('Eliminare "' + list.name + '"?')) return;
  STATE.allLists.splice(STATE.activeListIndex, 1);
  STATE.activeListIndex  = STATE.allLists.length > 0 ? 0 : -1;
  STATE.currentCueIndex  = 0;
  cancelAutogo();
  refreshListSelects();
  renderCueListEditor();
  saveCueList();
}

// ==========================================================================
// 10. CUE LIST — RENDER EDITOR
// ==========================================================================
function renderCueListEditor() {
  const container = document.getElementById('cue-list-container');
  const empty     = document.getElementById('cue-empty');
  const cues      = activeCues();

  if (STATE.activeListIndex < 0 || cues.length === 0) {
    container.innerHTML = '';
    if (empty) { container.appendChild(empty); empty.style.display = 'block'; }
    return;
  }
  if (empty) empty.style.display = 'none';
  container.innerHTML = '';

  cues.forEach((cue, idx) => {
    const el      = document.createElement('div');
    el.className  = 'cue-item' + (idx === editingCueIdx ? ' editing' : '');
    el.id         = 'cue-item-' + idx;

    const goBadge = cue.go === 'auto'
      ? `<span class="cue-item-badge badge-auto">AUTO ${cue.autogoDelay || 0}s</span>`
      : `<span class="cue-item-badge badge-manual">MANUAL</span>`;

    const snapName = cue.snap !== '' && cue.snap !== undefined
      ? (STATE.snapList.find(s => String(s.id) === String(cue.snap))?.name || 'Snap ' + cue.snap)
      : '—';

    el.innerHTML = `
      <div class="cue-item-stripe" style="background:${cue.color || '#7c3aed'};"></div>
      <div class="cue-item-num">${String(idx + 1).padStart(2, '0')}</div>
      <div class="cue-item-body">
        <div class="cue-item-top">
          <span class="cue-item-label">${cue.label || 'CUE ' + (idx + 1)}</span>
          ${goBadge}
        </div>
        <div class="cue-item-meta">
          <span>📸 ${snapName}</span>
          <span>⏱ ${cue.fade || 0}s</span>
        </div>
      </div>
    `;
    el.onclick = () => editCue(idx);
    container.appendChild(el);
  });
}

// ==========================================================================
// 11. CUE LIST — EDITOR SINGOLA CUE
// ==========================================================================
let editingCueIdx = -1;
let editingGoMode = 'manual';

function editCue(idx) {
  if (!activeList()) return;
  editingCueIdx = idx;
  const cue = activeCues()[idx];
  if (!cue) return;

  document.getElementById('edit-cue-label').value  = cue.label || '';
  document.getElementById('edit-cue-fade').value   = cue.fade  || 0;
  document.getElementById('edit-cue-color').value  = cue.color || '#7c3aed';
  document.getElementById('edit-cue-color-preview').style.background = cue.color || '#7c3aed';
  document.getElementById('edit-cue-autogo').value = cue.autogoDelay || 5;

  const sel = document.getElementById('edit-cue-snap');
  sel.innerHTML = '<option value="">-- nessuno --</option>';
  STATE.snapList.forEach(s => {
    const opt = document.createElement('option');
    opt.value = s.id;
    opt.text  = s.name || 'Snap ' + s.id;
    if (String(s.id) === String(cue.snap)) opt.selected = true;
    sel.appendChild(opt);
  });

  selectGoMode(cue.go || 'manual');
  setText('editor-title-label', 'MODIFICA CUE ' + String(idx + 1).padStart(2, '0'));
  document.getElementById('cue-editor').style.display = 'block';

  if (window.innerWidth < 768) {
    document.getElementById('cue-editor').scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }
  renderCueListEditor();
}

function addCue() {
  if (!activeList()) { alert('Crea prima una lista.'); return; }
  activeList().cues.push({
    label: 'CUE ' + (activeCues().length + 1),
    snap: '', fade: 0, color: '#7c3aed', go: 'manual', autogoDelay: 5,
  });
  editCue(activeList().cues.length - 1);
}

function selectGoMode(mode) {
  editingGoMode = mode;
  document.querySelectorAll('.go-mode-opt').forEach(el =>
    el.classList.toggle('selected', el.dataset.go === mode)
  );
  document.getElementById('autogo-delay-group').style.display = mode === 'auto' ? 'block' : 'none';
}

function updateColorPreview() {
  const val = document.getElementById('edit-cue-color').value;
  document.getElementById('edit-cue-color-preview').style.background = val;
}

function saveCueEdit() {
  if (editingCueIdx < 0 || !activeList()) return;
  activeList().cues[editingCueIdx] = {
    label:       document.getElementById('edit-cue-label').value || 'CUE ' + (editingCueIdx + 1),
    snap:        document.getElementById('edit-cue-snap').value,
    fade:        parseFloat(document.getElementById('edit-cue-fade').value) || 0,
    color:       document.getElementById('edit-cue-color').value,
    go:          editingGoMode,
    autogoDelay: parseFloat(document.getElementById('edit-cue-autogo').value) || 5,
  };
  closeEditor();
  renderCueListEditor();
  updateLiveCueUI();
}

function deleteCueItem() {
  if (editingCueIdx < 0 || !activeList()) return;
  if (!confirm('Eliminare questa cue?')) return;
  activeList().cues.splice(editingCueIdx, 1);
  if (STATE.currentCueIndex >= activeCues().length)
    STATE.currentCueIndex = Math.max(0, activeCues().length - 1);
  closeEditor();
  renderCueListEditor();
  updateLiveCueUI();
}

function cancelCueEdit() { closeEditor(); renderCueListEditor(); }

function closeEditor() {
  editingCueIdx = -1;
  if (window.innerWidth < 768)
    document.getElementById('cue-editor').style.display = 'none';
}

// ==========================================================================
// 12. CUE LIST — LIVE
// ==========================================================================
function updateLiveCueUI() {
  const cue  = activeCue();
  const next = nextCue();
  const cues = activeCues();

  const snapName = (c) => {
    if (!c || c.snap === '' || c.snap === undefined) return '--';
    return STATE.snapList.find(s => String(s.id) === String(c.snap))?.name || 'Snap ' + c.snap;
  };

  setText('tally-cue-num',  cue  ? String(STATE.currentCueIndex + 1).padStart(2,'0') : '--');
  setText('tally-cue-name', cue  ? (cue.label  || 'CUE ' + (STATE.currentCueIndex + 1)) : '--');
  setText('tally-cue-meta', cue  ? ('📸 ' + snapName(cue) + '  ⏱ ' + (cue.fade || 0) + 's') : '--');
  setText('tally-next-num', next ? String(STATE.currentCueIndex + 2).padStart(2,'0') : '--');
  setText('tally-next-name',next ? (next.label || 'CUE ' + (STATE.currentCueIndex + 2)) : '— fine lista —');
  setText('tally-next-meta',next ? ('⏱ ' + (next.fade || 0) + 's' + (next.go === 'auto' ? ' · auto ' + next.autogoDelay + 's' : '')) : '');
  setText('cue-counter',    cues.length ? (STATE.currentCueIndex + 1) + ' / ' + cues.length : '0 / 0');

  // Stripe colore sulla riga attiva
  const activeRow = document.getElementById('ctx-cuelist')?.querySelector('.active-row');
  if (activeRow && cue?.color) {
    activeRow.style.borderColor = cue.color;
    activeRow.style.background  = cue.color + '22';
  }
}

function cueNext() {
  cancelAutogo();
  if (STATE.currentCueIndex < activeCues().length - 1) {
    STATE.currentCueIndex++;
    updateLiveCueUI();
    const c = activeCue();
    if (c?.go === 'auto') startAutogo(c.autogoDelay || 5);
  }
}

function cuePrev() {
  cancelAutogo();
  if (STATE.currentCueIndex > 0) {
    STATE.currentCueIndex--;
    updateLiveCueUI();
  }
}

// Il GO viene dal tasto fisico — qui gestiamo solo il recall snap
function executeCueGo() {
  const cue = activeCue();
  if (!cue || !STATE.masterHost) return;
  if (cue.snap !== '' && cue.snap !== undefined) {
    fetch(`http://${STATE.masterHost}/api/snap/recall?id=${cue.snap}&fade=${cue.fade}`).catch(() => {});
  }
  if (STATE.currentCueIndex < activeCues().length - 1) {
    STATE.currentCueIndex++;
    updateLiveCueUI();
    const next = activeCue();
    if (next?.go === 'auto') startAutogo(next.autogoDelay || 5);
  }
}

function startAutogo(delaySec) {
  const wrap = document.getElementById('autogo-bar-wrap');
  const fill = document.getElementById('autogo-fill');
  const cd   = document.getElementById('autogo-countdown');
  if (wrap) wrap.style.display = 'block';
  if (fill) fill.style.width   = '100%';
  let remaining = delaySec;
  if (cd) cd.innerText = remaining;

  STATE.autogoInterval = setInterval(() => {
    remaining--;
    if (cd) cd.innerText = Math.max(0, remaining);
    if (fill) fill.style.width = ((remaining / delaySec) * 100) + '%';
    if (remaining <= 0) { clearInterval(STATE.autogoInterval); STATE.autogoInterval = null; }
  }, 1000);

  STATE.autogoTimer = setTimeout(() => {
    cancelAutogo();
    executeCueGo();
  }, delaySec * 1000);
}

function cancelAutogo() {
  if (STATE.autogoTimer)    { clearTimeout(STATE.autogoTimer);    STATE.autogoTimer    = null; }
  if (STATE.autogoInterval) { clearInterval(STATE.autogoInterval); STATE.autogoInterval = null; }
  const wrap = document.getElementById('autogo-bar-wrap');
  if (wrap) wrap.style.display = 'none';
}

function saveCueList() {
  fetch('/api/cuelist', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(STATE.allLists),
  }).then(() => console.log('[CL] Salvato.')).catch(() => {});
}

function loadCurrentCueList() {
  fetch('/api/cuelist')
    .then(r => r.json())
    .then(data => {
      STATE.allLists = Array.isArray(data) ? data : [];
      if (STATE.allLists.length > 0 && STATE.activeListIndex < 0) STATE.activeListIndex = 0;
      refreshListSelects();
      renderCueListEditor();
      updateLiveCueUI();
    })
    .catch(() => {
      if (STATE.allLists.length === 0) {
        STATE.allLists = [{
          name: 'Atto 1 (esempio)',
          cues: [
            { label:'Preshow',   snap:'0', fade:2,   color:'#1d4ed8', go:'manual', autogoDelay:0 },
            { label:'Apertura',  snap:'1', fade:3,   color:'#7c3aed', go:'manual', autogoDelay:0 },
            { label:'Scena 1',   snap:'2', fade:1.5, color:'#065f46', go:'auto',   autogoDelay:10 },
            { label:'Scena 2',   snap:'3', fade:2,   color:'#92400e', go:'manual', autogoDelay:0 },
            { label:'Buio',      snap:'4', fade:4,   color:'#1f2937', go:'manual', autogoDelay:0 },
          ],
        }];
        STATE.activeListIndex = 0;
        refreshListSelects();
        renderCueListEditor();
        updateLiveCueUI();
      }
    });
}

// ==========================================================================
// 13. BLACKOUT
// ==========================================================================
function toggleBlackout() {
  STATE.blackoutActive = !STATE.blackoutActive;
  const ind = document.getElementById('bo-indicator');
  if (ind) {
    ind.innerText = STATE.blackoutActive ? 'ON' : 'OFF';
    ind.className = 'bo-indicator' + (STATE.blackoutActive ? ' active' : '');
  }
  if (STATE.masterHost) {
    const url = STATE.blackoutActive
      ? `http://${STATE.masterHost}/api/blackout`
      : `http://${STATE.masterHost}/api/snap/release`;
    fetch(url).catch(() => {});
  }
}

// ==========================================================================
// 14. TALLY
// ==========================================================================
const TALLY_PARAM_DEFS = {
  running:   { label: 'STATO',     format: v => v ? 'LIVE'        : 'FERMO',      ok: v => v },
  mode:      { label: 'MODALITÀ',  format: v => (['--','DMX→ART','ART→DMX','STANDALONE'][v] || '--'), ok: () => true },
  blackout:  { label: 'BLACKOUT',  format: v => v ? '⬛ ON'        : 'OFF',         ok: v => !v },
  scene:     { label: 'SCENA',     format: v => v ? 'ATTIVA'      : 'NO',          ok: v => v },
  artnet:    { label: 'ARTNET',    format: v => v ? 'SEGNALE OK'  : 'NO SEGNALE',  ok: v => v },
  crossfade: { label: 'CROSSFADE', format: v => v ? 'IN CORSO'    : '--',          ok: v => v },
  universe:  { label: 'UNIVERSO',  format: v => v !== undefined ? String(v) : '--', ok: () => true },
  refresh:   { label: 'REFRESH',   format: v => v ? v + ' Hz'    : '--',           ok: () => true },
};

function renderTallyGrid() {
  const grid = document.getElementById('tally-params-grid');
  if (!grid) return;
  grid.innerHTML = '';
  STATE.tallyConfig.params.forEach(key => {
    const def = TALLY_PARAM_DEFS[key];
    if (!def) return;
    const row = document.createElement('div');
    row.className = 'tally-row';
    row.innerHTML = `<span class="tally-label">${def.label}</span><span class="tally-val" id="tv-${key}">--</span>`;
    grid.appendChild(row);
  });
  if (Object.keys(STATE.tallyData).length > 0) applyTallyData(STATE.tallyData);
}

function fetchTallyData() {
  if (!STATE.masterHost) return;
  fetch(`http://${STATE.masterHost}/api/status`)
    .then(r => r.json())
    .then(data => { STATE.tallyData = data; applyTallyData(data); updateBlackoutTally(data); })
    .catch(() => {});
}

function applyTallyData(data) {
  STATE.tallyConfig.params.forEach(key => {
    const el  = document.getElementById('tv-' + key);
    const def = TALLY_PARAM_DEFS[key];
    if (!el || !def) return;
    const rawVal = data[key === 'scene' ? 'sceneActive' : key === 'artnet' ? 'artnetConfirmed' : key === 'refresh' ? 'refreshRate' : key];
    el.innerText  = def.format(rawVal);
    el.style.color = def.ok(rawVal) ? 'var(--accent)' : '#888';
  });
}

function updateBlackoutTally(data) {
  const modes = ['--','DMX→ART','ART→DMX','STANDALONE'];
  setText('bo-toolbox-state', data.running ? 'LIVE' : 'FERMO');
  setText('bo-toolbox-mode',  modes[data.mode] || '--');
}

// ==========================================================================
// 15. SNAP RECALL
// ==========================================================================
function loadSnapList() {
  if (!STATE.masterHost) return;
  fetch(`http://${STATE.masterHost}/api/snapshots`)
    .then(r => r.json())
    .then(data => { STATE.snapList = data; renderSnapLive(); })
    .catch(() => {
      STATE.snapList = Array.from({length: 8}, (_, i) => ({ id: i, name: 'Snap ' + (i + 1) }));
      renderSnapLive();
    });
}

function loadSnapListSilent() {
  if (!STATE.masterHost) return;
  fetch(`http://${STATE.masterHost}/api/snapshots`)
    .then(r => r.json())
    .then(data => { STATE.snapList = data; })
    .catch(() => {
      STATE.snapList = Array.from({length: 8}, (_, i) => ({ id: i, name: 'Snap ' + (i + 1) }));
    });
}

function renderSnapLive() {
  const prev = STATE.snapList[STATE.currentSnapIndex - 1];
  const curr = STATE.snapList[STATE.currentSnapIndex];
  const next = STATE.snapList[STATE.currentSnapIndex + 1];

  const fmt = (s, idx) => s ? (s.name || 'Snap ' + (idx + 1)) : '--';
  const num = (idx) => idx >= 0 && idx < STATE.snapList.length ? String(idx + 1).padStart(2,'0') : '--';

  setText('snap-prev-num',  num(STATE.currentSnapIndex - 1));
  setText('snap-prev-name', fmt(prev, STATE.currentSnapIndex - 1));
  setText('snap-curr-num',  num(STATE.currentSnapIndex));
  setText('snap-curr-name', fmt(curr, STATE.currentSnapIndex));
  setText('snap-next-num',  num(STATE.currentSnapIndex + 1));
  setText('snap-next-name', fmt(next, STATE.currentSnapIndex + 1));
  setText('snap-counter',   STATE.snapList.length
    ? (STATE.currentSnapIndex + 1) + ' / ' + STATE.snapList.length : '0 / 0');

  // Colore badge snap corrente
  const badge = document.getElementById('snap-curr-badge');
  if (badge) badge.style.background = 'var(--success)';
}

function recallSnap(idx) {
  STATE.currentSnapIndex = idx;
  const snap = STATE.snapList[idx];
  if (!snap) return;
  renderSnapLive();
  if (STATE.masterHost)
    fetch(`http://${STATE.masterHost}/api/snap/recall?id=${snap.id}`).catch(() => {});
}

function snapNext() {
  if (STATE.currentSnapIndex < STATE.snapList.length - 1)
    recallSnap(STATE.currentSnapIndex + 1);
}
function snapPrev() {
  if (STATE.currentSnapIndex > 0)
    recallSnap(STATE.currentSnapIndex - 1);
}

// ==========================================================================
// 16. SCENE OVERRIDE
// ==========================================================================
function logOverride(msg) {
  const ts = new Date().toLocaleTimeString('it-IT', { hour:'2-digit', minute:'2-digit', second:'2-digit' });
  STATE.overrideLog.unshift({ msg, ts });
  const log = document.getElementById('override-log');
  if (log) log.innerHTML = STATE.overrideLog.length
    ? STATE.overrideLog.map(e => `<div class="log-entry">${e.ts} — ${e.msg}</div>`).join('')
    : '<div class="log-empty">Nessun salvataggio in questa sessione</div>';
}

// Chiamata dal firmware quando il tasto fisico fa long press
function onPhysicalCapture() {
  if (!STATE.masterHost) { logOverride('NO MASTER — configurare prima'); return; }
  fetch(`http://${STATE.masterHost}/api/snap/capture`)
    .then(r => r.json())
    .then(data => logOverride('Salvato: ' + (data.name || '?') + ' (slot ' + data.slot + ')'))
    .catch(() => logOverride('Salvato (mock): CAP_' + String(STATE.overrideLog.length + 1).padStart(2,'0')));
}

// ==========================================================================
// 17. SETUP — BRIGHTNESS
// ==========================================================================
function updateBrightness(val) {
  setText('brightness-display', val);
  STATE.brightness = parseInt(val);
}

function saveBrightness() {
  fetch('/api/brightness?val=' + STATE.brightness).catch(() => {});
  persistConfig();
}

// ==========================================================================
// 18. SETUP — TALLY CONFIG (lista ordinabile)
// ==========================================================================

// Tutti i parametri disponibili nell'ordine di default
const TALLY_ALL_PARAMS = [
  { key: 'running',   label: 'Stato Running (Live/Fermo)' },
  { key: 'mode',      label: 'Modalità Toolbox' },
  { key: 'blackout',  label: 'Blackout attivo' },
  { key: 'scene',     label: 'Scena attiva' },
  { key: 'artnet',    label: 'ArtNet confermato' },
  { key: 'crossfade', label: 'Crossfade in corso' },
  { key: 'universe',  label: 'Universo' },
  { key: 'refresh',   label: 'Refresh Rate' },
];

// Stato interno dell'editor — array di {key, label, enabled}
let tallyEditorItems = [];

function initTallyEditor() {
  // Costruisce la lista a partire da STATE.tallyConfig.params (ordine + abilitati)
  // Prima i parametri abilitati nell'ordine salvato, poi i disabilitati
  const enabled  = STATE.tallyConfig.params;
  const ordered  = [];

  // Abilitati nell'ordine salvato
  enabled.forEach(key => {
    const def = TALLY_ALL_PARAMS.find(p => p.key === key);
    if (def) ordered.push({ ...def, enabled: true });
  });

  // Disabilitati — quelli non presenti in enabled
  TALLY_ALL_PARAMS.forEach(def => {
    if (!enabled.includes(def.key)) ordered.push({ ...def, enabled: false });
  });

  tallyEditorItems = ordered;
  renderTallyEditor();
}

function renderTallyEditor() {
  const container = document.getElementById('tally-param-list-editor');
  if (!container) return;

  container.innerHTML = '';
  const list = document.createElement('div');
  list.className = 'tally-sortable';

  tallyEditorItems.forEach((item, idx) => {
    const row = document.createElement('div');
    row.className = 'tally-sort-item ' + (item.enabled ? 'enabled' : 'disabled');
    row.dataset.key = item.key;

    row.innerHTML = `
      <input type="checkbox" class="tally-sort-check" ${item.enabled ? 'checked' : ''}
             onchange="toggleTallyItem('${item.key}', this.checked)">
      <span class="tally-sort-label" onclick="toggleTallyItem('${item.key}', ${!item.enabled}); this.previousElementSibling.checked=${!item.enabled}">
        ${item.label}
      </span>
      <div class="tally-sort-btns">
        <button class="tally-sort-btn" onclick="moveTallyItem(${idx}, -1)" ${idx === 0 ? 'disabled' : ''} title="Su">▲</button>
        <button class="tally-sort-btn" onclick="moveTallyItem(${idx}, +1)" ${idx === tallyEditorItems.length - 1 ? 'disabled' : ''} title="Giù">▼</button>
      </div>
    `;
    list.appendChild(row);
  });

  container.appendChild(list);
}

function toggleTallyItem(key, enabled) {
  const item = tallyEditorItems.find(i => i.key === key);
  if (item) {
    item.enabled = enabled;
    renderTallyEditor();
  }
}

function moveTallyItem(idx, direction) {
  const newIdx = idx + direction;
  if (newIdx < 0 || newIdx >= tallyEditorItems.length) return;
  // Swap
  [tallyEditorItems[idx], tallyEditorItems[newIdx]] =
  [tallyEditorItems[newIdx], tallyEditorItems[idx]];
  renderTallyEditor();
}

function toggleAutoplay() {
  const on = document.getElementById('tally-autoplay').checked;
  document.getElementById('tally-interval-group').style.display = on ? 'block' : 'none';
}

function saveTallyConfig() {
  // Prende solo i parametri abilitati nell'ordine corrente
  const params = tallyEditorItems.filter(i => i.enabled).map(i => i.key);

  STATE.tallyConfig = {
    params,
    autoplay: document.getElementById('tally-autoplay').checked,
    interval: parseInt(document.getElementById('tally-interval').value) || 5,
  };

  setText('tally-interval-display', STATE.tallyConfig.interval);
  renderTallyGrid();
  persistConfig();
  alert('Config Tally salvata.');
}

function loadTallyConfigUI() {
  const cfg = STATE.tallyConfig;
  initTallyEditor();

  const autoplayEl = document.getElementById('tally-autoplay');
  if (autoplayEl) autoplayEl.checked = cfg.autoplay;
  const intervalEl = document.getElementById('tally-interval');
  if (intervalEl) intervalEl.value = cfg.interval;
  document.getElementById('tally-interval-group').style.display = cfg.autoplay ? 'block' : 'none';
  setText('tally-interval-display', cfg.interval);
}

// ==========================================================================
// 19. ACCORDION
// ==========================================================================
function toggleAccordion(id) {
  const body  = document.getElementById(id);
  const arrow = document.getElementById('arr-' + id);
  const header = arrow?.closest('.accordion-header');
  if (!body) return;

  const isOpen = body.style.display !== 'none';
  body.style.display  = isOpen ? 'none' : 'block';
  if (arrow)  arrow.classList.toggle('open', !isOpen);
  if (header) header.classList.toggle('open', !isOpen);
}

// ==========================================================================
// 20. PERSISTENZA CONFIG
// ==========================================================================
function persistConfig() {
  const cfg = {
    mode:        STATE.mode,
    masterHost:  STATE.masterHost,
    masterName:  STATE.masterName,
    brightness:  STATE.brightness,
    tallyConfig: STATE.tallyConfig,
  };
  fetch('/api/config', {
    method:  'POST',
    headers: {'Content-Type': 'application/json'},
    body:    JSON.stringify(cfg),
  }).catch(() => {});
}

function loadConfig() {
  fetch('/api/config')
    .then(r => r.json())
    .then(data => {
      if (data.mode !== undefined) { STATE.mode = data.mode; }
      if (data.masterHost && data.masterHost !== 'null' && data.masterHost !== 'undefined') {
        STATE.masterHost = data.masterHost;
        loadSnapListSilent(); // ← spostata qui
      }
      if (data.masterName) { STATE.masterName = data.masterName; }
      if (data.brightness) { STATE.brightness = data.brightness; document.getElementById('brightness-slider').value = data.brightness; updateBrightness(data.brightness); }
      if (data.tallyConfig) { STATE.tallyConfig = data.tallyConfig; }
      if (data.hostname) {
        const hi = document.getElementById('hostname-input');
        if (hi) { hi.value = data.hostname; validateHostname(hi); }
      }
      applyMode(STATE.mode);
      loadTallyConfigUI();
      loadCurrentCueList();
      // loadSnapListSilent(); ← rimossa da qui

      if (STATE.mode > 0) selectMode(STATE.mode);
      const pill = document.getElementById('live-master-pill');
      if (pill) pill.innerText = STATE.masterName || '--';
    })
    .catch(() => {
      applyMode(0);
      loadCurrentCueList();
      loadTallyConfigUI();
    });
}

// ==========================================================================
// 21. UTILITY
// ==========================================================================
function identifyAtom() { fetch('/identify').catch(() => {}); }

function rebootAtom() {
  if (confirm('Riavviare ATOM Companion?')) {
    fetch('/restart').catch(() => {});
    setTimeout(() => location.reload(), 5000);
  }
}
