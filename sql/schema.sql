-- Schéma de la base "bibliotheque" (MySQL 8.0.16+)
-- Exécution : mysql -u root -p < sql/schema.sql
-- IMPORTANT : remplacer CHANGE_MOI par un vrai mot de passe avant exécution.

CREATE DATABASE IF NOT EXISTS bibliotheque CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE bibliotheque;

CREATE TABLE IF NOT EXISTS utilisateurs (
    id                INT AUTO_INCREMENT PRIMARY KEY,
    login             VARCHAR(50)  NOT NULL UNIQUE,
    mot_de_passe_hash VARCHAR(255) NOT NULL,
    role              ENUM('ADMIN','MEMBRE') NOT NULL DEFAULT 'MEMBRE',
    actif             TINYINT(1)   NOT NULL DEFAULT 1,
    date_creation     TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS lecteurs (
    id               INT AUTO_INCREMENT PRIMARY KEY,
    nom              VARCHAR(100) NOT NULL,
    prenom           VARCHAR(100) NOT NULL,
    matricule        VARCHAR(50)  NOT NULL UNIQUE,
    contact          VARCHAR(100) NULL,
    date_inscription TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS livres (
    id                INT AUTO_INCREMENT PRIMARY KEY,
    titre             VARCHAR(255) NOT NULL,
    auteur            VARCHAR(150) NOT NULL,
    isbn              VARCHAR(20)  NULL UNIQUE,
    exemplaires_total INT NOT NULL,
    exemplaires_dispo INT NOT NULL,
    CONSTRAINT ck_livres_total CHECK (exemplaires_total >= 1),
    CONSTRAINT ck_livres_dispo CHECK (exemplaires_dispo >= 0 AND exemplaires_dispo <= exemplaires_total)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS emprunts (
    id             INT AUTO_INCREMENT PRIMARY KEY,
    id_livre       INT NOT NULL,
    id_lecteur     INT NOT NULL,
    id_utilisateur INT NOT NULL,
    date_emprunt   DATE NOT NULL,
    date_limite    DATE NOT NULL,
    date_retour    DATE NULL,
    statut         ENUM('EN_COURS','RETOURNE') NOT NULL DEFAULT 'EN_COURS',
    FOREIGN KEY (id_livre)       REFERENCES livres(id),
    FOREIGN KEY (id_lecteur)     REFERENCES lecteurs(id),
    FOREIGN KEY (id_utilisateur) REFERENCES utilisateurs(id),
    INDEX idx_emprunts_lecteur (id_lecteur, date_retour),
    INDEX idx_emprunts_retard (date_retour, date_limite)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS alertes_retard (
    id              INT AUTO_INCREMENT PRIMARY KEY,
    id_emprunt      INT NOT NULL UNIQUE,
    date_alerte     DATE NOT NULL,
    traitee         TINYINT(1) NOT NULL DEFAULT 0,
    sanction        VARCHAR(255) NULL,
    id_admin        INT NULL,
    date_traitement DATETIME NULL,
    FOREIGN KEY (id_emprunt) REFERENCES emprunts(id),
    FOREIGN KEY (id_admin)   REFERENCES utilisateurs(id)
) ENGINE=InnoDB;

CREATE TABLE IF NOT EXISTS audit (
    id             BIGINT AUTO_INCREMENT PRIMARY KEY,
    id_utilisateur INT NOT NULL,
    action         VARCHAR(50)  NOT NULL,
    table_cible    VARCHAR(50)  NOT NULL,
    detail         VARCHAR(500) NOT NULL,
    date_heure     TIMESTAMP    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (id_utilisateur) REFERENCES utilisateurs(id),
    INDEX idx_audit_date (date_heure)
) ENGINE=InnoDB;

-- Compte applicatif à privilèges minimaux.
-- L'audit est en ajout seul : même l'application ne peut ni modifier ni effacer le journal.
CREATE USER IF NOT EXISTS 'biblio_user'@'localhost' IDENTIFIED BY 'CHANGE_MOI';
GRANT SELECT, INSERT, UPDATE         ON bibliotheque.utilisateurs    TO 'biblio_user'@'localhost';
GRANT SELECT, INSERT, UPDATE         ON bibliotheque.lecteurs        TO 'biblio_user'@'localhost';
GRANT SELECT, INSERT, UPDATE, DELETE ON bibliotheque.livres          TO 'biblio_user'@'localhost';
GRANT SELECT, INSERT, UPDATE         ON bibliotheque.emprunts        TO 'biblio_user'@'localhost';
GRANT SELECT, INSERT, UPDATE         ON bibliotheque.alertes_retard  TO 'biblio_user'@'localhost';
GRANT SELECT, INSERT                 ON bibliotheque.audit           TO 'biblio_user'@'localhost';
FLUSH PRIVILEGES;
