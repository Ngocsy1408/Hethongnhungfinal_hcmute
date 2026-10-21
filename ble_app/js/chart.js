// Biểu đồ đường SVG nhỏ cho băng hộp đen: 1 chuỗi, crosshair + tooltip khi rê/chạm.

const SVG_NS = 'http://www.w3.org/2000/svg';
const H = 190;
const MIN_W = 280;
const M = { l: 38, r: 14, t: 18, b: 24 };
const Y_TICKS = 4;

function el(tag, attrs = {}, text) {
  const node = document.createElementNS(SVG_NS, tag);
  for (const [k, v] of Object.entries(attrs)) node.setAttribute(k, v);
  if (text !== undefined) node.textContent = text;
  return node;
}

function niceStep(span) {
  const raw = span / Y_TICKS;
  const mag = 10 ** Math.floor(Math.log10(raw));
  const norm = raw / mag;
  return (norm <= 1 ? 1 : norm <= 2 ? 2 : norm <= 5 ? 5 : 10) * mag;
}

function yDomain(values, includeZero) {
  let lo = Math.min(...values);
  let hi = Math.max(...values);
  if (includeZero) { lo = Math.min(lo, 0); hi = Math.max(hi, 0); }
  if (hi - lo < 1e-6) { lo -= 1; hi += 1; }
  const step = niceStep(hi - lo);
  return { lo: Math.floor(lo / step) * step, hi: Math.ceil(hi / step) * step, step };
}

/**
 * @param {HTMLElement} figure  khung chứa (đã có <h3> tiêu đề)
 * @param {{t:number}[]} rows
 * @param {{ key: string, unit: string, digits?: number, includeZero?: boolean }} opts
 */
export function renderLineChart(figure, rows, { key, unit, digits = 1, includeZero = false }) {
  figure.querySelectorAll('svg, .tooltip').forEach((n) => n.remove());
  if (rows.length === 0) return;

  // Vẽ theo bề rộng thật (px) để chữ trục không bị co nhỏ trên điện thoại
  const W = Math.max(MIN_W, Math.round(figure.clientWidth - 24));

  const ts = rows.map((r) => r.t);
  const tMin = Math.min(...ts);
  const tMax = Math.max(...ts);
  const { lo, hi, step } = yDomain(rows.map((r) => r[key]), includeZero);
  const x = (t) => M.l + ((t - tMin) / (tMax - tMin || 1)) * (W - M.l - M.r);
  const y = (v) => H - M.b - ((v - lo) / (hi - lo)) * (H - M.t - M.b);
  const fmt = (v) => `${v.toFixed(digits)}${unit}`;

  const svg = el('svg', { viewBox: `0 0 ${W} ${H}`, role: 'img', 'aria-label': figure.querySelector('h3')?.textContent ?? '' });

  for (let v = lo; v <= hi + step / 2; v += step) {
    svg.append(el('line', { class: 'grid', x1: M.l, x2: W - M.r, y1: y(v), y2: y(v) }));
    svg.append(el('text', { class: 'axis-label', x: M.l - 6, y: y(v) + 3, 'text-anchor': 'end' }, String(+v.toFixed(2))));
  }
  for (const t of ts) {
    if (t % 5 !== 0) continue;
    const label = t === 0 ? '0' : `${t > 0 ? '+' : ''}${t}s`;
    svg.append(el('text', { class: 'axis-label', x: x(t), y: H - 6, 'text-anchor': 'middle' }, label));
  }

  const impact = rows.find((r) => r.t === 0);
  if (impact) {
    svg.append(el('line', { class: 'impact-rule', x1: x(0), x2: x(0), y1: M.t - 6, y2: H - M.b }));
    svg.append(el('text', { class: 'impact-label', x: x(0) + 5, y: M.t - 2 }, 'Va chạm'));
  }

  const d = rows.map((r, i) => `${i ? 'L' : 'M'}${x(r.t).toFixed(1)} ${y(r[key]).toFixed(1)}`).join('');
  svg.append(el('path', { class: 'series', d }));
  if (impact) svg.append(el('circle', { class: 'impact-dot', cx: x(0), cy: y(impact[key]), r: 5 }));

  const cursor = el('line', { class: 'cursor', y1: M.t, y2: H - M.b, visibility: 'hidden' });
  const cursorDot = el('circle', { class: 'cursor-dot', r: 5, visibility: 'hidden' });
  const hit = el('rect', { x: M.l, y: 0, width: W - M.l - M.r, height: H, fill: 'transparent' });
  svg.append(cursor, cursorDot, hit);

  const tooltip = document.createElement('div');
  tooltip.className = 'tooltip num';
  tooltip.hidden = true;
  figure.append(svg, tooltip);

  const show = (event) => {
    const box = svg.getBoundingClientRect();
    const px = ((event.clientX - box.left) / box.width) * W;
    const row = rows.reduce((best, r) => (Math.abs(x(r.t) - px) < Math.abs(x(best.t) - px) ? r : best));
    const cx = x(row.t);
    const cy = y(row[key]);
    cursor.setAttribute('x1', cx);
    cursor.setAttribute('x2', cx);
    cursorDot.setAttribute('cx', cx);
    cursorDot.setAttribute('cy', cy);
    cursor.setAttribute('visibility', 'visible');
    cursorDot.setAttribute('visibility', 'visible');

    const figBox = figure.getBoundingClientRect();
    tooltip.textContent = `T${row.t >= 0 ? '+' : ''}${row.t}s · ${fmt(row[key])}`;
    tooltip.style.left = `${box.left - figBox.left + (cx / W) * box.width}px`;
    tooltip.style.top = `${box.top - figBox.top + (cy / H) * box.height}px`;
    tooltip.hidden = false;
  };
  const hide = () => {
    cursor.setAttribute('visibility', 'hidden');
    cursorDot.setAttribute('visibility', 'hidden');
    tooltip.hidden = true;
  };

  hit.addEventListener('pointermove', show);
  hit.addEventListener('pointerdown', show);
  hit.addEventListener('pointerleave', hide);
}
