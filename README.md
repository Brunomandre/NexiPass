# NexiPass

**NexiPass** is an embedded access and consumption management system designed to run on a Raspberry Pi. It combines NFC card reading, a PostgreSQL backend, a REST API, and physical LED/buzzer feedback into a real-time, multi-threaded application.

---

## Overview

NexiPass allows event or venue operators to:
- **Activate** NFC wristbands/cards linked to a phone number
- **Register consumptions** (food, drinks, etc.) against an active card
- **Close/checkout** a card at the end, aggregating all consumption data into permanent records
- **Monitor totals** per product and employee (owner view)

A companion web interface communicates with the device over HTTP (port 5000).

---

## Architecture

The system runs three concurrent threads with real-time (`SCHED_FIFO`) priorities:

```
┌─────────────────────────────────────────────────────┐
│                    ApiController                     │
│                                                      │
│  [NFC Thread P=80]  →  workQueue  →  [Worker P=30]  │
│       MFRC522 SPI                   DB Queries       │
│                                                      │
│  [Network Thread P=50]                               │
│       HTTP REST API (port 5000)                      │
└─────────────────────────────────────────────────────┘
         │                        │
   FeedbackController          Database
   (LED + Buzzer)              (PostgreSQL via libpq)
```

| Thread | Priority | Responsibility |
|---|---|---|
| NFC Thread | 80 (FIFO) | Polls MFRC522 via SPI, pushes UIDs to work queue |
| Worker Thread | 30 (FIFO) | Processes UIDs, queries DB, caches card state |
| Network Thread | 50 (FIFO) | Serves REST API with httplib |

> Real-time priorities require the process to run as root.

---

## Hardware

| Component | Interface | Notes |
|---|---|---|
| Raspberry Pi | — | BCM GPIO, tested on Pi 4 |
| MFRC522 RC522 | SPI (`/dev/spidev0.0`) | 13.56 MHz RFID/NFC reader |
| RGB LED | GPIO 23/24/25 | Driven by custom kernel module (`/dev/ledrgb`) |
| Buzzer | PWM (`/sys/class/pwm/pwmchip0/pwm1`) | Active piezo buzzer |

### LED Feedback

| Event | Color | Pattern |
|---|---|---|
| Card Activated | Green | 800ms on |
| Card Deactivated / Checkout | Blue | 800ms on + double beep |
| Error | Red | 800ms on + triple beep |

---

## Software Components

```
.
├── include/
│   ├── ApiController.h       # Thread orchestration & REST API
│   ├── CardService.h         # Business logic (activate, consume, close)
│   ├── Database.h            # PostgreSQL DTO definitions & interface
│   ├── FeedbackController.h  # LED + buzzer async feedback
│   ├── SimpleRFID.h          # MFRC522 SPI driver (header-only)
│   └── utility.h             # GPIO register abstraction
├── src/
│   ├── main_test.cpp         # Entry point with POSIX signal handling
│   ├── ApiController.cpp     # Thread implementations & HTTP routes
│   ├── CardService.cpp       # Card operation implementations
│   ├── Database.cpp          # libpq query implementations
│   ├── FeedbackController.cpp# timerfd/eventfd-based feedback engine
│   ├── utility.c             # GPIO set/clear helpers
│   └── led_dd.c              # Linux kernel module for RGB LED
└── scripts/
    ├── Makefile              # Main build
    └── Makefile.test         # Test build
```

---

## REST API

All endpoints run on **port 5000**. CORS is enabled for all origins.

| Method | Endpoint | Description |
|---|---|---|
| `POST` | `/login` | Authenticate user; returns role (`OWNER` / employee) |
| `GET` | `/wait_card` | Returns the last scanned card (fresh within 3s) |
| `POST` | `/activate_card` | Activate a card with a phone number |
| `POST` | `/add_consumption` | Register a product consumption on a card |
| `GET` | `/card_summary` | Get card details and consumption lines |
| `POST` | `/close_card` | Checkout and close a card |
| `POST` | `/validate_exit` | Check if a card has been closed |
| `GET` | `/products` | List available products |
| `GET` | `/product_totals` | Get aggregated totals (owner only) |

### Example: Activate a Card
```http
POST /activate_card
Content-Type: application/json

{
  "card_id": "A1B2C3D4",
  "phone": "912345678"
}
```

---

## Database Schema

The system uses PostgreSQL with the following key tables:

- **`users`** — operator accounts with roles (`OWNER`, employee)
- **`opencard`** — active card registry (`card_id`, `phone`, `status` A/D, `total_to_pay`)
- **`openconsumption`** — live consumption lines per active card
- **`product`** — product catalogue with unit prices
- **`producttotals`** — permanent aggregated totals (updated on card close)

Card lifecycle: `D` (inactive) → `A` (active) → `D` (closed, data committed to `producttotals`)

---

## Kernel Module (LED Driver)

The RGB LED is controlled via a custom character device driver (`led_dd.c`). It exposes `/dev/ledrgb` and accepts 3-byte strings (`"RGB"`) where each byte is `'0'` or `'1'`.

```bash
# Build and load the module
make -C /lib/modules/$(uname -r)/build M=$(pwd) modules
sudo insmod led_dd.ko

# Set LED to green
echo -n "010" > /dev/ledrgb
```

---

## Building & Running

### Prerequisites

```bash
sudo apt install libpq-dev g++ make
# nlohmann/json and cpp-httplib must be available in include/
```

### Build

```bash
cd scripts/
make
```

### Run

```bash
# Pass the DB host as argument (default: 192.168.1.156)
sudo ./nexipass [db_host]
```

> `sudo` is required for real-time thread priorities and SPI/GPIO access.

### Graceful Shutdown

Send `SIGINT` (Ctrl+C) or `SIGTERM`. The system will drain threads and close cleanly.

---

## Configuration

The database connection string is built at startup:

```
host=<db_host> port=5432 dbname=Nexipass user=postgres password=1234
```

To change credentials, edit `src/main_test.cpp` or extend the argument parsing.

---

## Dependencies

| Library | Purpose |
|---|---|
| [libpq](https://www.postgresql.org/docs/current/libpq.html) | PostgreSQL C client |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | Single-header HTTP server |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON serialization |
| Linux `timerfd` / `eventfd` | Deterministic timing (no `sleep`) |
| Linux `signalfd` | POSIX signal handling in main thread |

---

## License

This project was developed for academic/embedded systems purposes. See individual source files for licensing details.
