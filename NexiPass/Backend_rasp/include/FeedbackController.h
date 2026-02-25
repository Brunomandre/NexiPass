/* ==================== FeedbackController.h ==================== */

#ifndef FEEDBACKCONTROLLER_H
#define FEEDBACKCONTROLLER_H

#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string>

enum class FeedbackType {
    ACTIVATE,
    DEACTIVATE,
    ERROR,
    CHECKOUT
};

class FeedbackController {
private:
    std::thread feedbackThread_;
    std::thread ledTimerThread_;
    std::atomic<bool> running_;

    std::queue<FeedbackType> feedbackQueue_;
    std::mutex queueMtx_;
    std::condition_variable queueCv_;

    int timerFd_{-1};
    int ledTimerFd_{-1};
    int stopFd_{-1};
    
    std::atomic<bool> ledTimerArmed_{false};

    void feedbackWorker();
    void ledTimerWorker();
    void executeFeedback(FeedbackType type);

    void writeSys(const std::string& path, const std::string& value);
    void writeLed(const std::string& color);
    void scheduleLedOff(int delayMs);

    void initPwmIfNeeded();
    void beepOnceMs(int ms);
    bool waitMs(int ms);

public:
    FeedbackController();
    ~FeedbackController();

    void activateFB();
    void deactivateFB();
    void errorFB();
    void checkoutFB();
};

#endif
