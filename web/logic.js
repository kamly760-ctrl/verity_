// Verity logic: same idea as the C++ version (tokenize -> parse by precedence -> evaluate).
// Precedence (weak to strong): iff, implies, or, and, not.
(function (root) {
  const KEYWORDS = ["not", "and", "or", "implies", "iff"];

  function tokenize(src) {
    let s = " " + src + " ";
    s = s.replace(/<->|<=>|↔/g, " iff ")
         .replace(/->|=>|→/g, " implies ")
         .replace(/∧|&&|&/g, " and ")
         .replace(/∨|\|\||\|/g, " or ")
         .replace(/¬|!|~/g, " not ")
         .replace(/\(/g, " ( ").replace(/\)/g, " ) ");
    return s.split(/\s+/).filter(Boolean).map(function (w) {
      const lw = w.toLowerCase();
      if (KEYWORDS.indexOf(lw) >= 0) return lw;
      if (w === "(" || w === ")") return w;
      if (/^[A-Za-z]$/.test(w)) return w.toUpperCase();
      throw new Error("Unknown word: " + w);
    });
  }

  // parser: builds a tree (AST) instead of computing directly, so we can show steps
  function parse(src) {
    const t = tokenize(src);
    if (t.length === 0) throw new Error("Type an expression first");
    let p = 0;
    const left = function (next, op) {
      return function () {
        let r = next();
        while (p < t.length && t[p] === op) { p++; r = { t: "bin", op: op, l: r, r: next() }; }
        return r;
      };
    };
    function parseNot() {
      if (p >= t.length) throw new Error("Expression ended too early");
      const w = t[p++];
      if (w === "not") return { t: "not", a: parseNot() };
      if (w === "(") {
        const r = parseIff();
        if (t[p] !== ")") throw new Error("Missing )");
        p++;
        return r;
      }
      if (/^[A-Z]$/.test(w)) return { t: "var", n: w };
      throw new Error("Unexpected: " + w);
    }
    const parseAnd = left(parseNot, "and");
    const parseOr = left(parseAnd, "or");
    const parseImplies = left(parseOr, "implies");
    const parseIff = left(parseImplies, "iff");
    const ast = parseIff();
    if (p !== t.length) throw new Error("Unexpected: " + t[p]);
    return ast;
  }

  function evaluate(n, env) {
    switch (n.t) {
      case "var": return env[n.n];
      case "not": return !evaluate(n.a, env);
      default: {
        const a = evaluate(n.l, env), b = evaluate(n.r, env);
        if (n.op === "and") return a && b;
        if (n.op === "or") return a || b;
        if (n.op === "implies") return !a || b;
        return a === b;
      }
    }
  }

  const SYM = { and: "∧", or: "∨", implies: "→", iff: "↔" };
  function show(n, top) {
    if (n.t === "var") return n.n;
    if (n.t === "not") return "¬" + show(n.a, false);
    const s = show(n.l, false) + " " + SYM[n.op] + " " + show(n.r, false);
    return top ? s : "(" + s + ")";
  }

  function variables(n, set) {
    set = set || new Set();
    if (n.t === "var") set.add(n.n);
    else if (n.t === "not") variables(n.a, set);
    else { variables(n.l, set); variables(n.r, set); }
    return set;
  }

  // sub-expressions (not plain variables), in the order they are computed
  function steps(n, out, seen) {
    out = out || []; seen = seen || {};
    if (n.t === "var") return out;
    if (n.t === "not") steps(n.a, out, seen);
    else { steps(n.l, out, seen); steps(n.r, out, seen); }
    const k = show(n, true);
    if (!seen[k]) { seen[k] = 1; out.push({ label: k, node: n }); }
    return out;
  }

  function rowEnv(vars, i) {            // row 0 = all True, like the C++ version
    const env = {}, n = vars.length;
    for (let j = 0; j < n; j++) env[vars[j]] = !((i >> (n - 1 - j)) & 1);
    return env;
  }

  function kind(results) {
    const t = results.filter(Boolean).length;
    if (t === results.length) return "Tautology";
    if (t === 0) return "Contradiction";
    return "Contingency";
  }

  function build(exprs) {                // exprs: array of source strings
    const asts = exprs.map(parse);
    const set = new Set();
    asts.forEach(function (a) { variables(a, set); });
    const vars = Array.from(set).sort();
    if (vars.length === 0) throw new Error("Use at least one variable (A-Z)");
    if (vars.length > 6) throw new Error("Use at most 6 variables");
    const rows = 1 << vars.length, envs = [], results = asts.map(function () { return []; });
    for (let i = 0; i < rows; i++) {
      const env = rowEnv(vars, i); envs.push(env);
      asts.forEach(function (a, k) { results[k].push(evaluate(a, env)); });
    }
    return { asts: asts, vars: vars, envs: envs, results: results, rows: rows };
  }

  function normalForms(b) {              // uses the first expression
    const r = b.results[0], vars = b.vars, mins = [], maxs = [], dnf = [], cnf = [];
    for (let i = 0; i < b.rows; i++) {
      const e = b.envs[i];
      if (r[i]) {
        mins.push(i + 1);
        dnf.push("(" + vars.map(function (v) { return (e[v] ? "" : "¬") + v; }).join(" ∧ ") + ")");
      } else {
        maxs.push(i + 1);
        cnf.push("(" + vars.map(function (v) { return (e[v] ? "¬" : "") + v; }).join(" ∨ ") + ")");
      }
    }
    return { mins: mins, maxs: maxs, dnf: dnf.join(" ∨ ") || "False", cnf: cnf.join(" ∧ ") || "True" };
  }

  const api = { tokenize: tokenize, parse: parse, evaluate: evaluate, show: show, steps: steps,
                build: build, kind: kind, normalForms: normalForms };
  if (typeof module !== "undefined") module.exports = api; else root.Verity = api;
})(this);
