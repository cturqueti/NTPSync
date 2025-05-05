#include "NTPSync.h"
#include <Arduino.h>
#include <memory>

Preferences NTPSync::_prefs;
NTPSync::Timeval NTPSync::_timeval;
bool NTPSync::_timeSyncked = false;
uint32_t NTPSync::_syncInterval = 3600000;
uint32_t NTPSync::_retryInterval = 300000;
std::mutex NTPSync::_mutex;
bool NTPSync::_logEnabled = true;
tm NTPSync::_timeinfo;

// ----------------------------------------------------
//
//               Funções Públicas
//
// ----------------------------------------------------

void NTPSync::logControl(bool enabled)
{
    _logEnabled = enabled;
}

/**
 * @brief Inicializa a classe NTPSync
 *
 * Carrega o estado da sincroniza o de tempo do Preferences e inicializa a
 * tarefa de sincroniza o de tempo.
 *
 * @param syncInterval INTERVALO DE TEMPO (em minutos) entre solicita es
 *                     de sincroniza o de tempo com o servidor NTP.
 * @param retryInterval INTERVALO DE TEMPO (em minutos) entre tentativas
 *                     de sincroniza o de tempo com o servidor NTP.
 */
void NTPSync::begin(uint32_t syncInterval, uint32_t retryInterval)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _syncInterval = syncInterval * MINUTES_TO_MS;
    _retryInterval = retryInterval * MINUTES_TO_MS;

    loadTimeFromPrefs();
    startTask();
}

/**
 * @brief Tenta sincronizar o tempo com os servidores NTP
 *
 * Primeiramente, resolve todos os servidores NTP configurados e ordena-os
 * por performance. Em seguida, tenta sincronizar o tempo com cada servidor
 * NTP, em ordem de performance. Caso uma tentativa falhe, aumenta o
 * intervalo entre tentativas com um backoff exponencial.
 *
 * @param maxRetries Número máximo de tentativas por servidor NTP
 *
 * @return true se o tempo for sincronizado, false caso contrário
 */
bool NTPSync::syncTime(uint8_t maxRetries)
{
    std::lock_guard<std::mutex> lock(_mutex);
    if (WiFi.status() != WL_CONNECTED)
    {
        if (_logEnabled)
        {
            Serial0.printf("[NTP Sync] WiFi desconectado\n");
        }
        _timeSyncked = false;
        return false;
    }

    // Resolve todos os servidores uma vez
    if (!resolveAllServers())
    {
        if (_logEnabled)
        {
            Serial0.printf("Falha ao resolver servidores NTP\n");
        }
        return false;
    }

    // Ordena servidores por performance
    sortServersByPerformance();

    if (_logEnabled)
    {
        Serial0.printf("Iniciando sincronização\n");
    }

    for (auto &server : _timeval.servers)
    {
        if (!server.resolved)
            continue;
        for (uint8_t attempt = 0; attempt < maxRetries; attempt++)
        {
            if (_logEnabled)
            {
                Serial0.printf("Attempt %d with server: %s (%s)\n", attempt + 1, server.hostname.c_str(), server.ip.c_str());
            }

            if (syncWithServer(server))
            {
                _timeSyncked = true;
                _timeval.lastSync = time(nullptr);
                server.failureCount = 0;
                saveTimeToPrefs();

                return true;
            }
            server.failureCount++;
            uint32_t delayMs = getExponentialBackoffDelay(server.failureCount);
            delay(delayMs);
        }
    }

    if (_logEnabled)
    {
        Serial0.printf("Todos os servidores falharam\n");
    }
    _timeSyncked = false;

    return false;
}

/**
 * @brief Verifica se o tempo foi sincronizado com sucesso
 *
 * @return true se o tempo estiver sincronizado, false caso contrário
 */
bool NTPSync::isTimeSynced()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _timeSyncked;
}

bool NTPSync::hasTimeval()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return isTimeSynced() || _timeval.lastSync > 0;
}

time_t NTPSync::getLastTimeSync()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _timeval.lastSync;
}

/**
 * @brief Define o fuso horário e servidores NTP para sincronização de tempo
 *
 * @param timezone Fuso horário do local (ex. "America/Sao_Paulo")
 * @param ntpServers Vetor de servidores NTP para sincroniza o de tempo
 *                   (ex. {"pool.ntp.org", "a.st1.ntp.br", "ntp.cais.rnp.br"})
 */
void NTPSync::setTimeval(const char *timezone, const std::vector<std::string> &ntpServers)
{
    std::lock_guard<std::mutex> lock(_mutex);

    _timeval.time_zone = timezone ? String(timezone) : "";
    auto it = TIMEZONE_OFFSETS.find(std::string(timezone));
    _timeval.utc_offset = 0;
    if (it != TIMEZONE_OFFSETS.end())
    {
        _timeval.utc_offset = (it->second) * 3600;
    }

    _timeval.servers.clear();

    for (const auto &server : ntpServers)
    {
        _timeval.servers.push_back({
            String(server.c_str()), // hostname
            "",                     // ip
            false,                  // resolved
            1000,                   // lastResponseTime
            0,                      // stratum
            0                       // failureCount
        });
    }
}

/**
 * @brief Define os intervalos de sincroniza o e retry para a classe NTPSync
 *
 * @param syncInterval INTERVALO DE TEMPO (em minutos) entre solicita es
 *                     de sincroniza o de tempo com o servidor NTP.
 * @param retryInterval INTERVALO DE TEMPO (em minutos) entre tentativas
 *                     de sincroniza o de tempo com o servidor NTP.
 */
void NTPSync::setSyncIntervals(uint32_t syncInterval, uint32_t retryInterval)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _syncInterval = syncInterval * MINUTES_TO_MS;
    _retryInterval = retryInterval * MINUTES_TO_MS;
}

// ----------------------------------------------------
//
//               Funções Privadas
//
// ----------------------------------------------------

/**
 * @brief Ordena os servidores NTP por performance
 *
 * @details
 *     Essa função ordena os servidores NTP por performance,
 *     priorizando servidores resolvidos e com menor tempo de resposta.
 */
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

/**
 * @brief Resolve todos os servidores NTP configurados
 *
 * @details
 *     Essa função resolve todos os servidores NTP configurados
 *     e retorna true se todos os servidores forem resolvidos
 *     com sucesso.
 *
 * @return true se todos os servidores forem resolvidos com sucesso,
 *         false caso contrário
 */
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

/**
 * @brief Resolve o endereço IP de um servidor NTP a partir de seu hostname.
 *
 * @param server Referência para o objeto NTPServer que contém o hostname a ser resolvido.
 *
 * @return true se o endereço IP for resolvido com sucesso e atualizado no servidor,
 *         false caso contrário.
 */
bool NTPSync::resolveServer(NTPServer &server)
{
    if (server.resolved)
        return true;

    if (_logEnabled)
    {
        Serial0.printf("Resolvendo %s...\n", server.hostname.c_str());
    }

    IPAddress ip;
    ip.fromString(server.ip);
    if (WiFi.hostByName(server.hostname.c_str(), ip))
    {
        server.resolved = true;
        server.ip = ip.toString();
        if (_logEnabled)
        {
            Serial0.printf("Resolvido %s → %s\n", server.hostname.c_str(), server.ip.c_str());
        }
        return true;
    }

    if (_logEnabled)
    {
        Serial0.printf("Falha ao resolver %s\n", server.hostname.c_str());
    }
    return false;
}

/**
 * @brief Salva o estado da sincroniza o de tempo no Preferences.
 *
 * O Preferences é um sistema de armazenamento de dados no ESP32 que
 * permite armazenar pequenas quantidades de dados persistente no
 * dispositivo.
 *
 * Este método salva o timestamp da última sincronização bem como o
 * fuso horário no Preferences.
 */
void NTPSync::saveTimeToPrefs()
{
    _prefs.begin("ntp", false);
    _prefs.putULong("lastSync", _timeval.lastSync);
    _prefs.putInt("utcOffset", _timeval.utc_offset);
    _prefs.end();
}

/**
 * @brief Carrega o estado da sincroniza o de tempo do Preferences.
 *
 * Carrega o timestamp da última sincroniza o bem como o fuso horário
 * do Preferences e seta o tempo do sistema. Caso não haja um valor
 * salvo, deixa o tempo do sistema como está e não altera o fuso
 * horário.
 *
 * @return true se o estado foi carregado com sucesso, false caso
 *         contrário.
 */
void NTPSync::loadTimeFromPrefs()
{
    _prefs.begin("ntp", true);
    _timeval.lastSync = _prefs.getULong("lastSync", 0);
    _timeval.utc_offset = _prefs.getInt("utcOffset", _timeval.utc_offset); // Default UTC-3
    _prefs.end();

    if (_timeval.lastSync > 0)
    {
        time_t localTime = _timeval.lastSync + _timeval.utc_offset;

        struct timeval tv = {
            .tv_sec = localTime, // Já com o offset aplicado
            .tv_usec = 0};

        settimeofday(&tv, nullptr);

        if (_logEnabled)
        {
            Serial0.printf("Hora carregada das preferências: %s\n", ctime(&_timeval.lastSync));
        }
    }
}

/**
 * @brief Atualiza o estado do horário de ver o no Brasil.
 *
 * A regra simplificada para horário de verão no Brasil é a seguinte:
 * - Outubro a Fevereiro: horário de verão está ativo.
 * - Março a Setembro: horário de verão está inativo.
 *
 * @param now Timestamp atual.
 */
void NTPSync::updateDstStatus(time_t now)
{
    struct tm *tm = localtime(&now);
    // Regra simplificada para horário de verão no Brasil (Outubro a Fevereiro)
    _timeval.dst_active = (tm->tm_mon > 9 || tm->tm_mon < 2);
}

/**
 * @brief Inicia a tarefa de sincronização de tempo.
 *
 * Essa tarefa executa a função timeSyncTask, que verifica se o horário
 * está sincronizado e, caso não está, tenta sincronizar o horário
 * com o servidor NTP.
 */
void NTPSync::startTask()
{
    xTaskCreatePinnedToCore(
        timeSyncTaskNTP,
        "TimeSyncTaskNTP",
        4096,
        nullptr,
        1,
        nullptr,
        0);
    if (_logEnabled)
    {
        Serial0.printf("Tarefa de sincronização iniciada\n");
    }
}

/**
 * @brief Sincroniza o horário com um servidor NTP.
 *
 * @details
 *     Essa função configura o horário com o offset de fuso horário
 *     e ip do servidor NTP e, em seguida, tenta obter o horário
 *     atual do servidor NTP com o timeout de 10 segundos.
 *
 *     Se o horário for obtido com sucesso, o horário atual é
 *     atualizado e o tempo de resposta do servidor NTP é
 *     gravado.
 *
 * @param server Referência para o objeto NTPServer que contém
 *               o ip do servidor NTP.
 *
 * @return true se o horário for sincronizado com sucesso,
 *         false caso contrário.
 */
bool NTPSync::syncWithServer(NTPServer &server)
{
    configTime(_timeval.utc_offset, 0, server.ip.c_str());

    if (!getLocalTime(&_timeinfo, 10000))
    { // Timeout de 10 segundos
        if (_logEnabled)
        {
            Serial0.printf("Failed to get time from NTP\n");
        }
        return false;
    }

    time_t now = time(nullptr);
    server.lastResponseTime = millis() - (now - _timeval.lastSync) * 1000;

    if (_logEnabled)
    {
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%d/%m/%Y %H:%M:%S", &_timeinfo);
        Serial0.printf("Time synchronized successfully: %s\n", timeStr);
    }

    return true;
}

/**
 * @brief Calcula o tempo de atraso com base em um backoff exponencial.
 *
 * @param failureCount Número de falhas consecutivas na tentativa de sincronização.
 *
 * @return O tempo de atraso calculado em milissegundos, limitado por um valor máximo.
 */
time_t NTPSync::getExponentialBackoffDelay(uint32_t failureCount)
{
    const uint32_t baseDelay = 1000; // 1 segundo base
    const uint32_t maxDelay = 60000; // 1 minuto máximo
    uint32_t delay = baseDelay * (1 << (failureCount - 1));
    return min(delay, maxDelay);
}

void timeSyncTaskNTP(void *param)
{
    while (true)
    {
        bool success = false;
        success = NTPSync::syncTime();

        uint32_t delayTime = success ? NTPSync::_syncInterval : NTPSync::_retryInterval;
        vTaskDelay(pdMS_TO_TICKS(delayTime));
    }
}