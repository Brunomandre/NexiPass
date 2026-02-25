#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <cstdint>
#include <mutex>
#include <libpq-fe.h>

using namespace std;

/* ==================== DTOs ==================== */

struct UserDTO {
    uint32_t user_id;
    string username;
    string role;
};

struct ProductDTO {
    int product_id = 0;
    string name;
    double price_unit = 0.0;
};

struct ConsumptionLineDTO {
    uint64_t id_consumption = 0;
    string card_id;
    int product_id = 0;
    int employee_id = 0;
    int qty = 0;
    double price_unit = 0.0;
    double line_total = 0.0;
    string product_name;
};

struct CardSummaryDTO {
    string card_id;
    int phone = 0;
    char status = 'D';
    double total_to_pay = 0.0;
    vector<ConsumptionLineDTO> lines;
};

struct TotalsRowDTO {
    int product_id = 0;
    string product_name;
    int employee_id = 0;
    string employee_username;
    uint64_t qty_total = 0;
    double line_total = 0.0;
};

/* ==================== Result Enum ==================== */

enum class DbResult {
    Ok = 0,
    NotFound,
    InvalidState,
    ConstraintFailed,
    ConnectionError,
    TxError,
    UnknownError
};

/* ==================== Database Class ==================== */

class Database {
private:
    string connString_;
    void* conn_;
    bool employeeViewMode_;
    mutable std::mutex modeMutex_;

public:
    explicit Database(string connString);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) noexcept = default;
    Database& operator=(Database&&) noexcept = default;

    /* Connection Management */
    DbResult connect() noexcept;
    void close() noexcept;
    bool isAlive() const noexcept;

    /* Core Operations */
    DbResult authenticateUser(const string& username, const string& password, UserDTO& outUser) noexcept;
    DbResult activateCard(const string& card_id, int phone) noexcept;
    DbResult closeCard(const string& card_id) noexcept;
    DbResult registerConsumption(const string& card_id, int32_t productId, int32_t employeeId, int32_t quantidade) noexcept;
    
    /* Queries */
    DbResult getCardSummary(const string& card_id, CardSummaryDTO& out) noexcept;
    DbResult getTotals(vector<TotalsRowDTO>& out) noexcept;
    DbResult listProducts(vector<ProductDTO>& out) noexcept;
    
    /* Configuration */
    void setEmployeeViewMode(bool enabled) noexcept;
};

#endif
