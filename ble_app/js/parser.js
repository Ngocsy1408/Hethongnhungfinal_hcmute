// Parser cho giao thức text của firmware (Esp32_board/ble_app_config.h).
// Firmware gửi từng mẩu <= 20 byte, nên ghép lại theo dòng rồi mới hiểu nghĩa.

const NUM = '(-?\\d+(?:\\.\\d+)?)';

const INFO_HEADER = '--- TRANG THAI HOP DEN ---';
const LOG_HEADER = '=== PHUC DUNG';
const LOG_FOOTER = /^={10,}$/;

const LOG_LINE = new RegExp(
  `^\\[T\\s*([+-]?\\d+)s\\]\\s*Roll:\\s*${NUM}°?\\s*Pitch:\\s*${NUM}°?\\s*\\|\\s*${NUM}G\\s*\\|\\s*${NUM}km/h\\s*\\|\\s*(.*)$`
);

const INFO_FIELDS = [
  [/^SDT SOS:\s*(.+)$/, (m) => ({ phone: m[1].trim() })],
  [new RegExp(`^Roll:\\s*${NUM}\\s*\\|\\s*Pitch:\\s*${NUM}`), (m) => ({ roll: +m[1], pitch: +m[2] })],
  [/^GPS Fix:\s*(OK|Chua Fix)\s*\((\d+) sat\)/, (m) => ({ gpsFix: m[1] === 'OK', satellites: +m[2] })],
  [new RegExp(`^Vi tri:\\s*${NUM},${NUM}`), (m) => ({ lat: +m[1], lon: +m[2] })],
  [/^Trang thai:\s*(\d+)/, (m) => ({ crashState: +m[1] })],
];
const INFO_LAST = new RegExp(`^Toc do:\\s*${NUM}\\s*km/h`);

const ONE_LINERS = [
  [/^✅ Da luu SDT SOS moi:\s*(.+)$/, 'phoneSaved', (m) => m[1].trim()],
  [/^✅ Da xoa trang/, 'cleared'],
  [/^❌/, 'error', (m) => m.input],
  [/^Loi:/, 'error', (m) => m.input],
  [/^🚨 TAI NAN!/, 'crash'],
  [/^🚨 Dang kich hoat thu nghiem SOS/, 'testSos'],
  [/^💾 Da luu xong/, 'logSaved'],
  [/^Hop den W25Q128: Chua co vu tai nan/, 'log', () => []],
];

const EMPTY_STATUS = Object.freeze({
  phone: null, roll: null, pitch: null, gpsFix: null, satellites: null,
  lat: null, lon: null, crashState: null, speed: null,
});

function tagOf(text) {
  if (text.includes('DAM')) return 'impact';
  if (text.includes('NGA')) return 'fall';
  return 'ok';
}

function parseLogLine(line) {
  const m = LOG_LINE.exec(line);
  if (!m) return null;
  return { t: +m[1], roll: +m[2], pitch: +m[3], g: +m[4], speed: +m[5], tag: tagOf(m[6]) };
}

export function createParser(emit) {
  const decoder = new TextDecoder('utf-8');
  let pending = '';
  let block = null; // { kind: 'info', data } | { kind: 'log', rows }

  function handleLine(line) {
    emit({ type: 'line', data: line });

    if (line === INFO_HEADER) {
      block = { kind: 'info', data: EMPTY_STATUS };
      return;
    }
    if (line.startsWith(LOG_HEADER)) {
      block = { kind: 'log', rows: [] };
      return;
    }

    if (block?.kind === 'info') {
      const last = INFO_LAST.exec(line);
      if (last) {
        emit({ type: 'status', data: { ...block.data, speed: +last[1] } });
        block = null;
        return;
      }
      for (const [re, pick] of INFO_FIELDS) {
        const m = re.exec(line);
        if (m) {
          block = { ...block, data: { ...block.data, ...pick(m) } };
          return;
        }
      }
    }

    if (block?.kind === 'log') {
      if (LOG_FOOTER.test(line)) {
        emit({ type: 'log', data: block.rows });
        block = null;
        return;
      }
      const row = parseLogLine(line);
      if (row) block = { ...block, rows: [...block.rows, row] };
      return;
    }

    for (const [re, type, pick] of ONE_LINERS) {
      const m = re.exec(line);
      if (m) {
        emit(pick ? { type, data: pick(m) } : { type });
        return;
      }
    }
  }

  function push(text) {
    pending += text;
    const lines = pending.split('\n');
    pending = lines.pop();
    for (const raw of lines) {
      const line = raw.trim();
      if (line) handleLine(line);
    }
  }

  return {
    push,
    // stream:true giữ lại byte UTF-8 bị cắt đôi giữa hai gói BLE
    pushBytes(bytes) { push(decoder.decode(bytes, { stream: true })); },
    reset() { pending = ''; block = null; },
  };
}

// Cùng quy tắc với firmware: 9..15 ký tự, chỉ chữ số và dấu + ở đầu
export function validatePhone(input) {
  const phone = String(input).replace(/[\s.-]/g, '');
  return /^\+?\d+$/.test(phone) && phone.length >= 9 && phone.length <= 15 ? phone : null;
}

export function logToCsv(rows) {
  const header = 't_s,roll_deg,pitch_deg,peak_g,speed_kmh,tag\n';
  return header + rows.map((r) => [r.t, r.roll, r.pitch, r.g, r.speed, r.tag].join(',') + '\n').join('');
}
