#include "CydifyNetwork.h"

#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <time.h>

#include "HardwareConfig.h"

namespace {

DNSServer dnsServer;
WebServer webServer(80);
Preferences preferences;

bool portalActive = false;
bool webServerStarted = false;
bool routesConfigured = false;
uint32_t restartAt = 0;
char statusBuffer[48] = "WIFI OFFLINE";
String serverAddress;
bool clockConfigured = false;
CydifyNetwork::BrightnessGetter brightnessGetter = nullptr;
CydifyNetwork::BrightnessSetter brightnessSetter = nullptr;
bool otaStarted = false;
bool otaSucceeded = false;
size_t otaBytesWritten = 0;

void configureClock() {
  if (clockConfigured || WiFi.status() != WL_CONNECTED) return;
  // Horario de Brasilia (UTC-3). A sincronizacao NTP ocorre em segundo plano.
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  clockConfigured = true;
  Serial.println("Sincronizacao do relogio iniciada.");
}

String escapeHtml(const String& source) {
  String escaped;
  escaped.reserve(source.length() + 8);

  for (size_t i = 0; i < source.length(); i++) {
    const char value = source.charAt(i);
    if (value == '&') escaped += F("&amp;");
    else if (value == '<') escaped += F("&lt;");
    else if (value == '>') escaped += F("&gt;");
    else if (value == '\"') escaped += F("&quot;");
    else escaped += value;
  }

  return escaped;
}

String pageHeader(const char* title) {
  String html;
  html.reserve(5200);
  html += F("<!doctype html><html lang='pt-BR'><head>");
  html += F("<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>CYDify</title><style>");
  html += F("*{box-sizing:border-box}body{margin:0;background:#090b0a;color:#fff;font-family:Arial,sans-serif}");
  html += F("main{max-width:520px;margin:0 auto;padding:28px 18px}h1{color:#1ed760;margin:0 0 4px}");
  html += F("h2{font-size:20px;margin:26px 0 12px}.card{background:#171a18;border:1px solid #292d2a;border-radius:16px;padding:18px}");
  html += F("label{display:block;margin:14px 0 6px;color:#c8cbc9}select,input{width:100%;padding:13px;border-radius:10px;border:1px solid #3b403d;background:#222622;color:#fff}");
  html += F("button,.button{display:block;width:100%;margin-top:18px;padding:14px;border:0;border-radius:24px;background:#1ed760;color:#071009;font-weight:700;font-size:16px;text-align:center;text-decoration:none}");
  html += F(".secondary{background:#292d2a;color:#fff}.danger{background:#7f1d1d;color:#fff}.grid{display:grid;grid-template-columns:1fr 1fr;gap:12px}.metric{padding:12px;background:#101311;border-radius:12px}.metric strong{display:block;color:#fff;font-size:18px;margin-top:4px}");
  html += F("small,p{color:#a7aaa8;line-height:1.45}.pill{display:inline-block;padding:6px 10px;border-radius:12px;background:#222622;color:#1ed760}.ok{color:#1ed760}.warn{color:#f7b84b}</style></head><body><main>");
  html += F("<h1>CYDify</h1><small>");
  html += HardwareConfig::APP_VERSION;
  html += F("</small><h2>");
  html += title;
  html += F("</h2>");
  return html;
}

String dashboardPage() {
  String html = pageHeader("Painel do dispositivo");
  html += F("<div class='card'><div class='grid'>");
  html += F("<div class='metric'><small>Wi-Fi</small><strong>");
  if (WiFi.status() == WL_CONNECTED) html += F("Conectado");
  else html += F("Offline");
  html += F("</strong></div><div class='metric'><small>IP do CYD</small><strong>");
  if (WiFi.status() == WL_CONNECTED) html += WiFi.localIP().toString();
  else html += WiFi.softAPIP().toString();
  html += F("</strong></div><div class='metric'><small>Heap livre</small><strong>");
  html += String(ESP.getFreeHeap() / 1024);
  html += F(" KB</strong></div><div class='metric'><small>Brilho</small><strong>");
  html += String(brightnessGetter == nullptr ? 85 : brightnessGetter());
  html += F("%</strong></div><div class='metric'><small>Ligado ha</small><strong>");
  html += String(millis() / 60000);
  html += F(" min</strong></div></div>");
  html += F("<p>Servidor configurado:<br><span class='pill'>");
  if (serverAddress.length()) html += escapeHtml(serverAddress);
  else html += F("nao configurado");
  html += F("</span></p></div>");

  html += F("<h2>Tela</h2><div class='card'>");
  html += F("<form method='post' action='/brightness'><label for='brightness'>Brilho entre 5% e 100%</label>");
  html += F("<input id='brightness' name='brightness' type='range' min='5' max='100' value='");
  html += String(brightnessGetter == nullptr ? 85 : brightnessGetter());
  html += F("' oninput='brightnessValue.textContent=this.value+\"%\"'>");
  html += F("<p id='brightnessValue' class='ok'>");
  html += String(brightnessGetter == nullptr ? 85 : brightnessGetter());
  html += F("%</p><button type='submit'>Aplicar brilho</button></form></div>");

  html += F("<h2>Conexao</h2><div class='card'>");
  html += F("<p>Altere a rede Wi-Fi ou o endereco do servidor sem recompilar.</p>");
  html += F("<a class='button secondary' href='/wifi'>Configurar Wi-Fi e servidor</a></div>");

  html += F("<h2>Atualizacao OTA</h2><div class='card'>");
  html += F("<p>Envie o arquivo <code>.bin</code> exportado pela Arduino IDE. Nao desligue o CYD durante o envio.</p>");
  html += F("<form method='post' action='/update' enctype='multipart/form-data'>");
  html += F("<label for='firmware'>Firmware CYDify</label><input id='firmware' name='firmware' type='file' accept='.bin,application/octet-stream' required>");
  html += F("<button type='submit'>Instalar firmware</button></form></div>");

  html += F("<h2>Sistema</h2><div class='card'>");
  html += F("<form method='post' action='/restart'><button class='danger' type='submit'>Reiniciar CYDify</button></form></div>");
  html += F("</main></body></html>");
  return html;
}

String wifiFormPage() {
  String html = pageHeader("Configuracao Wi-Fi");
  html += F("<div class='card'><p>Escolha sua rede e informe a senha. As credenciais ficam salvas somente no ESP32.</p>");
  html += F("<form method='post' action='/save'><label for='ssid'>Rede Wi-Fi</label><select id='ssid' name='ssid' required>");

  const int networkCount = WiFi.scanNetworks();
  if (networkCount <= 0) {
    html += F("<option value=''>Nenhuma rede encontrada</option>");
  } else {
    for (int i = 0; i < networkCount; i++) {
      const String ssid = escapeHtml(WiFi.SSID(i));
      html += F("<option value=\"");
      html += ssid;
      if (ssid == WiFi.SSID()) html += F("\" selected>");
      else html += F("\">");
      html += ssid;
      html += F(" (");
      html += WiFi.RSSI(i);
      html += F(" dBm)</option>");
    }
  }
  WiFi.scanDelete();

  html += F("</select><label for='password'>Senha</label>");
  html += F("<input id='password' name='password' type='password' maxlength='63' autocomplete='current-password' placeholder='Deixe vazio para manter a senha atual'>");
  html += F("<label for='server'>Endereco do CYDify Server</label>");
  html += F("<input id='server' name='server' type='url' maxlength='96' required placeholder='http://192.168.1.10:8000' value=\"");
  html += escapeHtml(serverAddress);
  html += F("\">");
  html += F("<button type='submit'>Salvar e conectar</button></form></div>");
  if (!portalActive) {
    html += F("<a class='button secondary' href='/'>Voltar ao painel</a>");
  }

  if (WiFi.status() == WL_CONNECTED) {
    html += F("<p>Conectado a <span class='pill'>");
    html += escapeHtml(WiFi.SSID());
    html += F("</span><br>IP local: ");
    html += WiFi.localIP().toString();
    html += F("</p>");
  }

  html += F("</main></body></html>");
  return html;
}

void redirectToPortal() {
  webServer.sendHeader("Location", "http://192.168.4.1/", true);
  webServer.send(302, "text/plain", "");
}

void handleUpdateUpload() {
  HTTPUpload& upload = webServer.upload();

  if (upload.status == UPLOAD_FILE_START) {
    otaStarted = true;
    otaSucceeded = false;
    otaBytesWritten = 0;
    Serial.printf("OTA iniciado: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
      Update.printError(Serial);
      return;
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!otaStarted || Update.hasError()) return;
    const size_t written = Update.write(upload.buf, upload.currentSize);
    otaBytesWritten += written;
    if (written != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (!Update.hasError() && Update.end(true)) {
      otaSucceeded = true;
      Serial.printf(
          "OTA concluido: %u bytes\n",
          static_cast<unsigned int>(otaBytesWritten));
    } else {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaStarted = false;
    otaSucceeded = false;
    Serial.println("OTA cancelado.");
  }
}

void configureRoutes() {
  if (routesConfigured) return;
  routesConfigured = true;

  webServer.on("/", HTTP_GET, []() {
    webServer.send(
        200,
        "text/html; charset=utf-8",
        portalActive ? wifiFormPage() : dashboardPage());
  });

  webServer.on("/wifi", HTTP_GET, []() {
    webServer.send(200, "text/html; charset=utf-8", wifiFormPage());
  });

  webServer.on("/brightness", HTTP_POST, []() {
    if (!webServer.hasArg("brightness") || brightnessSetter == nullptr) {
      webServer.send(400, "text/plain; charset=utf-8", "Brilho invalido.");
      return;
    }

    const int value = webServer.arg("brightness").toInt();
    if (value < 5 || value > 100) {
      webServer.send(400, "text/plain; charset=utf-8", "Brilho invalido.");
      return;
    }

    brightnessSetter(static_cast<uint8_t>(value));
    webServer.sendHeader("Location", "/", true);
    webServer.send(303, "text/plain", "");
  });

  webServer.on("/restart", HTTP_POST, []() {
    String html = pageHeader("Reiniciando");
    html += F("<div class='card'><p>O CYDify sera reiniciado em instantes.</p></div></main></body></html>");
    webServer.send(200, "text/html; charset=utf-8", html);
    restartAt = millis() + 1200;
  });

  webServer.on("/api/device", HTTP_GET, []() {
    String json;
    json.reserve(360);
    json += F("{\"ok\":true,\"version\":\"");
    json += HardwareConfig::APP_VERSION;
    json += F("\",\"wifi_connected\":");
    json += WiFi.status() == WL_CONNECTED ? F("true") : F("false");
    json += F(",\"ip\":\"");
    if (WiFi.status() == WL_CONNECTED) json += WiFi.localIP().toString();
    else json += WiFi.softAPIP().toString();
    json += F("\",\"heap_free\":");
    json += String(ESP.getFreeHeap());
    json += F(",\"uptime_ms\":");
    json += String(millis());
    json += F(",\"brightness\":");
    json += String(brightnessGetter == nullptr ? 85 : brightnessGetter());
    json += F(",\"server_url\":\"");
    json += serverAddress;
    json += F("\"}");
    webServer.send(200, "application/json; charset=utf-8", json);
  });

  webServer.on("/save", HTTP_POST, []() {
    const String ssid = webServer.arg("ssid");
    String password = webServer.arg("password");
    String server = webServer.arg("server");
    server.trim();
    while (server.endsWith("/")) server.remove(server.length() - 1);

    if (ssid.length() == 0 || ssid.length() > 32 || password.length() > 63 ||
        server.length() < 10 || server.length() > 96 ||
        !server.startsWith("http://")) {
      webServer.send(400, "text/plain; charset=utf-8", "Dados Wi-Fi invalidos.");
      return;
    }

    preferences.begin("cydify", false);
    const String savedSsid = preferences.getString("wifi_ssid", "");
    if (password.length() == 0 && ssid == savedSsid) {
      password = preferences.getString("wifi_pass", "");
    }
    preferences.putString("wifi_ssid", ssid);
    preferences.putString("wifi_pass", password);
    preferences.putString("server_url", server);
    preferences.end();
    serverAddress = server;

    String html = pageHeader("Configuracao salva");
    html += F("<div class='card'><p>O CYDify sera reiniciado e tentara conectar a rede <strong>");
    html += escapeHtml(ssid);
    html += F("</strong>.</p><p>Pode fechar esta pagina e acompanhar o estado na tela.</p></div></main></body></html>");
    webServer.send(200, "text/html; charset=utf-8", html);
    restartAt = millis() + 1800;
  });

  webServer.on(
      "/update",
      HTTP_POST,
      []() {
        String html = pageHeader(
            otaSucceeded ? "Atualizacao concluida" : "Falha na atualizacao");
        if (otaSucceeded) {
          html += F("<div class='card'><p class='ok'>Firmware instalado com sucesso.</p><p>O CYDify sera reiniciado automaticamente.</p></div>");
          restartAt = millis() + 1800;
        } else {
          html += F("<div class='card'><p class='warn'>Nao foi possivel instalar o firmware.</p><p>Confirme se o arquivo selecionado e um binario valido para ESP32.</p><a class='button secondary' href='/'>Voltar</a></div>");
        }
        html += F("</main></body></html>");
        webServer.send(
            otaSucceeded ? 200 : 500,
            "text/html; charset=utf-8",
            html);
        otaStarted = false;
      },
      handleUpdateUpload);

  webServer.on("/generate_204", HTTP_ANY, redirectToPortal);
  webServer.on("/hotspot-detect.html", HTTP_ANY, redirectToPortal);
  webServer.on("/connecttest.txt", HTTP_ANY, redirectToPortal);
  webServer.onNotFound([]() {
    if (portalActive) {
      redirectToPortal();
    } else {
      webServer.send(404, "text/plain; charset=utf-8", "Pagina nao encontrada.");
    }
  });
}

void startWebServer() {
  configureRoutes();
  if (webServerStarted) return;
  webServer.begin();
  webServerStarted = true;
}

bool connectSavedWifi() {
  preferences.begin("cydify", true);
  const String ssid = preferences.getString("wifi_ssid", "");
  const String password = preferences.getString("wifi_pass", "");
  preferences.end();

  if (ssid.length() == 0) return false;

  Serial.printf("Conectando ao Wi-Fi %s...\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED &&
         millis() - startedAt < HardwareConfig::WIFI_CONNECT_TIMEOUT_MS) {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Nao foi possivel conectar. Abrindo portal de configuracao.");
    return false;
  }

  Serial.printf("Wi-Fi conectado. IP: %s\n", WiFi.localIP().toString().c_str());
  configureClock();
  startWebServer();
  return true;
}

}  // namespace

namespace CydifyNetwork {

void setBrightnessHandlers(
    BrightnessGetter getter,
    BrightnessSetter setter) {
  brightnessGetter = getter;
  brightnessSetter = setter;
}

void begin() {
  preferences.begin("cydify", true);
  serverAddress = preferences.getString("server_url", "");
  preferences.end();

  if (!connectSavedWifi()) {
    startConfigPortal();
  }
}

void update() {
  configureClock();
  if (portalActive) dnsServer.processNextRequest();
  if (webServerStarted) webServer.handleClient();

  if (restartAt != 0 && static_cast<int32_t>(millis() - restartAt) >= 0) {
    ESP.restart();
  }
}

void startConfigPortal() {
  if (portalActive) return;

  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(HardwareConfig::WIFI_AP_NAME)) {
    Serial.println("ERRO: nao foi possivel iniciar o portal Wi-Fi.");
    return;
  }

  delay(100);
  dnsServer.start(53, "*", WiFi.softAPIP());
  startWebServer();
  portalActive = true;

  Serial.printf(
      "Portal ativo: conecte em %s e abra http://%s/\n",
      HardwareConfig::WIFI_AP_NAME,
      WiFi.softAPIP().toString().c_str());
}

bool isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool isPortalActive() {
  return portalActive;
}

bool isUpdating() {
  return otaStarted;
}

const char* statusText() {
  if (portalActive) {
    snprintf(statusBuffer, sizeof(statusBuffer), "WIFI SETUP");
  } else if (isConnected()) {
    snprintf(statusBuffer, sizeof(statusBuffer), "%s", WiFi.localIP().toString().c_str());
  } else {
    snprintf(statusBuffer, sizeof(statusBuffer), "WIFI OFFLINE");
  }

  return statusBuffer;
}

const char* serverUrl() {
  return serverAddress.c_str();
}

bool hasServerUrl() {
  return serverAddress.length() >= 10;
}

}  // namespace CydifyNetwork
