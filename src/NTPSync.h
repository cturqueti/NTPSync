#ifndef NTP_SYNC_H
#define NTP_SYNC_H

// #include <LogLibrary.h>
#include "utc.h"
#include <LogLibrary.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <algorithm>
#include <mutex>
#include <vector>

constexpr uint32_t MINUTES_TO_MS = 60000;

/**
 * @brief Classe para sincronização de tempo via NTP com persistência e fallback
 *
 * Exemplo de uso:
 * NTPSync::setTimeval("America/Sao_Paulo",{"pool.ntp.org", "br.pool.ntp.org"});
 * NTPSync::begin();
 */
class NTPSync
{
public:
    static uint32_t _retryInterval;
    static uint32_t _syncInterval;
    static std::mutex _mutex;

    static void logControl(bool enabled = true);
    static void begin(uint32_t syncInterval = 3600000, uint32_t retryInterval = 300000);
    static bool syncTime(uint8_t maxRetries = 3);
    static bool isTimeSynced();

    static void setTimeval(const char *timezone, const std::vector<std::string> &ntpServers);

    static void setSyncIntervals(uint32_t syncInterval, uint32_t retryInterval);

private:
    struct NTPServer
    {
        String hostname;
        String ip;
        bool resolved;
        uint32_t lastResponseTime;
        uint8_t stratum; // Qualidade do servidor (0-15)
        uint32_t failureCount;
    };
    struct Timeval
    {
        String time_zone;
        std::vector<NTPServer> servers; // Usando vector para flexibilidade
        int32_t utc_offset;             // Offset em segundos (-3h = -10800)
        bool dst_active;                // Horário de verão
        time_t lastSync;                // Timestamp da última sincronização
    };

    static Preferences _prefs;
    static Timeval _timeval;
    static bool _timeSyncked;
    static bool _logEnabled;

    static void sortServersByPerformance();
    static bool resolveAllServers();
    static bool resolveServer(NTPServer &server);
    static void saveTimeToPrefs();
    static void loadTimeFromPrefs();
    static void updateDstStatus(time_t now);
    static void startTask();
    static bool syncWithServer(NTPServer &server);
    static time_t getExponentialBackoffDelay(uint32_t failureCount);
};
void timeSyncTaskNTP(void *pvParameters);
#endif // NTP_SYNC_H