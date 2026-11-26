(function () {
  // intro: plays once per visit (use ?intro=1 to force it), click, Esc or Skip closes it
  (function () {
    const el = document.getElementById("intro");
    const force = ["1", "hold"].indexOf(new URLSearchParams(location.search).get("intro")) >= 0;
    let seen = false;
    try { seen = sessionStorage.getItem("verityIntro") === "1"; } catch (e) {}
    if ((seen && !force) || new URLSearchParams(location.search).get("intro") === "0") return;
    el.hidden = false;
    if (new URLSearchParams(location.search).get("intro") === "hold") return;   // used for recording the demo video
    let done = false;
    function close() {
      if (done) return; done = true;
      el.classList.add("out");
      setTimeout(function () { el.hidden = true; }, 600);
      try { sessionStorage.setItem("verityIntro", "1"); } catch (e) {}
    }
    document.getElementById("skip").onclick = close;
    el.addEventListener("click", close);
    document.addEventListener("keydown", close, { once: true });
    setTimeout(close, 3200);
  })();
  const L = window.Verity;
  const $ = function (id) { return document.getElementById(id); };
  const e1 = $("e1"), e2 = $("e2"), out = $("out");
  let tab = "table", lastInput = e1;

  const PRESETS = {
    table: [["A and B", "A and B"], ["(A and B) or not C", "(A and B) or not C"], ["Excluded middle", "A or not A"], ["Contradiction", "A and not A"], ["Implication", "A implies B"]],
    equiv: [["De Morgan", ["not (A and B)", "not A or not B"]], ["Implication", ["A implies B", "not A or B"]], ["Contrapositive", ["A implies B", "not B implies not A"]], ["Not equal", ["A implies B", "B implies A"]]],
    forms: [["(A and B) or not C", "(A and B) or not C"], ["A iff B", "A iff B"], ["A implies B", "A implies B"]]
  };

  function esc(s) { return String(s).replace(/[&<>]/g, function (c) { return { "&": "&amp;", "<": "&lt;", ">": "&gt;" }[c]; }); }
  function tf(b) { return b ? "T" : "F"; }

  function renderPresets() {
    const box = $("presets"); box.innerHTML = "";
    PRESETS[tab].forEach(function (p) {
      const b = document.createElement("button");
      b.textContent = p[0];
      b.onclick = function () {
        if (tab === "equiv") { e1.value = p[1][0]; e2.value = p[1][1]; } else e1.value = p[1];
        run();
      };
      box.appendChild(b);
    });
  }

  function setTab(t) {
    tab = t;
    document.querySelectorAll(".tabs button").forEach(function (b) { b.classList.toggle("on", b.dataset.tab === t); });
    $("f2").hidden = t !== "equiv";
    $("l1").textContent = t === "equiv" ? "First expression" : "Expression";
    $("stepsWrap").style.display = t === "table" ? "" : "none";
    if (t === "equiv" && !e2.value) e2.value = "not A or not B";
    renderPresets();
    run();
  }

  function table(b, mode, showSteps) {
    const vars = b.vars;
    let heads = vars.map(function (v) { return '<th class="var">' + v + "</th>"; });
    let stepCols = [];
    if (mode === "table" && showSteps) {
      stepCols = L.steps(b.asts[0]).slice(0, -1);
      if (stepCols.length > 6) stepCols = stepCols.slice(-6);
      stepCols.forEach(function (s) { heads.push("<th>" + esc(s.label) + "</th>"); });
    }
    if (mode === "equiv") heads.push('<th class="res">' + esc(L.show(b.asts[0], true)) + '</th><th class="res">' + esc(L.show(b.asts[1], true)) + "</th><th>Match</th>");
    else heads.push('<th class="res">' + esc(L.show(b.asts[0], true)) + "</th>");
    let rows = "";
    for (let i = 0; i < b.rows; i++) {
      const env = b.envs[i];
      let r = '<td class="n">' + (i + 1) + "</td>".slice(0, 0);
      r = "";
      vars.forEach(function (v) { r += '<td class="' + (env[v] ? "t" : "f") + '">' + tf(env[v]) + "</td>"; });
      stepCols.forEach(function (s) { const x = L.evaluate(s.node, env); r += '<td class="' + (x ? "t" : "f") + '">' + tf(x) + "</td>"; });
      const a = b.results[0][i];
      r += '<td class="rs ' + (a ? "t" : "f") + '">' + tf(a) + "</td>";
      if (mode === "equiv") {
        const c = b.results[1][i];
        r += '<td class="rs ' + (c ? "t" : "f") + '">' + tf(c) + "</td>";
        r += a === c ? '<td class="t">✓</td>' : '<td class="dv">✗ differs</td>';
      }
      rows += "<tr><td class='n'>" + (i + 1) + "</td>" + r + "</tr>";
    }
    return '<div class="tablewrap"><table><thead><tr><th>#</th>' + heads.join("") + "</tr></thead><tbody>" + rows + "</tbody></table></div>";
  }

  function csv(b, mode) {
    const head = b.vars.slice();
    if (mode === "equiv") head.push("E1", "E2", "Match"); else head.push("Result");
    const lines = [head.join(",")];
    for (let i = 0; i < b.rows; i++) {
      const r = b.vars.map(function (v) { return tf(b.envs[i][v]); });
      r.push(tf(b.results[0][i]));
      if (mode === "equiv") { r.push(tf(b.results[1][i])); r.push(b.results[0][i] === b.results[1][i] ? "OK" : "DIFF"); }
      lines.push(r.join(","));
    }
    return lines.join("\n") + "\n";
  }

  function download(name, text) {
    const a = document.createElement("a");
    a.href = URL.createObjectURL(new Blob([text], { type: "text/csv" }));
    a.download = name; a.click(); URL.revokeObjectURL(a.href);
  }

  function badge(k) {
    const c = k === "Tautology" ? "b-taut" : k === "Contradiction" ? "b-contra" : "b-cont";
    return '<span class="badge ' + c + '">' + k + "</span>";
  }

  function run() {
    try {
      const mode = tab;
      const b = L.build(mode === "equiv" ? [e1.value, e2.value] : [e1.value]);
      const trues = b.results[0].filter(Boolean).length;
      let html = "";
      if (mode === "table") {
        html += '<div class="stats"><div class="card"><small>Classification</small>' + badge(L.kind(b.results[0])) + '</div>' +
          '<div class="card"><small>Variables</small><strong>' + b.vars.length + '</strong></div>' +
          '<div class="card"><small>Rows (2ⁿ)</small><strong>' + b.rows + '</strong></div>' +
          '<div class="card"><small>True / False</small><strong><span class="t">' + trues + '</span> / <span class="f">' + (b.rows - trues) + "</span></strong></div></div>";
        html += table(b, "table", $("steps").checked);
        html += '<div class="tools"><button class="btn pri" id="dl">Download CSV</button></div>';
      } else if (mode === "equiv") {
        const d = b.results[0].findIndex(function (x, i) { return x !== b.results[1][i]; });
        if (d < 0) html += '<div class="banner ok">✓ Logically equivalent <span class="expr">' + esc(L.show(b.asts[0], true)) + "  ≡  " + esc(L.show(b.asts[1], true)) + "</span></div>";
        else html += '<div class="banner no">✗ Not equivalent. First difference at row ' + (d + 1) + ": " + b.vars.map(function (v) { return v + "=" + tf(b.envs[d][v]); }).join(", ") + "</div>";
        html += table(b, "equiv");
        html += '<div class="tools"><button class="btn pri" id="dl">Download CSV</button></div>';
      } else {
        const nf = L.normalForms(b);
        html += '<div class="stats"><div class="card"><small>Classification</small>' + badge(L.kind(b.results[0])) + '</div>' +
          '<div class="card"><small>Minterms</small><strong>' + nf.mins.length + '</strong></div><div class="card"><small>Maxterms</small><strong>' + nf.maxs.length + "</strong></div></div>";
        html += '<div class="forms"><div class="card"><h3>Minterms (rows where the result is True)</h3><div class="mins">' + (nf.mins.map(function (m) { return "<span>" + m + "</span>"; }).join("") || "none") + "</div></div>" +
          '<div class="card"><h3>DNF · sum of products</h3><p>' + esc(nf.dnf) + "</p></div>" +
          '<div class="card"><h3>Maxterms (rows where the result is False)</h3><div class="mins mx">' + (nf.maxs.map(function (m) { return "<span>" + m + "</span>"; }).join("") || "none") + "</div></div>" +
          '<div class="card"><h3>CNF · product of sums</h3><p>' + esc(nf.cnf) + "</p></div></div>";
        html += '<div class="hint">These forms are not simplified. Merge terms that differ in one variable to simplify.</div>';
      }
      out.innerHTML = html;
      const dl = $("dl"); if (dl) dl.onclick = function () { download("truth_table.csv", csv(b, mode)); };
    } catch (err) {
      out.innerHTML = '<div class="banner err">⚠ ' + esc(err.message) + "</div>";
    }
  }

  document.querySelectorAll(".tabs button").forEach(function (b) { b.onclick = function () { setTab(b.dataset.tab); }; });
  document.querySelectorAll("#ops button").forEach(function (b) {
    b.onclick = function () {
      const el = lastInput, s = el.selectionStart || el.value.length, e = el.selectionEnd || s;
      el.value = el.value.slice(0, s) + b.dataset.ins + el.value.slice(e);
      el.focus(); el.selectionStart = el.selectionEnd = s + b.dataset.ins.length; run();
    };
  });
  [e1, e2].forEach(function (el) { el.addEventListener("input", run); el.addEventListener("focus", function () { lastInput = el; }); });
  $("steps").addEventListener("change", run);

  const q = new URLSearchParams(location.search);       // ?tab=equiv&e1=...&e2=...
  if (q.get("e1")) e1.value = q.get("e1");
  if (q.get("e2")) e2.value = q.get("e2");
  if (q.get("steps") === "0") $("steps").checked = false;
  setTab(q.get("tab") || "table");
})();
