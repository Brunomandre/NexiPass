/* ==================== ApiController.cpp ==================== */

#include "ApiController.h"
#include "SimpleRFID.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sched.h>

using json = nlohmann::json;
using namespace std;

/* ==================== Lifecycle ==================== */

ApiController::ApiController(Database* db, CardService* cs, FeedbackController* fb)
    : db_(db), cardService_(cs), feedback_(fb), running_(false) {
}

ApiController::~ApiController() {
    stop();
}

/* ==================== Set Thread Priority ==================== */

void ApiController::setThreadPriority(pthread_t handle, int priority) {
    if (geteuid() != 0) {
        cerr << "[WARN] Not running as root. Cannot set real-time priorities.\n";
        return;
    }
    
    sched_param param;
    param.sched_priority = priority;
    
    int result = pthread_setschedparam(handle, SCHED_FIFO, &param);
    
    if (result != 0) {
        cerr << "[ERROR] Failed to set thread priority: " << strerror(result) << "\n";
        cerr << "[INFO] Falling back to CFS scheduling\n";
    }
}

/* ==================== Start with Priorities ==================== */

void ApiController::start() {
    if (running_) return;
    running_ = true;

    cout << "[System] Starting threads (NFC, Worker, Network)...\n";

    thNFC_ = thread(&ApiController::nfcThreadFunction, this);
    thWorker_ = thread(&ApiController::workerThreadFunction, this);
    thNetwork_ = thread(&ApiController::networkThreadFunction, this);
    
    // SCHED_FIFO priorities: 1 (lowest) to 99 (highest)
    setThreadPriority(thNFC_.native_handle(), 80);      // High priority
    setThreadPriority(thNetwork_.native_handle(), 50);  // Medium priority  
    setThreadPriority(thWorker_.native_handle(), 30);   // Low priority
    
    cout << "[System] Threads started with RT priorities (if root)\n";
}

void ApiController::stop() {
    if (!running_) return;
    running_ = false;
    
    cout << "[API] Stopping services...\n";

    queueCv_.notify_all();
    server_.stop(); 

    if (thNFC_.joinable()) thNFC_.join();
    if (thWorker_.joinable()) thWorker_.join();
    if (thNetwork_.joinable()) thNetwork_.join();
    
    cout << "[API] All threads stopped.\n";
}

/* ==================== NFC Thread (High Priority Loop) ==================== */

void ApiController::nfcThreadFunction() {
    SimpleRFID rfid;
    if (!rfid.isReady()) {
        cerr << "[NFC] Hardware not ready\n";
        return;
    }

    string lastRawUid = "";
    
    while (running_) {
        if (rfid.isCardPresent()) {
            string uid;
            if (rfid.readCardUID(uid)) {
                if (uid != lastRawUid) {
                    {
                        lock_guard<mutex> lock(queueMtx_);
                        workQueue_.push(uid);
                    }
                    queueCv_.notify_one();
                    lastRawUid = uid;
                }
            }
        } else {
            lastRawUid = "";
        }
        
        this_thread::yield(); 
    }
}

/* ==================== Worker Thread (Business Logic) ==================== */

void ApiController::workerThreadFunction() {
    while (running_) {
        string uidToProcess;

        {
            unique_lock<mutex> lock(queueMtx_);
            queueCv_.wait(lock, [this] { 
                return !workQueue_.empty() || !running_; 
            });

            if (!running_ && workQueue_.empty()) break;

            uidToProcess = workQueue_.front();
            workQueue_.pop();
        }

        // Query card data from DB
        CardSummaryDTO summary;
        db_->setEmployeeViewMode(false); 
        DbResult res = db_->getCardSummary(uidToProcess, summary);

        CachedCardState newState;
        newState.card_id = uidToProcess;
        newState.scan_time = chrono::steady_clock::now();

        if (res == DbResult::Ok) {
            newState.is_valid_in_db = true;
            newState.status = summary.status;
            newState.total_pay = summary.total_to_pay;
        } else {
            newState.is_valid_in_db = false;
        }

        {
            lock_guard<mutex> lock(stateMtx_);
            latestCardState_ = newState;
        }
        
        cout << "[Worker] Processed: " << uidToProcess 
             << " (Valid: " << newState.is_valid_in_db << ")\n";
    }
}

/* ==================== Network Thread (REST API) ==================== */

void ApiController::networkThreadFunction() {
    server_.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "POST, GET, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"}
    });
    server_.Options(".*", [](const auto&, auto& res) { res.status = 204; });

    /* ==================== Login ==================== */
    
    server_.Post("/login", [this](const auto& req, auto& res) {
        try {
            auto body = json::parse(req.body);
            UserDTO user;
            if (db_->authenticateUser(body["username"], body["password"], user) == DbResult::Ok) {
                json j; 
                j["ok"] = true; 
                j["role"] = user.role;
                j["user_id"] = user.user_id;
                db_->setEmployeeViewMode(user.role != "OWNER");
                res.set_content(j.dump(), "application/json");
            } else {
                res.set_content("{\"ok\":false}", "application/json");
            }
        } catch(...) { 
            res.status = 400; 
        }
    });

    /* ==================== Wait for Card (Long Polling) ==================== */
    
    server_.Get("/wait_card", [this](const auto&, auto& res) {
        json j; 
        j["card_id"] = nullptr;
        
        lock_guard<mutex> lock(stateMtx_);
        auto now = chrono::steady_clock::now();
        
        // Card is fresh if scanned within last 3 seconds
        if (chrono::duration_cast<chrono::seconds>(now - latestCardState_.scan_time).count() < 3 
            && !latestCardState_.card_id.empty()) {
            
            j["card_id"] = latestCardState_.card_id;
            j["status"] = string(1, latestCardState_.status);
            j["valid"] = latestCardState_.is_valid_in_db;
        }
        res.set_content(j.dump(), "application/json");
    });

    /* ==================== Activate Card ==================== */
    
    server_.Post("/activate_card", [this](const auto& req, auto& res) {
        try {
            auto body = json::parse(req.body);
            DbResult r = cardService_->activateCard(body["card_id"], stoi(string(body["phone"])));
            json j; 
            j["ok"] = (r == DbResult::Ok);
            res.set_content(j.dump(), "application/json");
        } catch(...) { 
            res.status = 400; 
        }
    });

    /* ==================== Add Consumption ==================== */
    
    server_.Post("/add_consumption", [this](const auto& req, auto& res) {
        try {
            auto body = json::parse(req.body);
            string card_id = body["card_id"];
            int product_id = body["product_id"];
            int quantity = body["quantity"];
            int employee_id = body["employee_id"];
            
            DbResult r = cardService_->addConsumption(
                card_id, product_id, employee_id, quantity
            );
            
            json j; 
            j["ok"] = (r == DbResult::Ok);
            res.set_content(j.dump(), "application/json");
            
        } catch(...) {
            res.status = 400;
        }
    });

    /* ==================== Validate Exit ==================== */
    
    server_.Post("/validate_exit", [this](const auto& req, auto& res) {
        try {
            auto body = json::parse(req.body);
            CardSummaryDTO sum;
            db_->setEmployeeViewMode(false);
            DbResult r = db_->getCardSummary(body["card_id"], sum);
            
            json j; 
            j["closed"] = (r == DbResult::Ok && sum.status == 'D');
            res.set_content(j.dump(), "application/json");
        } catch(...) { 
            res.status = 400; 
        }
    });

    /* ==================== Get Products ==================== */
    
    server_.Get("/products", [this](const auto&, auto& res) {
        vector<ProductDTO> p; 
        cardService_->getProductList(p);
        json j = json::array();
        for(auto& i : p) {
            j.push_back({
                {"product_id", i.product_id}, 
                {"name", i.name}, 
                {"price", i.price_unit}
            });
        }
        res.set_content(j.dump(), "application/json");
    });

    /* ==================== Card Summary ==================== */
    
    server_.Get("/card_summary", [this](const auto& req, auto& res) {
        if (!req.has_param("card_id")) { 
            res.status = 400; 
            return; 
        }
        
        CardSummaryDTO sum;
        if (cardService_->getCardSummary(req.get_param_value("card_id"), sum) == DbResult::Ok) {
            json j;
            j["card_id"] = sum.card_id;
            j["phone"] = to_string(sum.phone);
            j["total"] = sum.total_to_pay;
            
            json lines = json::array();
            for(auto& l : sum.lines) {
                lines.push_back({
                   {"product_name", l.product_name},
                   {"quantity", l.qty},
                   {"unit_price", l.price_unit},
                   {"total", l.line_total}
                });
            }
            j["lines"] = lines;
            res.set_content(j.dump(), "application/json");
        } else {
            res.status = 404;
        }
    });
    
    /* ==================== Close Card (Checkout) ==================== */
    
    server_.Post("/close_card", [this](const auto& req, auto& res) {
        try {
            auto body = json::parse(req.body);
            string card_id = body["card_id"];

            DbResult r = cardService_->deactivateCard(card_id);

            json j;
            j["ok"] = (r == DbResult::Ok);
            res.set_content(j.dump(), "application/json");

        } catch(...) {
            res.status = 400;
        }
    });

    /* ==================== Product Totals (Owner Only) ==================== */

    server_.Get("/product_totals", [this](const auto& req, auto& res) {
        try {
            vector<TotalsRowDTO> totals;
            DbResult r = db_->getTotals(totals);

            if (r == DbResult::Ok) {
                json j = json::array();
                for (auto& t : totals) {
                    j.push_back({
                        {"product_name", t.product_name},
                        {"employee_name", t.employee_username},
                        {"total_quantity", t.qty_total},
                        {"total_revenue", t.line_total}
                    });
                }
                res.set_content(j.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content("{\"error\":\"Failed to retrieve data\"}", "application/json");
            }
        } catch(...) {
            res.status = 500;
        }
    });

    cout << "[HTTP] Server listening on port 5000\n";

    if (!server_.listen("0.0.0.0", 5000)) {
        cerr << "[HTTP] Critical error: Port 5000 unavailable?\n";
    }
}
