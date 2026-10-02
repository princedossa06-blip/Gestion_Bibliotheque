#pragma once
#include <iostream>
#include <stdexcept>
#include <string>
#include <termios.h>
#include <unistd.h>

// Saisies console avec validation
namespace ui {

inline std::string trim(const std::string& s) {
    const char* esp = " \t\r\n";
    size_t a = s.find_first_not_of(esp);
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(esp) - a + 1);
}

inline std::string lireLigne(const std::string& invite, bool obligatoire = true, size_t max = 255) {
    while (true) {
        std::cout << invite << std::flush;
        std::string s;
        if (!std::getline(std::cin, s)) throw std::runtime_error("Entrée fermée.");
        s = trim(s);
        if (s.empty() && obligatoire) { std::cout << "Champ obligatoire.\n"; continue; }
        if (s.size() > max) { std::cout << "Trop long (max " << max << " caractères).\n"; continue; }
        return s;
    }
}

inline int lireEntier(const std::string& invite, int min, int max) {
    while (true) {
        std::string s = lireLigne(invite, true, 12);
        try {
            size_t pos = 0;
            int v = std::stoi(s, &pos);
            if (pos == s.size() && v >= min && v <= max) return v;
        } catch (...) {}
        std::cout << "Entrez un nombre entre " << min << " et " << max << ".\n";
    }
}

inline std::string lireMotDePasse(const std::string& invite) {
    std::cout << invite << std::flush;
    std::string s;
    bool terminal = isatty(STDIN_FILENO);
    termios ancien{};
    if (terminal) {
        tcgetattr(STDIN_FILENO, &ancien);
        termios masque = ancien;
        masque.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &masque);
    }
    bool ok = static_cast<bool>(std::getline(std::cin, s));
    if (terminal) { tcsetattr(STDIN_FILENO, TCSANOW, &ancien); std::cout << "\n"; }
    if (!ok) throw std::runtime_error("Entrée fermée.");
    return s;
}

inline bool confirmer(const std::string& invite) {
    while (true) {
        std::string s = lireLigne(invite + " (o/n) : ", true, 3);
        if (s == "o" || s == "O") return true;
        if (s == "n" || s == "N") return false;
    }
}

} // namespace ui
