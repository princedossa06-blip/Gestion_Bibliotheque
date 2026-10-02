#pragma once
#include <iostream>
#include <optional>
#include "Database.h"
#include "Models.h"
#include "AuditService.h"

class LivreService {
public:
    static std::string valider(const Livre& l) {
        if (l.titre.empty() || l.auteur.empty()) return "Titre et auteur obligatoires.";
        if (l.total < 1 || l.total > 1000) return "Le nombre d'exemplaires doit être entre 1 et 1000.";
        return "";
    }

    static std::string ajouter(const Utilisateur& u, Livre l, int* idOut = nullptr) {
        l.dispo = l.total;
        if (auto e = valider(l); !e.empty()) return e;
        try {
            auto q = Database::instance().prep(
                "INSERT INTO livres(titre, auteur, isbn, exemplaires_total, exemplaires_dispo) VALUES (?,?,?,?,?)");
            q->setString(1, l.titre);
            q->setString(2, l.auteur);
            if (l.isbn.empty()) q->setNull(3, sql::DataType::VARCHAR); else q->setString(3, l.isbn);
            q->setInt(4, l.total);
            q->setInt(5, l.dispo);
            q->execute();
        } catch (sql::SQLException& e) {
            if (estDoublon(e)) return "Cet ISBN existe déjà.";
            throw;
        }
        int id = Database::instance().dernierId();
        if (idOut) *idOut = id;
        AuditService::log(u.id, "AJOUT_LIVRE", "livres", "id=" + std::to_string(id) + " " + l.titre);
        return "";
    }

    static std::optional<Livre> obtenir(int id) {
        auto q = Database::instance().prep(
            "SELECT id, titre, auteur, isbn, exemplaires_total, exemplaires_dispo FROM livres WHERE id = ?");
        q->setInt(1, id);
        Rows rs(q->executeQuery());
        if (!rs->next()) return std::nullopt;
        Livre l;
        l.id = rs->getInt(1); l.titre = rs->getString(2); l.auteur = rs->getString(3);
        l.isbn = rs->isNull(4) ? "" : std::string(rs->getString(4));
        l.total = rs->getInt(5); l.dispo = rs->getInt(6);
        return l;
    }

    // Le stock disponible est recalculé : on ne peut pas descendre sous le nombre d'exemplaires empruntés.
    static std::string modifier(const Utilisateur& u, const Livre& n) {
        if (auto e = valider(n); !e.empty()) return e;
        try {
            Transaction tx;
            auto q = Database::instance().prep(
                "SELECT exemplaires_total, exemplaires_dispo FROM livres WHERE id = ? FOR UPDATE");
            q->setInt(1, n.id);
            Rows rs(q->executeQuery());
            if (!rs->next()) return "Livre introuvable.";
            int enCours = rs->getInt(1) - rs->getInt(2);
            if (n.total < enCours)
                return "Impossible : " + std::to_string(enCours) + " exemplaire(s) actuellement empruntés.";

            auto up = Database::instance().prep(
                "UPDATE livres SET titre=?, auteur=?, isbn=?, exemplaires_total=?, exemplaires_dispo=? WHERE id=?");
            up->setString(1, n.titre);
            up->setString(2, n.auteur);
            if (n.isbn.empty()) up->setNull(3, sql::DataType::VARCHAR); else up->setString(3, n.isbn);
            up->setInt(4, n.total);
            up->setInt(5, n.total - enCours);
            up->setInt(6, n.id);
            up->executeUpdate();
            tx.commit();
        } catch (sql::SQLException& e) {
            if (estDoublon(e)) return "Cet ISBN existe déjà.";
            throw;
        }
        AuditService::log(u.id, "MODIF_LIVRE", "livres", "id=" + std::to_string(n.id) + " " + n.titre);
        return "";
    }

    static std::string supprimer(const Utilisateur& u, int id) {
        if (u.role != Role::ADMIN) return "Réservé à l'administrateur.";
        auto l = obtenir(id);
        if (!l) return "Livre introuvable.";
        auto c = Database::instance().prep("SELECT COUNT(*) FROM emprunts WHERE id_livre = ?");
        c->setInt(1, id);
        Rows rs(c->executeQuery());
        rs->next();
        if (rs->getInt(1) > 0) return "Suppression impossible : ce livre a un historique d'emprunts.";
        auto d = Database::instance().prep("DELETE FROM livres WHERE id = ?");
        d->setInt(1, id);
        d->executeUpdate();
        AuditService::log(u.id, "SUPPRESSION_LIVRE", "livres", "id=" + std::to_string(id) + " " + l->titre);
        return "";
    }

    // terme vide = tout lister (50 max)
    static void rechercher(const std::string& terme) {
        auto q = Database::instance().prep(
            "SELECT id, titre, auteur, isbn, exemplaires_dispo, exemplaires_total FROM livres "
            "WHERE titre LIKE ? OR auteur LIKE ? OR isbn LIKE ? ORDER BY titre LIMIT 50");
        std::string motif = "%" + terme + "%";
        for (int i = 1; i <= 3; ++i) q->setString(i, motif);
        Rows rs(q->executeQuery());
        bool vide = true;
        while (rs->next()) {
            vide = false;
            std::cout << "#" << rs->getInt(1) << " | " << rs->getString(2) << " | " << rs->getString(3)
                      << " | ISBN " << (rs->isNull(4) ? "-" : std::string(rs->getString(4)))
                      << " | dispo " << rs->getInt(5) << "/" << rs->getInt(6) << "\n";
        }
        if (vide) std::cout << "Aucun résultat.\n";
    }
};
