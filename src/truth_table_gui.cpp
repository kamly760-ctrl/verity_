// Verity GUI - Windows version (Win32 + GDI), styled like the Verity web demo.
// Tabs: Truth Table, Equivalence, Normal Forms.  Logic is in verity_core.h (same idea as the C++ console version).
#include <windows.h>
#include <commdlg.h>
#include <fstream>
#include "verity_core.h"

// ---------- colors (Verity identity) ----------
#define C_NAVY   RGB(15, 32, 67)
#define C_BLUE   RGB(37, 99, 235)
#define C_CLOUD  RGB(241, 245, 249)
#define C_WHITE  RGB(255, 255, 255)
#define C_LINE   RGB(203, 213, 225)
#define C_GREY   RGB(100, 116, 139)
#define C_GREEN  RGB(22, 163, 74)
#define C_GREENL RGB(220, 252, 231)
#define C_RED    RGB(220, 38, 38)
#define C_REDL   RGB(254, 226, 226)
#define C_AMBER  RGB(146, 64, 14)
#define C_AMBERL RGB(254, 243, 199)
#define C_CHIP   RGB(219, 234, 254)

int W = 960, H = 660;                       // logical client size (layout units, scaled by S)
int MX = 32, CW = 896, RES_BOT = 628;
double S = 1.0; int devW = 960, devH = 660; // S = device pixels per layout unit
const int HEAD_H = 68, RES_TOP = 262;
int hovKind = -1, hovArg = -1;
enum { ID_E1 = 101, ID_E2 };
enum { H_TAB = 1, H_CHIP, H_PRESET, H_STEPS, H_CSV };

struct Hit { RECT r; int kind; int arg; };
vector<Hit> hits;

HWND hE1, hE2, hLast;
HFONT fUI, fUIb, fSmall, fBig, fHuge, fMono, fEdit, fTab;
HBRUSH brWhite;
int tab = 0;                    // 0 truth table, 1 equivalence, 2 normal forms
bool showSteps = true;
bool intro = true;
int scrollY = 0, maxScroll = 0, contentH = 0;
bool dragging = false;
Result res;
bool empty1 = false;

wstring Wd(const string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
string getText(HWND h) { char b[512]; GetWindowTextA(h, b, 512); return string(b); }

// ---------- drawing helpers ----------
void fillR(HDC dc, RECT r, COLORREF c) { HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b); }
void rrect(HDC dc, RECT r, int rad, COLORREF fill, COLORREF border) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = border == (COLORREF)-1 ? (HPEN)GetStockObject(NULL_PEN) : CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op);
    DeleteObject(b); if (border != (COLORREF)-1) DeleteObject(p);
}
void shadow(HDC dc, RECT r, int rad) { r.top += 2; r.bottom += 2; rrect(dc, r, rad, RGB(226, 232, 240), (COLORREF)-1); }
void text(HDC dc, HFONT f, COLORREF c, const string& s, RECT r, UINT fl) {
    SelectObject(dc, f); SetTextColor(dc, c); SetBkMode(dc, TRANSPARENT);
    wstring w = Wd(s);
    DrawTextW(dc, w.c_str(), (int)w.size(), &r, fl);
}
int textW(HDC dc, HFONT f, const string& s) {
    SelectObject(dc, f); wstring w = Wd(s); SIZE sz = { 0, 0 };
    GetTextExtentPoint32W(dc, w.c_str(), (int)w.size(), &sz); return sz.cx;
}
int textHWrap(HDC dc, HFONT f, const string& s, int width) {
    SelectObject(dc, f); wstring w = Wd(s); RECT r = { 0, 0, width, 0 };
    DrawTextW(dc, w.c_str(), (int)w.size(), &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    return r.bottom;
}
void addHit(RECT r, int kind, int arg) { Hit h = { r, kind, arg }; hits.push_back(h); }

void drawLogo(HDC dc, int x, int y, int s) {      // navy rounded square, white V, blue dot
    RECT r = { x, y, x + s, y + s };
    rrect(dc, r, s * 30 / 128 * 2, C_NAVY, (COLORREF)-1);
    LOGBRUSH lb; lb.lbStyle = BS_SOLID; lb.lbColor = C_WHITE; lb.lbHatch = 0;
    HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND, s * 15 / 128 + 1, &lb, 0, NULL);
    HGDIOBJ old = SelectObject(dc, pen);
    MoveToEx(dc, x + s * 36 / 128, y + s * 48 / 128, NULL);
    LineTo(dc, x + s * 64 / 128, y + s * 94 / 128);
    LineTo(dc, x + s * 92 / 128, y + s * 48 / 128);
    SelectObject(dc, old); DeleteObject(pen);
    HBRUSH bl = CreateSolidBrush(C_BLUE); HGDIOBJ ob = SelectObject(dc, bl); HGDIOBJ op = SelectObject(dc, GetStockObject(NULL_PEN));
    int cr = s * 13 / 128, cx = x + s / 2, cy = y + s * 31 / 128;
    Ellipse(dc, cx - cr, cy - cr, cx + cr, cy + cr);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(bl);
}

// ---------- content ----------
struct Chip { const char* label; const char* ins; };
const Chip chips[] = { {"not","not "}, {"and","and "}, {"or","or "}, {"implies","implies "}, {"iff","iff "}, {"(","("}, {")",")"}, {"A","A"}, {"B","B"}, {"C","C"}, {"D","D"} };
struct Preset { const char* label; const char* a; const char* b; };
const Preset presT[] = { {"A and B","A and B",""}, {"(A and B) or not C","(A and B) or not C",""}, {"Excluded middle","A or not A",""}, {"Contradiction","A and not A",""}, {"Implication","A implies B",""} };
const Preset presE[] = { {"De Morgan","not (A and B)","not A or not B"}, {"Implication","A implies B","not A or B"}, {"Contrapositive","A implies B","not B implies not A"}, {"Not equal","A implies B","B implies A"} };

void recompute() {
    string a = getText(hE1), b = getText(hE2);
    empty1 = true;
    for (char c : a) if (c != ' ') empty1 = false;
    res = compute(tab, a, b, showSteps);
    scrollY = 0;
    InvalidateRect(GetParent(hE1), NULL, FALSE);
}
void mv(HWND h, int x, int y, int w, int hh) { MoveWindow(h, (int)(x * S), (int)(y * S), (int)(w * S), (int)(hh * S), TRUE); }
void layoutEdits() {
    int top = 110;
    if (tab == 1) {
        int w2 = (CW - 40 - 16) / 2;
        mv(hE1, MX + 20 + 12, top + 7, w2 - 24, 25);
        mv(hE2, MX + 20 + w2 + 16 + 12, top + 7, w2 - 24, 25);
        ShowWindow(hE2, SW_SHOW);
    } else {
        mv(hE1, MX + 20 + 12, top + 7, CW - 40 - 24, 25);
        ShowWindow(hE2, SW_HIDE);
    }
}
void relayout(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc); devW = rc.right; devH = rc.bottom;
    if (devW < 1) devW = 1; if (devH < 1) devH = 1;
    HDC d = GetDC(hwnd); double dpi = GetDeviceCaps(d, LOGPIXELSX) / 96.0; ReleaseDC(hwnd, d);
    double fit = devW / 1280.0; if (devH / 800.0 < fit) fit = devH / 800.0;
    S = dpi > fit ? dpi : fit;
    if (S > devW / 960.0) S = devW / 960.0;
    if (S < 0.6) S = 0.6;
    W = (int)(devW / S) + 1; H = (int)(devH / S) + 1;
    CW = W - 64; if (CW > 1240) CW = 1240; MX = (W - CW) / 2;
    RES_BOT = H - 32; if (RES_BOT < RES_TOP + 60) RES_BOT = RES_TOP + 60;
    if (fEdit) DeleteObject(fEdit);
    fEdit = CreateFontA(-(int)(20 * S + 0.5), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Consolas");
    if (hE1) { SendMessage(hE1, WM_SETFONT, (WPARAM)fEdit, TRUE); SendMessage(hE2, WM_SETFONT, (WPARAM)fEdit, TRUE); layoutEdits(); }
}
void setTab(int t) {
    tab = t;
    layoutEdits();
    recompute();
}
void drawHeader(HDC dc) {
    RECT r = { 0, 0, W, HEAD_H }; fillR(dc, r, C_WHITE);
    RECT l = { 0, HEAD_H - 1, W, HEAD_H }; fillR(dc, l, C_LINE);
    drawLogo(dc, MX, 11, 46);
    RECT t1 = { MX + 58, 10, 400, 38 }; text(dc, fBig, C_NAVY, "Verity", t1, DT_SINGLELINE | DT_NOPREFIX);
    RECT t2 = { MX + 58, 40, 480, 60 }; text(dc, fSmall, C_GREY, "Truth table engine for Discrete Structures", t2, DT_SINGLELINE | DT_NOPREFIX);
    const char* names[] = { "Truth Table", "Equivalence", "Normal Forms" };
    int tw = 128, gap = 4, total = 3 * tw + 4 * gap, x0 = W - MX - total;
    RECT seg = { x0, 14, x0 + total, 54 }; rrect(dc, seg, 14, C_CLOUD, C_LINE);
    for (int i = 0; i < 3; i++) {
        RECT b = { x0 + gap + i * (tw + gap), 18, x0 + gap + i * (tw + gap) + tw, 50 };
        if (i == tab) rrect(dc, b, 10, C_NAVY, (COLORREF)-1);
        text(dc, fTab, i == tab ? C_WHITE : C_GREY, names[i], b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        addHit(b, H_TAB, i);
    }
}
void drawCard(HDC dc) {
    RECT bgr = { 0, HEAD_H, W, RES_TOP }; fillR(dc, bgr, C_CLOUD);
    RECT card = { MX, 80, MX + CW, 250 }; shadow(dc, card, 18); rrect(dc, card, 18, C_WHITE, C_LINE);
    int top = 110, x = MX + 20;
    if (tab == 1) {
        int w2 = (CW - 40 - 16) / 2;
        RECT l1 = { x, 88, x + w2, 108 }; text(dc, fSmall, C_GREY, "FIRST EXPRESSION", l1, DT_SINGLELINE);
        RECT l2 = { x + w2 + 16, 88, x + 2 * w2 + 16, 108 }; text(dc, fSmall, C_GREY, "SECOND EXPRESSION", l2, DT_SINGLELINE);
        RECT b1 = { x, top, x + w2, top + 38 }; rrect(dc, b1, 12, C_WHITE, C_LINE);
        RECT b2 = { x + w2 + 16, top, x + 2 * w2 + 16, top + 38 }; rrect(dc, b2, 12, C_WHITE, C_LINE);
    } else {
        RECT l1 = { x, 88, x + 300, 108 }; text(dc, fSmall, C_GREY, "EXPRESSION", l1, DT_SINGLELINE);
        RECT b1 = { x, top, x + CW - 40, top + 38 }; rrect(dc, b1, 12, C_WHITE, C_LINE);
    }
    // operator chips
    int cx = x, cy = 160;
    for (int i = 0; i < 11; i++) {
        int w = textW(dc, fUIb, chips[i].label) + 26; if (w < 36) w = 36;
        RECT c = { cx, cy, cx + w, cy + 32 };
        rrect(dc, c, 10, C_WHITE, C_LINE);
        text(dc, fUIb, C_NAVY, chips[i].label, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        addHit(c, H_CHIP, i);
        cx += w + 8; if (i == 4 || i == 6) cx += 10;
    }
    // example chips
    RECT tr = { x, 206, x + 40, 232 }; text(dc, fSmall, C_GREY, "Try:", tr, DT_SINGLELINE | DT_VCENTER);
    const Preset* ps = tab == 1 ? presE : presT; int np = tab == 1 ? 4 : 5;
    int px = x + 44;
    for (int i = 0; i < np; i++) {
        int w = textW(dc, fSmall, ps[i].label) + 22;
        RECT c = { px, 205, px + w, 233 };
        rrect(dc, c, 14, C_CHIP, RGB(191, 219, 254));
        text(dc, fSmall, C_BLUE, ps[i].label, c, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        addHit(c, H_PRESET, i);
        px += w + 8;
    }
    // right side: show steps toggle and export
    if (tab != 2) {
        RECT eb = { MX + CW - 20 - 120, 205, MX + CW - 20, 235 };
        rrect(dc, eb, 10, C_BLUE, (COLORREF)-1);
        text(dc, fUIb, C_WHITE, "Export CSV", eb, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        addHit(eb, H_CSV, 0);
    }
    if (tab == 0) {
        RECT tg = { MX + CW - 20 - 120 - 20 - 124, 205, MX + CW - 20 - 120 - 20, 235 };
        RECT sw = { tg.left, 212, tg.left + 40, 232 };
        rrect(dc, sw, 20, showSteps ? C_BLUE : C_LINE, (COLORREF)-1);
        RECT kn = showSteps ? RECT{ sw.left + 22, 214, sw.left + 38, 230 } : RECT{ sw.left + 2, 214, sw.left + 18, 230 };
        rrect(dc, kn, 16, C_WHITE, (COLORREF)-1);
        RECT lb = { sw.left + 48, 205, tg.right, 235 }; text(dc, fUIb, C_NAVY, "Show steps", lb, DT_VCENTER | DT_SINGLELINE);
        addHit(tg, H_STEPS, 0);
    }
}
void statCard(HDC dc, int x, int y, int w, const char* label, const string& val, COLORREF vc, bool badge, COLORREF bg) {
    RECT r = { x, y, x + w, y + 76 }; shadow(dc, r, 14); rrect(dc, r, 14, C_WHITE, C_LINE);
    RECT l = { x + 16, y + 10, x + w - 8, y + 28 }; text(dc, fSmall, C_GREY, label, l, DT_SINGLELINE | DT_NOPREFIX);
    if (badge) {
        int bw = textW(dc, fUIb, val) + 28;
        RECT b = { x + 16, y + 36, x + 16 + bw, y + 64 };
        rrect(dc, b, 14, bg, C_AMBER == vc ? RGB(253, 230, 138) : (COLORREF)-1);
        text(dc, fUIb, vc, val, b, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        RECT v = { x + 16, y + 30, x + w - 8, y + 68 }; text(dc, fHuge, vc, val, v, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
}
void klassColors(const string& k, COLORREF& fg, COLORREF& bg) {
    if (k == "Tautology") { fg = C_GREEN; bg = C_GREENL; }
    else if (k == "Contradiction") { fg = C_RED; bg = C_REDL; }
    else { fg = C_AMBER; bg = C_AMBERL; }
}
void banner(HDC dc, int y, const string& s, COLORREF fg, COLORREF bg, COLORREF border) {
    RECT r = { MX, y, MX + CW, y + 48 }; rrect(dc, r, 12, bg, border);
    RECT t = { MX + 16, y, MX + CW - 12, y + 48 }; text(dc, fUIb, fg, s, t, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
}
int drawTable(HDC dc, int y) {
    int nc = (int)res.head.size(), numW = 52;
    vector<int> w(nc);
    int total = numW;
    for (int c = 0; c < nc; c++) { w[c] = textW(dc, fUIb, res.head[c]) + 34; if (w[c] < 64) w[c] = 64; total += w[c]; }
    if (total > CW) { int extra = total - CW; int flex = total - numW; for (int c = 0; c < nc; c++) w[c] -= extra * w[c] / flex; }
    total = numW; for (int c = 0; c < nc; c++) total += w[c];
    if (total < CW && nc > 0) { int add = (CW - total) / nc; for (int c = 0; c < nc; c++) w[c] += add; total += add * nc; }
    int x0 = MX, hh = 40, rh = 30;
    RECT frame = { x0, y, x0 + CW, y + hh + (int)res.cells.size() * rh }; shadow(dc, frame, 14); rrect(dc, frame, 14, C_WHITE, C_LINE);
    // header
    int x = x0;
    RECT hn = { x, y + 1, x + numW, y + hh }; fillR(dc, hn, C_NAVY); text(dc, fUIb, C_WHITE, "#", hn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    x += numW;
    for (int c = 0; c < nc; c++) {
        RECT h = { x, y + 1, x + w[c], y + hh };
        fillR(dc, h, c >= res.firstRes ? C_BLUE : C_NAVY);
        text(dc, fUIb, C_WHITE, res.head[c], h, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        x += w[c];
    }
    for (int i = 0; i < (int)res.cells.size(); i++) {
        int ry = y + hh + i * rh;
        if (ry + rh < RES_TOP || ry > RES_BOT) continue;
        x = x0;
        RECT n = { x, ry, x + numW, ry + rh }; if (i & 1) fillR(dc, n, RGB(248, 250, 252)); text(dc, fSmall, C_GREY, to_string(i + 1), n, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        x += numW;
        for (int c = 0; c < nc; c++) {
            char v = res.cells[i][c];
            RECT cell = { x, ry, x + w[c], ry + rh };
            bool isRes = c >= res.firstRes;
            COLORREF bg = (i & 1) ? RGB(248, 250, 252) : C_WHITE, fg = v == 1 ? C_GREEN : C_RED; const char* s = v == 1 ? "T" : "F";
            if (v == 2) { s = "OK"; fg = C_GREEN; }
            if (v == 3) { s = "differs"; fg = C_AMBER; }
            if (isRes) bg = (v == 1 || v == 2) ? C_GREENL : (v == 3 ? C_AMBERL : C_REDL);
            if (bg != C_WHITE) fillR(dc, cell, bg);
            text(dc, fUIb, fg, s, cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            x += w[c];
        }
        RECT ln = { x0 + 1, ry + rh - 1, x0 + CW - 1, ry + rh }; fillR(dc, ln, RGB(226, 232, 240));
    }
    return y + hh + (int)res.cells.size() * rh;
}
int panel(HDC dc, int y, const char* label, const string& body, HFONT f) {
    int th = textHWrap(dc, f, body, CW - 40);
    RECT r = { MX, y, MX + CW, y + 44 + th }; shadow(dc, r, 14); rrect(dc, r, 14, C_WHITE, C_LINE);
    RECT l = { MX + 20, y + 10, MX + CW - 20, y + 28 }; text(dc, fSmall, C_BLUE, label, l, DT_SINGLELINE | DT_NOPREFIX);
    RECT t = { MX + 20, y + 34, MX + CW - 20, y + 34 + th }; text(dc, f, C_NAVY, body, t, DT_WORDBREAK | DT_NOPREFIX);
    return y + 44 + th;
}
void drawResults(HDC dc) {
    int y = RES_TOP + 6 - scrollY;
    if (empty1) { banner(dc, y, "Type an expression above, for example (A and B) or not C", C_GREY, C_WHITE, C_LINE); y += 48; }
    else if (!res.ok) { banner(dc, y, "Error: " + res.err, C_RED, C_REDL, RGB(252, 165, 165)); y += 48; }
    else if (tab == 0) {
        int sw = (CW - 36) / 4; COLORREF fg, bg; klassColors(res.klass, fg, bg);
        statCard(dc, MX, y, sw, "CLASSIFICATION", res.klass, fg, true, bg);
        statCard(dc, MX + (sw + 12), y, sw, "VARIABLES", to_string(res.vars.size()), C_NAVY, false, 0);
        statCard(dc, MX + 2 * (sw + 12), y, sw, "ROWS (2^n)", to_string(res.rows), C_NAVY, false, 0);
        // true / false card with two colors
        RECT r = { MX + 3 * (sw + 12), y, MX + 3 * (sw + 12) + sw, y + 76 }; shadow(dc, r, 14); rrect(dc, r, 14, C_WHITE, C_LINE);
        RECT l = { r.left + 16, y + 10, r.right - 8, y + 28 }; text(dc, fSmall, C_GREY, "TRUE / FALSE", l, DT_SINGLELINE);
        string t1 = to_string(res.trues), f1 = to_string(res.rows - res.trues);
        int w1 = textW(dc, fHuge, t1);
        RECT a = { r.left + 16, y + 30, r.left + 16 + w1 + 4, y + 68 }; text(dc, fHuge, C_GREEN, t1, a, DT_SINGLELINE | DT_VCENTER);
        RECT s = { a.right, y + 30, a.right + 30, y + 68 }; text(dc, fHuge, C_NAVY, " / ", s, DT_SINGLELINE | DT_VCENTER);
        RECT b = { s.right + 14, y + 30, r.right - 8, y + 68 }; text(dc, fHuge, C_RED, f1, b, DT_SINGLELINE | DT_VCENTER);
        y += 76 + 14;
        y = drawTable(dc, y);
    } else if (tab == 1) {
        if (res.equiv) banner(dc, y, "Logically equivalent: same result in every row.", C_GREEN, C_GREENL, RGB(134, 239, 172));
        else banner(dc, y, "Not equivalent. First difference at row " + to_string(res.diffRow + 1) + ": " + res.diffText, C_RED, C_REDL, RGB(252, 165, 165));
        y += 48 + 14;
        y = drawTable(dc, y);
    } else {
        int sw = (CW - 24) / 3; COLORREF fg, bg; klassColors(res.klass, fg, bg);
        statCard(dc, MX, y, sw, "CLASSIFICATION", res.klass, fg, true, bg);
        statCard(dc, MX + (sw + 12), y, sw, "MINTERMS", to_string(res.mintermCount), C_NAVY, false, 0);
        statCard(dc, MX + 2 * (sw + 12), y, sw, "MAXTERMS", to_string(res.maxtermCount), C_NAVY, false, 0);
        y += 76 + 12;
        y = panel(dc, y, "MINTERMS (ROWS WHERE THE RESULT IS TRUE)", res.minterms, fMono) + 12;
        y = panel(dc, y, "DNF - SUM OF PRODUCTS", res.dnf, fMono) + 12;
        y = panel(dc, y, "MAXTERMS (ROWS WHERE THE RESULT IS FALSE)", res.maxterms, fMono) + 12;
        y = panel(dc, y, "CNF - PRODUCT OF SUMS", res.cnf, fMono);
    }
    contentH = y + scrollY - RES_TOP + 14;
}
void drawFooter(HDC dc) {
    RECT r = { 0, RES_BOT, W, H }; fillR(dc, r, C_CLOUD);
    RECT t = { 0, RES_BOT + 4, W, H - 2 };
    text(dc, fSmall, C_GREY, "Verity  |  Horus University  |  Made by Yusif Kamel", t, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
void drawIntro(HDC dc) {
    RECT rc = { 0, 0, W, H }; fillR(dc, rc, C_CLOUD);
    int cx = W / 2, cy = H / 2 - 60;
    drawLogo(dc, cx - 70, cy - 70, 140);
    RECT t1 = { 0, cy + 90, W, cy + 150 }; text(dc, fHuge, C_NAVY, "Verity", t1, DT_CENTER | DT_SINGLELINE);
    RECT t2 = { 0, cy + 150, W, cy + 180 }; text(dc, fUI, C_GREY, "Truth table engine for Discrete Structures", t2, DT_CENTER | DT_SINGLELINE);
    RECT t3 = { 0, cy + 190, W, cy + 220 }; text(dc, fUIb, C_BLUE, "Made by Yusif Kamel", t3, DT_CENTER | DT_SINGLELINE);
    RECT t4 = { 0, H - 44, W, H - 20 }; text(dc, fSmall, RGB(148, 163, 184), "click or press any key to skip", t4, DT_CENTER | DT_SINGLELINE);
}
void paintAll(HWND hwnd) {
    PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
    RECT rc; GetClientRect(hwnd, &rc);
    HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HGDIOBJ ob = SelectObject(mem, bmp);
    hits.clear();
    SetMapMode(mem, MM_ANISOTROPIC); SetWindowExtEx(mem, 1000, 1000, NULL); SetViewportExtEx(mem, (int)(S * 1000), (int)(S * 1000), NULL);
    if (intro) drawIntro(mem);
    else {
        { RECT full = { 0, 0, W, H }; fillR(mem, full, C_CLOUD); }
        SaveDC(mem); IntersectClipRect(mem, 0, RES_TOP, W, RES_BOT);
        drawResults(mem);
        RestoreDC(mem, -1);
        int newMax = contentH - (RES_BOT - RES_TOP); if (newMax < 0) newMax = 0;
        if (newMax != maxScroll || scrollY > newMax) { maxScroll = newMax; if (scrollY > maxScroll) scrollY = maxScroll; InvalidateRect(hwnd, NULL, FALSE); }
        if (maxScroll > 0) {      // thin scrollbar thumb
            int trackH = RES_BOT - RES_TOP - 8, thumbH = trackH * (RES_BOT - RES_TOP) / contentH; if (thumbH < 30) thumbH = 30;
            int ty = RES_TOP + 4 + (trackH - thumbH) * scrollY / maxScroll;
            RECT th = { W - 14, ty, W - 8, ty + thumbH }; rrect(mem, th, 6, RGB(148, 163, 184), (COLORREF)-1);
        }
        drawCard(mem); drawHeader(mem); drawFooter(mem);
        for (size_t i = 0; i < hits.size(); i++) if (hits[i].kind == hovKind && hits[i].arg == hovArg) {
            HPEN hp = CreatePen(PS_SOLID, 2, C_BLUE); HGDIOBJ op = SelectObject(mem, hp), ob2 = SelectObject(mem, GetStockObject(NULL_BRUSH));
            RoundRect(mem, hits[i].r.left, hits[i].r.top, hits[i].r.right, hits[i].r.bottom, 12, 12);
            SelectObject(mem, op); SelectObject(mem, ob2); DeleteObject(hp);
        }
    }
    SetMapMode(mem, MM_TEXT); BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob); DeleteObject(bmp); DeleteDC(mem);
    EndPaint(hwnd, &ps);
}
void scrollTo(HWND hwnd, int p) {
    if (p < 0) p = 0; if (p > maxScroll) p = maxScroll;
    if (p != scrollY) { scrollY = p; InvalidateRect(hwnd, NULL, FALSE); }
}
void doCsv(HWND hwnd) {
    if (!res.ok || res.cells.empty()) { MessageBoxA(hwnd, "Type a valid expression first, then export.", "Verity", MB_OK | MB_ICONINFORMATION); return; }
    char file[MAX_PATH] = "truth_table.csv";
    OPENFILENAMEA ofn; ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd; ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = "CSV files\0*.csv\0All files\0*.*\0"; ofn.lpstrDefExt = "csv"; ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameA(&ofn)) return;
    ofstream f(file);
    if (!f) { MessageBoxA(hwnd, "Could not write the file.", "Verity", MB_OK | MB_ICONERROR); return; }
    for (size_t i = 0; i < res.head.size(); i++) { string h = res.head[i]; f << (i ? "," : "") << "\"" << h << "\""; }
    f << "\n";
    for (auto& row : res.cells) {
        for (size_t i = 0; i < row.size(); i++) f << (i ? "," : "") << (row[i] == 1 ? "T" : row[i] == 0 ? "F" : row[i] == 2 ? "OK" : "DIFF");
        f << "\n";
    }
}
void insertText(const char* s) {
    HWND e = (hLast && IsWindowVisible(hLast)) ? hLast : hE1;
    SendMessageA(e, EM_REPLACESEL, TRUE, (LPARAM)s);
    SetFocus(e);
}
bool hitAt(int x, int y, Hit& out) {
    for (int i = (int)hits.size() - 1; i >= 0; i--) {
        const RECT& r = hits[i].r;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) { out = hits[i]; return true; }
    }
    return false;
}
void endIntro(HWND hwnd) {
    if (!intro) return;
    intro = false; KillTimer(hwnd, 1);
    ShowWindow(hE1, SW_SHOW); layoutEdits(); SetFocus(hE1);
    InvalidateRect(hwnd, NULL, FALSE);
}
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        brWhite = CreateSolidBrush(C_WHITE);
        fUI = CreateFontA(-18, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        fUIb = CreateFontA(-17, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        fTab = CreateFontA(-17, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        fSmall = CreateFontA(-14, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        fBig = CreateFontA(-30, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        fHuge = CreateFontA(-34, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Segoe UI");
        fMono = CreateFontA(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Consolas");
        hE1 = CreateWindowA("EDIT", "(A and B) or not C", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 24, hwnd, (HMENU)ID_E1, NULL, NULL);
        hE2 = CreateWindowA("EDIT", "not A or B", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 100, 24, hwnd, (HMENU)ID_E2, NULL, NULL);
        hLast = hE1;
        relayout(hwnd); ShowWindow(hE1, SW_HIDE); ShowWindow(hE2, SW_HIDE);
        recompute();
        SetTimer(hwnd, 1, 2600, NULL);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_E1 || LOWORD(wp) == ID_E2) {
            if (HIWORD(wp) == EN_CHANGE) recompute();
            if (HIWORD(wp) == EN_SETFOCUS) hLast = (HWND)lp;
        }
        return 0;
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp; SetTextColor(dc, C_NAVY); SetBkColor(dc, C_WHITE); return (LRESULT)brWhite;
    }
    case WM_SIZE:
        if (wp != SIZE_MINIMIZED && hE1) { relayout(hwnd); if (scrollY > 0) scrollY = 0; InvalidateRect(hwnd, NULL, FALSE); }
        return 0;
    case WM_GETMINMAXINFO: { MINMAXINFO* mi = (MINMAXINFO*)lp; mi->ptMinTrackSize.x = 820; mi->ptMinTrackSize.y = 560; return 0; }
    case WM_TIMER: endIntro(hwnd); return 0;
    case WM_KEYDOWN: if (intro) { endIntro(hwnd); return 0; }
        if (wp == VK_NEXT) scrollTo(hwnd, scrollY + 300); if (wp == VK_PRIOR) scrollTo(hwnd, scrollY - 300);
        break;
    case WM_LBUTTONDOWN: {
        if (intro) { endIntro(hwnd); return 0; }
        int x = (int)((short)LOWORD(lp) / S), y = (int)((short)HIWORD(lp) / S);
        if (maxScroll > 0 && x >= W - 22 && y >= RES_TOP && y < RES_BOT) {
            dragging = true; SetCapture(hwnd);
            scrollTo(hwnd, (y - RES_TOP) * maxScroll / (RES_BOT - RES_TOP)); return 0;
        }
        Hit h;
        if (hitAt(x, y, h)) {
            if (h.kind == H_TAB) setTab(h.arg);
            else if (h.kind == H_CHIP) insertText(chips[h.arg].ins);
            else if (h.kind == H_PRESET) {
                const Preset* p = tab == 1 ? presE : presT;
                SetWindowTextA(hE1, p[h.arg].a);
                if (tab == 1) SetWindowTextA(hE2, p[h.arg].b);
            }
            else if (h.kind == H_STEPS) { showSteps = !showSteps; recompute(); }
            else if (h.kind == H_CSV) doCsv(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        { int y = (int)((short)HIWORD(lp) / S), x = (int)((short)LOWORD(lp) / S);
          if (dragging) scrollTo(hwnd, (y - RES_TOP) * maxScroll / (RES_BOT - RES_TOP));
          else if (!intro) { Hit h; int k = -1, a = -1; if (hitAt(x, y, h)) { k = h.kind; a = h.arg; }
            if (k != hovKind || a != hovArg) { hovKind = k; hovArg = a; InvalidateRect(hwnd, NULL, FALSE); } } }
        return 0;
    case WM_LBUTTONUP: if (dragging) { dragging = false; ReleaseCapture(); } return 0;
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt); pt.x = (LONG)(pt.x / S); pt.y = (LONG)(pt.y / S); Hit h;
        if (!intro && LOWORD(lp) == HTCLIENT && hitAt(pt.x, pt.y, h)) { SetCursor(LoadCursor(NULL, IDC_HAND)); return TRUE; }
        break;
    }
    case WM_MOUSEWHEEL:
        scrollTo(hwnd, scrollY - GET_WHEEL_DELTA_WPARAM(wp) * 60 / 120);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: paintAll(hwnd); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int show) {
    WNDCLASSA wc; ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.hCursor = LoadCursor(NULL, IDC_ARROW); wc.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(1)); wc.lpszClassName = "VerityWnd";
    RegisterClassA(&wc);
    SetProcessDPIAware();
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT r = { 0, 0, 1100, 720 }; AdjustWindowRect(&r, style, FALSE);
    HWND hwnd = CreateWindowA("VerityWnd", "Verity - Truth Table Tool", style, CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, SW_SHOWMAXIMIZED); UpdateWindow(hwnd);
    MSG m; while (GetMessage(&m, NULL, 0, 0)) { TranslateMessage(&m); DispatchMessage(&m); }
    return 0;
}
