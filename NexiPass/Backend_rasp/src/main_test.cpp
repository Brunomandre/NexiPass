/* ==================== main_test.cpp ==================== */

#include <iostream>
#include <string>
#include <csignal>
#include <unistd.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <pthread.h>

#include "Database.h"
#include "FeedbackController.h"
#include "CardService.h"
#include "ApiController.h"

/* ==================== Signal Handling (POSIX) ==================== */

static int make_signalfd_for_shutdown() {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);

    // Block signals in all threads (prevents default handlers)
    if (pthread_sigmask(SIG_BLOCK, &mask, nullptr) != 0) {
        std::cerr << "[FATAL] pthread_sigmask failed\n";
        return -1;
    }

    // Receive SIGINT/SIGTERM via FD
    int sfd = signalfd(-1, &mask, SFD_CLOEXEC);
    if (sfd < 0) {
        std::cerr << "[FATAL] signalfd failed\n";
        return -1;
    }
    return sfd;
}

/* ==================== Main Entry Point ==================== */

int main(int argc, char** argv) {
    std::string db_host = (argc > 1) ? argv[1] : "192.168.1.156";
    std::string connStr = "host=" + db_host + " port=5432 dbname=Nexipass user=postgres password=1234";

    std::cout << "--- NexiPass RT System (3-Thread Architecture) ---\n";

    int sfd = make_signalfd_for_shutdown();
    if (sfd < 0) return 1;

    /* ==================== Database Connection ==================== */

    Database db(connStr);
    if (db.connect() != DbResult::Ok) {
        std::cerr << "[FATAL] Failed to connect to database\n";
        close(sfd);
        return 1;
    }

    /* ==================== System Initialization ==================== */

    FeedbackController feedback;
    CardService cardService(&db, &feedback);
    ApiController app(&db, &cardService, &feedback);

    app.start();
    std::cout << "[System] Running. Awaiting SIGINT/SIGTERM...\n";

    /* ==================== Wait for Shutdown Signal ==================== */

    pollfd pfd{};
    pfd.fd = sfd;
    pfd.events = POLLIN;

    while (true) {
        int r = poll(&pfd, 1, -1);
        if (r <= 0) continue;

        if (pfd.revents & POLLIN) {
            signalfd_siginfo si{};
            ssize_t n = read(sfd, &si, sizeof(si));
            if (n == sizeof(si)) {
                std::cout << "\n[System] Signal received (" << si.ssi_signo << "). Shutting down...\n";
                break;
            }
        }
    }

    /* ==================== Graceful Shutdown ==================== */

    std::cout << "[System] Stopping threads...\n";
    app.stop();

    close(sfd);
    std::cout << "[System] Shutdown complete.\n";
    return 0;
}
