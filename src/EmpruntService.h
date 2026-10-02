#pragma once
#include <iostream>
#include "Database.h"
#include "Models.h"
#include "AuditService.h"

class EmpruntService {
public:
    static constexpr int DUREE_JOURS  = 14;  // durée de prêt
    static constexpr int MAX_EMPRUNTS = 3;   // emprunts simultanés par lecteur

    // Retourne "" si OK, sinon le message d'erreur.
    static std::string emprunter(const Utilisateur& u, int idLivre, int idLecteur, int* idOut = nullptr) {
        auto& db = Database::instance();
        int idEmprunt = 0;
        {
            Transaction tx;   // annulée automatiquement en cas de retour anticipé
            auto q = db.prep("SELECT exemplaires_dispo FROM livres WHERE id = ? FOR UPDATE");
            q->setInt(1, idLivre);
            Rows rs(q->executeQuery());
            if (!rs->next()) return "Livre introuvable.";
            if (rs->getInt(1) <= 0) return "Aucun exemplaire disponible.";

            auto ql = db.prep("SELECT id FROM lecteurs WHERE id = ? FOR UPDATE");
            ql->setInt(1, idLecteur);
            Rows rl(ql->executeQuery());
            if (!rl->next()) return "Lecteur introuvable.";

            auto qc = db.prep(
                "SELECT COUNT(*), CAST(COALESCE(SUM(date_limite < CURDATE()),0) AS SIGNED) "
                "FROM emprunts WHERE id_lecteur = ? AND date_retour IS NULL");
            qc->setInt(1, idLecteur);
            Rows rc(qc->executeQuery());
            rc->next();
            if (rc->getInt(2) > 0) return "Ce lecteur a un livre en retard : emprunt refusé.";
            if (rc->getInt(1) >= MAX_EMPRUNTS)
                return "Ce lecteur a déjà " + std::to_string(MAX_EMPRUNTS) + " emprunts en cours.";

            auto ins = db.prep(
                "INSERT INTO emprunts(id_livre, id_lecteur, id_utilisateur, date_emprunt, date_limite, statut) "
                "VALUES (?,?,?,CURDATE(),DATE_ADD(CURDATE(), INTERVAL ? DAY),'EN_COURS')");
            ins->setInt(1, idLivre);
            ins->setInt(2, idLecteur);
            ins->setInt(3, u.id);
            ins->setInt(4, DUREE_JOURS);
            ins->execute();
            idEmprunt = db.dernierId();

            auto upd = db.prep("UPDATE livres SET exemplaires_dispo = exemplaires_dispo - 1 WHERE id = ?");
            upd->setInt(1, idLivre);
            upd->executeUpdate();
            tx.commit();
        }
        if (idOut) *idOut = idEmprunt;
        AuditService::log(u.id, "EMPRUNT", "emprunts",
                          "id=" + std::to_string(idEmprunt) + " livre=" + std::to_string(idLivre) +
                          " lecteur=" + std::to_string(idLecteur));
        return "";
    }

    static std::string retourner(const Utilisateur& u, int idEmprunt, bool* enRetard = nullptr) {
        auto& db = Database::instance();
        int idLivre = 0;
        bool retard = false;
        {
            Transaction tx;
            auto q = db.prep(
                "SELECT id_livre, (date_limite < CURDATE()) FROM emprunts "
                "WHERE id = ? AND date_retour IS NULL FOR UPDATE");
            q->setInt(1, idEmprunt);
            Rows rs(q->executeQuery());
            if (!rs->next()) return "Emprunt introuvable ou déjà retourné.";
            idLivre = rs->getInt(1);
            retard = rs->getInt(2) == 1;

            auto up = db.prep("UPDATE emprunts SET date_retour = CURDATE(), statut = 'RETOURNE' WHERE id = ?");
            up->setInt(1, idEmprunt);
            up->executeUpdate();
            auto lv = db.prep("UPDATE livres SET exemplaires_dispo = exemplaires_dispo + 1 WHERE id = ?");
            lv->setInt(1, idLivre);
            lv->executeUpdate();
            tx.commit();
        }
        if (enRetard) *enRetard = retard;
        AuditService::log(u.id, "RETOUR", "emprunts",
                          "id=" + std::to_string(idEmprunt) + (retard ? " (EN RETARD)" : ""));
        return "";
    }

    static void listerEnCours() {
        auto q = Database::instance().prep(
            "SELECT e.id, l.titre, r.nom, r.prenom, e.date_limite, (e.date_limite < CURDATE()) "
            "FROM emprunts e JOIN livres l ON l.id = e.id_livre JOIN lecteurs r ON r.id = e.id_lecteur "
            "WHERE e.date_retour IS NULL ORDER BY e.date_limite LIMIT 100");
        Rows rs(q->executeQuery());
        bool vide = true;
        while (rs->next()) {
            vide = false;
            std::cout << "#" << rs->getInt(1) << " | " << rs->getString(2) << " | "
                      << rs->getString(3) << " " << rs->getString(4)
                      << " | limite " << rs->getString(5)
                      << (rs->getInt(6) ? " | EN RETARD" : "") << "\n";
        }
        if (vide) std::cout << "Aucun emprunt en cours.\n";
    }
};
