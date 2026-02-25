/* ==================== FeedbackController.cpp ==================== */

#include "FeedbackController.h"
#include <iostream>
#include <fstream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <poll.h>

using namespace std;

static bool pathExists(const std::string& path) {
    return access(path.c_str(), F_OK) == 0;
}

/* ==================== Sysfs Helpers ==================== */

void FeedbackController::writeSys(const string& path, const string& value) {
    ofstream fs(path);
    if (fs.is_open()) {
        fs << value;
        fs.close();
    }
}

void FeedbackController::writeLed(const string& color) {
    int fd = open("/dev/ledrgb", O_WRONLY);
    if (fd >= 0) {
        write(fd, color.c_str(), 3);
        close(fd);
    }
}

/* ==================== Non-Blocking LED Auto-Off ==================== */

void FeedbackController::scheduleLedOff(int delayMs) {
    if (ledTimerFd_ < 0) return;

    itimerspec its{};
    its.it_value.tv_sec = delayMs / 1000;
    its.it_value.tv_nsec = (delayMs % 1000) * 1000000L;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;
    
    if (timerfd_settime(ledTimerFd_, 0, &its, nullptr) == 0) {
        ledTimerArmed_.store(true, std::memory_order_release);
    }
}

void FeedbackController::ledTimerWorker() {
    pollfd fds[2]{};
    fds[0].fd = ledTimerFd_;
    fds[0].events = POLLIN;
    fds[1].fd = stopFd_;
    fds[1].events = POLLIN;

    while (running_) {
        int r = poll(fds, 2, 1000);
        
        if (!running_) break;
        
        if (r > 0 && (fds[1].revents & POLLIN)) {
            uint64_t v;
            (void)read(stopFd_, &v, sizeof(v));
            break;
        }
        
        if (r > 0 && (fds[0].revents & POLLIN)) {
            uint64_t exp;
            (void)read(ledTimerFd_, &exp, sizeof(exp));
            
            if (ledTimerArmed_.load(std::memory_order_acquire)) {
                writeLed("000");
                ledTimerArmed_.store(false, std::memory_order_release);
            }
        }
    }
}

/* ==================== Deterministic Wait ==================== */

bool FeedbackController::waitMs(int ms) {
    if (ms <= 0) return true;
    if (timerFd_ < 0) return false;

    itimerspec its{};
    its.it_value.tv_sec = ms / 1000;
    its.it_value.tv_nsec = (ms % 1000) * 1000000L;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if (timerfd_settime(timerFd_, 0, &its, nullptr) != 0) return false;

    pollfd fds[2]{};
    fds[0].fd = timerFd_;
    fds[0].events = POLLIN;
    fds[1].fd = stopFd_;
    fds[1].events = POLLIN;

    while (running_) {
        int r = poll(fds, 2, -1);
        if (r <= 0) continue;

        if (fds[1].revents & POLLIN) {
            uint64_t v;
            (void)read(stopFd_, &v, sizeof(v));
            return false;
        }

        if (fds[0].revents & POLLIN) {
            uint64_t exp;
            (void)read(timerFd_, &exp, sizeof(exp));
            return true;
        }
    }

    return false;
}

/* ==================== PWM Buzzer Control ==================== */

void FeedbackController::initPwmIfNeeded() {
    if (!pathExists("/sys/class/pwm/pwmchip0/pwm1")) {
        writeSys("/sys/class/pwm/pwmchip0/export", "1");

        for (int t = 0; t < 200 && running_; t += 5) {
            if (pathExists("/sys/class/pwm/pwmchip0/pwm1")) break;
            if (!waitMs(5)) break;
        }
    }

    writeSys("/sys/class/pwm/pwmchip0/pwm1/enable", "0");
    writeSys("/sys/class/pwm/pwmchip0/pwm1/period", "500000");
    writeSys("/sys/class/pwm/pwmchip0/pwm1/duty_cycle", "0");
}

void FeedbackController::beepOnceMs(int ms) {
    writeSys("/sys/class/pwm/pwmchip0/pwm1/period", "500000");
    writeSys("/sys/class/pwm/pwmchip0/pwm1/duty_cycle", "250000");
    writeSys("/sys/class/pwm/pwmchip0/pwm1/enable", "1");

    (void)waitMs(ms);

    writeSys("/sys/class/pwm/pwmchip0/pwm1/enable", "0");
    writeSys("/sys/class/pwm/pwmchip0/pwm1/duty_cycle", "0");
}

/* ==================== Lifecycle ==================== */

FeedbackController::FeedbackController() : running_(true), ledTimerArmed_(false) {
    timerFd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    ledTimerFd_ = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    stopFd_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

    if (timerFd_ < 0 || ledTimerFd_ < 0 || stopFd_ < 0) {
        cerr << "[ERROR] Failed to create timerfd/eventfd\n";
    }

    initPwmIfNeeded();
    writeLed("000");

    ledTimerThread_ = thread(&FeedbackController::ledTimerWorker, this);
    feedbackThread_ = thread(&FeedbackController::feedbackWorker, this);
}

FeedbackController::~FeedbackController() {
    running_ = false;

    queueCv_.notify_all();
    if (stopFd_ >= 0) {
        uint64_t one = 1;
        (void)write(stopFd_, &one, sizeof(one));
    }

    if (feedbackThread_.joinable()) feedbackThread_.join();
    if (ledTimerThread_.joinable()) ledTimerThread_.join();

    writeSys("/sys/class/pwm/pwmchip0/pwm1/enable", "0");
    writeSys("/sys/class/pwm/pwmchip0/pwm1/duty_cycle", "0");
    writeLed("000");

    if (timerFd_ >= 0) close(timerFd_);
    if (ledTimerFd_ >= 0) close(ledTimerFd_);
    if (stopFd_ >= 0) close(stopFd_);
}

/* ==================== Worker Thread ==================== */

void FeedbackController::feedbackWorker() {
    while (running_) {
        FeedbackType type;

        {
            unique_lock<mutex> lock(queueMtx_);
            queueCv_.wait(lock, [this] {
                return !feedbackQueue_.empty() || !running_;
            });

            if (!running_ && feedbackQueue_.empty()) break;

            type = feedbackQueue_.front();
            feedbackQueue_.pop();
        }

        executeFeedback(type);
    }
}

/* ==================== Feedback Execution ==================== */

void FeedbackController::executeFeedback(FeedbackType type) {
    switch (type) {
        case FeedbackType::ACTIVATE:
            writeLed("010");
            scheduleLedOff(800);
            beepOnceMs(120);
            break;

        case FeedbackType::DEACTIVATE:
            writeLed("001");
            scheduleLedOff(800);
            beepOnceMs(120);
            break;

        case FeedbackType::CHECKOUT:
            writeLed("001");
            scheduleLedOff(800);
            beepOnceMs(100);
            (void)waitMs(100);
            beepOnceMs(100);
            break;

        case FeedbackType::ERROR:
            writeLed("100");
            scheduleLedOff(800);
            beepOnceMs(90);
            (void)waitMs(90);
            beepOnceMs(90);
            (void)waitMs(90);
            beepOnceMs(90);
            break;
    }
}

/* ==================== Public API ==================== */

void FeedbackController::activateFB() {
    lock_guard<mutex> lock(queueMtx_);
    feedbackQueue_.push(FeedbackType::ACTIVATE);
    queueCv_.notify_one();
}

void FeedbackController::deactivateFB() {
    lock_guard<mutex> lock(queueMtx_);
    feedbackQueue_.push(FeedbackType::DEACTIVATE);
    queueCv_.notify_one();
}

void FeedbackController::errorFB() {
    lock_guard<mutex> lock(queueMtx_);
    feedbackQueue_.push(FeedbackType::ERROR);
    queueCv_.notify_one();
}

void FeedbackController::checkoutFB() {
    lock_guard<mutex> lock(queueMtx_);
    feedbackQueue_.push(FeedbackType::CHECKOUT);
    queueCv_.notify_one();
}
