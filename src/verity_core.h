// Verity core logic (no Windows code): tokenize, parse to a tree, evaluate all rows.
// Precedence (weak to strong): iff, implies, or, and, not.   Variables: single capital letters A-Z.
#pragma once
#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <map>
using namespace std;

struct Node { int t; char v; int a, b; };       // t: 0 var, 1 not, 2 and, 3 or, 4 implies, 5 iff
struct Expr { vector<Node> n; int root = -1; };

static vector<string> g_tok;
static size_t g_pos = 0;
static Expr* g_cur = 0;

static int mk(int t, char v, int a, int b) { Node x = { t, v, a, b }; g_cur->n.push_back(x); return (int)g_cur->n.size() - 1; }
static bool isVarTok(const string& t) { return t.size() == 1 && t[0] >= 'A' && t[0] <= 'Z'; }
static int pIff();
static int pNot() {
    if (g_pos >= g_tok.size()) throw string("expression ended too early");
    string t = g_tok[g_pos++];
    if (t == "not") { int a = pNot(); return mk(1, 0, a, -1); }
    if (t == "(") {
        int r = pIff();
        if (g_pos >= g_tok.size() || g_tok[g_pos] != ")") throw string("missing )");
        g_pos++;
        return r;
    }
    if (isVarTok(t)) return mk(0, t[0], -1, -1);
    throw string("unknown word: " + t);
}
static int pAnd() { int l = pNot(); while (g_pos < g_tok.size() && g_tok[g_pos] == "and") { g_pos++; int r = pNot(); l = mk(2, 0, l, r); } return l; }
static int pOr() { int l = pAnd(); while (g_pos < g_tok.size() && g_tok[g_pos] == "or") { g_pos++; int r = pAnd(); l = mk(3, 0, l, r); } return l; }
static int pImp() { int l = pOr(); while (g_pos < g_tok.size() && g_tok[g_pos] == "implies") { g_pos++; int r = pOr(); l = mk(4, 0, l, r); } return l; }
static int pIff() { int l = pImp(); while (g_pos < g_tok.size() && g_tok[g_pos] == "iff") { g_pos++; int r = pImp(); l = mk(5, 0, l, r); } return l; }

static void tokenizeStr(const string& s) {
    string t;
    for (char c : s) { if (c == '(' || c == ')') { t += ' '; t += c; t += ' '; } else t += c; }
    stringstream ss(t); string w; g_tok.clear();
    while (ss >> w) g_tok.push_back(w);
}
// parse text into e (throws string on error)
static void parseExpr(const string& s, Expr& e) {
    e.n.clear(); e.root = -1; g_cur = &e;
    tokenizeStr(s);
    if (g_tok.empty()) throw string("type an expression");
    g_pos = 0;
    e.root = pIff();
    if (g_pos != g_tok.size()) throw string("extra word: " + g_tok[g_pos]);
}
static void addVars(const Expr& e, set<char>& s) { for (auto& x : e.n) if (x.t == 0) s.insert(x.v); }

static string nodeText(const Expr& e, int i);
static string wrapText(const Expr& e, int i) { string s = nodeText(e, i); return e.n[i].t >= 2 ? "(" + s + ")" : s; }
static string nodeText(const Expr& e, int i) {
    const Node& x = e.n[i];
    static const char* op[] = { "", "not", "and", "or", "implies", "iff" };
    if (x.t == 0) return string(1, x.v);
    if (x.t == 1) return "not " + wrapText(e, x.a);
    return wrapText(e, x.a) + " " + op[x.t] + " " + wrapText(e, x.b);
}
// value of every node for row i (row 0 = all variables true)
static void evalRow(const Expr& e, const vector<char>& vars, int row, vector<char>& val) {
    int nv = (int)vars.size();
    val.assign(e.n.size(), 0);
    for (size_t i = 0; i < e.n.size(); i++) {
        const Node& x = e.n[i];
        switch (x.t) {
        case 0: { int k = 0; while (vars[k] != x.v) k++; val[i] = !((row >> (nv - 1 - k)) & 1); break; }
        case 1: val[i] = !val[x.a]; break;
        case 2: val[i] = val[x.a] && val[x.b]; break;
        case 3: val[i] = val[x.a] || val[x.b]; break;
        case 4: val[i] = !val[x.a] || val[x.b]; break;
        default: val[i] = (val[x.a] == val[x.b]); break;
        }
    }
}
static bool varVal(int nv, int k, int row) { return !((row >> (nv - 1 - k)) & 1); }

// everything the window needs to draw
struct Result {
    bool ok = false; string err;
    vector<char> vars;
    vector<string> head;                // column titles (variables, steps, results, match)
    vector<vector<char>> cells;         // 1 = T, 0 = F, 2 = match ok, 3 = differs
    int firstRes = 0;                   // first result column
    int rows = 0, trues = 0;
    string klass;                       // Tautology / Contradiction / Contingency
    string minterms, maxterms, dnf, cnf;
    int mintermCount = 0, maxtermCount = 0;
    bool equiv = false; int diffRow = -1; string diffText;
};
static string klassOf(int t, int n) { return t == n ? "Tautology" : (t == 0 ? "Contradiction" : "Contingency"); }

// mode 0 = truth table (steps optional), 1 = equivalence, 2 = normal forms
static Result compute(int mode, const string& s1, const string& s2, bool steps) {
    Result R;
    try {
        Expr a, b;
        parseExpr(s1, a);
        set<char> sv; addVars(a, sv);
        if (mode == 1) { parseExpr(s2, b); addVars(b, sv); }
        vector<char> v(sv.begin(), sv.end());
        if (v.empty()) throw string("use at least one variable (A-Z)");
        if (v.size() > 6) throw string("use at most 6 variables");
        R.vars = v; int nv = (int)v.size(); R.rows = 1 << nv;
        for (char c : v) R.head.push_back(string(1, c));
        vector<int> stepIdx;
        if (mode == 0 && steps) {
            set<string> seen;
            for (int i = 0; i < (int)a.n.size(); i++) {
                if (a.n[i].t == 0 || i == a.root) continue;
                if (seen.insert(nodeText(a, i)).second) { stepIdx.push_back(i); R.head.push_back(nodeText(a, i)); }
            }
        }
        R.firstRes = (int)R.head.size();
        R.head.push_back(nodeText(a, a.root));
        if (mode == 1) { R.head.push_back(nodeText(b, b.root)); R.head.push_back("Match"); }
        vector<char> va, vb; vector<char> r1;
        for (int i = 0; i < R.rows; i++) {
            evalRow(a, v, i, va);
            vector<char> row;
            for (int k = 0; k < nv; k++) row.push_back(varVal(nv, k, i));
            for (int si : stepIdx) row.push_back(va[si]);
            char x = va[a.root]; row.push_back(x); r1.push_back(x);
            if (x) R.trues++;
            if (mode == 1) {
                evalRow(b, v, i, vb); char y = vb[b.root]; row.push_back(y);
                bool same = (x == y); row.push_back(same ? 2 : 3);
                if (!same && R.diffRow < 0) {
                    R.diffRow = i; R.diffText = "";
                    for (int k = 0; k < nv; k++) R.diffText += string(k ? ", " : "") + string(1, v[k]) + "=" + (varVal(nv, k, i) ? "T" : "F");
                }
            }
            R.cells.push_back(row);
        }
        R.equiv = (mode == 1 && R.diffRow < 0);
        R.klass = klassOf(R.trues, R.rows);
        for (int i = 0; i < R.rows; i++) {
            string term, clause;
            for (int k = 0; k < nv; k++) {
                bool t = varVal(nv, k, i); string name(1, v[k]);
                term += string(k ? " and " : "") + (t ? "" : "not ") + name;
                clause += string(k ? " or " : "") + (t ? "not " : "") + name;
            }
            if (r1[i]) { R.dnf += string(R.mintermCount ? " or " : "") + "(" + term + ")"; R.minterms += string(R.mintermCount ? ", " : "") + to_string(i + 1); R.mintermCount++; }
            else { R.cnf += string(R.maxtermCount ? " and " : "") + "(" + clause + ")"; R.maxterms += string(R.maxtermCount ? ", " : "") + to_string(i + 1); R.maxtermCount++; }
        }
        if (R.dnf.empty()) R.dnf = "False (no true rows)";
        if (R.cnf.empty()) R.cnf = "True (no false rows)";
        if (R.minterms.empty()) R.minterms = "none";
        if (R.maxterms.empty()) R.maxterms = "none";
        R.ok = true;
    } catch (string& e) { R.ok = false; R.err = e; }
    return R;
}
