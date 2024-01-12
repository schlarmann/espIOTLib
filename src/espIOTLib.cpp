/**
 * @file espIOTLib.h
 * @author Paul Schlarmann (paul.schlarmann@makerspace-minden.de)
 * @brief ESP32 / 8266 IOT WebConfig & MQTT Library
 * @version 0.1
 * @date 2023-04-11
 * 
 * @copyright Copyright (c) Paul Schlarmann 2023
 * 
 */

// --- Includes ---
#include "espIOTLib.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
# ifdef ESP8266
#  include <ESP8266mDNS.h>
#  include <ESP8266WiFi.h>
#  include <ESP8266HTTPUpdateServer.h>
# elif defined(ESP32)
#  include <ESPmDNS.h>
#  include <WiFi.h>
   // For ESP32 IotWebConf provides a drop-in replacement for UpdateServer.
#  include <IotWebConfESP32HTTPUpdateServer.h>
# endif
#include <IotWebConfUsing.h> // This loads aliases fosr easier class names.
#include <MQTT.h>

// --- Defines ---
#ifdef ESP8266
#define OTA_PORT 8266
#define CHIP_IDENT "ESP8266"
#elif defined(ESP32)
#define OTA_PORT 3232
#define CHIP_IDENT ESP.getChipModel()
#endif

#define ESP_IOTLIB_WEB_ROOT "/espIOTWeb"
#define ESP_IOTLIB_WEB_ENDPOINT ESP_IOTLIB_WEB_ROOT "/config"
#define ESP_IOTLIB_STATUS_ENDPOINT ESP_IOTLIB_WEB_ROOT "/status"
#define ESP_IOTLIB_RESET_ENDPOINT ESP_IOTLIB_WEB_ROOT "/reset"
#define ESP_IOTLIB_MQTT_DISCONNECT_ENDPOINT ESP_IOTLIB_WEB_ROOT "/mqttDisconnect"
#define ESP_IOTLIB_MQTT_CONNECT_ENDPOINT ESP_IOTLIB_WEB_ROOT "/mqttConnect"

#ifdef ESP_IOTLIB_MQTT_LOG
    #define LOG_MQTT_IDENT "[m] "
    #define MQTT_LOGF(...) Serial.print(LOG_MQTT_IDENT);Serial.printf(__VA_ARGS__)
#else
    #define MQTT_LOGF(...)
#endif
#ifdef ESP_IOTLIB_IOT_LOG
    #define LOG_IOT_IDENT "[i] "
    #define IOT_LOGF(...) Serial.print(LOG_IOT_IDENT);Serial.printf(__VA_ARGS__)
#else
    #define IOT_LOGF(...)
#endif
// --- Marcos ---

// --- Typedefs ---

// --- Private Vars ---

// --- Private Functions ---
void espIOTLib::_mqttConnect(){
    // Attempt to connect
    if (!this->_mqttClient->connect(this->_iotWebConf->getThingName(), this->_mqttUserName, this->_mqttUserPassword)) {
        MQTT_LOGF("Could not connect to MQTT server!!\n");
        MQTT_LOGF(" -- Connect return: %d // Error: %d, try again in 5 seconds.\n", mqttClient.returnCode(), mqttClient.lastError());
        this->_mqttLastConnectFailTime = millis();
    } else {
        MQTT_LOGF("Connected to MQTT\n");
        this->_mqttLastConnectFailTime = 0;
        // Subscribe to topics
        for(String topic : this->_mqttTopics){
            MQTT_LOGF("Subscribing to topic: %s\n", topic.c_str());
            this->_mqttClient->subscribe(topic.c_str());
        }
    }
}

const char* espIOTLib::_mqttReturnToString(lwmqtt_return_code_t retval){
    switch (retval)
    {
    case LWMQTT_CONNECTION_ACCEPTED:
        return "Connection Accepted (0)";
    case LWMQTT_UNACCEPTABLE_PROTOCOL:
        return "Unnacceptable Protocol (1)";
    case LWMQTT_IDENTIFIER_REJECTED:
        return "ID Rejected (2)";
    case LWMQTT_SERVER_UNAVAILABLE:
        return "Server Unavailable (3)";
    case LWMQTT_BAD_USERNAME_OR_PASSWORD:
        return "Bad Username / Password (4)";
    case LWMQTT_NOT_AUTHORIZED:
        return "Not Authorized (5)";
    
    default:
        return "Unknown Return Code (?)";
    }
}
const char* espIOTLib::_mqttErrorToString(lwmqtt_err_t errval){
    switch (errval)
    {
    case LWMQTT_SUCCESS:
        return "LWMQTT_SUCCESS (0)";
    case LWMQTT_BUFFER_TOO_SHORT:
        return "LWMQTT_BUFFER_TOO_SHORT (-1)";
    case LWMQTT_VARNUM_OVERFLOW:
        return "LWMQTT_VARNUM_OVERFLOW (-2)";
    case LWMQTT_NETWORK_FAILED_CONNECT:
        return "LWMQTT_NETWORK_FAILED_CONNECT (-3)";
    case LWMQTT_NETWORK_TIMEOUT:
        return "LWMQTT_NETWORK_TIMEOUT (-4)";
    case LWMQTT_NETWORK_FAILED_READ:
        return "LWMQTT_NETWORK_FAILED_READ (-5)";
    case LWMQTT_NETWORK_FAILED_WRITE:
        return "LWMQTT_NETWORK_FAILED_WRITE (-6)";
    case LWMQTT_REMAINING_LENGTH_OVERFLOW:
        return "LWMQTT_REMAINING_LENGTH_OVERFLOW (-7)";
    case LWMQTT_REMAINING_LENGTH_MISMATCH:
        return "LWMQTT_REMAINING_LENGTH_MISMATCH (-8)";
    case LWMQTT_MISSING_OR_WRONG_PACKET:
        return "LWMQTT_MISSING_OR_WRONG_PACKET (-9)";
    case LWMQTT_CONNECTION_DENIED:
        return "LWMQTT_CONNECTION_DENIED (-10)";
    case LWMQTT_FAILED_SUBSCRIPTION:
        return "LWMQTT_FAILED_SUBSCRIPTION (-11)";
    case LWMQTT_SUBACK_ARRAY_OVERFLOW:
        return "LWMQTT_SUBACK_ARRAY_OVERFLOW (-12)";
    case LWMQTT_PONG_TIMEOUT:
        return "LWMQTT_PONG_TIMEOUT (-13)";
    
    default:
        return "Unknown Error Code (?)";
    }
}

// Reconnect to MQTT server
void espIOTLib::_reconnectMQTT(){
    // Loop until we're reconnected
    if(this->_doMqtt && this->_connectedToWifi && !this->_mqttClient->connected() && (this->_mqttLastConnectFailTime - millis() > ESP_IOTLIB_MQTT_RECONNECT_INTERVAL)) {
        MQTT_LOGF(" -- Connect return: %d // Error: %d, try again in 5 seconds.\n", this->_mqttClient->returnCode(), this->_mqttClient->lastError());
        this->_mqttConnect();
    }
}

void espIOTLib::_wifiConnectCB(){
    this->_connectedToWifi = true;
    IOT_LOGF("Connected to WiFi \"%s\"\n", this->_iotWebConf->getWifiAuthInfo().ssid);
    if(this->_doMqtt){
        MQTT_LOGF("\tAttempt connection to MQTT server!\n");
        this->_mqttClient->setKeepAlive(30); // Send keepalive every 30 seconds
        this->_mqttClient->begin(this->_mqttServer, ESP_IOTLIB_MQTT_PORT, this->_wifiClient);
        this->_mqttConnect();
    }
    if(this->_doOTAUpdate){
        IOT_LOGF("\tStart ArduinoOTA\n");
#ifdef ESP8266
        ArduinoOTA.begin(false);
#elif defined(ESP32)
        ArduinoOTA.begin(); 
#endif
    }

    if(this->_extWifiConnectCB){
        IOT_LOGF("\tCall _extWifiConnectCB\n");
        this->_extWifiConnectCB();
    }
}

void espIOTLib::_connectWifi(const char* ssid, const char* password){
    this->_ip.fromString(String(this->_ipAddressValue));
    this->_mask.fromString(String(this->_netmaskValue));
    this->_gateway.fromString(String(this->_gatewayValue));
    this->_dns.fromString(String(this->_dnsValue));
#ifdef ESP8266
    if (! WiFi.config(this->_ip, this->_dns, this->_gateway, this->_mask)) {
#elif defined(ESP32)
    if (! WiFi.config(this->_ip, this->_gateway, this->_mask, this->_dns)) {
#endif
        IOT_LOGF("STA Failed to configure. Static IP?\n");
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
}

/**
 * Handle web requests to "/" path.
 */
void espIOTLib::_handleRoot()
{
    // -- Let this->_iotWebConf test and handle captive portal requests.
    if (this->_iotWebConf->handleCaptivePortal())
    {
        // -- Captive portal request were already served.
        return;
    }
    String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
    s += "<title>";
    s += this->_iotWebConf->getThingName();
    s += " - Main</title></head><body><div><p>Main page of ";
    s += this->_iotWebConf->getThingName();
    s += "</p><p>Using Chip: "; 
    s += String(CHIP_IDENT);
#if defined(ESP32)
    s += ", Revision: ";
    s += ESP.getChipRevision();
    s += ", ";
    s += ESP.getChipCores();
    s += " Cores @ ";
    s += ESP.getCpuFreqMHz();
    s += " MHz";
#endif
    s += "</p><p>SDK Version: ";
    s += String(ESP.getSdkVersion());
    s += "</p></div><hr/>";
    if(this->_doMqtt){
        s += "<p>MQTT Config: </p>";
        s += "<ul>";
        s += "<li>Server: ";
        s += this->_mqttServer;
        s += "</li>";
        s += "<li>User: ";
        s += this->_mqttUserName;
        s += "</li>";
        if(this->_mqttClient->connected()){
            s += "<li>Connected!</li>";
        } else {
            s += "<li>Not Connected</li>";
        }
        s += "</ul>";
        s += "<p>MQTT Defaults: </p>";
        s += "<ul>";
        s += "<li>Server: ";
        s += this->_mqttDefaultServer;
        s += "</li>";
        s += "<li>User: ";
        s += this->_mqttDefaultUserName;
        s += "</li>";
        s += "</ul>";
        s += "<hr/>";
    }
    if(this->_doStaticIP){
        s += "<p>IP Config: </p>";
        s += "<ul>";
        s += "<li>IP address: ";
        s += this->_ipAddressValue;
        s += "</li>";
        s += "<li>Gateway: ";
        s += this->_gatewayValue;
        s += "</li>";
        s += "<li>Netmask: ";
        s += this->_netmaskValue;
        s += "</li>";
        s += "<li>DNS address: ";
        s += this->_dnsValue;
        s += "</li>";
        s += "</ul>";
        s += "<hr/>";
    }
    if(this->_doOTAUpdate){
        s += "<p>OTA update available under: ";
        s += this->_ip.toString();
        s += ":";
        s += String(OTA_PORT);
        s += "</p>";
        s += "<hr/>";
    }
    s += "<p>Go to <a href='" ESP_IOTLIB_WEB_ENDPOINT "'>configure page</a> to change values.</p>";
    s += "<p><a href='" ESP_IOTLIB_STATUS_ENDPOINT "'>Status</a> | <a href='" ESP_IOTLIB_RESET_ENDPOINT "'>Reset CPU</a> | <a href='" ESP_IOTLIB_MQTT_DISCONNECT_ENDPOINT "'>Force MQTT Reconnect</a> | </p>";
    s += "<hr/><p>User Pages:</p><p>";
    for(espIOTLib_webPage page: this->_webPages){
        if(page.isShown){
            s += "<a href='";
            s += page.uri;
            s += "'>";
            s += page.menuName;
            s += "</a> | ";
        }
    }

    s += "</p></body></html>\n";

    this->_localServer->send(200, "text/html", s);
}

void espIOTLib::_handleStatus(){
    // -- Let this->_iotWebConf test and handle captive portal requests.
    if (this->_iotWebConf->handleCaptivePortal())
    {
        // -- Captive portal request were already served.
        return;
    }

    String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
    s += "<title>";
    s += this->_iotWebConf->getThingName();
    s += " - Status</title></head><body><div><p>Status page of ";
    s += this->_iotWebConf->getThingName();
    s += "</p>"; 
    s += "</p><p>Using Chip: "; 
    s += String(CHIP_IDENT);
    s += " @ SDK Version: ";
    s += String(ESP.getSdkVersion());
    s += "</p>";
    s += "<hr/>";

    s += "<h3>Free Memory</h3>";
    s += "<ul>";
    s += "<li>Heap: ";
    s += String(ESP.getFreeHeap()/1024.0);
    s += " kB</li><li>Flash: ";
    s += String(ESP.getFreeSketchSpace()/1024.0);
    s += " kB</li>";
#ifdef ESP8266
    s += "<li>Stack: ";
    s += String(ESP.getFreeContStack());
    s += " Bytes</li>";
#elif defined(ESP32)
    s += "<li>PSRAM: ";
    s += String(ESP.getFreePsram()/1024.0);
    s += " kB</li>";
#endif
    s += "</ul></div><hr/>";

    s += "<h3>Connection Status</h3><ul>";
    s += "<li>WiFi: ";
    if(WiFi.isConnected()){
        s += "Connected</li>";
        s += "<li>SSID: ";
        s += WiFi.SSID();
        s += "</li><li>IP: ";
        s += WiFi.localIP().toString();
        s += "</li><li>Mask: ";
        s += WiFi.subnetMask().toString();
        s += "</li><li>DNS: ";
        s += WiFi.dnsIP().toString();
        s += "</li><li>Broadcast: ";
        s += WiFi.broadcastIP().toString();
        s += "</li><li>MAC: ";
        s += WiFi.macAddress();
        s += "</li></ul>";
    } else {
        s += "Not Connected";
        s += "</li><li>MAC: ";
        s += WiFi.macAddress();
        s += "</li></ul>";
    }
    s += "<hr/>";

    if(this->_doMqtt){
        s += "<h3>MQTT Status</h3><ul>";
        s += "<li>Server: ";
        s += this->_mqttServer;
        s += "</li><li>User: ";
        s += this->_mqttUserName;
        s += "</li>";
        if(this->_mqttClient->connected()){
            s += "<li>Connected!</li>";
        } else {
            s += "<li>Not Connected</li>";
        }
        if(this->_mqttForceDisconnect){
            s += "<li>Force Disconnect!</li>";
        }
        s += "<li>Return Code: ";
        s += this->_mqttReturnToString(this->_mqttClient->returnCode());
        s += "</li><li>Last Error: ";
        s += this->_mqttErrorToString(this->_mqttClient->lastError());
        s += "</li>";
        s += "</ul>";
        s += "<hr/>";
    }

    s += "<p><a href='/'>HOME</a></p>";
    s += "</body></html>\n";
    this->_localServer->send(200, "text/html", s);
}

void espIOTLib::_handleResetReq(){
    this->_localServer->send(200, "text/html", "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><title>Resetting...</title></head><body><div><p>Resetting...</p></div><hr /><p><a href='/'>HOME</a></p></body></html>\n");
    delay(500);
    ESP.restart(); // Works for ESP8266 and ESP32
}
void espIOTLib::_handleMQTTDisconnReq(){
    String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
    s += "<title>MQTT Disconnect...</title></head><body><div><p>Trying MQTT Disconnect...</p>";
    bool result = this->_mqttClient->disconnect();
    if(result){
        s += "<p>MQTT Disconnected!</p>";
        this->_mqttForceDisconnect = true;
    } else {
        s += "<p>MQTT Disconnect failed!</p>";
    }

    s += "<ul><li>Return Code: ";
    s += this->_mqttReturnToString(this->_mqttClient->returnCode());
    s += "</li><li>Last Error: ";
    s += this->_mqttErrorToString(this->_mqttClient->lastError());
    s += "</li>";
    if(this->_mqttClient->connected()){
        s += "<li>Still Connected!</li>";
    } else {
        s += "<li>Not Connected</li>";
    }
    s += "</ul>";

    s += "</div><hr /><p>Go <a href='" ESP_IOTLIB_MQTT_CONNECT_ENDPOINT "'>here</a> to connect again</p></body></html>\n";
    this->_localServer->send(200, "text/html", s);
}

void espIOTLib::_handleMQTTConnReq(){
    String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
    s += "<title>MQTT Connect...</title></head><body><div><p>Trying MQTT Connect...</p>";
    this->_mqttConnect();
    delay(200);
    bool result = this->_mqttClient->connected();
    if(result){
        s += "<p>MQTT Connected!</p>";
        this->_mqttForceDisconnect = false;
    } else {
        s += "<p>MQTT Connect failed!</p>";
    }

    s += "<ul><li>Return Code: ";
    s += this->_mqttReturnToString(this->_mqttClient->returnCode());
    s += "</li><li>Last Error: ";
    s += this->_mqttErrorToString(this->_mqttClient->lastError());
    s += "</li>";
    s += "<li> mqttLastConnectFailTime (0 if not failed): ";
    s += this->_mqttLastConnectFailTime;
    s += "</li>";
    s += "</ul>";

    s += "</div><hr /><p><a href='/'>HOME</a></p></body></html>\n";
    this->_localServer->send(200, "text/html", s);
}


// --- Public Vars ---

// --- Public Functions ---


espIOTLib::espIOTLib(const char *deviceName, const char *version) {
    // Init espIOTLib
    this->_localServer = new WebServer(80);
    if(!this->_localServer || !deviceName || !version){
        IOT_LOGF("LibInit: Invalid parameters!\n");
        return;
    }
    IOT_LOGF("Initializing espIOTLib for %s at %s (Chip: %s)!\n", deviceName, version, CHIP_IDENT);
    IOT_LOGF("Free MEM %u, FLASH %u", ESP.getFreeHeap(), ESP.getFreeSketchSpace());

#ifdef ESP8266
    IOT_LOGF(", STACK %u\n", ESP.getFreeContStack());
#elif defined(ESP32)
    IOT_LOGF(", PSRAM %u\n", ESP.getFreePsram());
    IOT_LOGF("Chip Revision: %hhu, Cores: %hhu", ESP.getChipRevision(), ESP.getChipCores());
#endif

    this->_iotWebConf = new IotWebConf(deviceName, &(this->_dnsServer), this->_localServer, ESP_IOTLIB_AP_DEFAULT_PWD, version);
    this->_iotWebConf->setApTimeoutMs(30000);
    this->_iotWebConf->setupUpdateServer(
        [this](const char* updatePath) { this->_httpUpdater.setup(this->_localServer, updatePath); },
        [this](const char* userName, char* password) { this->_httpUpdater.updateCredentials(userName, password); }
    );
    this->_localServer->on("/", std::bind(&espIOTLib::_handleRoot, this));
    this->_localServer->on(ESP_IOTLIB_WEB_ENDPOINT, [this](){ this->_iotWebConf->handleConfig(); });
    this->_localServer->on(ESP_IOTLIB_RESET_ENDPOINT, std::bind(&espIOTLib::_handleResetReq, this));
    this->_localServer->on(ESP_IOTLIB_STATUS_ENDPOINT, std::bind(&espIOTLib::_handleStatus, this));
    this->_localServer->onNotFound([this](){ this->_iotWebConf->handleNotFound(); });
    this->_iotWebConf->setWifiConnectionCallback(std::bind(&espIOTLib::_wifiConnectCB, this));

    this->_mqttDefaultServer[0] = '\0';
    this->_mqttDefaultUserName[0] = '\0';
    this->_mqttDefaultUserPassword[0] = '\0';
    IOT_LOGF("\tespIOTLib initialized!\n");
}

espIOTLib::~espIOTLib()
{
}

void espIOTLib::start(){
    bool validWebConfig = false;
    if(this->_iotWebConf){
        IOT_LOGF("Starting this->_iotWebConf!\n");
        validWebConfig = this->_iotWebConf->init();
    }
    if (!validWebConfig){
        IOT_LOGF("Loading defaults\n");
        if(this->_doMqtt){
            strncpy(this->_mqttServer, this->_mqttDefaultServer, ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN);
            strncpy(this->_mqttUserName, this->_mqttDefaultUserName, ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN);
            strncpy(this->_mqttUserPassword, this->_mqttDefaultUserPassword, ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN);
            MQTT_LOGF("Set MQTT Defaults: %s@%s\n", this->_mqttUserName, this->_mqttServer);
        }
        
        if(this->_doStaticIP){
            strncpy(this->_ipAddressValue, this->_ip.toString().c_str(), ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN);
            strncpy(this->_gatewayValue, this->_gateway.toString().c_str(), ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN);
            strncpy(this->_netmaskValue, this->_mask.toString().c_str(), ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN);
            strncpy(this->_dnsValue, this->_dns.toString().c_str(), ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN);
        }

    }
}

void espIOTLib::configureStaticIP(IPAddress default_ip, IPAddress default_gateway, IPAddress default_mask, IPAddress default_dns){
    this->_ip = default_ip;
    this->_gateway = default_gateway;
    this->_mask = default_mask;
    this->_dns = default_dns;

    this->_doStaticIP = true;
    // TODO: Initalize strings?
    IOT_LOGF("Enabled Static IP, default: %s\n", default_ip.toString().c_str());

    this->_connGroup.addItem(&this->_ipAddressParam);
    this->_connGroup.addItem(&this->_gatewayParam);
    this->_connGroup.addItem(&this->_netmaskParam);
    this->_connGroup.addItem(&this->_dnsParam);
    this->_iotWebConf->addParameterGroup(&(this->_connGroup));
    
    this->_iotWebConf->setWifiConnectionHandler(std::bind(&espIOTLib::_connectWifi, this, std::placeholders::_1, std::placeholders::_2));
}

void espIOTLib::loop(){
    if(this->_iotWebConf)
        this->_iotWebConf->doLoop();
    if(this->_doMqtt){
        if(!this->_mqttForceDisconnect)
            this->_reconnectMQTT();
        if (this->_mqttClient->connected()){
            this->_mqttClient->loop();
        }
    }
    if(this->_doOTAUpdate){
        ArduinoOTA.handle();
    }
}

bool espIOTLib::isConnectedToWifi(){
    return this->_connectedToWifi;
}

    // Web Config
WebServer *espIOTLib::getWebServer(){
    return this->_localServer;
}
IotWebConf *espIOTLib::getIotWebConf(){
    return this->_iotWebConf;
}
const char *espIOTLib::getSSID(){
    return this->_iotWebConf->getWifiAuthInfo().ssid;
}

void espIOTLib::addWifiConnectedCB(espIOTLibCB callback){
    IOT_LOGF("Added wifi connection CB at %p\n", callback);
    this->_extWifiConnectCB = callback;
}

void espIOTLib::setConfigPin(int pin){
    this->_iotWebConf->setConfigPin(pin);
}

bool espIOTLib::addWebPage(const char *uri, WebServer::THandlerFunction handler){
    if(!uri || !handler)
        return false;
    
    espIOTLib_webPage newPage;
    newPage.uri = uri;
    for(espIOTLib_webPage page: this->_webPages){
        if(newPage == page){
            return false;
        }
    }
    this->_webPages.push_back(newPage);
    this->_localServer->on(uri, handler);
    return true;
}
bool espIOTLib::addWebPage(const char *uri, const char *menuName, WebServer::THandlerFunction handler){
    if(!uri || !handler || !menuName)
        return false;
    
    espIOTLib_webPage newPage;
    newPage.uri = uri;
    newPage.menuName = menuName;
    newPage.isShown = true;
    for(espIOTLib_webPage page: this->_webPages){
        if(newPage == page){
            return false;
        }
    }
    this->_webPages.push_back(newPage);
    this->_localServer->on(uri, handler);
    return true;
}

    // MQTT
MQTTClient *espIOTLib::getMQTTClient(){
    if(!this->_doMqtt)
        return NULL;
    return this->_mqttClient;
}
void espIOTLib::enableMQTT(const char *server, const char *username, const char *password){
    if(server && strlen(server) < ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN)
        strncpy(this->_mqttDefaultServer, server, ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN);
    if(username && strlen(username) < ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN)
        strncpy(this->_mqttDefaultUserName, username, ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN);
    if(password && strlen(password) < ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN)
        strncpy(this->_mqttDefaultUserPassword, password, ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN);
    MQTT_LOGF("Enabled MQTT, default server: %s\n", this->_mqttDefaultServer);
    this->_doMqtt = true;
    this->_mqttClient = new MQTTClient(ESP_IOTLIB_MQTT_BUFFER_SIZE);
    this->_mqttGroup.addItem(&this->_mqttServerParam);
    this->_mqttGroup.addItem(&this->_mqttUserNameParam);
    this->_mqttGroup.addItem(&this->_mqttUserPasswordParam);
    this->_iotWebConf->addParameterGroup(&this->_mqttGroup);
    this->_localServer->on(ESP_IOTLIB_MQTT_DISCONNECT_ENDPOINT, std::bind(&espIOTLib::_handleMQTTDisconnReq, this));
    this->_localServer->on(ESP_IOTLIB_MQTT_CONNECT_ENDPOINT, std::bind(&espIOTLib::_handleMQTTConnReq, this));
}
void espIOTLib::addMQTTSubscribeCB(espIOTLibMQTTCB mqttCB){
    MQTT_LOGF("Adding MQTT subscribe CB at %p\n", mqttCB);
    if(mqttCB && this->_doMqtt)
        this->_mqttClient->onMessageAdvanced(mqttCB);
}


void espIOTLib::subscribeMQTT(const char* topic){
    if(!topic || !this->_doMqtt)
        return;
    String topicStr = String(topic);
    if(topicStr.length() > 0){
        this->_mqttTopics.push_back(topicStr);
    }
}
// Publish int value to MQTT
void espIOTLib::publishInt(const char *topic, uint32_t value){
    if(!this->_doMqtt)
        return;
    // Turn int into string
    snprintf(this->_mqttDataBuffer, ESP_IOTLIB_MQTT_DATA_BUFFER_LEN-1, "%d", value);
    MQTT_LOGF("MQTT pub: %s Int: %s", topic, this->_mqttDataBuffer);
    // Publish if connected
    if (this->_connectedToWifi && this->_mqttClient->connected()){
        MQTT_LOGF(" OK\n");
        this->_mqttClient->publish(topic, this->_mqttDataBuffer);
    } else {
        MQTT_LOGF(" No Connection...\n");
    }
}
// Publish str value to MQTT (value _must_ be null terminated)
void espIOTLib::publishStr(const char *topic, char *value){
    if(!this->_doMqtt)
        return;
    MQTT_LOGF("MQTT pub: %s STR: %s", topic, value);
    // Publish if connected
    if (this->_connectedToWifi && this->_mqttClient->connected()){
        MQTT_LOGF(" OK\n");
        this->_mqttClient->publish(topic, value);
    } else {
        MQTT_LOGF(" No Connection...\n");
    }
}
// Publish float value to MQTT
void espIOTLib::publishFloat(const char *topic, double value){
    if(!this->_doMqtt)
        return;
    // Check for nan
    if(isnan(value)){
        return;
    }
    // Turn float into string
    dtostrf( value, ESP_IOTLIB_MQTT_DATA_BUFFER_LEN-1, ESP_IOTLIB_MQTT_FLOAT_PRECISION, this->_mqttDataBuffer);
    MQTT_LOGF("MQTT pub: %s Float: %s", topic, this->_mqttDataBuffer);
    // Publish if connected
    if (this->_connectedToWifi && this->_mqttClient->connected()){
        MQTT_LOGF(" OK\n");
        this->_mqttClient->publish(topic, this->_mqttDataBuffer);
    } else {
        MQTT_LOGF(" No Connection...\n");
    }
}

    // OTA
void espIOTLib::enableOTA(const char *md5Password){
    // Port defaults to 8266
    ArduinoOTA.setPort(OTA_PORT);
    // Hostname defaults to esp8266-[ChipID]
    ArduinoOTA.setHostname(this->_iotWebConf->getThingName());
    // Password set with it's md5 value
    ArduinoOTA.setPasswordHash(md5Password);
    this->_doOTAUpdate = true;
    IOT_LOGF("Enabling OTA at port %d\n", OTA_PORT);
}