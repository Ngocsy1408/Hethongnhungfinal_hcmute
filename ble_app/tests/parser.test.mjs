import { test } from 'node:test';
import assert from 'node:assert/strict';
import { createParser, validatePhone, logToCsv } from '../js/parser.js';

function collect() {
  const events = [];
  const parser = createParser((e) => events.push(e));
  return { events, parser };
}

const INFO_FULL =
  '--- TRANG THAI HOP DEN ---\n' +
  'SDT SOS: 0912345678\n' +
  'Roll: -12.5 | Pitch: 3.0\n' +
  'GPS Fix: OK (9 sat)\n' +
  'Vi tri: 10.850000,106.771900\n' +
  'Trang thai: 0\n' +
  'Toc do: 42.5 km/h\n';

const LOG_FULL =
  '\n=== PHUC DUNG 15 GIAY TAI NAN (W25Q128) ===\n' +
  '[T-10s] Roll:  2.1° Pitch:  1.4° |  1.0G | 42.5km/h | OK\n' +
  '[T -1s] Roll: 28.4° Pitch: -6.2° |  1.8G | 31.0km/h | OK\n' +
  '[T +0s] Roll: 67.5° Pitch: 18.2° |  4.9G |  0.0km/h | 💥 DAM!\n' +
  '[T +1s] Roll:-68.8° Pitch: 19.0° |  1.1G |  0.0km/h | ⚠️ NGA\n' +
  '===========================================\n';

test('parses a full INFO block into one status event', () => {
  const { events, parser } = collect();
  parser.push(INFO_FULL);
  const status = events.filter((e) => e.type === 'status');
  assert.equal(status.length, 1);
  assert.deepEqual(status[0].data, {
    phone: '0912345678',
    roll: -12.5,
    pitch: 3.0,
    gpsFix: true,
    satellites: 9,
    lat: 10.85,
    lon: 106.7719,
    crashState: 0,
    speed: 42.5,
  });
});

test('INFO from old firmware (no position/state lines) still parses', () => {
  const { events, parser } = collect();
  parser.push('--- TRANG THAI HOP DEN ---\nSDT SOS: +84912345678\nRoll: 1.0 | Pitch: 2.0\nGPS Fix: Chua Fix (0 sat)\nToc do: 0.0 km/h\n');
  const [status] = events.filter((e) => e.type === 'status');
  assert.equal(status.data.gpsFix, false);
  assert.equal(status.data.lat, null);
  assert.equal(status.data.crashState, null);
});

test('reassembles text split into 20-byte chunks, including split UTF-8 bytes', () => {
  const { events, parser } = collect();
  const bytes = new TextEncoder().encode(LOG_FULL);
  for (let i = 0; i < bytes.length; i += 20) parser.pushBytes(bytes.slice(i, i + 20));
  const [log] = events.filter((e) => e.type === 'log');
  assert.equal(log.data.length, 4);
  assert.deepEqual(log.data[0], { t: -10, roll: 2.1, pitch: 1.4, g: 1.0, speed: 42.5, tag: 'ok' });
  assert.equal(log.data[1].t, -1);
  assert.deepEqual(log.data[2], { t: 0, roll: 67.5, pitch: 18.2, g: 4.9, speed: 0, tag: 'impact' });
  assert.equal(log.data[3].roll, -68.8);
  assert.equal(log.data[3].tag, 'fall');
});

test('empty blackbox emits an empty log', () => {
  const { events, parser } = collect();
  parser.push('Hop den W25Q128: Chua co vu tai nan nao.\n');
  assert.deepEqual(events.find((e) => e.type === 'log').data, []);
});

test('maps one-line replies to typed events', () => {
  const { events, parser } = collect();
  parser.push('✅ Da luu SDT SOS moi: 0987654321\n');
  parser.push('❌ Loi: SDT khong hop le! VD: SDT:0912345678\n');
  parser.push('Loi: Chua ket noi W25Q128!\n');
  parser.push('✅ Da xoa trang lich su hop den W25Q128!\n');
  parser.push('🚨 TAI NAN! Dang gui SMS SOS & thu 5s sau va cham...\n');
  parser.push('🚨 Dang kich hoat thu nghiem SOS!\n');
  parser.push("💾 Da luu xong 15s Hop Den vao W25Q128! Go 'LOG' de xem.\n");
  const typed = events.filter((e) => e.type !== 'line').map((e) => e.type);
  assert.deepEqual(typed, ['phoneSaved', 'error', 'error', 'cleared', 'crash', 'testSos', 'logSaved']);
  assert.equal(events.find((e) => e.type === 'phoneSaved').data, '0987654321');
});

test('every non-empty line is echoed as a line event', () => {
  const { events, parser } = collect();
  parser.push('Lenh ho tro:\n- INFO\n\n');
  assert.deepEqual(events.filter((e) => e.type === 'line').map((e) => e.data), ['Lenh ho tro:', '- INFO']);
});

test('holds an incomplete line until its newline arrives', () => {
  const { events, parser } = collect();
  parser.push('✅ Da luu SDT SOS m');
  assert.equal(events.length, 0);
  parser.push('oi: 0911\n');
  assert.equal(events.find((e) => e.type === 'phoneSaved').data, '0911');
});

test('validatePhone accepts what the firmware accepts', () => {
  assert.equal(validatePhone(' 0912345678 '), '0912345678');
  assert.equal(validatePhone('+84912345678'), '+84912345678');
  assert.equal(validatePhone('0912 345 678'), '0912345678');
  assert.equal(validatePhone('12345678'), null);
  assert.equal(validatePhone('09123abc45'), null);
  assert.equal(validatePhone('+8491234567890123'), null);
});

test('logToCsv writes a header and one row per record', () => {
  const csv = logToCsv([{ t: 0, roll: 67.5, pitch: 18.2, g: 4.9, speed: 0, tag: 'impact' }]);
  assert.equal(csv, 't_s,roll_deg,pitch_deg,peak_g,speed_kmh,tag\n0,67.5,18.2,4.9,0,impact\n');
});
