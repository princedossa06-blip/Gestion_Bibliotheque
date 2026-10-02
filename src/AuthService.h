#pragma once
#include <optional>
#include <stdexcept>
#include <sodium.h>
#include "Database.h"
#include "Models.h"
#include "AuditService.h"

class AuthService {
public:
    static std::optional<Utilisateur> login(const std::string& login, const std::string& mdp) {
        auto ps = Database::instance().prep(
            "SELECT id, mot_de_passe_hash, role, actif FROM utilisateurs WHERE login = ?");
        ps->setString(1, login);
        Rows rs(ps->executeQuery());
        if (!rs->next()) return std::nullopt;

        int id = rs->getInt("id");
        std::string hash = rs->getString("mot_de_passe_hash");
        if (crypto_pwhash_str_verify(hash.c_str(), mdp.c_str(), mdp.size()) != 0) {
            AuditService::log(id, "ECHEC_CONNEXION", "utilisateurs", login);
            return std::nullopt;
        }
        if (rs->getInt("actif") != 1) {
            AuditService::log(id, "CONNEXION_REFUSEE", "utilisateurs", login + " (compte désactivé)");
            return std::nullopt;
        }
        Utilisateur u{id, login, rs->getString("role") == "ADMIN" ? Role::ADMIN : Role::MEMBRE};
        AuditService::log(u.id, "CONNEXION", "utilisateurs", login);
        return u;
    }

    static std::string hasher(const std::string& mdp) {
        char out[crypto_pwhash_STRBYTES];
        if (crypto_pwhash_str(out, mdp.c_str(), mdp.size(),
                              crypto_pwhash_OPSLIMIT_INTERACTIVE,
                              crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
            throw std::runtime_error("Hachage impossible (mémoire insuffisante).");
        return out;
    }

    static bool verifier(const std::string& hash, const std::string& mdp) {
        return crypto_pwhash_str_verify(hash.c_str(), mdp.c_str(), mdp.size()) == 0;
    }
};
