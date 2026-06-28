/* 依存ゼロの力学レイアウト・グラフビュー (SVG)。 graph-data.js の window.GRAPH を描画。 */
(function () {
  'use strict';
  var G = window.GRAPH;
  var SVGNS = 'http://www.w3.org/2000/svg';

  var svg = document.getElementById('graph');
  var gZoom = document.getElementById('zoomLayer'); // <g> 内に edges/nodes を入れる
  var infoEl = document.getElementById('nodeInfo');

  // ---- ノード/エッジの作業用コピー ----
  var W = svg.clientWidth || 760;
  var H = svg.clientHeight || 640;

  var nodes = G.nodes.map(function (n, i) {
    var angle = (i / G.nodes.length) * Math.PI * 2;
    return Object.assign({}, n, {
      x: W / 2 + Math.cos(angle) * 200 + (Math.random() - 0.5) * 40,
      y: H / 2 + Math.sin(angle) * 200 + (Math.random() - 0.5) * 40,
      vx: 0, vy: 0, fixed: false,
    });
  });
  var byId = {};
  nodes.forEach(function (n) { byId[n.id] = n; });

  var edges = G.edges.filter(function (e) {
    return byId[e.from] && byId[e.to];
  }).map(function (e) {
    return { source: byId[e.from], target: byId[e.to], rel: e.rel, dashed: e.dashed };
  });

  // 隣接（クリック時のハイライト用）
  var neighbors = {};
  nodes.forEach(function (n) { neighbors[n.id] = { out: [], in: [], adj: {} }; });
  edges.forEach(function (e) {
    neighbors[e.source.id].out.push(e);
    neighbors[e.target.id].in.push(e);
    neighbors[e.source.id].adj[e.target.id] = true;
    neighbors[e.target.id].adj[e.source.id] = true;
  });

  var hiddenDomains = {};   // domain -> true (凡例で消した)
  var selected = null;

  // ---- SVG 要素の生成 ----
  var edgeEls = [];
  var labelEls = [];
  edges.forEach(function (e) {
    var line = document.createElementNS(SVGNS, 'line');
    line.setAttribute('class', 'edge');
    if (e.dashed) line.setAttribute('stroke-dasharray', '5 4');
    gZoom.appendChild(line);
    edgeEls.push(line);

    var t = document.createElementNS(SVGNS, 'text');
    t.setAttribute('class', 'edge-label');
    t.setAttribute('text-anchor', 'middle');
    t.textContent = e.rel || '';
    gZoom.appendChild(t);
    labelEls.push(t);
  });

  var nodeEls = [];
  nodes.forEach(function (n) {
    var g = document.createElementNS(SVGNS, 'g');
    g.setAttribute('class', 'gnode');
    var r = radiusOf(n);
    var c = document.createElementNS(SVGNS, 'circle');
    c.setAttribute('r', r);
    c.setAttribute('fill', G.domains[n.domain].color);
    if (n.kind === 'interface' || n.kind === 'type') c.setAttribute('fill-opacity', '0.55');
    if (n.kind === 'external') c.setAttribute('stroke-dasharray', '3 3');
    g.appendChild(c);

    var label = document.createElementNS(SVGNS, 'text');
    label.setAttribute('text-anchor', 'middle');
    label.setAttribute('dy', r + 13);
    label.textContent = n.id;
    g.appendChild(label);

    gZoom.appendChild(g);
    n._el = g; n._circle = c;
    nodeEls.push(g);
    enableDrag(n, g);
    g.addEventListener('click', function (ev) { ev.stopPropagation(); select(n); });
  });

  function radiusOf(n) {
    var deg = (neighbors[n.id].out.length + neighbors[n.id].in.length);
    return 9 + Math.min(deg, 8) * 1.6;
  }

  // ---- 力学シミュレーション ----
  var alpha = 1;
  var KREP = 5200;    // 反発
  var KSPRING = 0.012; // ばね
  var LREST = 116;    // 自然長
  var CENTER = 0.008; // 中心引力
  var DAMP = 0.86;

  function tick() {
    W = svg.clientWidth || W; H = svg.clientHeight || H;
    var i, j, a, b, dx, dy, d2, d, f;

    // 反発（総当たり。 ノード数小なので O(n^2) で十分）
    for (i = 0; i < nodes.length; i++) {
      a = nodes[i];
      for (j = i + 1; j < nodes.length; j++) {
        b = nodes[j];
        dx = a.x - b.x; dy = a.y - b.y;
        d2 = dx * dx + dy * dy || 0.01;
        d = Math.sqrt(d2);
        f = (KREP / d2) * alpha;
        var ux = dx / d, uy = dy / d;
        a.vx += ux * f; a.vy += uy * f;
        b.vx -= ux * f; b.vy -= uy * f;
      }
    }
    // ばね
    for (i = 0; i < edges.length; i++) {
      a = edges[i].source; b = edges[i].target;
      dx = b.x - a.x; dy = b.y - a.y;
      d = Math.sqrt(dx * dx + dy * dy) || 0.01;
      f = KSPRING * (d - LREST) * alpha;
      var sx = (dx / d) * f, sy = (dy / d) * f;
      a.vx += sx; a.vy += sy;
      b.vx -= sx; b.vy -= sy;
    }
    // 中心引力 + 積分
    for (i = 0; i < nodes.length; i++) {
      a = nodes[i];
      if (a.fixed) { a.vx = 0; a.vy = 0; continue; }
      a.vx += (W / 2 - a.x) * CENTER * alpha;
      a.vy += (H / 2 - a.y) * CENTER * alpha;
      a.vx *= DAMP; a.vy *= DAMP;
      a.x += a.vx; a.y += a.vy;
      a.x = Math.max(40, Math.min(W - 40, a.x));
      a.y = Math.max(34, Math.min(H - 40, a.y));
    }
    if (alpha > 0.03) alpha *= 0.992;
    render();
    requestAnimationFrame(tick);
  }

  function render() {
    for (var i = 0; i < edges.length; i++) {
      var e = edges[i];
      edgeEls[i].setAttribute('x1', e.source.x);
      edgeEls[i].setAttribute('y1', e.source.y);
      edgeEls[i].setAttribute('x2', e.target.x);
      edgeEls[i].setAttribute('y2', e.target.y);
      labelEls[i].setAttribute('x', (e.source.x + e.target.x) / 2);
      labelEls[i].setAttribute('y', (e.source.y + e.target.y) / 2 - 3);
    }
    for (var k = 0; k < nodes.length; k++) {
      nodes[k]._el.setAttribute('transform', 'translate(' + nodes[k].x + ',' + nodes[k].y + ')');
    }
  }

  // ---- ドラッグ（pan/zoom 座標系を考慮） ----
  function enableDrag(n, el) {
    var dragging = false;
    el.addEventListener('pointerdown', function (ev) {
      ev.stopPropagation();
      dragging = true; n.fixed = true; alpha = Math.max(alpha, 0.5);
      el.setPointerCapture(ev.pointerId);
    });
    el.addEventListener('pointermove', function (ev) {
      if (!dragging) return;
      var p = toGraph(ev.clientX, ev.clientY);
      n.x = p.x; n.y = p.y; n.vx = 0; n.vy = 0;
    });
    el.addEventListener('pointerup', function (ev) {
      dragging = false; n.fixed = false;
      try { el.releasePointerCapture(ev.pointerId); } catch (_) {}
    });
  }

  // ---- pan / zoom ----
  var view = { x: 0, y: 0, k: 1 };
  function applyView() {
    gZoom.setAttribute('transform', 'translate(' + view.x + ',' + view.y + ') scale(' + view.k + ')');
  }
  function toGraph(clientX, clientY) {
    var rect = svg.getBoundingClientRect();
    return {
      x: (clientX - rect.left - view.x) / view.k,
      y: (clientY - rect.top - view.y) / view.k,
    };
  }
  svg.addEventListener('wheel', function (ev) {
    ev.preventDefault();
    var rect = svg.getBoundingClientRect();
    var mx = ev.clientX - rect.left, my = ev.clientY - rect.top;
    var factor = ev.deltaY < 0 ? 1.1 : 1 / 1.1;
    var nk = Math.max(0.3, Math.min(2.5, view.k * factor));
    view.x = mx - (mx - view.x) * (nk / view.k);
    view.y = my - (my - view.y) * (nk / view.k);
    view.k = nk;
    applyView();
  }, { passive: false });

  var panning = false, panStart = null;
  svg.addEventListener('pointerdown', function (ev) {
    panning = true; panStart = { x: ev.clientX - view.x, y: ev.clientY - view.y };
  });
  svg.addEventListener('pointermove', function (ev) {
    if (!panning) return;
    view.x = ev.clientX - panStart.x; view.y = ev.clientY - panStart.y; applyView();
  });
  svg.addEventListener('pointerup', function () { panning = false; });
  svg.addEventListener('click', function () { select(null); });

  // ---- 選択 / ハイライト ----
  function select(n) {
    selected = n;
    nodes.forEach(function (m) {
      var el = m._el;
      el.classList.remove('sel', 'dim');
      if (!n) return;
      if (m.id === n.id) el.classList.add('sel');
      else if (!neighbors[n.id].adj[m.id]) el.classList.add('dim');
    });
    edges.forEach(function (e, i) {
      var on = n && (e.source.id === n.id || e.target.id === n.id);
      edgeEls[i].classList.toggle('hi', !!on);
      edgeEls[i].style.opacity = (n && !on) ? '0.12' : '';
      labelEls[i].style.opacity = (n && !on) ? '0.12' : '';
    });
    applyDomainVisibility();
    renderInfo(n);
  }

  function renderInfo(n) {
    if (!n) {
      infoEl.innerHTML = '<p class="empty">ノードをクリックすると、その機能の説明と関連が表示されます。背景ドラッグで pan、ホイールで zoom。</p>';
      return;
    }
    var outs = neighbors[n.id].out.map(function (e) {
      return '<li>' + esc(e.rel) + ' → <b>' + esc(e.target.id) + '</b></li>'; });
    var ins = neighbors[n.id].in.map(function (e) {
      return '<li><b>' + esc(e.source.id) + '</b> ' + esc(e.rel) + ' →</li>'; });
    var html = '<h4>' + esc(n.id) + '</h4>'
      + '<div class="meta">' + esc(G.domains[n.domain].label) + ' · ' + esc(n.kind) + '</div>'
      + '<p>' + esc(n.desc) + '</p>';
    if (outs.length) html += '<div class="rel"><b>出力 / 依存先</b><ul>' + outs.join('') + '</ul></div>';
    if (ins.length) html += '<div class="rel"><b>入力 / 被依存</b><ul>' + ins.join('') + '</ul></div>';
    infoEl.innerHTML = html;
  }

  // ---- 凡例（ドメイン表示トグル） ----
  function applyDomainVisibility() {
    nodes.forEach(function (n) {
      var hide = hiddenDomains[n.domain];
      n._el.style.display = hide ? 'none' : '';
    });
    edges.forEach(function (e, i) {
      var hide = hiddenDomains[e.source.domain] || hiddenDomains[e.target.domain];
      edgeEls[i].style.display = hide ? 'none' : '';
      labelEls[i].style.display = hide ? 'none' : '';
    });
  }

  function buildLegend() {
    var box = document.getElementById('legend');
    var counts = {};
    nodes.forEach(function (n) { counts[n.domain] = (counts[n.domain] || 0) + 1; });
    Object.keys(G.domains).forEach(function (key) {
      if (!counts[key]) return;
      var d = G.domains[key];
      var row = document.createElement('div');
      row.className = 'row';
      row.innerHTML = '<span class="dot" style="background:' + d.color + '"></span>'
        + '<span>' + esc(d.label) + '</span><span class="count">' + counts[key] + '</span>';
      row.addEventListener('click', function () {
        hiddenDomains[key] = !hiddenDomains[key];
        row.classList.toggle('off', !!hiddenDomains[key]);
        applyDomainVisibility();
      });
      box.appendChild(row);
    });
  }

  function esc(s) {
    return String(s).replace(/[&<>"]/g, function (c) {
      return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c];
    });
  }

  // ---- コントロール ----
  document.getElementById('btnReset').addEventListener('click', function () {
    view = { x: 0, y: 0, k: 1 }; applyView();
    nodes.forEach(function (n, i) {
      var angle = (i / nodes.length) * Math.PI * 2;
      n.x = W / 2 + Math.cos(angle) * 200; n.y = H / 2 + Math.sin(angle) * 200;
      n.vx = 0; n.vy = 0; n.fixed = false;
    });
    alpha = 1; select(null);
  });
  document.getElementById('btnShake').addEventListener('click', function () {
    nodes.forEach(function (n) { n.vx += (Math.random() - 0.5) * 30; n.vy += (Math.random() - 0.5) * 30; });
    alpha = Math.max(alpha, 0.8);
  });

  buildLegend();
  renderInfo(null);
  applyView();
  requestAnimationFrame(tick);
})();
