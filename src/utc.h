#include <cstdint>
#include <string>
#include <unordered_map>

const std::unordered_map<std::string, int32_t> TIMEZONE_OFFSETS = {
    // Fusos Brasileiros
    {"America/Sao_Paulo", -3}, // Horário de Brasília (BRT)
    {"America/Recife", -3},    // Nordeste
    {"America/Bahia", -3},
    {"America/Fortaleza", -3},
    {"America/Belem", -3},
    {"America/Cuiaba", -4},      // Mato Grosso (AMT)
    {"America/Porto_Velho", -4}, // Rondônia
    {"America/Boa_Vista", -4},   // Roraima
    {"America/Manaus", -4},      // Amazonas (AMT)
    {"America/Rio_Branco", -5},  // Acre (ACT)
    {"America/Eirunepe", -5},    // Oeste do Amazonas

    // Fusos Internacionais
    {"UTC", 0},
    {"Europe/London", 0},        // GMT/BST
    {"Europe/Paris", 1},         // CET/CEST
    {"America/New_York", -5},    // EST/EDT
    {"America/Chicago", -6},     // CST/CDT
    {"America/Denver", -7},      // MST/MDT
    {"America/Los_Angeles", -8}, // PST/PDT
    {"Asia/Tokyo", 9},
    {"Australia/Sydney", 10}, // AEST/AEDT
    // Adicione outros conforme necessidade
};