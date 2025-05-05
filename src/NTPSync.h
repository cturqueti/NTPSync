#ifndef NTP_SYNC_H
#define NTP_SYNC_H

// #include <LogLibrary.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <algorithm>
#include <vector>

//----------- Definição dos pinos para a serial alternativa (RX, TX) -------------------
#ifndef UART_RX_PIN
#define UART_RX_PIN 44
#endif

#ifndef UART_TX_PIN
#define UART_TX_PIN 43
#endif
//--------------------------------------------------------------------------------------
//---------------------- Velocidades de comunicação serial -----------------------------
#ifndef MAIN_SERIAL_BAUDRATE
#define MAIN_SERIAL_BAUDRATE 115200
#endif
//--------------------------------------------------------------------------------------

struct NTPServer
{
    const char *hostname;
    String ip;
    bool resolved;
    uint32_t lastResponseTime;
    uint8_t stratum; // Qualidade do servidor (0-15)
};

struct Timeval
{
    const char *time_zone;
    std::vector<NTPServer> servers; // Usando vector para flexibilidade
    int32_t utc_offset;             // Offset em segundos (-3h = -10800)
    bool dst_active;                // Horário de verão
    time_t lastSync;                // Timestamp da última sincronização
};

class NTPSync
{
public:
    static uint32_t _syncInterval;
    static uint32_t _retryInterval;
    static void begin(uint32_t syncInterval = 3600000, uint32_t retryInterval = 300000);
    static bool syncTime(uint8_t maxRetries = 3);
    static bool isTimeSynced() { return _timeSyncked; }
    static time_t getLastSync() { return _timeval.lastSync; }

    static void setTimeval(const char *timezone, int32_t utc_offset,
                           const char *ntpServer1, const char *ntpServer2 = nullptr,
                           const char *ntpServer3 = nullptr);

    static void setSyncIntervals(uint32_t syncInterval, uint32_t retryInterval)
    {
        _syncInterval = syncInterval;
        _retryInterval = retryInterval;
    }

private:
    static Preferences _prefs;
    static Timeval _timeval;
    static bool _timeSyncked;

    static bool queryNTP(NTPServer &server);
    static void sortServersByPerformance();
    static bool resolveAllServers();
    static bool resolveServer(NTPServer &server);
    static void saveTimeToPrefs();
    static void loadTimeFromPrefs();
    static bool isValidNTPResponse(const byte *packet, size_t length);
    static void startTask();
    static void updateDstStatus(time_t now);
};
void timeSyncTask(void *pvParameters);
#endif // NTP_SYNC_H