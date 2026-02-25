/* ==================== CardService.h ==================== */

#ifndef CARDSERVICE_H
#define CARDSERVICE_H

#include "Database.h"
#include "FeedbackController.h"
#include <string>

class CardService {
private:
    Database* db_;
    FeedbackController* feedback_;

public:
    CardService(Database* database, FeedbackController* feedback);
    ~CardService();
    
    DbResult activateCard(const std::string& nfc_uid, int phone);
    DbResult addConsumption(const std::string& nfc_uid, int32_t productId, int32_t employeeId, int32_t quantity);
    DbResult deactivateCard(const std::string& nfc_uid);
    DbResult getCardSummary(const std::string& nfc_uid, CardSummaryDTO& out_summary);
    DbResult getProductList(std::vector<ProductDTO>& out_products);
};

#endif
