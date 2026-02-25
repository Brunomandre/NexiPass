#ifndef SIMPLERFID_H
#define SIMPLERFID_H

#include <iostream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <chrono>
#include <thread>
#include <sys/timerfd.h>
#include <poll.h>

/* ==================== MFRC522 Register Map ==================== */

#define CommandReg        0x01
#define CommIEnReg        0x02
#define CommIrqReg        0x04
#define ErrorReg          0x06
#define FIFODataReg       0x09
#define FIFOLevelReg      0x0A
#define BitFramingReg     0x0D
#define ModeReg           0x11
#define TxControlReg      0x14
#define TxASKReg          0x15
#define TModeReg          0x2A
#define TPrescalerReg     0x2B
#define TReloadRegL       0x2D

/* ==================== MFRC522 Commands ==================== */

#define PCD_IDLE          0x00
#define PCD_TRANSCEIVE    0x0C
#define PCD_RESETPHASE    0x0F

/* ==================== ISO 14443-A Commands ==================== */

#define PICC_REQIDL       0x26  // Request Idle
#define PICC_ANTICOLL     0x93  // Anti-collision

/* ==================== SimpleRFID Class ==================== */

class SimpleRFID {
private:
    int spi_fd;
    uint32_t speed = 1000000; // 1MHz SPI clock
    uint8_t bits = 8;

    /* ==================== SPI Communication ==================== */

    void spiWrite(uint8_t reg, uint8_t value) {
        uint8_t tx[2] = {(uint8_t)((reg << 1) & 0x7E), value};
        struct spi_ioc_transfer tr = {};
        tr.tx_buf = (unsigned long)tx;
        tr.len = 2;
        tr.speed_hz = speed;
        tr.bits_per_word = bits;
        ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
    }

    uint8_t spiRead(uint8_t reg) {
        uint8_t tx[2];
        uint8_t rx[2] = {0};
        tx[0] = ((reg << 1) & 0x7E) | 0x80;
        tx[1] = 0x00;
        struct spi_ioc_transfer tr = {
            .tx_buf = (unsigned long)tx,
            .rx_buf = (unsigned long)rx,
            .len = 2,
            .speed_hz = speed,
            .bits_per_word = bits,
        };
        ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
        return rx[1];
    }

    void setBitMask(uint8_t reg, uint8_t mask) {
        spiWrite(reg, spiRead(reg) | mask);
    }

    void clearBitMask(uint8_t reg, uint8_t mask) {
        spiWrite(reg, spiRead(reg) & (~mask));
    }

    /* ==================== Deterministic Wait (timerfd) ==================== */

    bool waitReadyMs(int timeout_ms) {
        int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
        if (tfd < 0) return false;

        itimerspec its{};
        its.it_value.tv_nsec = 1 * 1000000L;      // 1ms
        its.it_interval.tv_nsec = 1 * 1000000L;   // repeat every 1ms
        (void)timerfd_settime(tfd, 0, &its, nullptr);

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

        uint8_t last = 0xFF;
        int stable = 0;

        pollfd pfd{};
        pfd.fd = tfd;
        pfd.events = POLLIN;

        while (std::chrono::steady_clock::now() < deadline) {
            int r = poll(&pfd, 1, -1);
            if (r <= 0) continue;

            uint64_t exp;
            (void)read(tfd, &exp, sizeof(exp));

            uint8_t v = spiRead(CommandReg);
            if (v == last) stable++;
            else stable = 0;

            last = v;

            if (stable >= 5) {
                close(tfd);
                return true;
            }
        }

        close(tfd);
        return false;
    }

    /* ==================== ISO 14443-A Protocol ==================== */

    int communicateCard(uint8_t command, uint8_t *sendData, uint8_t sendLen,
                        uint8_t *backData, uint8_t *backLen) {
        uint8_t irqEn = 0x77;
        uint8_t waitIRq = 0x30;

        spiWrite(CommIEnReg, irqEn | 0x80);
        clearBitMask(CommIrqReg, 0x80);
        setBitMask(FIFOLevelReg, 0x80);
        spiWrite(CommandReg, PCD_IDLE);

        for (int i = 0; i < sendLen; i++) {
            spiWrite(FIFODataReg, sendData[i]);
        }

        spiWrite(CommandReg, command);
        if (command == PCD_TRANSCEIVE) {
            setBitMask(BitFramingReg, 0x80);
        }

        auto start = std::chrono::steady_clock::now();
        auto timeout = std::chrono::milliseconds(25);
        uint8_t n = 0;

        while (std::chrono::steady_clock::now() - start < timeout) {
            n = spiRead(CommIrqReg);
            if (n & waitIRq) break;
            if (n & 0x01) return -1; // Timeout
            std::this_thread::yield();
        }

        if (!(n & waitIRq)) return -2;

        clearBitMask(BitFramingReg, 0x80);

        if (spiRead(ErrorReg) & 0x1B) return -3; // Error flags

        if (backData && backLen) {
            n = spiRead(FIFOLevelReg);
            if (n > *backLen) n = *backLen;
            *backLen = n;
            for (int i = 0; i < n; i++) {
                backData[i] = spiRead(FIFODataReg);
            }
        }
        return 0;
    }

public:
    /* ==================== Initialization ==================== */

    SimpleRFID() {
        spi_fd = open("/dev/spidev0.0", O_RDWR);
        if (spi_fd < 0) {
            std::cerr << "[RFID] ERROR: Failed to open /dev/spidev0.0\n";
        } else {
            spiWrite(CommandReg, PCD_RESETPHASE);

            if (!waitReadyMs(80)) {
                std::cerr << "[RFID] WARNING: RC522 may not be ready (continuing...)\n";
            }

            // Configure RC522 for 13.56MHz
            spiWrite(TModeReg, 0x8D);
            spiWrite(TPrescalerReg, 0x3E);
            spiWrite(TReloadRegL, 30);
            spiWrite(TxASKReg, 0x40);
            spiWrite(ModeReg, 0x3D);
            setBitMask(TxControlReg, 0x03); // Enable antenna
            
            std::cout << "[RFID] Hardware initialized\n";
        }
    }

    ~SimpleRFID() {
        if (spi_fd >= 0) close(spi_fd);
    }

    /* ==================== Public API ==================== */

    bool isReady() { 
        return spi_fd >= 0; 
    }

    bool isCardPresent() {
        if (spi_fd < 0) return false;
        uint8_t bufferATQA[2];
        uint8_t bufferSize = sizeof(bufferATQA);
        spiWrite(BitFramingReg, 0x07);
        uint8_t cmd = PICC_REQIDL;
        return (communicateCard(PCD_TRANSCEIVE, &cmd, 1, bufferATQA, &bufferSize) == 0);
    }

    bool readCardUID(std::string &uid_hex) {
        if (spi_fd < 0) return false;
        
        uint8_t serNum[5];
        uint8_t serNumLen = 5;
        spiWrite(BitFramingReg, 0x00);

        uint8_t cmd[2] = {PICC_ANTICOLL, 0x20};

        if (communicateCard(PCD_TRANSCEIVE, cmd, 2, serNum, &serNumLen) == 0) {
            if (serNumLen == 5) {
                // Verify BCC (Block Check Character)
                uint8_t bcc = 0;
                for (int i = 0; i < 4; i++) bcc ^= serNum[i];

                if (bcc == serNum[4]) {
                    char hex[9];
                    sprintf(hex, "%02X%02X%02X%02X",
                            serNum[0], serNum[1], serNum[2], serNum[3]);
                    uid_hex = std::string(hex);
                    return true;
                }
            }
        }
        return false;
    }
};

#endif
