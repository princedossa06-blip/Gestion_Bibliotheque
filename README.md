# Gestion de bibliothèque (C++ / MySQL)

## Installation (Ubuntu/Debian)
    sudo apt install g++ cmake libmysqlcppconn-dev libsodium-dev mysql-server

## Base de données
1. Éditer `sql/schema.sql` : remplacer `CHANGE_MOI` par un vrai mot de passe.
2. `sudo mysql < sql/schema.sql`

## Compilation
    mkdir build && cd build && cmake .. && make

## Configuration (variables d'environnement)
    export BIBLIO_DB_PASS='le_mot_de_passe_choisi'
    # facultatif : BIBLIO_DB_HOST (défaut tcp://127.0.0.1:3306), BIBLIO_DB_USER (biblio_user), BIBLIO_DB_NAME (bibliotheque)

## Premier lancement
    ./biblio --init-admin     # crée le premier administrateur (une seule fois)
    ./biblio                  # connexion et menus

## Règles métier
- Durée de prêt : 14 jours ; 3 emprunts simultanés max par lecteur ; emprunt refusé si le lecteur a un retard.
- Alertes de retard générées automatiquement à l'ouverture du menu admin ; l'admin les traite (sanction libre).
- Audit : accessible à l'admin seul ; la table est en ajout seul (aucun UPDATE/DELETE possible par l'application).
- Livres et lecteurs : gérés par tous les membres ; suppression d'un livre réservée à l'admin.

## Tests
    bash tests/run_tests.sh     # recrée une base bibliotheque_test, compile, lance 65 vérifications
