#ifndef GPS_M10_PARSER_H
#define GPS_M10_PARSER_H

#include "config_types.h"

class GPS_M10 {
private:
    String nmeaLine = "";

    double convertNMEAToDecimal(String val, String dir) {
        if (val.length() < 4) return 0.0;
        int dotIndex = val.indexOf('.');
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
        if (line.startsWith("$GNRMC") || line.startsWith("$GPRMC")) {
            String status = getField(line, ',', 2);
            if (status == "A") {
                gps->is_valid  = true;
                gps->latitude  = convertNMEAToDecimal(getField(line, ',', 3), getField(line, ',', 4));
                gps->longitude = convertNMEAToDecimal(getField(line, ',', 5), getField(line, ',', 6));
                gps->speed_kmh = getField(line, ',', 7).toFloat() * 1.852f;
            } else {
                gps->is_valid = false;
            }
        } else if (line.startsWith("$GNGGA") || line.startsWith("$GPGGA")) {
            gps->satellites = (uint8_t)getField(line, ',', 7).toInt();
        }
    }

public:
    void feed(char c, volatile GPSData_t *gps) {
        if (c == '\n') {
            parseLine(nmeaLine, gps);
            nmeaLine = "";
        } else if (c != '\r') {
            nmeaLine += c;
            if (nmeaLine.length() > 120) nmeaLine = "";
        }
    }
};

#endif