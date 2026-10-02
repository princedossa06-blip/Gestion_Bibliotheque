#pragma once
#include <iostream>
#include <optional>
#include "Database.h"
#include "Models.h"
#include "AuditService.h"

class LecteurService {
public:
    static std::string valider(const Lecteur& l) {
        if (l.nom.empty() || l.prenom.empty() || l.matricule.empty())
            return "Nom, prénom et matricule obligatoires.";
        return "";
    }

    static std::string ajouter(const Utilisateur& u, const Lecteur& l, int* idOut = nullptr) {
        if (auto e = valider(l); !e.empty()) return e;
        try {
            auto q = Database::instance().prep(
                "INSERT INTO lecteurs(nom, prenom, matricule, contact) VALUES (?,?,?,?)");
            q->setString(1, l.nom);
            q->setString(2, l.prenom);
            q->setString(3, l.matricule);
            if (l.contact.empty()) q->setNull(4, sql::DataType::VARCHAR); else q->setString(4, l.contact);
            q->execute();
        } catch (sql::SQLException& e) {
            if (estDoublon(e)) return "Ce matricule existe déjà.";
            throw;
        }
        int id = Database::instance().dernierId();
        if (idOut) *idOut = id;
        AuditService::log(u.id, "AJOUT_LECTEUR", "lecteurs", "id=" + std::to_string(id) + " " + l.matricule);
        return "";
    }

    static std::optional<Lecteur> obtenir(int id) {
        auto q = Database::instance().prep(
            "SELECT id, nom, prenom, matricule, contact FROM lecteurs WHERE id = ?");
        q->setInt(1, id);
        Rows rs(q->executeQuery());
        if (!rs->next()) return std::nullopt;
        Lecteur l;
        l.id = rs->getInt(1); l.nom = rs->getString(2); l.prenom = rs->getString(3);
        l.matricule = rs->getString(4);
        l.contact = rs->isNull(5) ? "" : std::string(rs->getString(5));
        return l;
    }

    static std::string modifier(const Utilisateur& u, const Lecteur& n) {
        if (auto e = valider(n); !e.empty()) return e;
        if (!obtenir(n.id)) return "Lecteur introuvable.";
        try {
            auto q = Database::instance().prep(
                "UPDATE lecteurs SET nom=?, prenom=?, matricule=?, contact=? WHERE id=?");
            q->setString(1, n.nom);
            q->setString(2, n.prenom);
            q->setString(3, n.matricule);
            if (n.contact.empty()) q->setNull(4, sql::DataType::VARCHAR); else q->setString(4, n.contact);
            q->setInt(5, n.id);
            q->executeUpdate();
        } catch (sql::SQLException& e) {
            if (estDoublon(e)) return "Ce matricule existe déjà.";
            throw;
        }
        AuditService::log(u.id, "MODIF_LECTEUR", "lecteurs", "id=" + std::to_string(n.id));
        return "";
    }

    static void rechercher(const std::string& terme) {
        auto q = Database::instance().prep(
            "SELECT id, nom, prenom, matricule, contact FROM lecteurs "
            "WHERE nom LIKE ? OR prenom LIKE ? OR matricule LIKE ? ORDER BY nom LIMIT 50");
        std::string motif = "%" + terme + "%";
        for (int i = 1; i <= 3; ++i) q->setString(i, motif);
        Rows rs(q->executeQuery());
        bool vide = true;
        while (rs->next()) {
            vide = false;
            std::cout << "#" << rs->getInt(1) << " | " << rs->getString(2) << " " << rs->getString(3)
                      << " | " << rs->getString(4)
                      << " | " << (rs->isNull(5) ? "-" : std::string(rs->getString(5))) << "\n";
        }
        if (vide) std::cout << "Aucun résultat.\n";
    }
};
