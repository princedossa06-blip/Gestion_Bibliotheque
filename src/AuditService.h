#pragma once
#include <iostream>
#include "Database.h"

class AuditService {
public:
    static void log(int idUtilisateur, const std::string& action,
                    const std::string& table, const std::string& detail) {
        auto ps = Database::instance().prep(
            "INSERT INTO audit(id_utilisateur, action, table_cible, detail) VALUES (?,?,?,?)");
        ps->setInt(1, idUtilisateur);
        ps->setString(2, action);
        ps->setString(3, table);
        ps->setString(4, detail.substr(0, 500));
        ps->execute();
    }

    // Réservé à l'ADMIN (contrôlé dans le menu)
    static void afficher(int limite = 30) {
        auto ps = Database::instance().prep(
            "SELECT a.date_heure, u.login, a.action, a.table_cible, a.detail "
            "FROM audit a JOIN utilisateurs u ON u.id = a.id_utilisateur "
            "ORDER BY a.id DESC LIMIT ?");
        ps->setInt(1, limite);
        Rows rs(ps->executeQuery());
        while (rs->next())
            std::cout << rs->getString(1) << " | " << rs->getString(2) << " | "
                      << rs->getString(3) << " | " << rs->getString(4) << " | "
                      << rs->getString(5) << "\n";
    }
};
