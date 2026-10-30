#ifndef GPS_M10_PARSER_H
#define GPS_M10_PARSER_H

#include "config_types.h"

extern volatile uint32_t gpsNmeaCount;

class GPS_M10 {
private:
    String nmeaLine = "";

    double convertNMEAToDecimal(String val, String dir) {
        if (val.length() < 4) return 0.0;
        int dotIndex = val.indexOf('.');
        if (dotIndex < 2) return 0.0;
        int degLen = dotIndex - 2;
        double degrees = val.substring(0, degLen).toDouble();
        double minutes = val.substring(degLen).toDouble();
        double dec = degrees + (minutes / 60.0);
        if (dir == "S" || dir == "W") dec = -dec;
        return dec;
    }

    String getField(String data, char separator, int index) {
        int found = 0;
        int strIndex[] = { 0, -1 };
        int maxIndex = data.length() - 1;
        for (int i = 0; i <= maxIndex && found <= index; i++) {
            if (data.charAt(i) == separator || i == maxIndex) {
                found++;
                strIndex[0] = strIndex[1] + 1;
                strIndex[1] = (i == maxIndex) ? i + 1 : i;
            }
        }
        return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
    }

    void parseLine(String line, volatile GPSData_t *gps) {
        line.trim();
        if (!line.startsWith("$G")) return;

        gpsNmeaCount++;

        // 1. Giải mã $GNRMC (Lấy trạng thái Fix 'A', Vĩ độ, Kinh độ, Vận tốc km/h)
        if (line.startsWith("$GNRMC") || line.startsWith("$GPRMC")) {
            String status = getField(line, ',', 2);
            if (status == "A") {
                double lat = convertNMEAToDecimal(getField(line, ',', 3), getField(line, ',', 4));
                double lon = convertNMEAToDecimal(getField(line, ',', 5), getField(line, ',', 6));
                if (lat != 0.0 && lon != 0.0) {
                    gps->is_valid  = true;
                    gps->latitude  = lat;
                    gps->longitude = lon;
                    gps->speed_kmh = getField(line, ',', 7).toFloat() * 1.852f;
                }
            }
        }
        // 2. Giải mã $GNGGA (Lấy cả Tọa độ, Fix Status và Số vệ tinh = 12!)
        else if (line.startsWith("$GNGGA") || line.startsWith("$GPGGA")) {
            int fixQual = getField(line, ',', 6).toInt();
            int usedSat = getField(line, ',', 7).toInt();
            if (usedSat >= 0) gps->satellites = (uint8_t)usedSat;

            if (fixQual > 0) {
                double lat = convertNMEAToDecimal(getField(line, ',', 2), getField(line, ',', 3));
                double lon = convertNMEAToDecimal(getField(line, ',', 4), getField(line, ',', 5));
                if (lat != 0.0 && lon != 0.0) {
                    gps->is_valid  = true;
                    gps->latitude  = lat;
                    gps->longitude = lon;
                }
            }
        }
        // 3. Giải mã $GNGLL (Dự phòng thêm tọa độ từ câu $GNGLL)
        else if (line.startsWith("$GNGLL") || line.startsWith("$GPGLL")) {
            String status = getField(line, ',', 6);
            if (status == "A") {
                double lat = convertNMEAToDecimal(getField(line, ',', 1), getField(line, ',', 2));
                double lon = convertNMEAToDecimal(getField(line, ',', 3), getField(line, ',', 4));
                if (lat != 0.0 && lon != 0.0) {
                    gps->is_valid  = true;
                    gps->latitude  = lat;
                    gps->longitude = lon;
                }
            }
        }
    }

public:
    void feed(char c, volatile GPSData_t *gps) {
        // QUAN TRỌNG NHẤT: Hễ gặp dấu '$' là bắt đầu câu NMEA mới, vứt bỏ toàn bộ rác UBX dính phía trước!
        if (c == '$') {
            nmeaLine = "$";
        } else if (c == '\n' || c == '\r') {
            if (nmeaLine.startsWith("$G")) {
                parseLine(nmeaLine, gps);
            }
            nmeaLine = "";
        } else if (nmeaLine.length() > 0 && nmeaLine.length() < 120 && c >= 32 && c <= 126) {
            nmeaLine += c;
        }
    }
};

#endif