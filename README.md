# NTPSync
## Biblioteca de Sincronização de Tempo para ESP32

![PlatformIO](https://img.shields.io/badge/PlatformIO-Compatible-orange?style=plastic&logo=platformio)  
![Licença](https://img.shields.io/badge/licen%C3%A7a-Apache%202.0-blue.svg?style=plastic&logo=apache)  
![Versão](https://img.shields.io/badge/Vers%C3%A3o-1.0.0-green.svg?style=plastic&logo=github)  

## 📋 Recursos:  
✅ Sincronização automática de tempo via NTP  
🌐 Suporte a múltiplos servidores NTP com fallback automático  
⏱ Armazenamento persistente do último horário sincronizado  
🔄 Tarefa em background para sincronização periódica  
📡 Suporte a fusos horários e horário de verão  
🔒 Thread-safe com mutex para operações concorrentes  
📊 Logs detalhados para diagnóstico  
 
## 📦 Instalação:  
### Via PlatformIO (recomendado)
Adicione no seu platformio.ini:

```ini
lib_deps = 
    https://github.com/seu-usuario/NTPSync.git
```
### Via Arduino IDE:  
Baixe o último release

Extraia para ~/Arduino/libraries/NTPSync

Reinicie a Arduino IDE

## 🚀 Uso Básico:
```cpp
#include <NTPSync.h>
#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    WiFi.begin("SSID", "senha");
    
    // Configura com fuso horário e servidores NTP
    NTPSync::setTimeval("America/Sao_Paulo", {"pool.ntp.org", "br.pool.ntp.org"});
    
    // Inicia com sincronização a cada 30 min e retentativas a cada 5 min
    NTPSync::begin(30, 5);
}

void loop() {
    if (NTPSync::isTimeSynced()) {
        time_t now = NTPSync::getLastTimeSync();
        Serial.printf("Horário atual: %s", ctime(&now));
    }
    delay(1000);
}
```
## ⚙️ Configuração Avançada:
### Fusos Horários Suportados
```cpp
// Exemplos de fusos válidos:
NTPSync::setTimeval("America/Sao_Paulo", {"pool.ntp.org"});  // UTC-3
NTPSync::setTimeval("America/New_York", {"pool.ntp.org"});   // UTC-5
NTPSync::setTimeval("Europe/London", {"pool.ntp.org"});       // UTC+0/+1
```
### Intervalos Personalizados
```cpp
// Sincroniza a cada 1 hora, retentativas a cada 10 minutos
NTPSync::setSyncIntervals(60, 10);
```
### Controle de Logs
```cpp
NTPSync::logControl(false);  // Desativa logs
```

## 📝 Exemplo Completo:
```cpp
#include <NTPSync.h>
#include <WiFi.h>

void printLocalTime() {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)) {
        Serial.println("Falha ao obter horário");
        return;
    }
    Serial.println(&timeinfo, "%d/%m/%Y %H:%M:%S");
}

void setup() {
    Serial.begin(115200);
    WiFi.begin("SSID", "senha");

    // Configuração inicial
    NTPSync::setTimeval("America/Sao_Paulo", {
        "pool.ntp.org",
        "a.st1.ntp.br",
        "b.st1.ntp.br"
    });

    // Inicia com sincronização a cada 30 minutos
    NTPSync::begin(30);

    // Força sincronização imediata
    if(NTPSync::syncTime()) {
        Serial.println("Sincronizado com sucesso!");
        printLocalTime();
    }
}

void loop() {
    static time_t lastPrint = 0;
    if(millis() - lastPrint > 10000) {
        printLocalTime();
        lastPrint = millis();
    }
}
```

## 🌍 Compatibilidade:  
|Plataforma	|Testado em|
|---|---|
|ESP32	|ESP32-S3, ESP32-C3|
|ESP8266	|NodeMCU 1.0|



## 🤝 Contribuição:  
Contribuições são bem-vindas! Por favor:

Faça um fork do projeto

Crie uma branch (git checkout -b feature/nova-funcionalidade)

Commit suas mudanças (git commit -m 'Adiciona nova funcionalidade')

Push para a branch (git push origin feature/nova-funcionalidade)

Abra um Pull Request

## 📜 Licença
Copyright 2025 cturqueti

Licenciado sob a Apache License, Versão 2.0 (a "Licença");
você não pode usar este arquivo exceto em conformidade com a Licença.
Você pode obter uma cópia da Licença em:

http://www.apache.org/licenses/LICENSE-2.0

A menos que exigido por lei aplicável ou acordado por escrito, o software
distribuído sob a Licença é distribuído "COMO ESTÁ",
SEM GARANTIAS OU CONDIÇÕES DE QUALQUER TIPO, expressas ou implícitas.
Consulte a Licença para o idioma específico que rege as permissões e
limitações sob a Licença.

Consulte o arquivo [LICENSE](LICENSE) para o texto completo da licença e
[NOTICE](NOTICE) para informações sobre atribuições e histórico de modificações.


## 📝 Dicas:
Para melhor precisão, use pelo menos 3 servidores NTP

Em ambientes sem RTC, o horário será mantido por ~48h após desligamento

Use NTPSync::hasTimeval() para verificar se há algum horário válido armazenado