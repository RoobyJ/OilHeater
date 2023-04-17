#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

// consts
const char *ssid = "*****"; // Routers name
const char *password = "*****"; // Wifi password

const long utcOffsetInSeconds = 7200;

char daysOfTheWeek[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print ( "." );
  }

  timeClient.begin();
}

void loop() {
  String response = getDataFromSerial();

  if (response == "UNOREADY") {
    timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    Serial.write(String(epochTime).c_str());
  }

  delay(1000);
}

String getDataFromSerial() {
  String dataString;
  while (Serial.available()) {
    delay(1);
    if (Serial.available() > 0) {
      dataString = Serial.readStringUntil('#');
      break;
    }
  }
  return dataString;
}
