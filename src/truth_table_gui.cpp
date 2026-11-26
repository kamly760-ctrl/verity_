// Verity GUI - Windows version (Win32 API) - Discrete Structure project
// Same logic as the console version (tokenize + parse functions), with a window.
// Operators: not, and, or, implies, iff   | Variables: single capital letters A-Z
#include <windows.h>
#include <commdlg.h>
#include <string>
#include <cstring>
#include <vector>
#include <sstream>
#include <map>
#include <set>
#include <fstream>
using namespace std;

// ===================== LOGIC (same as the console version) =====================
vector<string> tokens;
size_t pos = 0;
map<char, bool> value;

bool parseIff();

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
vector<char> getVars(const vector<string>& tk) {
    set<char> s;
    for (const string& t : tk) if (isVar(t)) s.insert(t[0]);
    return vector<char>(s.begin(), s.end());
}
void setRow(const vector<char>& v, int i) {
    int n = v.size();
    for (int j = 0; j < n; j++) value[v[j]] = !((i >> (n - 1 - j)) & 1);
}
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

// ===================== TABLE shown in the window =====================
struct Table {
    vector<string> head;             // column titles
    vector<vector<string>> cells;    // "T", "F", "OK", "DIFF"
    int firstResult = 0;             // columns from here on are results (brighter colors)
};
Table table;
string message = "Type an expression and press a button.";
bool messageIsError = false;

// build a table of variables + one or two result columns
Table makeTable(const vector<char>& v, const vector<bool>& r1, const vector<bool>* r2) {
    Table t;
    for (char c : v) t.head.push_back(string(1, c));
    t.firstResult = v.size();
    t.head.push_back(r2 ? "E1" : "Result");
    if (r2) { t.head.push_back("E2"); t.head.push_back("Match"); }
    for (int i = 0; i < (int)r1.size(); i++) {
        setRow(v, i);
        vector<string> row;
        for (char c : v) row.push_back(value[c] ? "T" : "F");
        row.push_back(r1[i] ? "T" : "F");
        if (r2) {
            row.push_back((*r2)[i] ? "T" : "F");
            row.push_back(r1[i] == (*r2)[i] ? "OK" : "DIFF");
        }
        t.cells.push_back(row);
    }
    return t;
}

// ===================== WINDOW =====================
enum { ID_E1 = 101, ID_E2, ID_B1, ID_B2, ID_B3, ID_B4 };
HWND hE1, hE2, hMsg;
HFONT fontUI, fontBig;
bool intro = true;                    // opening screen is shown first
HBRUSH bgBrush;
int scrollTop = 0;                   // first visible row
const int GRID_X = 20, GRID_Y = 270, CELL_H = 26, CELL_W = 70, VISIBLE = 10;

string getText(HWND h) {
    char buf[512];
    GetWindowTextA(h, buf, 512);
    return string(buf);
}
void showMessage(const string& s, bool err) {
    message = s;
    messageIsError = err;
    SetWindowTextA(hMsg, s.c_str());
}
void updateScroll(HWND hwnd) {
    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMin = 0;
    si.nMax = (int)table.cells.size() - 1;
    si.nPage = VISIBLE;
    si.nPos = scrollTop;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}
void newTable(HWND hwnd, const Table& t) {
    table = t;
    scrollTop = 0;
    updateScroll(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

// Button 1: truth table
void doTable(HWND hwnd) {
    try {
        string e = getText(hE1);
        tokenize(e);
        vector<char> v = getVars(tokens);
        if (v.empty() || v.size() > 6) { showMessage("Use between 1 and 6 variables (A-Z).", true); return; }
        vector<bool> r = resultColumn(e, v);
        newTable(hwnd, makeTable(v, r, NULL));
        showMessage(kind(r), false);
    } catch (string& err) { showMessage("Error: " + err, true); }
}

// Button 2: equivalence of expression 1 and 2
void doEquiv(HWND hwnd) {
    try {
        string e1 = getText(hE1), e2 = getText(hE2);
        tokenize(e1); vector<char> v1 = getVars(tokens);
        tokenize(e2); vector<char> v2 = getVars(tokens);
        set<char> all(v1.begin(), v1.end());
        all.insert(v2.begin(), v2.end());
        vector<char> v(all.begin(), all.end());
        if (v.empty() || v.size() > 6) { showMessage("Use between 1 and 6 variables (A-Z).", true); return; }
        vector<bool> r1 = resultColumn(e1, v);
        vector<bool> r2 = resultColumn(e2, v);
        newTable(hwnd, makeTable(v, r1, &r2));
        int diff = -1;
        for (int i = 0; i < (int)r1.size(); i++) if (r1[i] != r2[i]) { diff = i; break; }
        if (diff < 0) showMessage("Logically EQUIVALENT: same result in every row.", false);
        else {
            setRow(v, diff);
            string s = "NOT equivalent. First difference at row " + to_string(diff + 1) + ": ";
            for (char c : v) s += string(1, c) + "=" + (value[c] ? "T " : "F ");
            showMessage(s, true);
        }
    } catch (string& err) { showMessage("Error: " + err, true); }
}

// Button 3: minterms and DNF of expression 1
void doDnf(HWND hwnd) {
    try {
        string e = getText(hE1);
        tokenize(e);
        vector<char> v = getVars(tokens);
        if (v.empty() || v.size() > 6) { showMessage("Use between 1 and 6 variables (A-Z).", true); return; }
        vector<bool> r = resultColumn(e, v);
        newTable(hwnd, makeTable(v, r, NULL));
        string dnf, mins;
        int count = 0;
        for (int i = 0; i < (int)r.size(); i++) {
            if (!r[i]) continue;
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
        if (count == 0) showMessage(kind(r) + "\r\nNo true rows, so the DNF is just: False", false);
        else showMessage(kind(r) + "\r\nMinterms (true rows): " + mins + "\r\nDNF: " + dnf, false);
    } catch (string& err) { showMessage("Error: " + err, true); }
}

// Button 4: save the table on screen as CSV
void doCsv(HWND hwnd) {
    if (table.cells.empty()) { showMessage("Make a table first, then export it.", true); return; }
    char file[MAX_PATH] = "truth_table.csv";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "CSV files\0*.csv\0All files\0*.*\0";
    ofn.lpstrDefExt = "csv";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameA(&ofn)) return;
    ofstream f(file);
    if (!f) { showMessage("Could not write the file.", true); return; }
    for (size_t i = 0; i < table.head.size(); i++) f << (i ? "," : "") << table.head[i];
    f << "\n";
    for (auto& row : table.cells) {
        for (size_t i = 0; i < row.size(); i++) f << (i ? "," : "") << row[i];
        f << "\n";
    }
    f.close();
    showMessage(string("Saved: ") + file, false);
}

// draw the colored grid (T = green, F = red)
void paintGrid(HDC dc) {
    if (table.head.empty()) return;
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, fontUI);
    int cols = table.head.size();
    for (int c = 0; c < cols; c++) {
        RECT r = { GRID_X + c * CELL_W, GRID_Y, GRID_X + (c + 1) * CELL_W - 2, GRID_Y + CELL_H - 2 };
        HBRUSH b = CreateSolidBrush(RGB(68, 71, 90));
        FillRect(dc, &r, b); DeleteObject(b);
        SetTextColor(dc, RGB(248, 248, 242));
        DrawTextA(dc, table.head[c].c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    for (int i = 0; i < VISIBLE; i++) {
        int row = scrollTop + i;
        if (row >= (int)table.cells.size()) break;
        for (int c = 0; c < cols; c++) {
            const string& s = table.cells[row][c];
            bool res = c >= table.firstResult;
            COLORREF col;
            if (s == "T" || s == "OK") col = res ? RGB(80, 250, 123) : RGB(40, 120, 70);
            else if (s == "DIFF") col = RGB(255, 184, 108);
            else col = res ? RGB(255, 85, 85) : RGB(130, 45, 55);
            RECT r = { GRID_X + c * CELL_W, GRID_Y + (i + 1) * CELL_H,
                       GRID_X + (c + 1) * CELL_W - 2, GRID_Y + (i + 2) * CELL_H - 2 };
            HBRUSH b = CreateSolidBrush(col);
            FillRect(dc, &r, b); DeleteObject(b);
            SetTextColor(dc, res ? RGB(20, 20, 30) : RGB(248, 248, 242));
            DrawTextA(dc, s.c_str(), -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
}


// opening screen: Verity logo + "Made by Yusif Kamel" (click or press a key to skip)
BOOL CALLBACK showChild(HWND h, LPARAM on) { ShowWindow(h, on ? SW_SHOW : SW_HIDE); return TRUE; }
void endIntro(HWND hwnd) {
    if (!intro) return;
    intro = false;
    KillTimer(hwnd, 1);
    EnumChildWindows(hwnd, showChild, TRUE);
    updateScroll(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}
void paintIntro(HDC dc, RECT rc) {
    HBRUSH bg = CreateSolidBrush(RGB(241, 245, 249));
    FillRect(dc, &rc, bg); DeleteObject(bg);
    int cx = rc.right / 2, cy = rc.bottom / 2 - 50;
    HBRUSH navy = CreateSolidBrush(RGB(15, 32, 67));
    SelectObject(dc, navy); SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, cx - 70, cy - 70, cx + 70, cy + 70, 34, 34);
    LOGBRUSH lb; lb.lbStyle = BS_SOLID; lb.lbColor = RGB(255, 255, 255); lb.lbHatch = 0;
    HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, 15, &lb, 0, NULL);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, cx - 30, cy - 12, NULL); LineTo(dc, cx, cy + 40); LineTo(dc, cx + 30, cy - 12);
    SelectObject(dc, old); DeleteObject(pen);
    HBRUSH blue = CreateSolidBrush(RGB(37, 99, 235));
    SelectObject(dc, blue); SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, cx - 14, cy - 56, cx + 14, cy - 28);
    DeleteObject(blue); DeleteObject(navy);
    SetBkMode(dc, TRANSPARENT);
    SelectObject(dc, fontBig);
    SetTextColor(dc, RGB(15, 32, 67));
    RECT t1 = { 0, cy + 90, rc.right, cy + 140 };
    DrawTextA(dc, "Verity", -1, &t1, DT_CENTER | DT_SINGLELINE);
    SelectObject(dc, fontUI);
    SetTextColor(dc, RGB(100, 116, 139));
    RECT t2 = { 0, cy + 142, rc.right, cy + 170 };
    DrawTextA(dc, "Truth table engine for Discrete Structures", -1, &t2, DT_CENTER | DT_SINGLELINE);
    SetTextColor(dc, RGB(37, 99, 235));
    RECT t3 = { 0, cy + 180, rc.right, cy + 210 };
    DrawTextA(dc, "Made by Yusif Kamel", -1, &t3, DT_CENTER | DT_SINGLELINE);
    SetTextColor(dc, RGB(148, 163, 184));
    RECT t4 = { 0, rc.bottom - 40, rc.right, rc.bottom - 16 };
    DrawTextA(dc, "click or press any key to skip", -1, &t4, DT_CENTER | DT_SINGLELINE);
}

void paintAll(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    HDC mem = CreateCompatibleDC(dc);                 // draw off-screen first: no flicker
    HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
    SelectObject(mem, bmp);
    if (intro) {
        paintIntro(mem, rc);
        BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return;
    }
    FillRect(mem, &rc, bgBrush);
    SetBkMode(mem, TRANSPARENT);
    SelectObject(mem, fontBig);
    SetTextColor(mem, RGB(139, 233, 253));
    TextOutA(mem, 20, 8, "Verity_", (int)strlen("Verity_"));
    SetTextColor(mem, RGB(255, 121, 198));
    SelectObject(mem, fontUI);
    TextOutA(mem, 160, 18, "truth tables // Discrete Structures // Made by Yusif Kamel", (int)strlen("truth tables // Discrete Structures // Made by Yusif Kamel"));
    SetTextColor(mem, RGB(189, 147, 249));
    TextOutA(mem, 20, 52, "Expression 1  (operators: not, and, or, implies, iff)", (int)strlen("Expression 1  (operators: not, and, or, implies, iff)"));
    TextOutA(mem, 20, 108, "Expression 2  (only used for the equivalence check)", (int)strlen("Expression 2  (only used for the equivalence check)"));
    paintGrid(mem);
    BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    DeleteObject(bmp); DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

void scrollTo(HWND hwnd, int p) {
    int maxTop = (int)table.cells.size() - VISIBLE;
    if (maxTop < 0) maxTop = 0;
    if (p < 0) p = 0;
    if (p > maxTop) p = maxTop;
    scrollTop = p;
    updateScroll(hwnd);
    InvalidateRect(hwnd, NULL, FALSE);
}

HWND makeButton(HWND parent, const char* text, int id, int x) {
    HWND b = CreateWindowA("BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, 160, 172, 34,
                           parent, (HMENU)(INT_PTR)id, NULL, NULL);
    SendMessage(b, WM_SETFONT, (WPARAM)fontUI, TRUE);
    return b;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        bgBrush = CreateSolidBrush(RGB(40, 42, 54));
        fontUI = CreateFontA(18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Consolas");
        fontBig = CreateFontA(34, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Consolas");
        hE1 = CreateWindowA("EDIT", "A implies B", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                            20, 74, 720, 28, hwnd, (HMENU)ID_E1, NULL, NULL);
        hE2 = CreateWindowA("EDIT", "not A or B", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                            20, 130, 720, 28, hwnd, (HMENU)ID_E2, NULL, NULL);
        hMsg = CreateWindowA("STATIC", message.c_str(), WS_CHILD | WS_VISIBLE,
                             20, 204, 720, 58, hwnd, NULL, NULL, NULL);
        SendMessage(hE1, WM_SETFONT, (WPARAM)fontUI, TRUE);
        SendMessage(hE2, WM_SETFONT, (WPARAM)fontUI, TRUE);
        SendMessage(hMsg, WM_SETFONT, (WPARAM)fontUI, TRUE);
        makeButton(hwnd, "Truth Table", ID_B1, 20);
        makeButton(hwnd, "Check Equivalence", ID_B2, 204);
        makeButton(hwnd, "Minterms + DNF", ID_B3, 388);
        makeButton(hwnd, "Export CSV", ID_B4, 572);
        EnumChildWindows(hwnd, showChild, FALSE);   // hide controls during the intro
        SetTimer(hwnd, 1, 2600, NULL);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_B1: doTable(hwnd); break;
        case ID_B2: doEquiv(hwnd); break;
        case ID_B3: doDnf(hwnd); break;
        case ID_B4: doCsv(hwnd); break;
        }
        return 0;
    case WM_CTLCOLORSTATIC: {            // colors of the message text
        HDC dc = (HDC)wp;
        SetTextColor(dc, messageIsError ? RGB(255, 85, 85) : RGB(80, 250, 123));
        SetBkColor(dc, RGB(40, 42, 54));
        return (LRESULT)bgBrush;
    }
    case WM_TIMER: endIntro(hwnd); return 0;
    case WM_LBUTTONDOWN: case WM_KEYDOWN: if (intro) { endIntro(hwnd); return 0; } break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        paintAll(hwnd);
        return 0;
    case WM_VSCROLL: {
        int p = scrollTop;
        switch (LOWORD(wp)) {
        case SB_LINEUP: p--; break;
        case SB_LINEDOWN: p++; break;
        case SB_PAGEUP: p -= VISIBLE; break;
        case SB_PAGEDOWN: p += VISIBLE; break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: p = HIWORD(wp); break;
        }
        scrollTo(hwnd, p);
        return 0;
    }
    case WM_MOUSEWHEEL:
        scrollTo(hwnd, scrollTop - (GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 3 : -3));
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int show) {
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "VerityWnd";
    RegisterClassA(&wc);
    DWORD style = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VSCROLL);
    RECT r = { 0, 0, 780, 570 };
    AdjustWindowRect(&r, style, FALSE);
    HWND hwnd = CreateWindowA("VerityWnd", "Verity - Truth Table Tool", style, CW_USEDEFAULT, CW_USEDEFAULT,
                              r.right - r.left, r.bottom - r.top, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, show);
    ShowScrollBar(hwnd, SB_VERT, FALSE);
    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) { TranslateMessage(&m); DispatchMessage(&m); }
    return 0;
}
