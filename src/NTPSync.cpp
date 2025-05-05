#include "NTPSync.h"
#include <Arduino.h>

Preferences NTPSync::_prefs;
Timeval NTPSync::_timeval;
bool NTPSync::_timeSyncked = false;
uint32_t NTPSync::_syncInterval = 3600000; // 1 hora
uint32_t NTPSync::_retryInterval = 300000; // 5 minutos

void NTPSync::begin(uint32_t syncInterval, uint32_t retryInterval)
{
    _syncInterval = syncInterval;
    _retryInterval = retryInterval;

    loadTimeFromPrefs();
    startTask();
}

bool NTPSync::syncTime(uint8_t maxRetries)
{
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial0.println("WiFi desconectado");
        _timeSyncked = false;
        return false;
    }

    // Resolve todos os servidores uma vez
    if (!resolveAllServers())
    {
        Serial0.println("Falha ao resolver servidores NTP");
        return false;
    }

    // Ordena servidores por performance
    sortServersByPerformance();

    Serial0.println("Iniciando sincronização");

    // Tenta cada servidor
    for (uint8_t attempt = 0; attempt < maxRetries; attempt++)
    {
        for (auto &server : _timeval.servers)
        {
            if (!server.resolved)
                continue;

            Serial0.printf("Tentando: %s, IP: %s\n", server.hostname, server.ip.c_str());
            configTime(_timeval.utc_offset, 0, server.ip.c_str());
            Serial0.println("Solicitando hora NTP...");
            struct tm timeinfo;
            if (!getLocalTime(&timeinfo))
            {
                Serial0.println("Falha ao obter hora.");
                return false;
            }
            else
            {
                _timeSyncked = true;
                _timeval.lastSync = time(nullptr);
                saveTimeToPrefs();
            }

            Serial0.println("Hora atual:");
            Serial0.println(&timeinfo, "%d/%m/%Y %H:%M:%S");
            return true;

            // if (queryNTP(server))
            // {
            //     _timeSyncked = true;
            //     _timeval.lastSync = time(nullptr);
            //     saveTimeToPrefs();
            //     return true;
            // }
        }
        delay(1000); // Espera entre tentativas
    }

    Serial0.println("Todos os servidores falharam");
    _timeSyncked = false;
    return false;
}

void NTPSync::setTimeval(const char *timezone, int32_t utc_offset,
                         const char *ntpServer1, const char *ntpServer2,
                         const char *ntpServer3)
{
    _timeval.time_zone = timezone;
    _timeval.utc_offset = utc_offset * 3600;
    _timeval.servers.clear();

    if (ntpServer1)
        _timeval.servers.push_back({ntpServer1, "", false, 1000, 0});
    if (ntpServer2)
        _timeval.servers.push_back({ntpServer2, "", false, 1000, 0});
    if (ntpServer3)
        _timeval.servers.push_back({ntpServer3, "", false, 1000, 0});

    // updateDstStatus(time(nullptr));
}

void NTPSync::sortServersByPerformance()
{
    std::sort(_timeval.servers.begin(), _timeval.servers.end(),
              [](const NTPServer &a, const NTPServer &b)
              {
                  // Prioriza servidores resolvidos e com menor tempo de resposta
                  if (a.resolved != b.resolved)
                      return a.resolved;
                  return a.lastResponseTime < b.lastResponseTime;
              });
}

bool NTPSync::resolveAllServers()
{
    bool allResolved = true;
    for (auto &server : _timeval.servers)
    {
        if (!server.resolved)
        {
            if (!resolveServer(server))
            {
                allResolved = false;
            }
        }
    }
    return allResolved;
}

bool NTPSync::resolveServer(NTPServer &server)
{
    if (server.resolved)
        return true;

    Serial0.printf("Resolvendo %s...\n", server.hostname);
    IPAddress ip;
    ip.fromString(server.ip);
    if (WiFi.hostByName(server.hostname, ip))
    {
        server.resolved = true;
        server.ip = ip.toString();
        Serial0.printf("Resolvido %s → %s\n", server.hostname, server.ip.c_str());
        return true;
    }

    Serial0.printf("Falha ao resolver %s\n", server.hostname);
    return false;
}

void NTPSync::saveTimeToPrefs()
{
    _prefs.begin("ntp", false);
    _prefs.putULong("lastSync", _timeval.lastSync);
    _prefs.putInt("utcOffset", _timeval.utc_offset);
    _prefs.end();
}

void NTPSync::loadTimeFromPrefs()
{
    _prefs.begin("ntp", true);
    _timeval.lastSync = _prefs.getULong("lastSync", 0);
    _timeval.utc_offset = _prefs.getInt("utcOffset", _timeval.utc_offset); // Default UTC-3
    _prefs.end();

    if (_timeval.lastSync > 0)
    {
        struct timeval tv = {.tv_sec = _timeval.lastSync};
        settimeofday(&tv, nullptr);
        Serial0.printf("Hora carregada das preferências: %s", ctime(&_timeval.lastSync));
    }
}

void NTPSync::updateDstStatus(time_t now)
{
    struct tm *tm = localtime(&now);
    // Regra simplificada para horário de verão no Brasil (Outubro a Fevereiro)
    _timeval.dst_active = (tm->tm_mon > 9 || tm->tm_mon < 2);
}

void NTPSync::startTask()
{
    xTaskCreatePinnedToCore(
        timeSyncTask,
        "TimeSyncTask",
        4096,
        nullptr,
        1,
        nullptr,
        0);
    Serial0.println("Tarefa de sincronização iniciada");
}

void timeSyncTask(void *param)
{
    while (true)
    {
        if (NTPSync::syncTime())
        {
            vTaskDelay(pdMS_TO_TICKS(NTPSync::_syncInterval));
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(NTPSync::_retryInterval));
        }
    }
}