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
#ifndef ESPIOTLIB_H
#define ESPIOTLIB_H

// --- Includes ---
#include <Arduino.h>

#include <vector>

#include <IotWebConf.h>
# ifdef ESP8266
#  include <ESP8266HTTPUpdateServer.h>
# elif defined(ESP32)
   // For ESP32 IotWebConf provides a drop-in replacement for UpdateServer.
#  include <IotWebConfESP32HTTPUpdateServer.h>
# endif
#include <IotWebConfUsing.h> // This loads aliases fosr easier class names.
#include <MQTT.h>
// --- Defines ---
#ifndef ESP_IOTLIB_AP_DEFAULT_PWD
    #define ESP_IOTLIB_AP_DEFAULT_PWD "1234paul"
#endif


#ifndef ESP_IOTLIB_MQTT_BUFFER_SIZE
    #define ESP_IOTLIB_MQTT_BUFFER_SIZE 512
#endif
#ifndef ESP_IOTLIB_MQTT_DATA_BUFFER_LEN
    #define ESP_IOTLIB_MQTT_DATA_BUFFER_LEN 20
#endif
#ifndef ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN
    #define ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN 255
#endif
#ifndef ESP_IOTLIB_MQTT_PORT
    #define ESP_IOTLIB_MQTT_PORT 1883
#endif
#ifndef ESP_IOTLIB_MQTT_FLOAT_PRECISION
    #define ESP_IOTLIB_MQTT_FLOAT_PRECISION 3
#endif
#ifndef ESP_IOTLIB_MQTT_RECONNECT_INTERVAL
    #define ESP_IOTLIB_MQTT_RECONNECT_INTERVAL 5000
#endif
#ifndef ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN
    #define ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN 20
#endif

//Use these for debug logging
//#define ESP_IOTLIB_MQTT_LOG
//#define ESP_IOTLIB_IOT_LOG

#ifndef ESP_IOTLIB_IOT_LOG
    #define IOTWEBCONF_DEBUG_DISABLED
#endif

// --- Marcos ---

// --- Typedefs ---
typedef void (*espIOTLibCB)(void);
typedef void (*espIOTLibMQTTCB)(MQTTClient *client, char topic[], char bytes[], int length);

// --- Public Vars ---

// --- Public Classes ---


struct espIOTLib_webPage{
    String uri = "";
    String menuName = "";
    bool isShown = false;

    friend bool operator==(const espIOTLib_webPage& lhs, const espIOTLib_webPage& rhs) {
        bool retVal = false;
        if(lhs.uri == rhs.uri){
            retVal = true;
        }
        if(lhs.isShown && rhs.isShown && lhs.menuName == rhs.menuName){
            retVal = true;
        }
        return retVal;
    }
};

class espIOTLib
{
protected:
    // --- Private Vars ---
        // IOTWeb
    DNSServer _dnsServer;
    WebServer *_localServer;
    IotWebConf *_iotWebConf;
    #ifdef ESP8266
        ESP8266HTTPUpdateServer _httpUpdater;
    #elif defined(ESP32)
        HTTPUpdateServer _httpUpdater;
    #endif
    espIOTLibCB _extWifiConnectCB;
    WiFiClient _wifiClient;
    bool _connectedToWifi = false;
    std::vector<espIOTLib_webPage> _webPages;

        // Static IP
    bool _doStaticIP = false;
    IPAddress _ip, _gateway, _mask, _dns;
    char _ipAddressValue[ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN];
    char _gatewayValue[ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN];
    char _netmaskValue[ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN];
    char _dnsValue[ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN];
    IotWebConfParameterGroup _connGroup = IotWebConfParameterGroup("conn", "Connection parameters");
    IotWebConfTextParameter _ipAddressParam = IotWebConfTextParameter("IP address", "ipAddress", this->_ipAddressValue, ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN);
    IotWebConfTextParameter _gatewayParam = IotWebConfTextParameter("Gateway", "gateway", this->_gatewayValue, ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN);
    IotWebConfTextParameter _netmaskParam = IotWebConfTextParameter("Subnet mask", "netmask", this->_netmaskValue, ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN);
    IotWebConfTextParameter _dnsParam = IotWebConfTextParameter("DNS", "dns", this->_dnsValue, ESP_IOTLIB_IP_ADDRESS_BUFFER_LEN);

        // MQTT
    bool _doMqtt = false;
    MQTTClient *_mqttClient;
    bool _mqttForceDisconnect = false;
    char _mqttDefaultServer[ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN];
    char _mqttDefaultUserName[ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN];
    char _mqttDefaultUserPassword[ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN];
    char _mqttServer[ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN];
    char _mqttUserName[ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN];
    char _mqttUserPassword[ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN];
    IotWebConfParameterGroup _mqttGroup = IotWebConfParameterGroup("mqtt", "MQTT configuration");
    IotWebConfTextParameter _mqttServerParam = IotWebConfTextParameter("MQTT server", "mqttServer", this->_mqttServer, ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN);
    IotWebConfTextParameter _mqttUserNameParam = IotWebConfTextParameter("MQTT user", "mqttUser", this->_mqttUserName, ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN);
    IotWebConfPasswordParameter _mqttUserPasswordParam = IotWebConfPasswordParameter("MQTT password", "mqttPass", this->_mqttUserPassword, ESP_IOTLIB_MQTT_TOPIC_BUFFER_LEN);
    char _mqttDataBuffer[ESP_IOTLIB_MQTT_DATA_BUFFER_LEN];
    uint32_t _mqttLastConnectFailTime = 0;
    std::vector<String> _mqttTopics;

        // OTA update
    bool _doOTAUpdate = false;
    

    // --- Private Functions ---
    void _mqttConnect();
    const char* _mqttReturnToString(lwmqtt_return_code_t retval);
    const char* _mqttErrorToString(lwmqtt_err_t errval);
    void _reconnectMQTT();
    void _wifiConnectCB();
    void _connectWifi(const char* ssid, const char* password);
    void _handleRoot();
    void _handleStatus();
    void _handleResetReq();
    void _handleMQTTDisconnReq();
    void _handleMQTTConnReq();

public:
    espIOTLib(const char *deviceName, const char *version);
    ~espIOTLib();

    void start();
    void configureStaticIP(IPAddress default_ip, IPAddress default_gateway, IPAddress default_mask, IPAddress default_dns);
    bool isConnectedToWifi();
    void loop();

        // Web Config
    WebServer *getWebServer();
    IotWebConf *getIotWebConf();
    const char *getSSID();
    void addWifiConnectedCB(espIOTLibCB callback);
    void setConfigPin(int pin);
    bool addWebPage(const char *uri, WebServer::THandlerFunction handler);
    bool addWebPage(const char *uri, const char *menuName, WebServer::THandlerFunction handler);

        // MQTT
    void enableMQTT(const char *server, const char *username, const char *password);
    MQTTClient *getMQTTClient();
    void addMQTTSubscribeCB(espIOTLibMQTTCB mqttCB);
    /**
     * @brief Subscribe to a MQTT topic. Must be called in setup
     * 
     * @param topic (char *) String of topic to subscribe to
     */
    void subscribeMQTT(const char* topic);
    void publishInt(const char *topic, uint32_t value);
    void publishStr(const char *topic, char *value);
    void publishFloat(const char *topic, double value);
    
        // OTA
    void enableOTA(const char *md5Password);
};

#endif /* ESPIOTLIB_H */