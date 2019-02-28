// rf95_reliable_datagram_server.pde
// -*- mode: C++ -*-
// Example sketch showing how to create a simple addressed, reliable messaging server
// with the RHReliableDatagram class, using the RH_RF95 driver to control a RF95 radio.
// It is designed to work with the other example rf95_reliable_datagram_client
// Tested with Anarduino MiniWirelessLoRa, Rocket Scream Mini Ultra Pro with the RFM95W

#include <RHReliableDatagram.h>
#include <RH_RF95.h>
#include <SPI.h>

#define CLIENT_ADDRESS 1
#define SERVER_ADDRESS 2

// Singleton instance of the radio driver
//H_RF95 driver;
RH_RF95 driver(0, 34); 

// Class to manage message delivery and receipt, using the driver declared above
RHReliableDatagram manager(driver, SERVER_ADDRESS);

// Need this on Arduino Zero with SerialUSB port (eg RocketScream Mini Ultra Pro)
//#define Serial SerialUSB
#define OTA_ENABLE 1
#define WIFI_ENABLE 1
#define MQTT_ENABLE 1
//#define DEBUG_ENABLE 1

#ifdef OTA_ENABLE
//OTA Update

#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#endif

#ifdef WIFI_ENABLE
#include <WiFi.h> //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <WebServer.h>
const char *ssid = "NETLLAR-Dani";
const char *ssidpassword = "86799461";
//const char *ssid = "FibraValencia_32F9A";
//const char *ssidpassword = "25nuuegg";
#endif

#if DEBUG_ENABLE
#include "TelnetSpy.h"
TelnetSpy SerialAndTelnet;
#define Serial SerialAndTelnet
#endif

#if MQTT_ENABLE
// BEGIN MQTT
#include <PubSubClient.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#define mqtt_server "m20.cloudmqtt.com"
#define port 19123
#define dioty_id "hovjocvq"           // email address used to register with DIoTY
#define dioty_password "4n6h6CLEmsa7" // your DIoTY password
#define clientNr "0"                  // used for client id (increment when deploying
// multiple instances at the same time)
// Some project settings
// DIoTY requires topic names to start with your userId, so stick it in the front
#define Modconcat(first, second) first second
#define slash "/"                            // all topics are prefixed with slash and your dioty_id
#define topicConnect "/ESP32_GW/connected"   // topic to say we're alive
#define topicIn "/ESP32_GW/inTopic"          // topic to switch off power system
#define SrcValueOut "/ESP32_GW/Src/"         // topic for select colors
#define SeqValueOut "/ESP32_GW/Seq/"         // topic for set brightness
#define RssiPktValueOut "/ESP32_GW/RssiPkt/" // topic for enable/disable lamp
//#define topicOut        "/ESP8266/outTopic"    // topic to subscribe
char msg[50];  // message to publish
int value = 0; // connection attempt
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char *connectTopic = Modconcat(Modconcat(slash, dioty_id), topicConnect);
const char *inTopic = Modconcat(Modconcat(slash, dioty_id), topicIn);
//const char *SrcValueOutTopic = Modconcat(Modconcat(slash, dioty_id), SrcValueOut);
//const char *SeqValueOutTopic = Modconcat(Modconcat(slash, dioty_id), SeqValueOut);
//const char *RssiPktValueOutTopic = Modconcat(Modconcat(slash, dioty_id), RssiPktValueOut);
const char *client_id = Modconcat(clientNr, dioty_id);
unsigned long mqttConnectionPreviousMillis = millis();
const long mqttConnectionInterval = 60000;
///END  MQTT
#endif

#define LED_RX 32
#define LED_MQTT_OK 25

#if DEBUG_ENABLE
void telnetConnected()
{
  Serial.println("Telnet connection established.");
}

void telnetDisconnected()
{
  Serial.println("Telnet connection closed.");
}
#endif

#if defined MQTT_ENABLE
void reconnect()
{
  // Loop until we're reconnected
  while (!mqttClient.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    ++value; //connection attempt
    if (mqttClient.connect(client_id, dioty_id, dioty_password))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      snprintf(msg, 50, "Successful connect #%ld", value);
      Serial.print("Publish message: ");
      Serial.println(msg);
      mqttClient.publish(connectTopic, msg, true);
      // ... and resubscribe
      mqttClient.subscribe(inTopic);
      //mqttClient.subscribe(SrcValueOutTopic);
      //mqttClient.subscribe(SeqValueOutTopic);
      //mqttClient.subscribe(RssiPktValueOutTopic);
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // The mobile app will publish 'true' or 'false' to the topic
  // Switch off the POWER if an '0' was received as first character, otherwise switch it on
  if ((char)payload[0] == '1') {  
    Serial.println("Resetting");
    delay(2500);
    ESP.restart();
  }
}
#endif

uint16_t NumPktRcv = 0;

void setup()
{
  pinMode(LED_MQTT_OK, OUTPUT);
  digitalWrite(LED_MQTT_OK, LOW); // turn the LED off at reset
  pinMode(LED_RX, OUTPUT);
#if DEBUG_ENABLE
  SerialAndTelnet.setWelcomeMsg("Welcome to the TelnetSpy example\n\n");
  SerialAndTelnet.setCallbackOnConnect(telnetConnected);
  SerialAndTelnet.setCallbackOnDisconnect(telnetDisconnected);
  Serial.begin(74880);
  delay(100); // Wait for serial port
  Serial.setDebugOutput(false);
  Serial.print("\n\nConnecting to WiFi ");
#else
  Serial.begin(115200);
  Serial.println("Booting GW");
#endif

#ifdef WIFI_ENABLE
  //WIFI and OTA Update Init
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, ssidpassword);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }
  Serial.println("Connected to the WiFi network");
#endif

#ifdef OTA_ENABLE
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);
  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("OTA ESP32 GW");
  // No authentication by default
  // ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  //ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  //End WIFI Ota Update Init

  btStop(); // Turn off bluetooth for saving battery
#endif

#if MQTT_ENABLE
  mqttClient.setServer(mqtt_server, port);
  mqttClient.setCallback(callback);
#endif



  //Serial.begin(9600);
  //while (!Serial)
  //  ; // Wait for serial port to be available
  if (!manager.init())
  Serial.println("init failed");
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on

  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then
  // you can set transmitter powers from 5 to 23 dBm:
  driver.setFrequency(868.0);
  driver.setTxPower(23, false); // Also tried with 20
  driver.setModemConfig(RH_RF95::Bw125Cr45Sf128);
  // If you are using Modtronix inAir4 or inAir9,or any other module which uses the
  // transmitter RFO pins and not the PA_BOOST pins
  // then you can configure the power transmitter power for -1 to 14 dBm and with useRFO true.
  // Failure to do that will result in extremely low transmit powers.
  //  driver.setTxPower(14, true);
  // You can optionally require this module to wait until Channel Activity
  // Detection shows no activity on the channel before transmitting by setting
  // the CAD timeout to non-zero:
  //  driver.setCADTimeout(10000);
}

uint8_t data[] = "And hello back to you";
// Dont put this on the stack:
uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];

void loop()
{

#if DEBUG_ENABLE
  SerialAndTelnet.handle();
#endif

#ifdef OTA_ENABLE
  ArduinoOTA.handle();
#endif

#if MQTT_ENABLE
  // Reconnect if there is an issue with the MQTT connection
  // const unsigned long mqttConnectionMillis = millis();
  // if ((false == mqttClient.connected()) && (mqttConnectionInterval <= (mqttConnectionMillis - mqttConnectionPreviousMillis)))
  // {
  //   mqttConnectionPreviousMillis = mqttConnectionMillis;
  //   reconnect();
  // }
  if (!mqttClient.connected())
  {
    reconnect();
  }

  mqttClient.loop();
  digitalWrite(LED_MQTT_OK, HIGH); // turn the LED on (HIGH is the voltage level)
  //Serial.println("Mqtt OK");
#endif
  if (manager.available())
  {
    // Wait for a message addressed to us from the client
    uint8_t len = sizeof(buf);
    uint8_t from;
    if (manager.recvfromAck(buf, &len, &from))
    {
      Serial.print("got request from  : 0x");
      Serial.print(from, HEX);
      Serial.print(": ");
      Serial.println((char *)buf);
      Serial.println("");
      Serial.print("SNR from packet:");
      Serial.print(driver.lastSNR(),DEC);
      Serial.println("");
      Serial.print("Rssi from packet:");
      Serial.print(driver.lastRssi(),DEC);
      Serial.println("");
      NumPktRcv++;
      Serial.print("Num pkt:");
      Serial.print(NumPktRcv, DEC);
      Serial.println("");

#if defined MQTT_ENABLE
      memset(msg, 0, sizeof(msg));
      dtostrf(from, 2, 1, msg);
      mqttClient.publish("SrcValueOutTopic", msg);
      memset(msg, 0, sizeof(msg));
      dtostrf(NumPktRcv, 3, 1, msg);
      mqttClient.publish("SeqValueOutTopic", msg);
      memset(msg, 0, sizeof(msg));
      dtostrf(driver.lastSNR(), 3, 1, msg);
      mqttClient.publish("SnrValueOutTopic", msg);
      memset(msg, 0, sizeof(msg));
      dtostrf(driver.lastRssi(), 3, 1, msg);
      mqttClient.publish("RssiValueOutTopic", msg);
#endif

      // Send a reply back to the originator client
      if (!manager.sendtoWait(data, sizeof(data), from))
        Serial.println("sendtoWait failed");
    }
  }
}
