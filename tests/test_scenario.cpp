// Test de scénario complet. À lancer via tests/run_tests.sh (base dédiée, recréée à chaque fois).
#include <iostream>
#include <sodium.h>
#include "Config.h"
#include "Database.h"
#include "AuditService.h"
#include "AuthService.h"
#include "UtilisateurService.h"
#include "LivreService.h"
#include "LecteurService.h"
#include "EmpruntService.h"
#include "AlerteService.h"

static int echecs = 0, total = 0;
#define CHECK(cond) do { ++total; if (!(cond)) { ++echecs; \
    std::cerr << "ECHEC ligne " << __LINE__ << " : " #cond "\n"; } } while (0)

static int compter(const std::string& requete) {
    auto q = Database::instance().prep(requete);
    Rows rs(q->executeQuery());
    rs->next();
    return rs->getInt(1);
}
static void exec(const std::string& requete) {
    std::unique_ptr<sql::Statement> st(Database::instance().get()->createStatement());
    st->execute(requete);
}

int main() {
    if (sodium_init() < 0) return 2;
    Config cfg = Config::depuisEnv();
    if (cfg.name.find("test") == std::string::npos) {
        std::cerr << "Refus : le nom de la base doit contenir 'test'.\n";
        return 2;
    }
    Database::instance().connect(cfg.host, cfg.user, cfg.pass, cfg.name);

    // --- Comptes ---
    CHECK(UtilisateurService::creerPremierAdmin("admin", "motdepasse1").empty());
    CHECK(!UtilisateurService::creerPremierAdmin("admin2", "motdepasse1").empty());   // un seul premier admin
    auto admin = AuthService::login("admin", "motdepasse1");
    CHECK(admin.has_value() && admin->role == Role::ADMIN);
    CHECK(!AuthService::login("admin", "mauvais").has_value());
    CHECK(!AuthService::login("inconnu", "x").has_value());

    CHECK(UtilisateurService::creer(*admin, "marie", "secret1234").empty());
    CHECK(!UtilisateurService::creer(*admin, "marie", "secret1234").empty());         // doublon
    CHECK(!UtilisateurService::creer(*admin, "x y", "secret1234").empty());           // login invalide
    CHECK(!UtilisateurService::creer(*admin, "court", "abc").empty());                // mdp trop court
    auto marie = AuthService::login("marie", "secret1234");
    CHECK(marie.has_value() && marie->role == Role::MEMBRE);
    CHECK(!UtilisateurService::creer(*marie, "pirate", "secret1234").empty());        // membre ne crée pas de compte
    CHECK(!UtilisateurService::changerActif(*admin, admin->id, false).empty());       // pas d'auto-désactivation
    CHECK(UtilisateurService::changerActif(*admin, marie->id, false).empty());
    CHECK(!AuthService::login("marie", "secret1234").has_value());                    // compte désactivé
    CHECK(UtilisateurService::changerActif(*admin, marie->id, true).empty());
    CHECK(UtilisateurService::changerMonMotDePasse(*marie, "secret1234", "nouveau5678").empty());
    CHECK(!AuthService::login("marie", "secret1234").has_value());
    marie = AuthService::login("marie", "nouveau5678");
    CHECK(marie.has_value());

    // --- Livres ---
    int l1 = 0, l2 = 0, l3 = 0, l4 = 0;
    Livre a; a.titre = "Les Misérables"; a.auteur = "Victor Hugo"; a.isbn = "111"; a.total = 2;
    CHECK(LivreService::ajouter(*marie, a, &l1).empty());
    CHECK(!LivreService::ajouter(*marie, a).empty());                                  // ISBN en double
    Livre b; b.titre = "Germinal"; b.auteur = "Zola"; b.total = 1;
    CHECK(LivreService::ajouter(*marie, b, &l2).empty());
    b.titre = "Nana"; CHECK(LivreService::ajouter(*marie, b, &l3).empty());            // 2 livres sans ISBN OK
    b.titre = "Thérèse Raquin"; CHECK(LivreService::ajouter(*marie, b, &l4).empty());
    Livre vide; CHECK(!LivreService::ajouter(*marie, vide).empty());
    CHECK(!LivreService::supprimer(*marie, l4).empty());                               // réservé admin

    // --- Lecteurs ---
    int r1 = 0, r2 = 0;
    Lecteur x; x.nom = "Kossou"; x.prenom = "Ama"; x.matricule = "M001";
    CHECK(LecteurService::ajouter(*marie, x, &r1).empty());
    CHECK(!LecteurService::ajouter(*marie, x).empty());                                // matricule en double
    x.matricule = "M002"; x.nom = "Dossou";
    CHECK(LecteurService::ajouter(*marie, x, &r2).empty());
    auto lec = LecteurService::obtenir(r1);
    CHECK(lec && lec->nom == "Kossou");
    lec->contact = "0100000000";
    CHECK(LecteurService::modifier(*marie, *lec).empty());
    CHECK(LecteurService::obtenir(r1)->contact == "0100000000");

    // --- Emprunts / retours ---
    int e1 = 0, e2 = 0, e3 = 0;
    CHECK(EmpruntService::emprunter(*marie, l1, r1, &e1).empty());
    CHECK(LivreService::obtenir(l1)->dispo == 1);
    CHECK(EmpruntService::emprunter(*marie, l1, r2, &e2).empty());
    CHECK(LivreService::obtenir(l1)->dispo == 0);
    CHECK(!EmpruntService::emprunter(*marie, l1, r2).empty());                         // plus de stock
    CHECK(!EmpruntService::emprunter(*marie, 9999, r1).empty());                       // livre inconnu
    CHECK(!EmpruntService::emprunter(*marie, l2, 9999).empty());                       // lecteur inconnu
    CHECK(!LivreService::supprimer(*admin, l1).empty());                               // historique d'emprunts
    Livre m = *LivreService::obtenir(l1); m.total = 1;
    CHECK(!LivreService::modifier(*marie, m).empty());                                 // 2 empruntés > 1
    m.total = 5; CHECK(LivreService::modifier(*marie, m).empty());
    CHECK(LivreService::obtenir(l1)->dispo == 3);

    // limite de 3 emprunts par lecteur (r1 en a déjà 1)
    CHECK(EmpruntService::emprunter(*marie, l2, r1).empty());
    CHECK(EmpruntService::emprunter(*marie, l3, r1).empty());
    CHECK(!EmpruntService::emprunter(*marie, l4, r1).empty());

    bool retard = true;
    CHECK(EmpruntService::retourner(*marie, e1, &retard).empty() && !retard);
    CHECK(!EmpruntService::retourner(*marie, e1).empty());                             // déjà retourné
    CHECK(LivreService::obtenir(l1)->dispo == 4);

    // --- Retard, alerte, sanction ---
    exec("UPDATE emprunts SET date_limite = DATE_SUB(CURDATE(), INTERVAL 5 DAY) WHERE id = " + std::to_string(e2));
    CHECK(AlerteService::generer(*marie) == 0);                                        // membre : interdit
    CHECK(AlerteService::generer(*admin) == 1);
    CHECK(AlerteService::generer(*admin) == 0);                                        // pas de doublon
    CHECK(!EmpruntService::emprunter(*marie, l4, r2).empty());                         // r2 en retard : bloqué
    int idAlerte = compter("SELECT id FROM alertes_retard LIMIT 1");
    CHECK(!AlerteService::traiter(*marie, idAlerte, "x").empty());                     // membre : interdit
    CHECK(AlerteService::traiter(*admin, idAlerte, "Suspension 7 jours").empty());
    CHECK(!AlerteService::traiter(*admin, idAlerte, "x").empty());                     // déjà traitée
    bool r = false;
    CHECK(EmpruntService::retourner(*marie, e2, &r).empty() && r);                     // retour signalé en retard
    CHECK(EmpruntService::emprunter(*marie, l4, r2, &e3).empty());                     // débloqué

    // --- Suppression autorisée + audit ---
    Livre neuf; neuf.titre = "Temp"; neuf.auteur = "T"; int lt = 0;
    CHECK(LivreService::ajouter(*marie, neuf, &lt).empty());
    CHECK(LivreService::supprimer(*admin, lt).empty());
    CHECK(!LivreService::obtenir(lt).has_value());

    CHECK(compter("SELECT COUNT(*) FROM audit WHERE action='EMPRUNT'") >= 5);
    CHECK(compter("SELECT COUNT(*) FROM audit WHERE action='SANCTION'") == 1);
    CHECK(compter("SELECT COUNT(*) FROM audit WHERE action='ECHEC_CONNEXION'") >= 1);

    // Le journal d'audit est en ajout seul, même pour l'application
    bool refuse = false;
    try { exec("DELETE FROM audit"); } catch (sql::SQLException&) { refuse = true; }
    CHECK(refuse);
    refuse = false;
    try { exec("UPDATE audit SET detail='x'"); } catch (sql::SQLException&) { refuse = true; }
    CHECK(refuse);

    std::cout << (total - echecs) << "/" << total << " vérifications réussies.\n";
    return echecs == 0 ? 0 : 1;
}
