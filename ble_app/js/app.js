import { createBlackboxLink, isBluetoothSupported } from './ble.js';
import { createParser, validatePhone, logToCsv } from './parser.js';
import { renderLineChart } from './chart.js';

const POLL_INTERVAL_MS = 2000;
const LOG_TIMEOUT_MS = 6000;
const CONFIRM_WINDOW_MS = 4000;
const RECONNECT_DELAYS_MS = [1000, 2000, 4000, 8000];
const CONSOLE_MAX_LINES = 400;

// Khớp crash_state của STM32 (README: 0, 1, 2, 3, 99)
const CRASH_STATES = {
  0: { text: 'Bình thường', level: 'good', icon: 'check' },
  1: { text: 'Phanh gấp', level: 'warning', icon: 'warn' },
  2: { text: 'Ngã xe', level: 'serious', icon: 'warn' },
  3: { text: 'Va chạm', level: 'critical', icon: 'alert' },
  99: { text: 'Lỗi cảm biến IMU', level: 'critical', icon: 'alert' },
};
const TAG_TEXT = { ok: 'OK', fall: 'Ngã', impact: 'Va chạm' };
const ICONS = {
  check: '<path d="M4 8.5 7 11l5-6" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>',
  warn: '<path d="M8 2 1.5 14h13L8 2Z" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round"/><path d="M8 7v3" stroke="currentColor" stroke-width="1.8" stroke-linecap="round"/>',
  alert: '<circle cx="8" cy="8" r="6.5" fill="currentColor"/><path d="M8 4.5v4M8 11v.3" stroke="var(--surface)" stroke-width="2" stroke-linecap="round"/>',
};

const $ = (id) => document.getElementById(id);

const state = {
  logRows: [],
  logPendingUntil: 0,
  userDisconnected: false,
  reconnectAttempt: 0,
  pollTimer: null,
};

/* ---------------- Tiện ích giao diện ---------------- */

let toastTimer = null;
function toast(message) {
  const node = $('toast');
  node.textContent = message;
  node.dataset.show = 'true';
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => { node.dataset.show = 'false'; }, 3200);
}

function consoleLine(text, kind) {
  const box = $('console');
  const line = document.createElement('div');
  line.className = kind;
  line.textContent = text;
  box.append(line);
  while (box.childElementCount > CONSOLE_MAX_LINES) box.firstElementChild.remove();
  box.scrollTop = box.scrollHeight;
}

// Nút nguy hiểm: bấm lần 1 để "lên đạn", bấm lần 2 trong 4 giây mới thực thi
function armedButton(button, armedLabel, action) {
  const idleLabel = button.textContent;
  let timer = null;
  const disarm = () => {
    clearTimeout(timer);
    button.dataset.armed = 'false';
    button.textContent = idleLabel;
  };
  button.addEventListener('click', () => {
    if (button.dataset.armed === 'true') {
      disarm();
      action();
      return;
    }
    button.dataset.armed = 'true';
    button.textContent = armedLabel;
    timer = setTimeout(disarm, CONFIRM_WINDOW_MS);
  });
}

function fmtSigned(v, digits = 1) {
  return `${v > 0 ? '+' : ''}${v.toFixed(digits)}`;
}

/* ---------------- Hiển thị dữ liệu ---------------- */

function drawGaugeTicks() {
  const group = $('gauge-ticks');
  const ns = 'http://www.w3.org/2000/svg';
  for (let deg = -90; deg <= 90; deg += 15) {
    const rad = (deg * Math.PI) / 180;
    const major = deg % 45 === 0;
    const r1 = major ? 112 : 118;
    const line = document.createElementNS(ns, 'line');
    line.setAttribute('x1', 160 + r1 * Math.sin(rad));
    line.setAttribute('y1', 170 - r1 * Math.cos(rad));
    line.setAttribute('x2', 160 + 126 * Math.sin(rad));
    line.setAttribute('y2', 170 - 126 * Math.cos(rad));
    line.setAttribute('stroke', 'var(--ink-3)');
    line.setAttribute('stroke-width', major ? 2 : 1);
    group.append(line);
    if (major) {
      const label = document.createElementNS(ns, 'text');
      label.setAttribute('x', 160 + 96 * Math.sin(rad));
      label.setAttribute('y', 174 - 96 * Math.cos(rad));
      label.setAttribute('text-anchor', 'middle');
      label.setAttribute('fill', 'var(--ink-3)');
      label.setAttribute('font-size', '11');
      label.setAttribute('font-family', 'var(--font-num)');
      label.textContent = `${Math.abs(deg)}°`;
      group.append(label);
    }
  }
}

function renderCrashState(code) {
  const info = CRASH_STATES[code];
  const chip = $('state-chip');
  chip.dataset.level = info?.level ?? 'none';
  chip.querySelector('svg').innerHTML = ICONS[info?.icon] ?? ICONS.check;
  $('state-text').textContent = info?.text ?? (code === null ? 'Firmware chưa gửi trạng thái' : `Mã ${code}`);
}

function renderStatus(s) {
  const roll = Math.max(-90, Math.min(90, s.roll ?? 0));
  $('needle').style.transform = `rotate(${roll}deg)`;
  $('roll-value').textContent = s.roll === null ? '—' : `${fmtSigned(s.roll)}°`;
  $('pitch-value').textContent = s.pitch === null ? '—' : `${fmtSigned(s.pitch)}°`;
  $('speed-value').textContent = s.speed === null ? '—' : s.speed.toFixed(0);
  $('gps-value').textContent = s.gpsFix === null ? '—' : `${s.gpsFix ? 'Đã fix' : 'Chưa fix'} · ${s.satellites} vệ tinh`;

  const pos = $('pos-value');
  pos.replaceChildren();
  if (s.gpsFix && s.lat !== null) {
    const a = document.createElement('a');
    a.href = `https://maps.google.com/?q=${s.lat},${s.lon}`;
    a.target = '_blank';
    a.rel = 'noopener';
    a.className = 'num';
    a.textContent = `${s.lat.toFixed(5)}, ${s.lon.toFixed(5)}`;
    pos.append(a);
  } else {
    pos.textContent = s.lat === null ? 'Firmware chưa gửi' : 'Chờ GPS fix';
  }

  if (s.phone) $('sos-current').textContent = s.phone;
  renderCrashState(s.crashState);
  $('live-updated').textContent = `Cập nhật ${new Date().toLocaleTimeString('vi-VN')}`;
}

function renderLog(rows) {
  state.logRows = rows;
  state.logPendingUntil = 0;
  const hasRows = rows.length > 0;
  $('log-empty').hidden = hasRows;
  $('log-view').hidden = !hasRows;
  $('btn-csv').disabled = !hasRows;
  $('log-empty').textContent = hasRows ? '' : 'Hộp đen trống — chưa ghi nhận vụ tai nạn nào.';

  const impact = rows.find((r) => r.t === 0);
  $('log-meta').textContent = impact ? `Đỉnh ${impact.g.toFixed(1)}G · nghiêng ${fmtSigned(impact.roll)}°` : '';
  if (!hasRows) return;

  renderLineChart($('chart-roll'), rows, { key: 'roll', unit: '°', includeZero: true });
  renderLineChart($('chart-g'), rows, { key: 'g', unit: 'G', includeZero: true });

  const body = $('log-rows');
  body.replaceChildren(...rows.map((r) => {
    const tr = document.createElement('tr');
    tr.dataset.tag = r.tag;
    const cells = [
      `T${r.t >= 0 ? '+' : ''}${r.t}s`,
      `${r.roll.toFixed(1)}°`,
      `${r.pitch.toFixed(1)}°`,
      `${r.g.toFixed(1)}G`,
      `${r.speed.toFixed(1)} km/h`,
      TAG_TEXT[r.tag],
    ];
    for (const [i, text] of cells.entries()) {
      const td = document.createElement('td');
      td.textContent = text;
      if (i < 5) td.className = 'num';
      tr.append(td);
    }
    return tr;
  }));
}

function showCrashAlert(text) {
  $('crash-alert-text').textContent = text;
  $('crash-alert').hidden = false;
  navigator.vibrate?.([400, 150, 400, 150, 800]);
  if ('Notification' in window && Notification.permission === 'granted') {
    new Notification('Hộp Đen: phát hiện tai nạn!', { body: text, tag: 'crash', requireInteraction: true });
  }
}

/* ---------------- Sự kiện từ hộp đen ---------------- */

function handleEvent({ type, data }) {
  switch (type) {
    case 'line': consoleLine(data, 'out'); break;
    case 'status': renderStatus(data); break;
    case 'log': renderLog(data); break;
    case 'phoneSaved':
      $('sos-current').textContent = data;
      $('sos-input').value = '';
      toast(`Đã lưu số SOS ${data}`);
      break;
    case 'error': toast(data); break;
    case 'cleared': renderLog([]); toast('Đã xóa hộp đen W25Q128'); break;
    case 'crash': showCrashAlert('Hộp đen đang gửi SMS SOS và ghi 5 giây sau va chạm.'); break;
    case 'testSos': toast('Đã kích hoạt thử SOS'); break;
    case 'logSaved':
      toast('Đã ghi xong băng 15 giây — đang tải về…');
      requestLog();
      break;
    default: break;
  }
}

const parser = createParser(handleEvent);

/* ---------------- Kết nối ---------------- */

const link = createBlackboxLink({
  onBytes: (bytes) => parser.pushBytes(bytes),
  onDisconnect: () => {
    setLinkState('idle');
    consoleLine('Mất kết nối', 'sys');
    if (!state.userDisconnected) scheduleReconnect();
  },
});

function setLinkState(mode) {
  const connected = mode === 'connected';
  $('link-pill').dataset.state = mode;
  $('link-text').textContent = { idle: 'Ngắt', connecting: 'Đang nối…', connected: 'Đã nối' }[mode];
  $('btn-connect').textContent = connected ? 'Ngắt' : 'Kết nối';
  $('btn-connect').classList.toggle('btn-primary', !connected);
  $('device-name').textContent = link.deviceName ?? 'Chưa kết nối';
  $('welcome').hidden = connected;
  document.querySelectorAll('[data-needs-link]').forEach((b) => { b.disabled = !connected; });
  if (connected) startPolling(); else stopPolling();
}

async function send(command) {
  try {
    consoleLine(command, 'in');
    await link.send(command);
  } catch (err) {
    toast(`Gửi lệnh lỗi: ${err.message}`);
  }
}

async function onConnected() {
  parser.reset();
  state.reconnectAttempt = 0;
  setLinkState('connected');
  consoleLine(`Đã kết nối ${link.deviceName}`, 'sys');
  await send('INFO');
}

async function connect() {
  if (link.isConnected) {
    state.userDisconnected = true;
    link.disconnect();
    return;
  }
  state.userDisconnected = false;
  if ('Notification' in window && Notification.permission === 'default') Notification.requestPermission();
  setLinkState('connecting');
  try {
    await link.connect();
    await onConnected();
  } catch (err) {
    setLinkState('idle');
    if (err.name !== 'NotFoundError') toast(`Không kết nối được: ${err.message}`);
  }
}

function scheduleReconnect() {
  const delay = RECONNECT_DELAYS_MS[state.reconnectAttempt];
  if (delay === undefined) {
    toast('Không thể kết nối lại. Bấm Kết nối để thử lại.');
    return;
  }
  state.reconnectAttempt += 1;
  setLinkState('connecting');
  setTimeout(async () => {
    if (state.userDisconnected) { setLinkState('idle'); return; }
    try {
      await link.reconnect();
      await onConnected();
    } catch {
      scheduleReconnect();
    }
  }, delay);
}

/* ---------------- Lệnh ---------------- */

function requestLog() {
  state.logPendingUntil = Date.now() + LOG_TIMEOUT_MS;
  send('LOG');
}

function startPolling() {
  stopPolling();
  state.pollTimer = setInterval(() => {
    const busy = Date.now() < state.logPendingUntil;
    if ($('auto-refresh').checked && !document.hidden && !busy && link.isConnected) send('INFO');
  }, POLL_INTERVAL_MS);
}

function stopPolling() {
  clearInterval(state.pollTimer);
  state.pollTimer = null;
}

function downloadCsv() {
  const blob = new Blob([logToCsv(state.logRows)], { type: 'text/csv' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `hop-den-${new Date().toISOString().slice(0, 19).replace(/[:T]/g, '-')}.csv`;
  a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}

/* ---------------- Khởi động ---------------- */

function init() {
  drawGaugeTicks();

  if (!isBluetoothSupported()) {
    $('unsupported').hidden = false;
    $('btn-connect').disabled = true;
    $('btn-connect-hero').disabled = true;
  }

  $('btn-connect').addEventListener('click', connect);
  $('btn-connect-hero').addEventListener('click', connect);
  $('btn-info').addEventListener('click', () => send('INFO'));
  $('btn-log').addEventListener('click', requestLog);
  $('btn-csv').addEventListener('click', downloadCsv);
  $('btn-dismiss-alert').addEventListener('click', () => { $('crash-alert').hidden = true; });
  armedButton($('btn-clear'), 'Bấm lần nữa để xóa', () => send('CLEAR'));
  armedButton($('btn-test-sos'), 'Bấm lần nữa: gửi SMS thật', () => send('TEST_SOS'));

  $('sos-form').addEventListener('submit', (e) => {
    e.preventDefault();
    const input = $('sos-input');
    const phone = validatePhone(input.value);
    input.setAttribute('aria-invalid', String(!phone));
    $('sos-hint').classList.toggle('err', !phone);
    if (!phone) {
      $('sos-hint').textContent = 'Số không hợp lệ: cần 9–15 chữ số, ví dụ 0912345678 hoặc +84912345678.';
      return;
    }
    $('sos-hint').textContent = '9–15 chữ số, có thể bắt đầu bằng +84. Lưu vĩnh viễn vào NVS của ESP32.';
    send(`SDT:${phone}`);
  });

  $('raw-form').addEventListener('submit', (e) => {
    e.preventDefault();
    const cmd = $('raw-input').value.trim();
    if (!cmd) return;
    $('raw-input').value = '';
    if (cmd.toUpperCase() === 'LOG') requestLog(); else send(cmd);
  });

  let resizeTimer = null;
  window.addEventListener('resize', () => {
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(() => { if (state.logRows.length) renderLog(state.logRows); }, 150);
  });

  if ('serviceWorker' in navigator) {
    navigator.serviceWorker.register('sw.js').catch((err) => consoleLine(`Không cài được chế độ offline: ${err.message}`, 'sys'));
  }
}

init();
