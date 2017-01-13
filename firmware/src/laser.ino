
#include <pgmspace.h>
#include <ESP8266WiFi.h>
#include "secrets.h"
#define LASER 14
#define LED 5

#define SAMPLETIME_MS 60000.0 //1 minutes in ms

// eeprom addresses (storing 2byte ints so each address is +2)
#define EEP_WIFI_CONN 0
#define EEP_REBOOTS 2 

// state machine
#define NOT_CONNECTED 1
#define PRE_SAMPLE 2
#define SAMPLING 3
#define CHECK_WIFI 4
#define POSTING 5

#define LASER_OFF_DELAY 60000

int state = NOT_CONNECTED;

void setup()
{
    Serial.begin(9600);
    Serial.println();
    Serial.println();

    // update number of times rebooted
    setupEEPROM();
    EEPROMWriteInt(EEP_REBOOTS, EEPROMReadInt(EEP_REBOOTS) + 1);
    /*
    EEPROMWriteInt(EEP_REBOOTS, 0);
    EEPROMWriteInt(EEP_WIFI_CONN, 0);
    */

    // pin modes
    pinMode(LASER, INPUT);
    pinMode(LED, OUTPUT);
}

volatile unsigned long last_on = 0;
boolean laser_on = false;

void laser_ISR()
{
    last_on = millis();
}

void loop()
{
    static unsigned long start_time;

    switch(state)
    {
        case NOT_CONNECTED:
        {
            start_wifi();
            EEPROMWriteInt(EEP_WIFI_CONN, EEPROMReadInt(EEP_WIFI_CONN) + 1);
            state = PRE_SAMPLE;
            break;
        }
        case PRE_SAMPLE:
            attachInterrupt(digitalPinToInterrupt(LASER), laser_ISR, RISING); 
            state = SAMPLING;
            break;
        case SAMPLING:
        {
            if((millis() - start_time) > SAMPLETIME_MS)
            {
                state = CHECK_WIFI;
                detachInterrupt(digitalPinToInterrupt(LASER));
            }

            // only valid after first interrupt
            if(last_on != 0)
                laser_on = millis() - last_on < LASER_OFF_DELAY ? true : false;

            digitalWrite(LED, laser_on);
            break;
        }
        case CHECK_WIFI:
        {
            if(WiFi.status() != WL_CONNECTED) 
                state = NOT_CONNECTED;
            else
                state = POSTING;
            break;
        }
        case POSTING:
        {
            post(laser_on);
            start_time = millis();
            state = PRE_SAMPLE;
            break;
        }
    }
    Serial.println(laser_on);
    Serial.println(last_on);
    Serial.println(millis());
    delay(200);

}

void post(boolean laser_on)
{
    //send the data to sparkfun
    Serial.print("connecting to ");
    Serial.println(host);
    
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 80;
    if(!client.connect(host, httpPort)) 
    {
        Serial.println("connection failed");
        return;
    }
    
    // We now create a URI for the request
    String url = "/input/";
    url += streamId;
    url += "?private_key=";
    url += privateKey;
    url += "&laser=";
    url += laser_on;
    url += "&uptime=";
    url += millis();
    url += "&num_boots=";
    url += EEPROMReadInt(EEP_REBOOTS);
    url += "&num_connects=";
    url += EEPROMReadInt(EEP_WIFI_CONN);
    
    Serial.print("Requesting URL: ");
    Serial.println(url);
    
    // This will send the request to the server
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" + 
                 "Connection: close\r\n\r\n");
    delay(10);
    
    // Read all the lines of the reply from server and print them to Serial
    while(client.available())
    {
        String line = client.readStringUntil('\r');
        Serial.print(line);
    }
  
    Serial.println();
    Serial.println("closing connection");
}

void start_wifi()
{
    Serial.print("Connecting to ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
        //update_lcd(0);
    }

    Serial.println("");
    Serial.println("WiFi connected");  
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}