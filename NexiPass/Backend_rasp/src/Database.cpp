#include "Database.h"
#include <libpq-fe.h>
#include <cstring>

Database::Database(std::string connString)
    : connString_(std::move(connString)), conn_(nullptr), employeeViewMode_(true) { 
}

Database::~Database() {
    close();
}

/* Connection Management */

DbResult Database::connect() noexcept {
    try {
        if (conn_ != nullptr) {
            PGconn* pg = static_cast<PGconn*>(conn_);
            // Check if existing connection is still healthy
            if (PQstatus(pg) == CONNECTION_OK) {
                return DbResult::Ok;
            }
            PQfinish(pg);
            conn_ = nullptr;
        }

        PGconn* pg = PQconnectdb(connString_.c_str());
        
        if (PQstatus(pg) != CONNECTION_OK) {
            PQfinish(pg);
            return DbResult::ConnectionError;
        }

        conn_ = static_cast<void*>(pg);
        return DbResult::Ok;
        
    } catch (...) {
        return DbResult::UnknownError;
    }
}

void Database::close() noexcept {
    if (conn_ != nullptr) {
        PGconn* pg = static_cast<PGconn*>(conn_);
        PQfinish(pg);
        conn_ = nullptr;
    }
}

bool Database::isAlive() const noexcept {
    if (conn_ == nullptr) return false;
    PGconn* pg = static_cast<PGconn*>(conn_);
    return PQstatus(pg) == CONNECTION_OK;
}

/* Authentication Logic */

DbResult Database::authenticateUser(const std::string& username, const std::string& password, UserDTO& outUser) noexcept {
    if (!isAlive()) return DbResult::ConnectionError;
    
    PGconn* pg = static_cast<PGconn*>(conn_);
    const char* paramValues[1] = { username.c_str() };
    
    PGresult* res = PQexecParams(pg,
        "SELECT user_id, username, password_hash, role FROM users WHERE username = $1",
        1, nullptr, paramValues, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DbResult::UnknownError;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return DbResult::NotFound;
    }

    std::string storedHash = PQgetvalue(res, 0, 2);
    
    // Verify password match
    if (password != storedHash) {
        PQclear(res);
        return DbResult::ConstraintFailed;
    }

    outUser.user_id = std::stoi(PQgetvalue(res, 0, 0));
    outUser.username = PQgetvalue(res, 0, 1);
    outUser.role = PQgetvalue(res, 0, 3);
    
    PQclear(res);
    return DbResult::Ok;
}

/* Card Operations */

DbResult Database::activateCard(const std::string& card_id, int phone) noexcept {
    if (!isAlive()) return DbResult::ConnectionError;
        
    PGconn* pg = static_cast<PGconn*>(conn_);
    const char* checkParams[1] = { card_id.c_str() };
    
    PGresult* res = PQexecParams(pg,
        "SELECT status FROM opencard WHERE card_id = $1",
        1, nullptr, checkParams, nullptr, nullptr, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DbResult::UnknownError;
    }

    if (PQntuples(res) == 0) {
        PQclear(res);
        return DbResult::NotFound;
    }

    std::string currentStatus = PQgetvalue(res, 0, 0);
    PQclear(res);
    
    // Prevent reactivation if already active
    if (currentStatus == "A") return DbResult::InvalidState;

    std::string phoneStr = std::to_string(phone);
    const char* updateParams[2] = { phoneStr.c_str(), card_id.c_str() };
    
    res = PQexecParams(pg,
        "UPDATE opencard SET status = 'A', phone = $1, total_to_pay = 0 WHERE card_id = $2",
        2, nullptr, updateParams, nullptr, nullptr, 0);
    
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        PQclear(res);
        return DbResult::UnknownError;
    }
    
    PQclear(res);
    return DbResult::Ok;
}

DbResult Database::closeCard(const std::string& card_id) noexcept {
    if (!isAlive()) return DbResult::ConnectionError;

    PGconn* pg = static_cast<PGconn*>(conn_);
    const char* checkParams[1] = { card_id.c_str() };
    
    PGresult* checkRes = PQexecParams(pg,
        "SELECT status FROM opencard WHERE card_id = $1",
        1, nullptr, checkParams, nullptr, nullptr, 0);

    if (PQresultStatus(checkRes) != PGRES_TUPLES_OK) {
        PQclear(checkRes);
        return DbResult::UnknownError;
    }

    if (PQntuples(checkRes) == 0) {
        PQclear(checkRes);
        return DbResult::NotFound;
    }

    std::string currentStatus = PQgetvalue(checkRes, 0, 0);
    PQclear(checkRes);

    if (currentStatus == "D") return DbResult::InvalidState;

    // Begin atomic transaction for checkout
    PGresult* beginRes = PQexec(pg, "BEGIN");
    if (PQresultStatus(beginRes) != PGRES_COMMAND_OK) {
        PQclear(beginRes);
        return DbResult::TxError;
    }
    PQclear(beginRes);

    const char* card_param[1] = { card_id.c_str() };
    
    // Move temporary consumption data to permanent history
    PGresult* aggRes = PQexecParams(pg,
        "INSERT INTO producttotals (product_id, employee_id, qty_total, line_total) "
        "SELECT product_id, employee_id, SUM(qty), SUM(line_total) "
        "FROM openconsumption "
        "WHERE card_id = $1 "
        "GROUP BY product_id, employee_id "
        "ON CONFLICT (product_id, employee_id) "
        "DO UPDATE SET "
        "qty_total = producttotals.qty_total + EXCLUDED.qty_total, "
        "line_total = producttotals.line_total + EXCLUDED.line_total",
        1, nullptr, card_param, nullptr, nullptr, 0);

    if (PQresultStatus(aggRes) != PGRES_COMMAND_OK) {
        PQclear(aggRes);
        PQexec(pg, "ROLLBACK");
        return DbResult::UnknownError;
    }
    PQclear(aggRes);
    
    // Clear temporary data
    PGresult* delRes = PQexecParams(pg,
        "DELETE FROM openconsumption WHERE card_id = $1",
        1, nullptr, card_param, nullptr, nullptr, 0);
    
    if (PQresultStatus(delRes) != PGRES_COMMAND_OK) {
        PQclear(delRes);
        PQexec(pg, "ROLLBACK");
        return DbResult::UnknownError;
    }
    PQclear(delRes);

    // Reset card status
    PGresult* updateRes = PQexecParams(pg,
        "UPDATE opencard SET status = 'D', phone = 0, total_to_pay = 0 WHERE card_id = $1",
        1, nullptr, card_param, nullptr, nullptr, 0);
    
    if (PQresultStatus(updateRes) != PGRES_COMMAND_OK) {
        PQclear(updateRes);
        PQexec(pg, "ROLLBACK");
        return DbResult::UnknownError;
    }
    PQclear(updateRes);

    // Commit transaction
    PGresult* commitRes = PQexec(pg, "COMMIT");
    if (PQresultStatus(commitRes) != PGRES_COMMAND_OK) {
        PQclear(commitRes);
        return DbResult::TxError;
    }
    PQclear(commitRes);

    return DbResult::Ok;
}

DbResult Database::registerConsumption(const std::string& card_id, int32_t productId, int32_t employeeId, int32_t quantidade) noexcept {
    if (!isAlive()) return DbResult::ConnectionError;

    PGconn* pg = static_cast<PGconn*>(conn_);
    const char* checkParams[1] = { card_id.c_str() };
    
    PGresult* checkRes = PQexecParams(pg,
        "SELECT status FROM opencard WHERE card_id = $1",
        1, nullptr, checkParams, nullptr, nullptr, 0);

    if (PQresultStatus(checkRes) != PGRES_TUPLES_OK) {
        PQclear(checkRes);
        return DbResult::UnknownError;
    }

    if (PQntuples(checkRes) == 0) {
        PQclear(checkRes);
        return DbResult::NotFound;
    }

    std::string status = PQgetvalue(checkRes, 0, 0);
    PQclear(checkRes);

    // Only active cards can consume
    if (status != "A") return DbResult::InvalidState;

    std::string prodIdStr = std::to_string(productId);
    std::string empIdStr = std::to_string(employeeId);
    std::string qtyStr = std::to_string(quantidade);

    const char* insertParams[4] = {
        card_id.c_str(),
        prodIdStr.c_str(),
        empIdStr.c_str(),
        qtyStr.c_str()
    };

    PGresult* insertRes = PQexecParams(pg,
        "INSERT INTO openconsumption (card_id, product_id, employee_id, qty, price_unit) "
        "SELECT $1, $2, $3, $4, price_unit "
        "FROM product WHERE product_id = $2",
        4, nullptr, insertParams, nullptr, nullptr, 0);

    if (PQresultStatus(insertRes) != PGRES_COMMAND_OK) {
        PQclear(insertRes);
        return DbResult::UnknownError;
    }
    PQclear(insertRes);

    // Update running total on the card
    PGresult* updateRes = PQexecParams(pg,
        "UPDATE opencard SET total_to_pay = ("
        "  SELECT COALESCE(SUM(line_total), 0) "
        "  FROM openconsumption WHERE card_id = $1"
        ") WHERE card_id = $1",
        1, nullptr, checkParams, nullptr, nullptr, 0);

    if (PQresultStatus(updateRes) != PGRES_COMMAND_OK) {
        PQclear(updateRes);
        return DbResult::UnknownError;
    }
    PQclear(updateRes);
    
    return DbResult::Ok;
}

/* Data Retrieval & Reporting */

DbResult Database::getCardSummary(const std::string& card_id, CardSummaryDTO& out) noexcept {
    if (!isAlive()) return DbResult::ConnectionError;
    
    PGconn* pg = static_cast<PGconn*>(conn_);
    const char* cardParams[1] = { card_id.c_str() };
    
    bool useEmployeeView;
    {
        // Thread-safe access to configuration
        std::lock_guard<std::mutex> lock(modeMutex_);
        useEmployeeView = employeeViewMode_;
    }
    
    PGresult* cardRes = PQexecParams(pg,
        "SELECT card_id, phone, status, total_to_pay FROM opencard WHERE card_id = $1",
        1, nullptr, cardParams, nullptr, nullptr, 0);

    if (PQresultStatus(cardRes) != PGRES_TUPLES_OK) {
        PQclear(cardRes);
        return DbResult::UnknownError;
    }

    if (PQntuples(cardRes) == 0) {
        PQclear(cardRes);
        return DbResult::NotFound;
    }

    if (PQnfields(cardRes) < 4) {
        PQclear(cardRes);
        return DbResult::UnknownError;
    }

    out.card_id = PQgetvalue(cardRes, 0, 0);
    
    const char* phoneVal = PQgetvalue(cardRes, 0, 1);
    // Hide phone number based on privacy setting
    out.phone = (phoneVal && strlen(phoneVal) > 0) ? (useEmployeeView ? 0 : std::stoi(phoneVal)) : 0;
    
    const char* statusVal = PQgetvalue(cardRes, 0, 2);
    out.status = (statusVal && strlen(statusVal) > 0) ? statusVal[0] : '?';
    
    const char* totalVal = PQgetvalue(cardRes, 0, 3);
    out.total_to_pay = (totalVal && strlen(totalVal) > 0) ? std::stod(totalVal) : 0.0;
    
    PQclear(cardRes);

    PGresult* consRes = PQexecParams(pg,
        "SELECT c.id_consumption, c.card_id, c.product_id, c.employee_id, c.qty, "
        "c.price_unit, c.line_total, p.product_name "
        "FROM openconsumption c "
        "JOIN product p ON p.product_id = c.product_id "
        "WHERE c.card_id = $1",
        1, nullptr, cardParams, nullptr, nullptr, 0);

    if (PQresultStatus(consRes) != PGRES_TUPLES_OK) {
        PQclear(consRes);
        return DbResult::UnknownError;
    }

    int nRows = PQntuples(consRes);
    out.lines.clear();
    out.lines.reserve(nRows);

    for (int i = 0; i < nRows; ++i) {
        ConsumptionLineDTO line;
        
        const char* idVal = PQgetvalue(consRes, i, 0);
        line.id_consumption = (idVal && strlen(idVal) > 0) ? std::stoull(idVal) : 0;
        
        line.card_id = PQgetvalue(consRes, i, 1);
        line.product_id = std::stoi(PQgetvalue(consRes, i, 2));
        line.employee_id = std::stoi(PQgetvalue(consRes, i, 3));
        line.qty = std::stoi(PQgetvalue(consRes, i, 4));
        line.price_unit = std::stod(PQgetvalue(consRes, i, 5));
        line.line_total = std::stod(PQgetvalue(consRes, i, 6));
        line.product_name = PQgetvalue(consRes, i, 7);

        out.lines.push_back(std::move(line));
    }

    PQclear(consRes);
    return DbResult::Ok;
}

DbResult Database::getTotals(std::vector<TotalsRowDTO>& out) noexcept {
    if (!isAlive()) return DbResult::ConnectionError;

    PGconn* pg = static_cast<PGconn*>(conn_);

    PGresult* res = PQexec(pg,
        "SELECT t.product_id, p.product_name, t.employee_id, u.username, "
        "t.qty_total, t.line_total "
        "FROM producttotals t "
        "JOIN product p ON p.product_id = t.product_id "
        "JOIN users u ON u.user_id = t.employee_id "
        "ORDER BY t.product_id, t.employee_id");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DbResult::UnknownError;
    }

    int nRows = PQntuples(res);
    out.clear();
    out.reserve(nRows);

    for (int i = 0; i < nRows; ++i) {
        TotalsRowDTO row;
        row.product_id = std::stoi(PQgetvalue(res, i, 0));
        row.product_name = PQgetvalue(res, i, 1);
        row.employee_id = std::stoi(PQgetvalue(res, i, 2));
        row.employee_username = PQgetvalue(res, i, 3);
        row.qty_total = std::stoull(PQgetvalue(res, i, 4));
        row.line_total = std::stod(PQgetvalue(res, i, 5));

        out.push_back(std::move(row));
    }

    PQclear(res);
    return DbResult::Ok;
}

DbResult Database::listProducts(std::vector<ProductDTO>& out) noexcept {
    if (!isAlive()) return DbResult::ConnectionError;
    
    PGconn* pg = static_cast<PGconn*>(conn_);
    
    PGresult* res = PQexec(pg,
        "SELECT product_id, product_name, price_unit FROM product ORDER BY product_name");

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return DbResult::UnknownError;
    }
    
    int nRows = PQntuples(res);
    out.clear();
    out.reserve(nRows);
    
    for (int i = 0; i < nRows; ++i) {
        ProductDTO prod;
        prod.product_id = std::stoi(PQgetvalue(res, i, 0));
        prod.name = PQgetvalue(res, i, 1);
        prod.price_unit = std::stod(PQgetvalue(res, i, 2));
        out.push_back(std::move(prod));
    }

    PQclear(res);
    return DbResult::Ok;
}

/* System Configuration */

void Database::setEmployeeViewMode(bool enabled) noexcept {
    std::lock_guard<std::mutex> lock(modeMutex_);
    employeeViewMode_ = enabled;
}
