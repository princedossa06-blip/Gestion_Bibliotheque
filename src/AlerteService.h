#pragma once
#include <iostream>
#include "Database.h"
#include "Models.h"
#include "AuditService.h"

// Alertes de retard et sanctions (réservé à l'administrateur)
class AlerteService {
public:
    // Crée une alerte pour chaque emprunt en retard qui n'en a pas encore. Retourne le nombre créé.
    static int generer(const Utilisateur& admin) {
        if (admin.role != Role::ADMIN) return 0;
        std::unique_ptr<sql::Statement> st(Database::instance().get()->createStatement());
        int n = st->executeUpdate(
            "INSERT INTO alertes_retard(id_emprunt, date_alerte) "
            "SELECT e.id, CURDATE() FROM emprunts e "
            "WHERE e.date_retour IS NULL AND e.date_limite < CURDATE() "
            "AND NOT EXISTS (SELECT 1 FROM alertes_retard a WHERE a.id_emprunt = e.id)");
        if (n > 0)
            AuditService::log(admin.id, "GENERATION_ALERTES", "alertes_retard",
                              std::to_string(n) + " nouvelle(s) alerte(s)");
        return n;
    }

    static int afficherNonTraitees() {
        auto q = Database::instance().prep(
            "SELECT a.id, l.titre, r.nom, r.prenom, e.date_limite, "
            "DATEDIFF(COALESCE(e.date_retour, CURDATE()), e.date_limite), (e.date_retour IS NULL) "
            "FROM alertes_retard a JOIN emprunts e ON e.id = a.id_emprunt "
            "JOIN livres l ON l.id = e.id_livre JOIN lecteurs r ON r.id = e.id_lecteur "
            "WHERE a.traitee = 0 ORDER BY e.date_limite");
        Rows rs(q->executeQuery());
        int n = 0;
        while (rs->next()) {
            ++n;
            std::cout << "Alerte #" << rs->getInt(1) << " | " << rs->getString(2) << " | "
                      << rs->getString(3) << " " << rs->getString(4)
                      << " | " << rs->getInt(6) << " j de retard | "
                      << (rs->getInt(7) ? "NON RENDU" : "rendu") << "\n";
        }
        if (n == 0) std::cout << "Aucune alerte en attente.\n";
        return n;
    }

    // sanction vide = "Aucune sanction"
    static std::string traiter(const Utilisateur& admin, int idAlerte, std::string sanction) {
        if (admin.role != Role::ADMIN) return "Réservé à l'administrateur.";
        if (sanction.empty()) sanction = "Aucune sanction";
        auto q = Database::instance().prep(
            "UPDATE alertes_retard SET traitee = 1, sanction = ?, id_admin = ?, date_traitement = NOW() "
            "WHERE id = ? AND traitee = 0");
        q->setString(1, sanction);
        q->setInt(2, admin.id);
        q->setInt(3, idAlerte);
        if (q->executeUpdate() == 0) return "Alerte introuvable ou déjà traitée.";
        AuditService::log(admin.id, "SANCTION", "alertes_retard",
                          "alerte=" + std::to_string(idAlerte) + " : " + sanction);
        return "";
    }
};
