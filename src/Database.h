#pragma once
#include <memory>
#include <string>
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>
#include <cppconn/datatype.h>

using Stmt = std::unique_ptr<sql::PreparedStatement>;
using Rows = std::unique_ptr<sql::ResultSet>;

class Database {
public:
    static Database& instance() { static Database db; return db; }

    void connect(const std::string& host, const std::string& user,
                 const std::string& pass, const std::string& schema) {
        auto* driver = sql::mysql::get_mysql_driver_instance();
        conn_.reset(driver->connect(host, user, pass));
        conn_->setSchema(schema);
    }
    sql::Connection* get() { return conn_.get(); }
    Stmt prep(const std::string& requete) { return Stmt(conn_->prepareStatement(requete)); }

    int dernierId() {
        std::unique_ptr<sql::Statement> st(conn_->createStatement());
        Rows rs(st->executeQuery("SELECT LAST_INSERT_ID()"));
        rs->next();
        return rs->getInt(1);
    }

private:
    std::unique_ptr<sql::Connection> conn_;
};

// Transaction RAII : annulée automatiquement si commit() n'est pas appelé (retour anticipé, exception).
class Transaction {
public:
    Transaction() : c_(Database::instance().get()) { c_->setAutoCommit(false); }
    void commit() { c_->commit(); c_->setAutoCommit(true); fini_ = true; }
    ~Transaction() {
        if (!fini_) { try { c_->rollback(); c_->setAutoCommit(true); } catch (...) {} }
    }
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
private:
    sql::Connection* c_;
    bool fini_ = false;
};

inline bool estDoublon(const sql::SQLException& e) { return e.getErrorCode() == 1062; }
