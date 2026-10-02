#pragma once
#include <cctype>
#include <iostream>
#include <optional>
#include "Database.h"
#include "Models.h"
#include "AuditService.h"
#include "AuthService.h"

// Gestion des membres (réservée à l'administrateur, sauf changerMonMotDePasse)
class UtilisateurService {
public:
    static std::string validerLogin(const std::string& l) {
        if (l.size() < 3 || l.size() > 50) return "Le login doit faire 3 à 50 caractères.";
        for (char c : l)
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.'))
                return "Login : lettres, chiffres, '_' et '.' uniquement.";
        return "";
    }
    static std::string validerMdp(const std::string& m) {
        if (m.size() < 8) return "Le mot de passe doit faire au moins 8 caractères.";
        if (m.size() > 128) return "Mot de passe trop long.";
        return "";
    }

    static std::string creer(const Utilisateur& admin, const std::string& login,
                             const std::string& mdp, Role role = Role::MEMBRE) {
        if (admin.role != Role::ADMIN) return "Réservé à l'administrateur.";
        return inserer(admin.id, login, mdp, role);
    }

    // Création du tout premier admin (refusée s'il en existe déjà un)
    static std::string creerPremierAdmin(const std::string& login, const std::string& mdp) {
        auto q = Database::instance().prep("SELECT COUNT(*) FROM utilisateurs WHERE role = 'ADMIN'");
        Rows rs(q->executeQuery());
        rs->next();
        if (rs->getInt(1) > 0) return "Un administrateur existe déjà.";
        return inserer(0, login, mdp, Role::ADMIN);
    }

    static void lister() {
        auto q = Database::instance().prep(
            "SELECT id, login, role, actif FROM utilisateurs ORDER BY id");
        Rows rs(q->executeQuery());
        while (rs->next())
            std::cout << "#" << rs->getInt(1) << " | " << rs->getString(2) << " | "
                      << rs->getString(3) << " | " << (rs->getInt(4) ? "actif" : "DÉSACTIVÉ") << "\n";
    }

    static std::string changerActif(const Utilisateur& admin, int id, bool actif) {
        if (admin.role != Role::ADMIN) return "Réservé à l'administrateur.";
        if (id == admin.id) return "Vous ne pouvez pas modifier votre propre compte.";
        auto login = loginDe(id);
        if (!login) return "Membre introuvable.";
        auto q = Database::instance().prep("UPDATE utilisateurs SET actif = ? WHERE id = ?");
        q->setInt(1, actif ? 1 : 0);
        q->setInt(2, id);
        q->executeUpdate();
        AuditService::log(admin.id, actif ? "ACTIVATION_MEMBRE" : "DESACTIVATION_MEMBRE",
                          "utilisateurs", *login);
        return "";
    }

    static std::string reinitialiserMdp(const Utilisateur& admin, int id, const std::string& nouveau) {
        if (admin.role != Role::ADMIN) return "Réservé à l'administrateur.";
        if (auto e = validerMdp(nouveau); !e.empty()) return e;
        auto login = loginDe(id);
        if (!login) return "Membre introuvable.";
        miseAJourHash(id, nouveau);
        AuditService::log(admin.id, "REINIT_MDP", "utilisateurs", *login);
        return "";
    }

    static std::string changerMonMotDePasse(const Utilisateur& u, const std::string& ancien,
                                            const std::string& nouveau) {
        if (auto e = validerMdp(nouveau); !e.empty()) return e;
        auto q = Database::instance().prep("SELECT mot_de_passe_hash FROM utilisateurs WHERE id = ?");
        q->setInt(1, u.id);
        Rows rs(q->executeQuery());
        if (!rs->next() || !AuthService::verifier(rs->getString(1), ancien))
            return "Ancien mot de passe incorrect.";
        miseAJourHash(u.id, nouveau);
        AuditService::log(u.id, "CHANGEMENT_MDP", "utilisateurs", u.login);
        return "";
    }

private:
    static std::string inserer(int idAuteur, const std::string& login, const std::string& mdp, Role role) {
        if (auto e = validerLogin(login); !e.empty()) return e;
        if (auto e = validerMdp(mdp); !e.empty()) return e;
        try {
            auto q = Database::instance().prep(
                "INSERT INTO utilisateurs(login, mot_de_passe_hash, role) VALUES (?,?,?)");
            q->setString(1, login);
            q->setString(2, AuthService::hasher(mdp));
            q->setString(3, roleVersTexte(role));
            q->execute();
        } catch (sql::SQLException& e) {
            if (estDoublon(e)) return "Ce login existe déjà.";
            throw;
        }
        int nouvelId = Database::instance().dernierId();
        AuditService::log(idAuteur == 0 ? nouvelId : idAuteur,
                          "CREATION_MEMBRE", "utilisateurs", login + " (" + roleVersTexte(role) + ")");
        return "";
    }

    static void miseAJourHash(int id, const std::string& mdp) {
        auto q = Database::instance().prep("UPDATE utilisateurs SET mot_de_passe_hash = ? WHERE id = ?");
        q->setString(1, AuthService::hasher(mdp));
        q->setInt(2, id);
        q->executeUpdate();
    }

    static std::optional<std::string> loginDe(int id) {
        auto q = Database::instance().prep("SELECT login FROM utilisateurs WHERE id = ?");
        q->setInt(1, id);
        Rows rs(q->executeQuery());
        if (!rs->next()) return std::nullopt;
        return rs->getString(1);
    }
};
