
#include <pgmspace.h>
#include <ESP8266WiFi.h>
#include "secrets.h"

#define LASER 4
#define RELAY 5 // also wired to an LED
#define LASER_ON_LED 2
/*
* SPI MOSI 13
* SPI MISO 12
* SPI CLK 14
*/
#define SS_PIN 15
#define RST_PIN 16

#define POST_TIME_MS 60000.0 //1 minutes in ms

// eeprom addresses (storing 2byte ints so each address is +2)
#define EEP_WIFI_CONN 0
#define EEP_REBOOTS 2 

// state machine
#define START 1
#define PRE_SAMPLE 2
#define SAMPLING 3
#define CHECK_WIFI 4
#define POSTING 5
#define WAIT_RFID 6

#define LASER_OFF_DELAY 10000 // how long after the last control pulse do we assume laser is finished
#define MAX_CONNECT_ATTEMPTS 10

int state = START;

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
    pinMode(LASER_ON_LED, OUTPUT);
    pinMode(RELAY, OUTPUT);
    digitalWrite(RELAY, false);
    digitalWrite(LASER_ON_LED, false);

    setup_rfid();
}

volatile unsigned long last_on = 0;
boolean laser_on = false;
String current_rfid = "";

void laser_ISR()
{
    last_on = millis();
}

void loop()
{
    static unsigned long start_time;
    delay(10);

    switch(state)
    {
        case START:
        {
            start_wifi();
            state = WAIT_RFID;
            break;
        }
        case WAIT_RFID:
        {
            digitalWrite(RELAY, false);
            current_rfid = read_rfid();
            //rfid is present
            if(current_rfid != "")
            {
                if(valid_rfid(current_rfid))
                {
                    Serial.println(current_rfid);
                    // turn on relay to enable laser
                    digitalWrite(RELAY, true);
                    state = PRE_SAMPLE;
                }
            }
            //no rfid present
            break;
        }
        case PRE_SAMPLE:
            attachInterrupt(digitalPinToInterrupt(LASER), laser_ISR, RISING); 
            state = SAMPLING;
            start_time = millis();
            break;
        case SAMPLING:
        {
            if((millis() - start_time) > POST_TIME_MS)
            {
                state = CHECK_WIFI;
                detachInterrupt(digitalPinToInterrupt(LASER));
            }

            // only valid after first interrupt
            if(last_on != 0)
                laser_on = millis() - last_on < LASER_OFF_DELAY ? true : false;

            digitalWrite(LASER_ON_LED, laser_on);
            break;
        }
        case CHECK_WIFI:
        {
            if(WiFi.status() != WL_CONNECTED) 
                // try and start it
                start_wifi();
            state = POSTING;
            break;
        }
        case POSTING:
        {
            // only try and post if wifi is connected
            if(WiFi.status() == WL_CONNECTED) 
            {
                Serial.println("posting");
                post(laser_on, current_rfid);
            }

            // if laser is on, keep sampling
            if(laser_on)
            {
                Serial.println("laser still on, continue waiting");
                state = PRE_SAMPLE;
            }
            // otherwise, need to check the rfid
            else
            {
                Serial.println("laser off, waiting for RFID...");
                state = WAIT_RFID;
            }
            break;
        }
    }
/*    Serial.println(laser_on);
    Serial.println(last_on);
    Serial.println(millis());
    */
    delay(200);

}

// todo - make request to mattvenn.net with rfid and check it's OK
bool valid_rfid(String rfid)
{
    if(rfid == "c184932b")
        return true;
    return false;
}

void post(boolean laser_on, String current_rfid)
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
    url += "&rfid=";
    url += current_rfid;
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

    int count = 0;
    while (WiFi.status() != WL_CONNECTED) 
    {
        delay(500);
        Serial.print(".");
        // don't hang if no internet
        if(count > MAX_CONNECT_ATTEMPTS)
            break;
    }
    Serial.println("");
    EEPROMWriteInt(EEP_WIFI_CONN, EEPROMReadInt(EEP_WIFI_CONN) + 1);
    
    if(WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected");  
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    }
}
