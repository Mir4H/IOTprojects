/*
Kytkennät:

PmodTC1
L432KC D12  <->  3 PmodTC1 = MISO
L432KC D13  <->  4 PmodTC1 = SCLK
L432KC D3   <->  1 PmodTC1 = CS
L432KC 3V3  <->  5 PmodTC1 = 3.3V
L432KC GND  <->  6 PmodTC1 = GND

PmodALS
L432KC D12  <->  3 PmodALS = MISO
L432KC D13  <->  4 PmodALS = SCLK
L432KC D4   <->  1 PmodALS = CS
L432KC 3V3  <->  6 PmodALS = 3.3V
L432KC GND  <->  5 PmodALS = GND

MOD WIFI ESP8266
L432KC D1   <->  3 MOD WIFI ESP8266 = RXD
L432KC D0   <->  4 MOD WIFI ESP8266 = TXD
L432KC 3V3  <->  1 MOD WIFI ESP8266 = 3.3V
L432KC GND  <->  2 MOD WIFI ESP8266 = GND

PmodOLEDrgb
L432KC D13  <->  4 PmodOLEDrgb = SCK
L432KC D11  <->  2 PmodOLEDrgb = MOSI
L432KC D9   <->  1 PmodOLEDrgb = CS
L432KC D10  <->  7 PmodOLEDrgb = DC
L432KC D6   <->  8 PmodOLEDrgb = RES
L432KC 3V3  <->  9 PmodOLEDrgb = VCCEN
L432KC 3V3  <-> 10 PmodOLEDrgb = PMODEN
L432KC GND  <->  5 PmodOLEDrgb = GND
L432KC 3V3  <->  6 PmodOLEDrgb = 3.3V

LEDs
L432KC D2   <->  Blue LED
L432KC A5   <->  Green LED
L432KC A2   <->  Red LED
*/

#include "DigitalOut.h"
#include "mbed.h"
#include "Adafruit_SSD1331.h" //https://os.mbed.com/users/timo_k2/code/Adafruit_SSD1331_MbedOS6/
#include "Adafruit_GFX.h" //https://os.mbed.com/users/timo_k2/code/Adafruit-GFX-MbedOS6/
#include "ESP8266Interface.h" //included in OS
#include <MQTTClientMbedOs.h> //https://github.com/ARMmbed/mbed-mqtt
#include "ntp-client/NTPClient.h" //https://github.com/ARMmbed/ntp-client
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <stdlib.h>

//MOD WIFI ESP8266 define Transmit & Receive Pins
ESP8266Interface esp(MBED_CONF_APP_ESP_TX_PIN, MBED_CONF_APP_ESP_RX_PIN);
            
// PmodOLEDrgb
Adafruit_SSD1331 OLED(D9, D6, D10, D11, NC, D13); // cs, res, dc, mosi, (nc), sck  

// PmodALS
DigitalOut alsCS(D4); // chip select for sensor SPI communication

// PmodTC1
DigitalOut tc1cs(D3); 

// mosi, miso, sck
SPI spi(D11, D12, D13); 

// LED:s
DigitalOut ledB(D2);
DigitalOut ledG(A5);
DigitalOut ledR(A2);

// OLED display colors
#define Black 0x0000
#define Blue 0x001F
#define Red 0xF800
#define Green 0x07E0
#define Cyan 0x07FF
#define Magenta 0xF81F
#define Yellow 0xFFE0
#define White 0xFFFF

// for PmodALS
int pctLight = 0;     
int getALS();      

// for PmodTC1
int tempInt = 0;
int tempDec = 0;
int rawTemp = 0;
int getTemp();

// for time
time_t getTime();
time_t currentTime;
struct tm *localTime;
char timeis [32];
int hours;
int minutes;
int seconds;
#define ntpAddress "time.nist.gov"
#define ntpPort 123

// for MQTT
char buffer [80];
bool connect = true;

// for LEDs
void redLedOn();
void greenLedOn();
void blueLedOn();

int main()
{   // initialization of display object
    OLED.begin();
    OLED.clearScreen();  
    ThisThread::sleep_for(500ms); // waiting for the ESP8266 to wake up. 
    //Store device IP
    SocketAddress deviceIP;
    //Store broker IP
    SocketAddress MQTTBroker;
    //Establish MQTT over TCP
    TCPSocket socket;
    MQTTClient client(&socket);
    
    //Connecting to Wifi
    printf("\r\nConnecting...\r\n");
    int ret = esp.connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\r\nConnection error\r\n");
        return -1;
    } else
    {
        printf("\nConnected successfully!\n");
    }

    //show details of connection
    printf("MAC: %s\n", esp.get_mac_address());
    esp.get_ip_address(&deviceIP);
    printf("IP: %s\n", deviceIP.get_ip_address());
    printf("Netmask: %s\n", esp.get_netmask());
    printf("Gateway: %s\n", esp.get_gateway());

    MQTT::Message msg;
    msg.qos = MQTT::QOS0;
    msg.retained = false;
    msg.dup = false;

    while (true) {

        rawTemp = getTemp();
        // The 14 temperature bits digged out and scaled
        tempInt = (rawTemp >> 18) / 4;
        tempDec = ((rawTemp >> 18) % 4)*25;

        pctLight = getALS();

        //Setup OLED and print
        OLED.clearScreen();
        OLED.fillScreen(Black);
        OLED.setTextColor(White);
        OLED.setCursor(0,0);
        OLED.setTextSize(2);
        currentTime = getTime();
        localTime = localtime( &currentTime );
        strftime(timeis, 80,"%H:%M",localTime);
        OLED.printf("%s", timeis); 
        OLED.setTextSize(1);
        OLED.setCursor(0,20);
        OLED.printf("Temp = %d.%d\n", tempInt, tempDec);
        if (tempInt > 30){ 
            OLED.setTextColor(Red);
            OLED.printf("HOT \n\n");
            redLedOn();
            }
        else if (tempInt < 20) {
            OLED.setTextColor(Blue);
            OLED.printf("COLD \n\n");
            blueLedOn();
            }   
        else{
            OLED.setTextColor(Green);
            OLED.printf("TEMP OK \n\n");
            greenLedOn();
            }    
        OLED.setTextColor(White);
        //Print on OLED screen based on the amount of light
        OLED.printf("Light = %d%%\n", pctLight);   
        if (pctLight <= 97){ 
            OLED.setTextColor(Red);
            OLED.printf("TOO DARK-LAMP ON\n");
        } else {
            OLED.setTextColor(Green);
            OLED.printf("SUNNY-LAMP OFF\n");
        }   
        ThisThread::sleep_for(5s);

        // get hours, minutes and seconds
        hours = localTime->tm_hour;
        minutes = localTime->tm_min;
        seconds = localTime->tm_sec;
        // Console print the time
        printf("%d:%d:%d\n", hours, minutes, seconds);
    
    // Connect to MQTT Client and publish values every 10min while hours is greater or equal to 8 and smaller or equal to 19
    if(hours >= 8 && hours <= 19) {
        //Connect to MQTT server first time and after every disconnect
        if (connect){
            printf("Connecting to MQTT broker...\n");
            esp.gethostbyname(MBED_CONF_APP_MQTT_BROKER_HOSTNAME, &MQTTBroker, NSAPI_IPv4, "esp");
            MQTTBroker.set_port(MBED_CONF_APP_MQTT_BROKER_PORT);
            MQTTPacket_connectData data = MQTTPacket_connectData_initializer;       
            data.MQTTVersion = 3;
            char *id = MBED_CONF_APP_MQTT_ID;
            data.clientID.cstring = id;
            socket.open(&esp);
            socket.connect(MQTTBroker);
            client.connect(data);
        }
        //Publish MQTT message
        sprintf(buffer, "{\"values\":{\"ALSSensor\":\"%d\",\"TempSensor\":\"%d.%d\"}}", pctLight, tempInt, tempDec);
        msg.payload = (void*)buffer;
        msg.payloadlen = strlen(buffer);
        printf("Publishing: {\"values\":{\"ALSSensor\":\"%d\",\"TempSensor\":\"%d.%d\"}}\n", pctLight, tempInt, tempDec);
        client.publish(MBED_CONF_APP_MQTT_TOPIC, msg);
        connect = false;
        ThisThread::sleep_for(55s); //1min = 60s, decreased by 5s (sleep_for earlier in the while loop).
    }

    // Disconnect from MQTT server when hours is 20 and sleep for 11hours and 50minutes
    if(hours == 20){
        printf("Disconnecting from MQTT broker...\n");
        client.disconnect();
        ThisThread::sleep_for(2s);
        socket.close();
        printf("Will resume measuring in the morning!\n");  
        connect = true;
        OLED.clearScreen();
        OLED.fillScreen(Black);
        OLED.setTextColor(White);
        OLED.setCursor(0,0);
        OLED.printf("Will resume in \nthe morning.");
        ThisThread::sleep_for(42600s);
    }
  }
}

int getALS(){
    // SPI for the ALS      
    alsCS = 1;

    // Setup the spi for 8 bit data, high steady state clock,
    // second edge capture, with a 12MHz clock rate
    spi.format(8,0);           
    spi.frequency(12000000);

    char lightRawHigh = 0; // unsigned 8bit data from sensor board
    char lightRawLow = 0; // unsigned 8bit data from sensor board
    char light8bit = 0;  // unsigned 8 bit integer value
    float lightPct = 0;  // 32 bit floating point
    
    // Begin the conversion process and serial data output
    alsCS.write(0); 
    ThisThread::sleep_for(1ms);

    // Reading two 8bit bytes by writing two 8bit to nowhere
    lightRawHigh = spi.write(0x00);
    lightRawLow = spi.write(0x00);

    // End of serial data output and back to tracking mode
    alsCS.write(1);
    ThisThread::sleep_for(1ms);

    // Shifting bits
    lightRawHigh = lightRawHigh << 4;
    lightRawLow = lightRawLow >> 4;
    
    // bitwise OR
    light8bit =( lightRawHigh | lightRawLow );

    // Conversion to 32 bit 
    // Percentage representation of the amount of light
    lightPct = ((float(light8bit))/255)*100; 
    
    return (int)lightPct; 
}

time_t getTime() {   
    time_t timestamp; 
    bool check = true;
    NTPClient ntp(&esp);
    int timestampNro;
    
    while(check){
        ntp.set_server(ntpAddress, ntpPort);
        timestamp = ntp.get_timestamp();
        timestampNro = (int)timestamp;
        // printf("%d\n", timestampNro); to console print the timestamp int 

        if(timestampNro>0){
            timestamp += (60*60*3);
            check = false;  
        }
    }
    return timestamp;
}

int getTemp()
{
    int rawLow = 0;
    int rawHigh = 0;
    int raw = 0;

    // Chip must be deselected
    tc1cs.write(1);
 
    // Setup the spi for 16 bit data, low steady state clock,
    // rising edge capture, with a 1MHz clock rate
    spi.format(16, 0);
    spi.frequency(1000000);
    ThisThread::sleep_for(100ms);
    
    // Select the device by seting chip select low
    tc1cs.write(0);
    ThisThread::sleep_for(1ms);  // > 100 ns for the MAX31855
    
    // Send 0x0000 to nowhere to read the 16 bits
    rawHigh = spi.write(0x0000);
    rawLow = spi.write(0x0000);
    
    ThisThread::sleep_for(1ms);
 
    // Deselect the device
    tc1cs.write(1);
    
    // Higher 16 bits and lower 16 bits combined
    rawHigh = rawHigh << 16;
    raw = rawHigh | rawLow;   // bitwise OR
    
    return (int)raw;
}

void redLedOn()
{
    ledR.write(1);
    ledG.write(0);
    ledB.write(0);
}

void greenLedOn()
{
    ledR.write(0);
    ledG.write(1);
    ledB.write(0);
}

void blueLedOn()
{
    ledR.write(0);
    ledG.write(0);
    ledB.write(1);
}
