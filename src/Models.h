#pragma once
#include <string>

enum class Role { ADMIN, MEMBRE };

inline std::string roleVersTexte(Role r) { return r == Role::ADMIN ? "ADMIN" : "MEMBRE"; }

struct Utilisateur {
    int id;
    std::string login;
    Role role;
};

struct Livre {
    int id = 0;
    std::string titre, auteur, isbn;   // isbn vide = non renseigné
    int total = 1;
    int dispo = 1;
};

struct Lecteur {
    int id = 0;
    std::string nom, prenom, matricule, contact;
};
