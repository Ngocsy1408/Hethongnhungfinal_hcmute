#ifndef W25Q128_FLASH_H
#define W25Q128_FLASH_H

#include <SPI.h>
#include "config_types.h"

#define W25Q_CMD_WRITE_ENABLE   0x06
#define W25Q_CMD_READ_STATUS1   0x05
#define W25Q_CMD_READ_DATA      0x03
#define W25Q_CMD_PAGE_PROGRAM   0x02
#define W25Q_CMD_SECTOR_ERASE   0x20
#define W25Q_CMD_JEDEC_ID       0x9F

#define LOG_RECORD_MAGIC        0xCAFEBABE
#define LOG_SECTOR_ADDR         0x001000UL // Sector 1 lưu băng ghi hộp đen 15 giây

class W25Q128_Flash {
private:
    uint8_t csPin;
    bool hasSavedCrash = false;

    void csLow()  { digitalWrite(csPin, LOW); }
    void csHigh() { digitalWrite(csPin, HIGH); }

    void waitBusy() {
        csLow();
        SPI.transfer(W25Q_CMD_READ_STATUS1);
        while (SPI.transfer(0x00) & 0x01) {
            delay(1);
        }
        csHigh();
    }

    void writeEnable() {
        csLow();
        SPI.transfer(W25Q_CMD_WRITE_ENABLE);
        csHigh();
    }

public:
    bool isConnected = false;

    bool begin(uint8_t cs, uint8_t sck, uint8_t miso, uint8_t mosi) {
        csPin = cs;
        pinMode(csPin, OUTPUT);
        csHigh();

        SPI.begin(sck, miso, mosi, csPin);

        csLow();
        SPI.transfer(W25Q_CMD_JEDEC_ID);
        uint8_t mfId  = SPI.transfer(0x00);
        uint8_t memId = SPI.transfer(0x00);
        uint8_t capId = SPI.transfer(0x00);
        csHigh();

        if (mfId == 0xEF && capId == 0x18) {
            isConnected = true;
            Serial.printf("[W25Q128] Đã nhận chip Flash 16MB (ID: %02X %02X %02X)\n", mfId, memId, capId);
            checkExistingLog();
            return true;
        } else {
            isConnected = false;
            Serial.printf("[W25Q128] CẢNH BÁO: Chưa cắm W25Q128 (ID: %02X %02X %02X)\n", mfId, memId, capId);
            return false;
        }
    }

    void readBytes(uint32_t addr, uint8_t *buf, uint16_t len) {
        waitBusy();
        csLow();
        SPI.transfer(W25Q_CMD_READ_DATA);
        SPI.transfer((addr >> 16) & 0xFF);
        SPI.transfer((addr >> 8) & 0xFF);
        SPI.transfer(addr & 0xFF);
        for (uint16_t i = 0; i < len; i++) {
            buf[i] = SPI.transfer(0x00);
        }
        csHigh();
    }

    void writePage(uint32_t addr, const uint8_t *buf, uint16_t len) {
        waitBusy();
        writeEnable();
        csLow();
        SPI.transfer(W25Q_CMD_PAGE_PROGRAM);
        SPI.transfer((addr >> 16) & 0xFF);
        SPI.transfer((addr >> 8) & 0xFF);
        SPI.transfer(addr & 0xFF);
        for (uint16_t i = 0; i < len; i++) {
            SPI.transfer(buf[i]);
        }
        csHigh();
        waitBusy();
    }

    void eraseSector(uint32_t addr) {
        waitBusy();
        writeEnable();
        csLow();
        SPI.transfer(W25Q_CMD_SECTOR_ERASE);
        SPI.transfer((addr >> 16) & 0xFF);
        SPI.transfer((addr >> 8) & 0xFF);
        SPI.transfer(addr & 0xFF);
        csHigh();
        waitBusy();
    }

    void checkExistingLog() {
        BlackboxLogRecord_t rec;
        readBytes(LOG_SECTOR_ADDR, (uint8_t*)&rec, sizeof(BlackboxLogRecord_t));
        hasSavedCrash = (rec.magic == LOG_RECORD_MAGIC);
        if (hasSavedCrash) {
            Serial.println("[W25Q128] Phát hiện có băng ghi Hộp Đen 15 giây trong bộ nhớ! Gõ 'LOG' qua Bluetooth để xem.");
        }
    }

    // Ghi trọn vẹn 16 mốc thời gian (T-10s đến T+5s) xuống W25Q128
    bool save15SecCrashWindow(BlackboxLogRecord_t *windowRecords) {
        if (!isConnected) return false;

        // Xóa Sector cũ và ghi 16 mốc thời gian mới (Mỗi mốc cách nhau 64 bytes)
        eraseSector(LOG_SECTOR_ADDR);
        for (uint8_t i = 0; i < BLACKBOX_WINDOW_LEN; i++) {
            windowRecords[i].magic = LOG_RECORD_MAGIC;
            uint32_t addr = LOG_SECTOR_ADDR + (uint32_t)i * 64UL;
            writePage(addr, (const uint8_t*)&windowRecords[i], sizeof(BlackboxLogRecord_t));
        }

        hasSavedCrash = true;
        Serial.println("\n💾 [W25Q128] ĐÃ GHI XONG BĂNG HỘP ĐEN 15 GIÂY (10s TRƯỚC + 5s SAU TAI NẠN) VÀO FLASH!\n");
        return true;
    }

    // Phục dựng lại diễn biến 15 giây từ W25Q128 để xem trên Điện thoại / Máy tính
    String getCrashHistorySummary() {
        if (!isConnected) return "Loi: Chua ket noi W25Q128!\n";
        if (!hasSavedCrash) return "Hop den W25Q128: Chua co vu tai nan nao.\n";

        String out = "\n=== PHUC DUNG 15 GIAY TAI NAN (W25Q128) ===\n";
        BlackboxLogRecord_t rec;
        for (uint8_t i = 0; i < BLACKBOX_WINDOW_LEN; i++) {
            uint32_t addr = LOG_SECTOR_ADDR + (uint32_t)i * 64UL;
            readBytes(addr, (uint8_t*)&rec, sizeof(BlackboxLogRecord_t));
            if (rec.magic == LOG_RECORD_MAGIC) {
                char lineBuf[120];
                const char *tag = (rec.rel_sec == 0) ? "💥 DAM!" : ((rec.crash_state >= 2) ? "⚠️ NGA" : "OK");
                snprintf(lineBuf, sizeof(lineBuf), "[T%+3ds] Roll:%5.1f° Pitch:%5.1f° | %4.1fG | %4.1fkm/h | %s\n",
                         rec.rel_sec, rec.roll, rec.pitch, rec.max_g, rec.speed_kmh, tag);
                out += String(lineBuf);
            }
        }
        out += "===========================================\n";
        return out;
    }

    void clearAllLogs() {
        if (!isConnected) return;
        eraseSector(LOG_SECTOR_ADDR);
        hasSavedCrash = false;
        Serial.println("[W25Q128] Đã xóa trắng bộ nhớ hộp đen!");
    }
};

#endif