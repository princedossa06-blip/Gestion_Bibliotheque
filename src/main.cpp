#include <functional>
#include <iostream>
#include <sodium.h>
#include "Config.h"
#include "Database.h"
#include "Utils.h"
#include "AuthService.h"
#include "AuditService.h"
#include "UtilisateurService.h"
#include "LivreService.h"
#include "LecteurService.h"
#include "EmpruntService.h"
#include "AlerteService.h"

using namespace std;

namespace {

// Exécute une action en affichant proprement les erreurs (le programme ne plante pas)
void executer(const function<void()>& action) {
    try { action(); }
    catch (const sql::SQLException& e) { cerr << "Erreur base de données : " << e.what() << "\n"; }
    catch (const runtime_error&) { throw; }   // entrée fermée : on quitte
    catch (const exception& e) { cerr << "Erreur : " << e.what() << "\n"; }
}

void resultat(const string& erreur, const string& succes) {
    cout << (erreur.empty() ? succes : "Échec : " + erreur) << "\n";
}

void menuLivres(const Utilisateur& u) {
    int c = -1;
    while (c != 0) {
        cout << "\n--- Livres ---\n1. Rechercher / lister\n2. Ajouter\n3. Modifier\n";
        if (u.role == Role::ADMIN) cout << "4. Supprimer\n";
        cout << "0. Retour\n";
        c = ui::lireEntier("> ", 0, 4);
        executer([&] {
            if (c == 1) {
                LivreService::rechercher(ui::lireLigne("Recherche (vide = tout) : ", false, 100));
            } else if (c == 2) {
                Livre l;
                l.titre  = ui::lireLigne("Titre : ");
                l.auteur = ui::lireLigne("Auteur : ", true, 150);
                l.isbn   = ui::lireLigne("ISBN (facultatif) : ", false, 20);
                l.total  = ui::lireEntier("Nombre d'exemplaires : ", 1, 1000);
                int id = 0;
                string e = LivreService::ajouter(u, l, &id);
                resultat(e, "Livre ajouté (id " + to_string(id) + ").");
            } else if (c == 3) {
                int id = ui::lireEntier("ID du livre : ", 1, 2000000000);
                auto l = LivreService::obtenir(id);
                if (!l) { cout << "Livre introuvable.\n"; return; }
                cout << "(Entrée vide = garder la valeur actuelle)\n";
                string t = ui::lireLigne("Titre [" + l->titre + "] : ", false);
                string a = ui::lireLigne("Auteur [" + l->auteur + "] : ", false, 150);
                string i = ui::lireLigne("ISBN [" + l->isbn + "] : ", false, 20);
                string n = ui::lireLigne("Exemplaires [" + to_string(l->total) + "] : ", false, 4);
                if (!t.empty()) l->titre = t;
                if (!a.empty()) l->auteur = a;
                if (!i.empty()) l->isbn = i;
                if (!n.empty()) { try { l->total = stoi(n); } catch (...) { cout << "Nombre invalide.\n"; return; } }
                resultat(LivreService::modifier(u, *l), "Livre modifié.");
            } else if (c == 4 && u.role == Role::ADMIN) {
                int id = ui::lireEntier("ID du livre à supprimer : ", 1, 2000000000);
                if (ui::confirmer("Confirmer la suppression"))
                    resultat(LivreService::supprimer(u, id), "Livre supprimé.");
            }
        });
    }
}

void menuLecteurs(const Utilisateur& u) {
    int c = -1;
    while (c != 0) {
        cout << "\n--- Lecteurs ---\n1. Rechercher / lister\n2. Enregistrer\n3. Modifier\n0. Retour\n";
        c = ui::lireEntier("> ", 0, 3);
        executer([&] {
            if (c == 1) {
                LecteurService::rechercher(ui::lireLigne("Recherche (vide = tout) : ", false, 100));
            } else if (c == 2) {
                Lecteur l;
                l.nom       = ui::lireLigne("Nom : ", true, 100);
                l.prenom    = ui::lireLigne("Prénom : ", true, 100);
                l.matricule = ui::lireLigne("Matricule : ", true, 50);
                l.contact   = ui::lireLigne("Contact (facultatif) : ", false, 100);
                int id = 0;
                string e = LecteurService::ajouter(u, l, &id);
                resultat(e, "Lecteur enregistré (id " + to_string(id) + ").");
            } else if (c == 3) {
                int id = ui::lireEntier("ID du lecteur : ", 1, 2000000000);
                auto l = LecteurService::obtenir(id);
                if (!l) { cout << "Lecteur introuvable.\n"; return; }
                cout << "(Entrée vide = garder la valeur actuelle)\n";
                string v;
                v = ui::lireLigne("Nom [" + l->nom + "] : ", false, 100);            if (!v.empty()) l->nom = v;
                v = ui::lireLigne("Prénom [" + l->prenom + "] : ", false, 100);      if (!v.empty()) l->prenom = v;
                v = ui::lireLigne("Matricule [" + l->matricule + "] : ", false, 50); if (!v.empty()) l->matricule = v;
                v = ui::lireLigne("Contact [" + l->contact + "] : ", false, 100);    if (!v.empty()) l->contact = v;
                resultat(LecteurService::modifier(u, *l), "Lecteur modifié.");
            }
        });
    }
}

void menuMembres(const Utilisateur& admin) {
    int c = -1;
    while (c != 0) {
        cout << "\n--- Gestion des membres ---\n1. Lister\n2. Créer un membre\n3. Désactiver\n"
                "4. Réactiver\n5. Réinitialiser un mot de passe\n0. Retour\n";
        c = ui::lireEntier("> ", 0, 5);
        executer([&] {
            if (c == 1) {
                UtilisateurService::lister();
            } else if (c == 2) {
                string login = ui::lireLigne("Login : ", true, 50);
                string mdp = ui::lireMotDePasse("Mot de passe (8 caractères min.) : ");
                Role r = ui::confirmer("Donner le rôle ADMIN") ? Role::ADMIN : Role::MEMBRE;
                resultat(UtilisateurService::creer(admin, login, mdp, r), "Membre créé.");
            } else if (c == 3 || c == 4) {
                int id = ui::lireEntier("ID du membre : ", 1, 2000000000);
                resultat(UtilisateurService::changerActif(admin, id, c == 4), "Compte mis à jour.");
            } else if (c == 5) {
                int id = ui::lireEntier("ID du membre : ", 1, 2000000000);
                string mdp = ui::lireMotDePasse("Nouveau mot de passe : ");
                resultat(UtilisateurService::reinitialiserMdp(admin, id, mdp), "Mot de passe réinitialisé.");
            }
        });
    }
}

void menuAlertes(const Utilisateur& admin) {
    int c = -1;
    while (c != 0) {
        cout << "\n--- Alertes de retard ---\n";
        executer([&] {
            int n = AlerteService::generer(admin);
            if (n > 0) cout << n << " nouvelle(s) alerte(s) générée(s).\n";
            AlerteService::afficherNonTraitees();
        });
        cout << "1. Traiter une alerte (appliquer une sanction)\n0. Retour\n";
        c = ui::lireEntier("> ", 0, 1);
        if (c == 1) executer([&] {
            int id = ui::lireEntier("ID de l'alerte : ", 1, 2000000000);
            string s = ui::lireLigne("Sanction (vide = aucune) : ", false, 255);
            resultat(AlerteService::traiter(admin, id, s), "Alerte traitée.");
        });
    }
}

void menuPrincipal(const Utilisateur& u) {
    bool admin = u.role == Role::ADMIN;
    if (admin) {
        executer([&] {
            int n = AlerteService::generer(u);
            cout << "--- Alertes de retard en attente ---\n";
            int restantes = AlerteService::afficherNonTraitees();
            if (n > 0 || restantes > 0) cout << "(menu 7 pour traiter)\n";
        });
    }
    int c = -1;
    while (c != 0) {
        cout << "\n=== Bibliothèque (" << u.login << " - " << roleVersTexte(u.role) << ") ===\n"
                "1. Livres\n2. Lecteurs\n3. Emprunter\n4. Retourner\n5. Emprunts en cours\n"
                "6. Changer mon mot de passe\n";
        if (admin) cout << "7. Alertes de retard / sanctions\n8. Gestion des membres\n9. Journal d'audit\n";
        cout << "0. Quitter\n";
        c = ui::lireEntier("> ", 0, admin ? 9 : 6);
        switch (c) {
            case 1: menuLivres(u); break;
            case 2: menuLecteurs(u); break;
            case 3: executer([&] {
                int l = ui::lireEntier("ID du livre : ", 1, 2000000000);
                int r = ui::lireEntier("ID du lecteur : ", 1, 2000000000);
                int id = 0;
                string e = EmpruntService::emprunter(u, l, r, &id);
                resultat(e, "Emprunt n°" + to_string(id) + " enregistré (" +
                            to_string(EmpruntService::DUREE_JOURS) + " jours).");
            }); break;
            case 4: executer([&] {
                int e = ui::lireEntier("ID de l'emprunt : ", 1, 2000000000);
                bool retard = false;
                string err = EmpruntService::retourner(u, e, &retard);
                resultat(err, retard ? "Retour enregistré (EN RETARD, signalé à l'administrateur)."
                                     : "Retour enregistré.");
            }); break;
            case 5: executer([&] { EmpruntService::listerEnCours(); }); break;
            case 6: executer([&] {
                string a = ui::lireMotDePasse("Ancien mot de passe : ");
                string n = ui::lireMotDePasse("Nouveau mot de passe : ");
                resultat(UtilisateurService::changerMonMotDePasse(u, a, n), "Mot de passe modifié.");
            }); break;
            case 7: if (admin) menuAlertes(u); break;
            case 8: if (admin) menuMembres(u); break;
            case 9: if (admin) executer([&] {
                AuditService::afficher(ui::lireEntier("Nombre d'entrées à afficher (1-500) : ", 1, 500));
            }); break;
            default: break;
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (sodium_init() < 0) { cerr << "Erreur d'initialisation de libsodium.\n"; return 1; }

    Config cfg = Config::depuisEnv();
    if (cfg.pass.empty()) {
        cerr << "Définissez la variable d'environnement BIBLIO_DB_PASS (mot de passe MySQL).\n"
                "Autres variables : BIBLIO_DB_HOST, BIBLIO_DB_USER, BIBLIO_DB_NAME.\n";
        return 1;
    }

    try {
        Database::instance().connect(cfg.host, cfg.user, cfg.pass, cfg.name);

        // Première utilisation : ./biblio --init-admin
        if (argc > 1 && string(argv[1]) == "--init-admin") {
            string login = ui::lireLigne("Login de l'administrateur : ", true, 50);
            string m1 = ui::lireMotDePasse("Mot de passe (8 caractères min.) : ");
            string m2 = ui::lireMotDePasse("Confirmez le mot de passe : ");
            if (m1 != m2) { cerr << "Les mots de passe ne correspondent pas.\n"; return 1; }
            string e = UtilisateurService::creerPremierAdmin(login, m1);
            if (!e.empty()) { cerr << "Échec : " << e << "\n"; return 1; }
            cout << "Administrateur créé. Relancez sans option pour vous connecter.\n";
            return 0;
        }

        optional<Utilisateur> user;
        for (int essai = 1; essai <= 3 && !user; ++essai) {
            string login = ui::lireLigne("Login : ", true, 50);
            string mdp = ui::lireMotDePasse("Mot de passe : ");
            user = AuthService::login(login, mdp);
            if (!user) cout << "Identifiants invalides ou compte désactivé.\n";
        }
        if (!user) { cout << "Trop de tentatives.\n"; return 1; }

        menuPrincipal(*user);
        AuditService::log(user->id, "DECONNEXION", "utilisateurs", user->login);
    } catch (const sql::SQLException& e) {
        cerr << "Erreur SQL : " << e.what() << "\n";
        return 1;
    } catch (const exception& e) {
        cerr << "Arrêt : " << e.what() << "\n";
        return 1;
    }
    return 0;
}
