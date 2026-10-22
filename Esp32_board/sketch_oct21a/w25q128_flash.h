#ifndef W25Q128_FLASH_H
#define W25Q128_FLASH_H

#include <SPI.h>
#include "config_types.h"

class W25Q128_Flash {
private:
    uint8_t csPin;
    bool hasSavedCrash = false;

    void csLow()  { digitalWrite(csPin, LOW); }
    void csHigh() { digitalWrite(csPin, HIGH); }

    void waitBusy() {
        csLow();
        SPI.transfer(0x05);
        while (SPI.transfer(0x00) & 0x01) { delay(1); }
        csHigh();
    }

    void writeEnable() {
        csLow();
        SPI.transfer(0x06);
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
        SPI.transfer(0x9F); // Đọc JEDEC ID
        uint8_t mfId  = SPI.transfer(0x00);
        uint8_t memId = SPI.transfer(0x00);
        uint8_t capId = SPI.transfer(0x00);
        csHigh();

        if (mfId == 0xEF && capId == 0x18) {
            isConnected = true;
            Serial.printf("✅ [W25Q128] Đã kết nối Flash 16MB (ID: %02X %02X %02X)\n", mfId, memId, capId);
            BlackboxLogRecord_t rec;
            readBytes(LOG_SECTOR_ADDR, (uint8_t*)&rec, sizeof(BlackboxLogRecord_t));
            hasSavedCrash = (rec.magic == LOG_RECORD_MAGIC);
            return true;
        }
        Serial.printf("⚠️ [W25Q128] Chưa cắm W25Q128 (ID: %02X %02X %02X)\n", mfId, memId, capId);
        return false;
    }

    void readBytes(uint32_t addr, uint8_t *buf, uint16_t len) {
        waitBusy();
        csLow();
        SPI.transfer(0x03);
        SPI.transfer((addr >> 16) & 0xFF);
        SPI.transfer((addr >> 8) & 0xFF);
        SPI.transfer(addr & 0xFF);
        for (uint16_t i = 0; i < len; i++) buf[i] = SPI.transfer(0x00);
        csHigh();
    }

    void writePage(uint32_t addr, const uint8_t *buf, uint16_t len) {
        waitBusy();
        writeEnable();
        csLow();
        SPI.transfer(0x02);
        SPI.transfer((addr >> 16) & 0xFF);
        SPI.transfer((addr >> 8) & 0xFF);
        SPI.transfer(addr & 0xFF);
        for (uint16_t i = 0; i < len; i++) SPI.transfer(buf[i]);
        csHigh();
        waitBusy();
    }

    void eraseSector(uint32_t addr) {
        waitBusy();
        writeEnable();
        csLow();
        SPI.transfer(0x20);
        SPI.transfer((addr >> 16) & 0xFF);
        SPI.transfer((addr >> 8) & 0xFF);
        SPI.transfer(addr & 0xFF);
        csHigh();
        waitBusy();
    }

    bool save15SecCrashWindow(BlackboxLogRecord_t *windowRecords) {
        if (!isConnected) return false;
        eraseSector(LOG_SECTOR_ADDR);
        for (uint8_t i = 0; i < BLACKBOX_WINDOW_LEN; i++) {
            windowRecords[i].magic = LOG_RECORD_MAGIC;
            writePage(LOG_SECTOR_ADDR + (uint32_t)i * 64UL, (const uint8_t*)&windowRecords[i], sizeof(BlackboxLogRecord_t));
        }
        hasSavedCrash = true;
        Serial.println("\n💾 [W25Q128] ĐÃ LƯU XONG 15 GIÂY HỘP ĐEN VÀO FLASH!\n");
        return true;
    }

    String getCrashHistorySummary() {
        if (!isConnected) return "Loi: Chua ket noi W25Q128!\n";
        if (!hasSavedCrash) return "Hop den W25Q128: Chua co vu tai nan nao.\n";

        String out = "\n=== PHUC DUNG 15 GIAY TAI NAN (W25Q128) ===\n";
        BlackboxLogRecord_t rec;
        for (uint8_t i = 0; i < BLACKBOX_WINDOW_LEN; i++) {
            readBytes(LOG_SECTOR_ADDR + (uint32_t)i * 64UL, (uint8_t*)&rec, sizeof(BlackboxLogRecord_t));
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
        Serial.println("✅ [W25Q128] Đã xóa trắng lịch sử hộp đen!");
    }
};

#endif