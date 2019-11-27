#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <Ticker.h>

#include "SN74141.h"

// SN74141 input pins
#define A_PIN 16  // D0
#define B_PIN 5   // D1
#define C_PIN 4   // D2
#define D_PIN 0   // D3

#define S_PIN 17  // D4 as blinking pin

// Tube select pins
#define ts1 15    // D8 Hour tens
#define ts2 14    // D5 Hours
#define ts3 12    // D6 Minute tens
#define ts4 13    // D7 Minutes
#define ts5 1     // TX Second tens
#define ts6 3     // RX Seconds

SN74141 sn74141(A_PIN, B_PIN, C_PIN, D_PIN);

// NTP Servers:
static const char ntpServerName[] = "hu.pool.ntp.org";
//static const char ntpServerName[] = "time.nist.gov";
//static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
//static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

const int timeZone = 1;     // Central European Time
//const int timeZone = -5;  // Eastern Standard Time (USA)
//const int timeZone = -4;  // Eastern Daylight Time (USA)
//const int timeZone = -8;  // Pacific Standard Time (USA)
//const int timeZone = -7;  // Pacific Daylight Time (USA)

WiFiUDP Udp;

unsigned int localPort = 8888;  // local port to listen for UDP packets

time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
void sendNTPpacket(IPAddress &address);

Ticker secondpin;

void setup()
{
  pinMode(ts1, OUTPUT);
  pinMode(ts2, OUTPUT);
  pinMode(ts3, OUTPUT);
  pinMode(ts4, OUTPUT);
  pinMode(ts5, OUTPUT);
  pinMode(ts6, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  digitalWrite(ts1, LOW);
  digitalWrite(ts2, LOW);
  digitalWrite(ts3, LOW);
  digitalWrite(ts4, LOW);
  digitalWrite(LED_BUILTIN, HIGH);

  //Serial.begin(9600);

  WiFiManager wifiManager;

  // Uncomment for testing wifi manager
  //wifiManager.resetSettings();

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect();

  sn74141.begin();
  sn74141.outputNone();

  Udp.begin(localPort);

  setSyncProvider(getNtpTime);
  setSyncInterval(300); //Sets time synchronisation interval (secs)

  secondpin.attach(0.5, invert);

}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop()
{
  if (timeStatus() != timeNotSet) {

    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      // digitalClockDisplay();  //Send time and date to Serial
    }
  } else {
    invert();
    getNtpTime();
  }

  digitalWrite(ts1, HIGH);
  sn74141.outputPin(hour() / 10);
  delayMicroseconds(4000);
  digitalWrite(ts1, LOW);
  delayMicroseconds(100);

  digitalWrite(ts2, HIGH);
  sn74141.outputPin(hour() % 10);
  delayMicroseconds(4000);
  digitalWrite(ts2, LOW);
  delayMicroseconds(100);

  digitalWrite(ts3, HIGH);
  sn74141.outputPin(minute() / 10);
  delayMicroseconds(4000);
  digitalWrite(ts3, LOW);
  delayMicroseconds(100);

  digitalWrite(ts4, HIGH);
  sn74141.outputPin(minute() % 10);
  delayMicroseconds(4000);
  digitalWrite(ts4, LOW);
  delayMicroseconds(100);

  digitalWrite(ts5, HIGH);
  sn74141.outputPin(second() / 10);
  delayMicroseconds(4000);
  digitalWrite(ts5, LOW);
  delayMicroseconds(100);

  digitalWrite(ts6, HIGH);
  sn74141.outputPin(second() % 10);
  delayMicroseconds(4000);
  digitalWrite(ts6, LOW);
  delayMicroseconds(100);

}

void invert(void) {
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void digitalClockDisplay()
{
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits)
{
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

void getTime(void)
{
  getNtpTime();
}

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
