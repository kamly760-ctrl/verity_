// splash.h - Verity developer-style opening (terminal / IDE look).
// Use: #include "splash.h" then showSplash(); first in main(). ~4 seconds. Needs color + Unicode terminal (VS Code / Windows Terminal).
#pragma once
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
namespace sl {
inline void wait(int ms) { std::cout << std::flush; std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
inline void type(const std::string& s, int ms = 12) { for (char c : s) { std::cout << c; wait(ms); } }
}
inline void showSplash() {
    using namespace sl;
    const std::string R = "\033[0m", GR = "\033[38;2;98;114;164m", GN = "\033[1;38;2;80;250;123m", CY = "\033[1;38;2;139;233;253m",
                      PK = "\033[1;38;2;255;121;198m", PU = "\033[38;2;189;147;249m", YL = "\033[38;2;241;250;140m", OR = "\033[38;2;255;184;108m", W = "\033[1;97m";
    std::cout << "\033[2J\033[H\033[?25l\n";
    std::cout << GR << "  ┌─ Verity ───────────────────────────────── main.cpp ─ ● ● ●" << R << "\n" << GR << "  │" << R << "\n";
    // typed code with syntax colors
    std::cout << GR << "  │ " << "1  " << R << PK << "#include " << YL << "\"verity.h\"" << R << "\n"; wait(200);
    std::cout << GR << "  │ " << "2  " << R << CY << "int " << GN << "main" << R << "() {\n"; wait(200);
    std::cout << GR << "  │ " << "3  " << R << "    " << CY << "bool " << R << "result = " << GN << "evaluate" << R << "(" << YL << "\"A and B\"" << R << ");\n"; wait(250);
    std::cout << GR << "  │ " << "4  " << R << "    " << PK << "return " << OR << "0" << R << ";\n" << GR << "  │ " << "5  " << R << "}\n" << GR << "  │" << R << "\n"; wait(250);
    std::cout << GR << "  └─────────────────────────────────────────────────────────────" << R << "\n\n";
    // terminal part
    std::cout << "  " << GN << "$ " << R; type("g++ -std=c++11 main.cpp -o verity"); std::cout << "\n"; wait(150);
    const char* steps[] = {"parsing source", "linking logic engine", "linking truth table core"};
    for (int i = 0; i < 3; i++) { std::cout << "  " << GR << "[" << GN << (i + 1) * 33 << "%" << GR << "] " << R << steps[i] << "\n"; wait(200); }
    std::cout << "  " << GN << "build succeeded" << R << GR << "  0 errors, 0 warnings" << R << "\n\n";
    std::cout << "  " << GN << "$ " << R; type("./verity", 40); std::cout << "\n\n"; wait(300);
    std::cout << "  " << CY << "{ T  F }" << R << "   " << W << "Verity" << PU << " v1.0" << R << "\n"
              << "  " << PK << "{ F  T }" << R << "   " << GR << "truth table generator // Discrete Structures // Horus University" << R << "\n"
              << "           " << OR << "Made by Yusif Kamel" << R << "\n\n";
    std::cout << "  " << GN << "[ OK ]" << R << " logic engine  " << GN << "[ OK ]" << R << " parser  " << GN << "[ OK ]" << R << " ready\n\n\033[?25h";
    wait(300);
}
