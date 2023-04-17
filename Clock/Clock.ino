#include <U8g2lib.h>
#include <TimeLib.h>
#include <string.h>

// consts
const unsigned long endHeatTime = 65900000;  // 69600000; 07:00 is  25200000
const byte heatDays[] = { 0, 1, };

#define heater 3

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

U8G2_SSD1312_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, A5, A4, U8X8_PIN_NONE);

int speedCorrection = 3545;  // Number of milliseconds my Nano clock runs 3450
                             // slow per hour
                             // negative number here if it is running fast
                             // change to match your Arduino


// Volatile variables as changed in an interrupt, and we
//
//need to force the system to read the actual variable
// when used outside the
//  interrupt and not use a cached version
volatile unsigned long currentTime;  //  Duration in milliseconds from midnight

unsigned long lastTime = -1000;  //   lastTime that ShowTime was called initialised to -1000 so shows immediately

volatile unsigned long elapsed;  // Timer used for delay and hour count

unsigned long millisecondsInADay;  // Milliseconds in 24 hours
unsigned long millisecondsInHour;
// Milliseconds in 1 hour
unsigned long startHeatTime;


float currentDate;  // Julian date

int currentDay;
int currentMonth;
int currentYear;

byte ThermistorPin = 0;
int V0;
float R1 = 10000;
float logR2, R2, T;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;

bool heat = false;
bool wasHeated = false;

char *dayArray[] = { "Tue.  ",           
                     "Wed.  ",
                     "Thur.",
                     "Fri.  ",
                     "Sat.  ",
                     "Sun.  ",
                     "Mon.  " };

void setup() {
  Serial.begin(9600);
  u8g2.begin();
  u8g2.setDisplayRotation(U8G2_MIRROR);
  // Set up  time interrupt - millis() rolls over after 50 days so

  delay(5000);
  Serial.write("UNOREADY#");

  while (true) {
    String response = getDataFromSerial();

    if (response.length() != 0) {
      unsigned long time = response.toInt();
      currentDay = day(time);
      currentMonth = month(time);
      currentYear = year(time);
      int x = hour(time);
      int y = minute(time);
      int z = second(time);
      Serial.println(x);
      unsigned long cHour = x * 3600000ul;   // 3600 * 1000
      unsigned long cMinutes = y * 60000ul;  // 60 * 1000
      unsigned int cSeconds = z * 1000ul;
      currentTime = cHour + cMinutes + cSeconds + 2500;                 // Set to current time to mindnight in ms 12:00 is 4 320 000
      currentDate = JulianDate(currentDay, currentMonth, currentYear);  // Set base date
      break;
    }
  }

  // we are using our own millisecond counter which we can reset at
  // the end of each day
  TCCR0A = (1 << WGM01);    //Set the CTC mode Compare time and trigger interrupt
  OCR0A = 0xF9;             //Set value for time to compare to ORC0A for 1ms = 249  (8 bits so max is 256)
                            //[(Clock speed/Prescaler value)*Time in seconds] - 1
                            //[(16,000,000/64) * .001] - 1 = 249 = 1 millisecond
  TIMSK0 |= (1 << OCIE0A);  //set timer compare interrupt
  TCCR0B |= (1 << CS01);    //Set the prescale 1/64 clock
  TCCR0B |= (1 << CS00);    // ie 110 for last 3 bits

  TCNT0 = 0;  //initialize counter value to 0
  sei();      //Enable interrupt

  elapsed = 0;  // Set period counter to 0
  millisecondsInADay = 24ul * 60 * 60 * 1000;
  millisecondsInHour = 60ul * 60 * 1000;

  pinMode(heater, OUTPUT);

  ShowTime(currentTime);
}

void loop() {
  // loop runs every 150 milliseconds

  // If at end of the day reset time and increase date
  if (currentTime > millisecondsInADay) {
    //Next day
    // Stop interrupts while reset time
    noInterrupts();
    currentTime -= millisecondsInADay;
    interrupts();
    wasHeated = false;
    currentDate++;
  }
  // At the end of each hour adjust the elapsed time for
  // the inacuracy in the Arduino clock
  if (elapsed >= millisecondsInHour) {
    noInterrupts();
    // Adjust time for slow/fast running Arduino clock

    currentTime += speedCorrection;
    // Reset to count the next hour
    elapsed = 0;
    interrupts();
  }



  ShowTime(currentTime);

  // 01:00 is 3600000
  if (currentTime > 64800000 && heat == false && wasHeated == false) {
    heat = true;
    wasHeated = true;
    for (int i = 0; i < sizeof(heatDays) / sizeof(byte); i++) {
      byte dayOfWeek = (unsigned long)currentDate % 7;
      if (heatDays[i] == dayOfWeek) {
        int temp = GetOutsideTemp();
        float heatTime = CalculateHeatONTime(temp);
        heatTime = heatTime * 60000;  // 60 * 1000
        startHeatTime = endHeatTime - heatTime;
      }
    }
  }

  if (currentTime > startHeatTime && heat == true) {
    digitalWrite(heater, HIGH);
  }

  if (currentTime > endHeatTime && heat == true) {
    digitalWrite(heater, LOW);
    heat = false;
  }

  Wait(80);
}

// This is interrupt is called when
//  the compare time has been reached
// hence will be called once a millisecond
//  based on the
// OCR0A register setting.
ISR(TIMER0_COMPA_vect) {
  currentTime++;
  elapsed++;
}

float JulianDate(int iday, int imonth, int iyear) {
  // Calculate julian date (tested up to the year 20,000)
  unsigned long d = iday;
  unsigned long m = imonth;
  unsigned long y = iyear;
  if (m < 3) {
    m = m + 12;
    y = y - 1;
  }

  unsigned long t1 = (153 * m - 457) / 5;
  unsigned long t2 = 365 * y + (y / 4) - (y / 100) + (y / 400);
  return 1721118.5 + d + t1 + t2;
}

void GregorianDate(float jd, int &iday, int &imonth, int &iyear) {
  // Note 2100 is the next skipped leap year - compensates for skipped leap years
  unsigned long f = jd + 68569.5;
  unsigned long e = (4.0 * f) / 146097;
  unsigned long g = f - (146097 * e + 3) / 4;
  unsigned long h = 4000ul * (g + 1) / 1461001;

  unsigned long t = g - (1461 * h / 4) + 31;
  unsigned long u = (80ul * t) / 2447;
  unsigned long v = u / 11;
  iyear = 100 * (e - 49) + h + v;
  imonth = u + 2 - 12 * v;
  iday = t - 2447 * u / 80;
}

void SplitTime(unsigned long curr, unsigned long &ulHour, unsigned long &ulMin, unsigned long &ulSec) {
  // Calculate HH:MM:SS from millisecond count
  ulSec = curr / 1000;
  ulMin = ulSec / 60;
  ulHour = ulMin / 60;
  ulMin -= ulHour * 60;
  ulSec = ulSec - ulMin * 60 - ulHour * 3600;
}

unsigned long SetTime(unsigned long ulHour, unsigned long ulMin, unsigned long ulSec) {
  // Sets the number of milliseconds from midnight to current time

  return (ulHour * 60 * 60 * 1000) + (ulMin * 60 * 1000) + (ulSec * 1000);
}

void Wait(unsigned long value) {
  // Create our own dealy function
  // We have set our own interrupt on TCCR0A
  // hence millis() and delay() will no longer work
  unsigned long startTime = elapsed;
  while ((elapsed - startTime) < value) {
    // Just wait
  }
}

String FormatNumber(int value) {
  // To add a leading 0 if required
  if (value < 10) {
    return "0" + String(value);
  }

  else {
    return String(value);
  }
}

void ShowTime(unsigned long value) {
  // Update display once a second
  // or when rolls over midnight

  if ((value > lastTime + 1000) || (value < lastTime)) {
    lastTime = value;

    unsigned long iHours;
    unsigned long iMinutes;
    unsigned long iSeconds;

    SplitTime(value, iHours, iMinutes, iSeconds);

    String timeToDisplay = "Time: " + FormatNumber(iHours) + ":" + FormatNumber(iMinutes) + ":" + FormatNumber(iSeconds);
    char tempToDisplay[5];
    char *temp = "Temp: ";
    sprintf(tempToDisplay, "%d", GetOutsideTemp());
    String tempString = temp + String(GetOutsideTemp()) + "°C";

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setCursor(10, 10);
    u8g2.print(timeToDisplay);
    u8g2.setCursor(10, 40);
    u8g2.print(tempString);
    u8g2.sendBuffer();
  }
}

float CalculateHeatONTime(int temp) {
  // linear formula needed
  float result = temp - 14.1061;
  result = result / (-0.161569);
  if (result < 0) result = result * -1;
  //return result; // the result is in minutes
  return 5;
}

int GetOutsideTemp() {
  V0 = analogRead(ThermistorPin);
  R2 = R1 * (1023.0 / (float)V0 - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2));
  T = T - 273.15;
  return T;
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