#pragma once
/**
 * @file wserial.h
 * @brief Classe de comunicação serial/UDP. Usa Serial quando não há link UDP ativo.
 * Uso: wserial.setup(); wserial.loop(); wserial.plot("var", valor);
 */
#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>

#define WSERIAL_NEWLINE "\r\n"

class WSerial {
private:
    IPAddress   _udpTargetIP;
    uint16_t    _udpTargetPort  = 0;
    uint16_t    _listenPort     = 0;
    bool        _udpAvailable   = false;
    bool        _udpLinked      = false;
    uint32_t    _base_ms        = 0;
    AsyncUDP    _udp;
    std::function<void(std::string)> _onInput;

    void _send(const String &txt) {
        if (_udpLinked) {
            _udp.writeTo(
                reinterpret_cast<const uint8_t*>(txt.c_str()),
                txt.length(), _udpTargetIP, _udpTargetPort);
        } else {
            Serial.print(txt);
        }
    }

    bool _parseHostPort(const String &s, String &cmd, String &host, uint16_t &port) {
        int c1 = s.indexOf(':');
        int c2 = s.lastIndexOf(':');
        if (c1 <= 0 || c2 <= c1) return false;
        cmd  = s.substring(0, c1);
        host = s.substring(c1 + 1, c2);
        long v = s.substring(c2 + 1).toInt();
        if (v <= 0 || v > 65535) return false;
        port = (uint16_t)v;
        return true;
    }

    void _handlePacket(AsyncUDPPacket packet) {
        String s((const char*)packet.data(), packet.length());
        s.trim();
        String cmd, host;
        uint16_t port;
        if (!_parseHostPort(s, cmd, host, port)) {
            if (_onInput) _onInput(std::string(s.c_str()));
            return;
        }
        IPAddress ip;
        if (!ip.fromString(host)) {
            if (WiFi.hostByName(host.c_str(), ip) != 1) return;
        }
        if (ip == IPAddress()) return;
        _udpTargetIP   = ip;
        _udpTargetPort = port;
        if (cmd == "CONNECT") {
            _udpLinked = true;
            Serial.println("[WSerial] Chaveando para UDP -> " + _udpTargetIP.toString() + ":" + String(_udpTargetPort));
            _send("CONNECT:" + WiFi.localIP().toString() + ":" + String(_udpTargetPort) + "\n");
        } else if (cmd == "DISCONNECT" && _udpLinked) {
            _send("DISCONNECT:" + WiFi.localIP().toString() + ":" + String(_udpTargetPort) + "\n");
            _udpLinked = false;
            Serial.println("[WSerial] Chaveando de volta para Serial");
        }
    }

    void _startListen() {
        if (_udp.listen(_listenPort)) {
            _udpAvailable = true;
            _udp.onPacket([this](AsyncUDPPacket pkt){ _handlePacket(pkt); });
        }
    }

public:
    /**
     * @brief Inicializa a Serial e tenta abrir o socket UDP (requer WiFi já iniciado).
     * @param baudrate  Velocidade da Serial (padrão 115200).
     * @param port      Porta UDP de escuta (padrão 47268). Passa 0 para desabilitar UDP.
     */
    void begin(unsigned long baudrate = 115200, uint16_t port = 47268) {
        Serial.begin(baudrate);
        _listenPort = port;
        if (_listenPort != 0 && WiFi.status() == WL_CONNECTED) {
            _startListen();
        }
    }

    /**
     * @brief Deve ser chamado no loop(). Processa Serial e retenta UDP se necessário.
     */
    void update() {
        // Retenta UDP quando WiFi conectar
        if (_listenPort != 0 && !_udpAvailable && WiFi.status() == WL_CONNECTED) {
            static uint32_t lastRetry = 0;
            if (millis() - lastRetry > 2000) {
                lastRetry = millis();
                _startListen();
            }
        }
        if (Serial.available()) {
            String linha = Serial.readStringUntil('\n');
            if (_onInput) _onInput(linha.c_str());
        }
    }

    /** @brief Registra callback chamado ao receber dados. */
    void onInputReceived(std::function<void(std::string)> cb) { _onInput = cb; }

    // === plot com timestamp explícito ===
    template <typename T>
    void plot(const char *varName, TickType_t x, T y, const char *unit = nullptr) {
        String str(">");
        str += varName; str += ":";
        uint32_t ts_ms = (uint32_t)x;
        if (ts_ms < 100000) ts_ms = millis();
        str += String(ts_ms); str += ":"; str += String(y);
        if (unit && unit[0]) { str += "\xC2\xA7"; str += unit; }
        str += WSERIAL_NEWLINE;
        _send(str);
    }

    // === plot simples (timestamp automático) ===
    template <typename T>
    void plot(const char *varName, T y, const char *unit = nullptr) {
        plot(varName, (TickType_t)xTaskGetTickCount(), y, unit);
    }

    // === plot de array com dt fixo ===
    template <typename T>
    void plot(const char *varName, uint32_t dt_ms, const T* y, size_t ylen, const char *unit = nullptr) {
        String str(">");
        str += varName; str += ":";
        for (size_t i = 0; i < ylen; i++) {
            str += String((uint32_t)_base_ms); str += ":";
            str += String((double)y[i], 6);
            _base_ms += dt_ms;
            if (i < ylen - 1) str += ";";
        }
        if (unit) { str += "\xC2\xA7"; str += unit; }
        str += WSERIAL_NEWLINE;
        _send(str);
    }

    void log(const char *text, uint32_t ts_ms = 0) {
        if (ts_ms == 0) ts_ms = millis();
        _send(String(ts_ms) + ":" + String(text ? text : "") + WSERIAL_NEWLINE);
    }

    template <typename T>
    void println(const T &data) { _send(String(data) + WSERIAL_NEWLINE); }

    // std::string não possui conversão implícita para Arduino String.
    void println(const std::string &data) {
        _send(String(data.c_str()) + WSERIAL_NEWLINE);
    }
    void println() { _send(WSERIAL_NEWLINE); }

    template <typename T>
    void print(const T &data) { _send(String(data)); }
};

inline WSerial wserial; ///< Instância global de comunicação serial/UDP.
