/* ==================== CardService.cpp ==================== */

#include "CardService.h"

using namespace std;

CardService::CardService(Database* database, FeedbackController* feedback) 
    : db_(database), feedback_(feedback) {
}

CardService::~CardService() {
}

/* ==================== Card Activation ==================== */

DbResult CardService::activateCard(const string& nfc_uid, int phone) {
    if (phone <= 0) {
        feedback_->errorFB();
        return DbResult::ConstraintFailed;
    }

    DbResult result = db_->activateCard(nfc_uid, phone);
    
    if (result == DbResult::Ok) {
        feedback_->activateFB();
    } else {
        feedback_->errorFB();
    }
    
    return result;
}

/* ==================== Add Consumption ==================== */

DbResult CardService::addConsumption(const string& nfc_uid, int32_t productId, int32_t employeeId, int32_t quantity) {
    if (quantity <= 0 || productId <= 0 || employeeId <= 0) {
        feedback_->errorFB();
        return DbResult::ConstraintFailed;
    }

    DbResult result = db_->registerConsumption(nfc_uid, productId, employeeId, quantity);
    
    if (result == DbResult::Ok) {
        feedback_->activateFB();
    } else {
        feedback_->errorFB();
    }
    
    return result;
}

/* ==================== Card Deactivation (Checkout) ==================== */

DbResult CardService::deactivateCard(const string& nfc_uid) {
    DbResult result = db_->closeCard(nfc_uid);
    
    if (result == DbResult::Ok) {
        feedback_->checkoutFB();
    } else {
        feedback_->errorFB();
    }
    
    return result;
}

/* ==================== Queries ==================== */

DbResult CardService::getCardSummary(const string& nfc_uid, CardSummaryDTO& out_summary) {
    return db_->getCardSummary(nfc_uid, out_summary);
}

DbResult CardService::getProductList(vector<ProductDTO>& out_products) {
    return db_->listProducts(out_products);
}