#ifndef W25Q128_FLASH_H
#define W25Q128_FLASH_H

#include <SPI.h>
#include "config_types.h"

class W25Q128_Flash {
private:
    uint8_t csPin   = W25Q_CS_PIN;
    uint8_t sckPin  = W25Q_SCK_PIN;
    uint8_t misoPin = W25Q_MISO_PIN;
    uint8_t mosiPin = W25Q_MOSI_PIN;
    bool hasSavedCrash = false;

    void csLow()  { digitalWrite(csPin, LOW); }
    void csHigh() { digitalWrite(csPin, HIGH); }

    void waitBusy() {
        csLow();
        SPI.transfer(0x05);
        uint32_t t0 = millis();
        while ((SPI.transfer(0x00) & 0x01) && (millis() - t0 < 2000)) {
            delay(1);
        }
        csHigh();
    }

    void writeEnable() {
        csLow();
        SPI.transfer(0x06);
        csHigh();
    }

    bool probeChip(uint8_t sck, uint8_t miso, uint8_t mosi, uint8_t &mfId, uint8_t &memId, uint8_t &capId) {
        SPI.end();
        SPI.begin(sck, miso, mosi, csPin);
        delay(10);

        // Đánh thức chip khỏi chế độ Power-Down (Lệnh 0xAB)
        csLow();
        SPI.transfer(0xAB);
        csHigh();
        delay(5);

        // Đọc mã nhà sản xuất JEDEC ID (Lệnh 0x9F)
        csLow();
        SPI.transfer(0x9F);
        mfId  = SPI.transfer(0x00);
        memId = SPI.transfer(0x00);
        capId = SPI.transfer(0x00);
        csHigh();

        // Winbond W25Qxx có mfId = 0xEF (W25Q128 = 0xEF 0x40 0x18)
        return (mfId == 0xEF && capId != 0x00 && capId != 0xFF);
    }

public:
    bool isConnected = false;

    bool begin(uint8_t cs, uint8_t sck, uint8_t miso, uint8_t mosi) {
        csPin = cs; sckPin = sck; misoPin = miso; mosiPin = mosi;
        pinMode(csPin, OUTPUT);
        csHigh();

        uint8_t mfId = 0, memId = 0, capId = 0;

        // 1. Thử chiều chuẩn: MISO = 13 (DO), MOSI = 11 (DI)
        if (probeChip(sckPin, misoPin, mosiPin, mfId, memId, capId)) {
            isConnected = true;
            Serial.printf("✅ [W25Q128] Đã kết nối Flash thành công (DO=%d, DI=%d | ID: %02X %02X %02X)\n",
                          misoPin, mosiPin, mfId, memId, capId);
        }
        // 2. Tự động thử đảo chiều: MISO = 11, MOSI = 13 (Nếu người dùng cắm ngược DI và DO)
        else if (probeChip(sckPin, mosiPin, misoPin, mfId, memId, capId)) {
            isConnected = true;
            uint8_t tmp = misoPin; misoPin = mosiPin; mosiPin = tmp;
            Serial.printf("✅ [W25Q128] Đã tự động đảo chân & kết nối thành công (DO=%d, DI=%d | ID: %02X %02X %02X)\n",
                          misoPin, mosiPin, mfId, memId, capId);
        }
        else {
            isConnected = false;
            Serial.printf("⚠️ [W25Q128] Chưa nhận được chip W25Q128 (ID đọc về: %02X %02X %02X) -> Kiểm tra dây CS(10), DI(11), CLK(12), DO(13) và 3.3V!\n",
                          mfId, memId, capId);
            return false;
        }

        BlackboxLogRecord_t rec;
        readBytes(LOG_SECTOR_ADDR, (uint8_t*)&rec, sizeof(BlackboxLogRecord_t));
        hasSavedCrash = (rec.magic == LOG_RECORD_MAGIC);
        return true;
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
        if (!isConnected) {
            begin(csPin, sckPin, misoPin, mosiPin);
            if (!isConnected) return false;
        }
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
        // Nếu lúc bật máy chưa ăn dây, tự động dò lại khi người dùng gõ LOG
        if (!isConnected) {
            begin(csPin, sckPin, misoPin, mosiPin);
            if (!isConnected) return "❌ Loi: Chua ket noi duoc chip W25Q128! Kiem tra 6 day SPI.\n";
        }
        if (!hasSavedCrash) {
            return "✅ [W25Q128 OK] Bộ nhớ sẵn sàng! Hiện chưa có vụ tai nạn nào được ghi.\n";
        }

        String out = "\n=== PHUC DUNG 15 GIAY TAI NAN (W25Q128) ===\n";
        BlackboxLogRecord_t rec;
        for (uint8_t i = 0; i < BLACKBOX_WINDOW_LEN; i++) {
            readBytes(LOG_SECTOR_ADDR + (uint32_t)i * 64UL, (uint8_t*)&rec, sizeof(BlackboxLogRecord_t));
            if (rec.magic == LOG_RECORD_MAGIC) {
                char lineBuf[140];
                const char *tag = (rec.rel_sec == 0) ? "💥 DAM!" : ((rec.crash_state >= 2) ? "⚠️ NGA" : "OK");
                snprintf(lineBuf, sizeof(lineBuf),
                         "[T%+3ds] Roll:%5.1f° Pitch:%5.1f° | %4.1fG | GPS:%.5f,%.5f (%4.1fkm/h) | %s\n",
                         rec.rel_sec, rec.roll, rec.pitch, rec.max_g, rec.latitude, rec.longitude, rec.speed_kmh, tag);
                out += String(lineBuf);
            }
        }
        out += "===========================================\n";
        return out;
    }

    void clearAllLogs() {
        if (!isConnected) begin(csPin, sckPin, misoPin, mosiPin);
        if (!isConnected) return;
        eraseSector(LOG_SECTOR_ADDR);
        hasSavedCrash = false;
        Serial.println("✅ [W25Q128] Đã xóa trắng lịch sử hộp đen!");
    }
};

#endif