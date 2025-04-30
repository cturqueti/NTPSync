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
        Serial.println("WiFi desconectado");
        _timeSyncked = false;
        return false;
    }

    // Resolve todos os servidores uma vez
    if (!resolveAllServers())
    {
        Serial.println("Falha ao resolver servidores NTP");
        return false;
    }

    // Ordena servidores por performance
    sortServersByPerformance();

    // Tenta cada servidor
    for (uint8_t attempt = 0; attempt < maxRetries; attempt++)
    {
        for (auto &server : _timeval.servers)
        {
            if (!server.resolved)
                continue;

            Serial.printf("Tentando %s...\n", server.hostname);
            if (queryNTP(server))
            {
                _timeSyncked = true;
                _timeval.lastSync = time(nullptr);
                saveTimeToPrefs();
                return true;
            }
        }
        delay(1000); // Espera entre tentativas
    }

    Serial.println("Todos os servidores falharam");
    _timeSyncked = false;
    return false;
}

void NTPSync::setTimeval(const char *timezone, int32_t utc_offset,
                         const char *ntpServer1, const char *ntpServer2,
                         const char *ntpServer3)
{
    _timeval.time_zone = timezone;
    _timeval.utc_offset = utc_offset;
    _timeval.servers.clear();

    if (ntpServer1)
        _timeval.servers.push_back({ntpServer1, INADDR_NONE, false, 1000, 0});
    if (ntpServer2)
        _timeval.servers.push_back({ntpServer2, INADDR_NONE, false, 1000, 0});
    if (ntpServer3)
        _timeval.servers.push_back({ntpServer3, INADDR_NONE, false, 1000, 0});

    updateDstStatus(time(nullptr));
}

bool NTPSync::queryNTP(NTPServer &server)
{
    WiFiUDP udp;
    if (!udp.begin(123))
    {
        Serial.println("Falha ao iniciar UDP");
        return false;
    }

    byte ntpPacket[48];
    memset(ntpPacket, 0, 48);
    ntpPacket[0] = 0x1B; // Modo cliente NTP

    uint32_t start = millis();
    if (!udp.beginPacket(server.ip, 123) || !udp.write(ntpPacket, 48) || !udp.endPacket())
    {
        Serial.printf("Falha no envio para %s\n", server.hostname);
        udp.stop();
        return false;
    }

    // Espera resposta com timeout
    bool success = false;
    time_t ntpTime = 0;

    while (millis() - start < 1000)
    {
        if (udp.parsePacket() >= 48)
        {
            udp.read(ntpPacket, 48);

            if (isValidNTPResponse(ntpPacket, 48))
            {
                server.lastResponseTime = millis() - start;
                server.stratum = ntpPacket[1]; // Nível de stratum do servidor

                // Extrai o timestamp (posições 40-43)
                uint32_t secsSince1900 = (uint32_t)ntpPacket[40] << 24 |
                                         (uint32_t)ntpPacket[41] << 16 |
                                         (uint32_t)ntpPacket[42] << 8 |
                                         (uint32_t)ntpPacket[43];
                ntpTime = secsSince1900 - 2208988800UL;

                // Aplica offset e horário de verão
                ntpTime += _timeval.utc_offset;
                if (_timeval.dst_active)
                    ntpTime += 3600;

                struct timeval tv = {.tv_sec = ntpTime};
                settimeofday(&tv, nullptr);
                success = true;
            }
            break;
        }
        delay(10);
    }

    udp.stop();

    if (success)
    {
        Serial.printf("Sincronizado com %s em %ums (stratum %u)\n",
                      server.hostname, server.lastResponseTime, server.stratum);
        Serial.printf("Hora atual: %s", ctime(&ntpTime));
        return true;
    }

    Serial.printf("Timeout com %s\n", server.hostname);
    return false;
}

bool NTPSync::isValidNTPResponse(const byte *packet, size_t length)
{
    if (length < 48)
        return false;
    // Verifica bits de modo e stratum
    return (packet[0] & 0x07) == 0x04 &&    // Modo servidor (4)
           packet[1] > 0 && packet[1] < 16; // Stratum válido (1-15)
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

    Serial.printf("Resolvendo %s...\n", server.hostname);
    if (WiFi.hostByName(server.hostname, server.ip))
    {
        server.resolved = true;
        Serial.printf("Resolvido %s → %s\n", server.hostname, server.ip.toString().c_str());
        return true;
    }

    Serial.printf("Falha ao resolver %s\n", server.hostname);
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
    _timeval.utc_offset = _prefs.getInt("utcOffset", -10800); // Default UTC-3
    _prefs.end();

    if (_timeval.lastSync > 0)
    {
        struct timeval tv = {.tv_sec = _timeval.lastSync};
        settimeofday(&tv, nullptr);
        Serial.printf("Hora carregada das preferências: %s", ctime(&_timeval.lastSync));
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
    Serial.println("Tarefa de sincronização iniciada");
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