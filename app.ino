#include <HardwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include "GatewayIntents.h"
#include "WebSocketClient.h"
#include "libs/ArduinoJson.h"

#define DEBUG
#ifdef DEBUG
#define DEBUG_MSG Serial.println
#else
#define DEBUG_MSG(MSG)
#endif

/**
 * CONFIG
 */
#define wifi_ssid ""
#define wifi_password ""

const String bot_token = "";
// Intent options can be found in GatewayIntents.h
const uint16_t gateway_intents = GUILD_MESSAGES_INTENT | GUILD_MESSAGE_TYPING_INTENT | GUILD_VOICE_STATES_INTENT;

static const char* USER_ID = "132691466177871872";
static const uint8_t LED_PIN = 5;
/**
 * END CONFIG
 */

void setup_wifi();

WebSocketClient ws(true);
DynamicJsonDocument doc(1024);

const char *host = "discord.com";
const int httpsPort = 443;  //HTTPS= 443 and HTTP = 80

unsigned long heartbeatInterval = 0;
unsigned long lastHeartbeatAck = 0;
unsigned long lastHeartbeatSend = 0;

bool hasWsSession = false;
String websocketSessionId;
bool hasReceivedWSSequence = false;
unsigned long lastWebsocketSequence = 0;


void setup()
{
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    setup_wifi();
}

void setup_wifi() {
    delay(10);
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(wifi_ssid);

    WiFi.begin(wifi_ssid, wifi_password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void loop()
{
    // Generic HTTPS client for sending messages at some point

    // WiFiClientSecure httpsClient;

    // Serial.print("HTTPS Connecting");
    // int r=0; //retry counter
    // while((!httpsClient.connect(host, httpsPort)) && (r < 30))
    // {
    //     delay(100);
    //     Serial.print(".");
    //     r++;
    // }
    // if(r==30)
    // {
    //     Serial.println("Connection failed");
    // }
    // else
    // {
    //     Serial.println("Connected to web");
    // }
    // httpsClient.print(String("GET ") + "https://discord.com/api/gateway" + " HTTP/1.1\r\n" +
    //             "Host: " + host + "\r\n" +
    //             "Authorization: " + bot_token + "\r\n" +
    //             "Connection: close\r\n\r\n");
    // Serial.println("request sent");

    // while (httpsClient.connected())
    // {
    //     String line = httpsClient.readStringUntil('\n');
    //     if (line == "\r")
    //     {
    //         Serial.println("headers received");
    //         break;
    //     }
    // }
    // Serial.println("reply was:");
    // Serial.println("==========");
    // String line;
    // while(httpsClient.available())
    // {
    //     line = httpsClient.readStringUntil('\n');  //Read Line by Line
    //     Serial.println(line); //Print response
    // }
    // Serial.println("==========");
    // Serial.println("closing connection");

    // delay(2000);  //GET Data at every 2 seconds

    if (!ws.isConnected())
    {
        Serial.println("connecting");
        // It technically should fetch url from discord.com/api/gateway
        ws.connect("gateway.discord.gg", "/?v=8&encoding=json", 443);
    }
    else
    {
        unsigned long now = millis();
        if(heartbeatInterval > 0)
        {
            if(now > lastHeartbeatSend + heartbeatInterval)
            {
                if(hasReceivedWSSequence)
                {
                    DEBUG_MSG("Send:: {\"op\":1,\"d\":" + String(lastWebsocketSequence, 10) + "}");
                    ws.send("{\"op\":1,\"d\":" + String(lastWebsocketSequence, 10) + "}");
                }
                else
                {
                    DEBUG_MSG("Send:: {\"op\":1,\"d\":null}");
                    ws.send("{\"op\":1,\"d\":null}");
                }
                lastHeartbeatSend = now;
            }
            if(lastHeartbeatAck > lastHeartbeatSend + (heartbeatInterval / 2))
            {
                DEBUG_MSG("Heartbeat ack timeout");
                ws.disconnect();
                heartbeatInterval = 0;
            }
        }

        String msg;
        if (ws.getMessage(msg))
        {
            Serial.println(msg);
            deserializeJson(doc, msg);

            // TODO Should maintain heartbeat
            if(doc["op"] == 0) // Message
            {
                if(doc.containsKey("s"))
                {
                    lastWebsocketSequence = doc["s"];
                    hasReceivedWSSequence = true;
                }

                if(doc["t"] == "READY")
                {
                    websocketSessionId = doc["d"]["session_id"].as<String>();
                    hasWsSession = true;
                }
                else if(doc["t"] == "GUILD_CREATE")
                {
                    for(JsonVariant v : doc["d"]["voice_states"].as<JsonArray>())
                    {
                        if(v["user_id"] == USER_ID)
                        {
                            if(v["self_mute"] || v["mute"])
                            {
                                digitalWrite(LED_PIN, HIGH);
                            }
                            else
                            {
                                digitalWrite(LED_PIN, LOW);
                            }
                            break;
                        }
                    }
                }
                else if(doc["t"] == "VOICE_STATE_UPDATE")
                {
                    if(doc["d"]["member"]["user"]["id"] == USER_ID)
                    {
                        if(doc["d"]["channel_id"] && (doc["d"]["self_mute"] || doc["d"]["mute"]))
                        {
                            digitalWrite(LED_PIN, HIGH);
                        }
                        else
                        {
                            digitalWrite(LED_PIN, LOW);
                        }
                    }
                }
            }
            else if(doc["op"] == 9) // Connection invalid
            {
                ws.disconnect();
                hasWsSession = false;
                heartbeatInterval = 0;
            }
            else if(doc["op"] == 11) // Heartbeat ACK
            {
                lastHeartbeatAck = now;
            }
            else if(doc["op"] == 10) // Start
            {
                heartbeatInterval = doc["d"]["heartbeat_interval"];

                if(hasWsSession)
                {
                    DEBUG_MSG("Send:: {\"op\":6,\"d\":{\"token\":\"" + bot_token + "\",\"session_id\":\"" + websocketSessionId + "\",\"seq\":\"" + String(lastWebsocketSequence, 10) + "\"}}");
                    ws.send("{\"op\":6,\"d\":{\"token\":\"" + bot_token + "\",\"session_id\":\"" + websocketSessionId + "\",\"seq\":\"" + String(lastWebsocketSequence, 10) + "\"}}");
                }
                else
                {
                    DEBUG_MSG("Send:: {\"op\":2,\"d\":{\"token\":\"" + bot_token + "\",\"intents\":" + gateway_intents + ",\"properties\":{\"$os\":\"linux\",\"$browser\":\"ESP8266\",\"$device\":\"ESP8266\"},\"compress\":false,\"large_threshold\":250}}");
                    ws.send("{\"op\":2,\"d\":{\"token\":\"" + bot_token + "\",\"intents\":" + gateway_intents + ",\"properties\":{\"$os\":\"linux\",\"$browser\":\"ESP8266\",\"$device\":\"ESP8266\"},\"compress\":false,\"large_threshold\":250}}");
                }

                lastHeartbeatSend = now;
                lastHeartbeatAck = now;
            }
        }
    }
}
