
#include <pgmspace.h>
#include <ESP8266WiFi.h>
#include "secrets.h"
#include "users.h"

#define LASER_IN 4 // has an external 10k pull down and diode for protection of laser in/outs
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
#define LASER_OFF_DELAY 10000 // how long after the last control pulse do we assume laser is finished
#define AUTH_TIMEOUT 30000
#define MAX_CONNECT_ATTEMPTS 10

// eeprom addresses (storing 2byte ints so each address is +2)
#define EEP_WIFI_CONN 0
#define EEP_REBOOTS 2 

// state machine
#define AUTH_START 1
#define AUTH_WAIT_RFID 2
#define AUTH_WAIT_LASER 3

#define LOG_START 1
#define LOG_SAMPLING 2
#define LOG_POSTING 3


int auth_state = AUTH_START;
int log_state = LOG_START;

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
    pinMode(LASER_IN, INPUT);
    digitalWrite(LASER_IN, false);
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
    static unsigned long auth_time;
    delay(100);

    switch(auth_state)
    {
        case AUTH_START:
        {
            // laser starts off
            Serial.println("auth start");
            digitalWrite(RELAY, false);
            auth_state = AUTH_WAIT_RFID;
            // think interrupts interfere with the rfid reader
            detachInterrupt(digitalPinToInterrupt(LASER_IN)); 
            break;
        }
        case AUTH_WAIT_RFID:
        {
            Serial.println("rfid wait");
            current_rfid = read_rfid();
            //rfid is present
            if(current_rfid != "")
            {
                Serial.println(current_rfid);
                if(valid_rfid(current_rfid))
                {
                    Serial.println("ok");
                    // turn on relay to enable laser
                    digitalWrite(RELAY, true);
                    auth_state = AUTH_WAIT_LASER;
                    auth_time = millis();

                    // attach laser interrupt
                    attachInterrupt(digitalPinToInterrupt(LASER_IN), laser_ISR, RISING); 
                }
                else
                {
                    Serial.println("bad id");
                }
            }
            //no rfid present
            break;
        }
        case AUTH_WAIT_LASER:
        {
            Serial.println("WAIT LASER");
            if(laser_on == false && (millis() - auth_time > AUTH_TIMEOUT))
                auth_state = AUTH_START;
            break;
        }
    }

    //log state machine
    switch(log_state)
    {
        case LOG_START:
        {
            // start wifi if necessary 
            if(WiFi.status() != WL_CONNECTED) 
                start_wifi();

            // start ISR for laser control pulses
            start_time = millis();
            log_state = LOG_SAMPLING;
            break;
        }
        case LOG_SAMPLING:
        {
            if((millis() - start_time) > POST_TIME_MS)
                log_state = LOG_POSTING;

            // only valid after first interrupt
            if(last_on != 0)
                laser_on = millis() - last_on < LASER_OFF_DELAY ? true : false;

            digitalWrite(LASER_ON_LED, laser_on);
            break;
        }
        case LOG_POSTING:
        {
            Serial.println("posting");
            post(laser_on, current_rfid);
            log_state = LOG_START;
            break;
        }
    }
}

// todo - make request to mattvenn.net with rfid and check it's OK
bool valid_rfid(String rfid)
{
    for(int i=0; i<NUM_USERS; i++)
    {
        if(users[i].rfid == rfid)
        {
            Serial.println(users[i].name);
            return true;
        }
    }
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
