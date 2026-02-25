#ifndef APICONTROLLER_H
#define APICONTROLLER_H

#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <pthread.h>

#include "httplib.h" 
#include "Database.h"
#include "CardService.h"
#include "FeedbackController.h"

using namespace std;

struct CachedCardState {
    string card_id;
    bool is_valid_in_db = false;
    char status = '?';
    double total_pay = 0.0;
    chrono::steady_clock::time_point scan_time;
};

class ApiController {
private:
    Database* db_;
    CardService* cardService_;
    FeedbackController* feedback_;
    
    httplib::Server server_;
    
    thread thNFC_;
    thread thNetwork_;
    thread thWorker_;
    atomic<bool> running_;
    
    queue<string> workQueue_;
    mutex queueMtx_;
    condition_variable queueCv_;

    mutex stateMtx_;
    CachedCardState latestCardState_;

    void nfcThreadFunction();
    void workerThreadFunction();
    void networkThreadFunction();
    void setThreadPriority(pthread_t handle, int priority);

public:
    ApiController(Database* db, CardService* cs, FeedbackController* fb);
    ~ApiController();
    
    void start();
    void stop();
};

#endif
