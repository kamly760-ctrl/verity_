// Verity - Truth Table Tool (Discrete Structure project)
// Operators: not, and, or, implies, iff   | Variables: single capital letters A-Z
// Features: 1) truth table + type  2) equivalence of two expressions
//           3) minterms + DNF form 4) export table to CSV
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <map>
#include <set>
#ifdef _WIN32
#include <windows.h>     // only on Windows: to turn on colors and UTF-8 in the console
#endif
#include "splash.h"      // the animated splash screen (keep splash.h next to this file)
using namespace std;

vector<string> tokens;      // the expression split into words
size_t pos = 0;             // current position in tokens
map<char, bool> value;      // current True/False value of each variable

bool parseIff();            // declared first because the functions call each other

void tokenize(string s) {
    string t;
    for (char c : s) {
        if (c == '(' || c == ')') { t += ' '; t += c; t += ' '; }
        else t += c;
    }
    stringstream ss(t);
    string w;
    tokens.clear();
    while (ss >> w) tokens.push_back(w);
}

bool isVar(const string& t) { return t.size() == 1 && t[0] >= 'A' && t[0] <= 'Z'; }

bool parseNot() {
    if (pos >= tokens.size()) throw string("expression ended too early");
    string t = tokens[pos++];
    if (t == "not") return !parseNot();
    if (t == "(") {
        bool r = parseIff();
        if (pos >= tokens.size() || tokens[pos] != ")") throw string("missing )");
        pos++;
        return r;
    }
    if (isVar(t)) return value[t[0]];
    throw string("unknown word: " + t);
}
bool parseAnd() {
    bool r = parseNot();
    while (pos < tokens.size() && tokens[pos] == "and") { pos++; bool x = parseNot(); r = r && x; }
    return r;
}
bool parseOr() {
    bool r = parseAnd();
    while (pos < tokens.size() && tokens[pos] == "or") { pos++; bool x = parseAnd(); r = r || x; }
    return r;
}
bool parseImplies() {
    bool r = parseOr();
    while (pos < tokens.size() && tokens[pos] == "implies") { pos++; bool x = parseOr(); r = (!r) || x; }
    return r;
}
bool parseIff() {
    bool r = parseImplies();
    while (pos < tokens.size() && tokens[pos] == "iff") { pos++; bool x = parseImplies(); r = (r == x); }
    return r;
}
bool evaluate() {
    pos = 0;
    bool r = parseIff();
    if (pos != tokens.size()) throw string("extra word: " + tokens[pos]);
    return r;
}

// ---- helpers shared by the features ----

// all variables used in the current tokens
vector<char> getVars(const vector<string>& tk) {
    set<char> s;
    for (const string& t : tk) if (isVar(t)) s.insert(t[0]);
    return vector<char>(s.begin(), s.end());
}

// give variables the values of row i (row 0 = all True)
void setRow(const vector<char>& v, int i) {
    int n = v.size();
    for (int j = 0; j < n; j++) value[v[j]] = !((i >> (n - 1 - j)) & 1);
}

// compute the result column of an expression: one bool per row
vector<bool> resultColumn(const string& expr, const vector<char>& v) {
    tokenize(expr);
    vector<bool> res;
    for (int i = 0; i < (1 << v.size()); i++) { setRow(v, i); res.push_back(evaluate()); }
    return res;
}

string kind(const vector<bool>& r) {
    int t = 0;
    for (bool b : r) if (b) t++;
    if (t == (int)r.size()) return "Tautology (always true)";
    if (t == 0) return "Contradiction (always false)";
    return "Contingency (sometimes true, sometimes false)";
}

// build the table as text lines; used for the screen and the CSV file
// sep = " | " for screen, "," for CSV
vector<string> buildTable(const string& expr, const vector<char>& v, const vector<bool>& res, const string& sep) {
    vector<string> lines;
    string head;
    for (char c : v) head += string(1, c) + sep;
    lines.push_back(head + "Result");
    for (int i = 0; i < (int)res.size(); i++) {
        setRow(v, i);
        string row;
        for (char c : v) row += string(value[c] ? "T" : "F") + sep;
        lines.push_back(row + (res[i] ? "T" : "F"));
    }
    return lines;
}

// ---- features ----

string readExpr(const string& msg) {
    cout << msg;
    string line;
    getline(cin, line);
    return line;
}

// 1) truth table
void feature1() {
    string e = readExpr("Enter expression: ");
    tokenize(e);
    vector<char> v = getVars(tokens);
    if (v.empty() || v.size() > 6) { cout << "Use between 1 and 6 variables.\n"; return; }
    try {
        vector<bool> res = resultColumn(e, v);
        for (string& l : buildTable(e, v, res, " | ")) cout << l << "\n";
        cout << kind(res) << "\n";
    } catch (string& err) { cout << "Error: " << err << "\n"; }
}

// 2) are two expressions equivalent?
void feature2() {
    string e1 = readExpr("First expression : ");
    string e2 = readExpr("Second expression: ");
    try {
        tokenize(e1); vector<char> v1 = getVars(tokens);
        tokenize(e2); vector<char> v2 = getVars(tokens);
        set<char> all(v1.begin(), v1.end());
        all.insert(v2.begin(), v2.end());
        vector<char> v(all.begin(), all.end());
        if (v.empty() || v.size() > 6) { cout << "Use between 1 and 6 variables.\n"; return; }
        vector<bool> r1 = resultColumn(e1, v);
        vector<bool> r2 = resultColumn(e2, v);
        int diff = -1;
        cout << "Row | E1 | E2\n";
        for (int i = 0; i < (int)r1.size(); i++) {
            cout << " " << i + 1 << "  | " << (r1[i] ? "T" : "F") << "  | " << (r2[i] ? "T" : "F") << "\n";
            if (r1[i] != r2[i] && diff < 0) diff = i;
        }
        if (diff < 0) cout << "Logically EQUIVALENT (same result in every row)\n";
        else {
            cout << "NOT equivalent. First difference at row " << diff + 1 << ": ";
            setRow(v, diff);
            for (char c : v) cout << c << "=" << (value[c] ? "T" : "F") << " ";
            cout << "\n";
        }
    } catch (string& err) { cout << "Error: " << err << "\n"; }
}

// 3) minterms and DNF (a simplification hint)
void feature3() {
    string e = readExpr("Enter expression: ");
    tokenize(e);
    vector<char> v = getVars(tokens);
    if (v.empty() || v.size() > 6) { cout << "Use between 1 and 6 variables.\n"; return; }
    try {
        vector<bool> res = resultColumn(e, v);
        string dnf, mins;
        int count = 0;
        for (int i = 0; i < (int)res.size(); i++) {
            if (!res[i]) continue;
            setRow(v, i);
            string term;
            for (char c : v) {
                if (!term.empty()) term += " and ";
                term += (value[c] ? "" : "not ") + string(1, c);
            }
            dnf += (count ? " or " : "") + string("(") + term + ")";
            mins += (count ? ", " : "") + to_string(i + 1);
            count++;
        }
        cout << kind(res) << "\n";
        if (count == 0) { cout << "No true rows, so the DNF is just: False\n"; return; }
        cout << "True rows (minterms): " << mins << "\n";
        cout << "DNF: " << dnf << "\n";
        cout << "Hint: each (..) is one true row. Try to merge terms that differ in one variable.\n";
    } catch (string& err) { cout << "Error: " << err << "\n"; }
}

// 4) export to CSV
void feature4() {
    string e = readExpr("Enter expression: ");
    tokenize(e);
    vector<char> v = getVars(tokens);
    if (v.empty() || v.size() > 6) { cout << "Use between 1 and 6 variables.\n"; return; }
    try {
        vector<bool> res = resultColumn(e, v);
        ofstream f("truth_table.csv");
        for (string& l : buildTable(e, v, res, ",")) f << l << "\n";
        f.close();
        cout << "Saved to truth_table.csv (open it with Excel). " << kind(res) << "\n";
    } catch (string& err) { cout << "Error: " << err << "\n"; }
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001);                       // show Unicode characters
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (GetConsoleMode(h, &mode)) SetConsoleMode(h, mode | 0x0004);  // enable ANSI colors
#endif
    showSplash();            // animated splash from splash.h
    cout << "\nOperators: not, and, or, implies, iff. Variables: A-Z. Example: (A and B) or not C\n";
    while (true) {
        cout << "\n=== Verity Menu ===\n"
             << "1) Truth table\n2) Check equivalence of two expressions\n"
             << "3) Minterms and DNF\n4) Export truth table to CSV\n0) Exit\nChoose: ";
        string c;
        getline(cin, c);
        if (!cin) break;
        if (c == "1") feature1();
        else if (c == "2") feature2();
        else if (c == "3") feature3();
        else if (c == "4") feature4();
        else if (c == "0") break;
        else cout << "Please choose 0-4.\n";
    }
    cout << "Bye!\n";
    return 0;
}
