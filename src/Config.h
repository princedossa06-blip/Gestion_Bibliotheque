#pragma once
#include <cstdlib>
#include <string>

// Les identifiants ne sont plus écrits en dur : ils viennent de variables d'environnement.
struct Config {
    std::string host, user, pass, name;

    static Config depuisEnv() {
        auto lire = [](const char* cle, const char* defaut) {
            const char* v = std::getenv(cle);
            return std::string(v ? v : defaut);
        };
        return { lire("BIBLIO_DB_HOST", "tcp://127.0.0.1:3306"),
                 lire("BIBLIO_DB_USER", "biblio_user"),
                 lire("BIBLIO_DB_PASS", ""),
                 lire("BIBLIO_DB_NAME", "bibliotheque") };
    }
};
