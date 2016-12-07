#include <MicroView.h>
#include <TimerOne.h>
#include <Switch.h>
#include <LedControl.h>
#include <TimeAlarms.h>
#include <Adafruit_NeoPixel.h>
#include <DS3232RTC.h>    //http://github.com/JChristensen/DS3232RTC
#include <Time.h>         //http://www.arduino.cc/playground/Code/Time  
#include <Wire.h>         //http://arduino.cc/en/Reference/Wire (included with Arduino IDE)
#include <math.h>

// #define alarmPin  0  // alarm off pin
#define alarmPin  5  // alarm off pin
bool alarmState = true;
bool AlarmOnOff = true; // on is true, off is false
int alarmID = 0;

time_t alarm1Time;

/* neopixel daylight display */
#define PIN 6
#define Pixels 64

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
Adafruit_NeoPixel strip = Adafruit_NeoPixel(Pixels, PIN, NEO_GRB + NEO_KHZ800);

uint8_t red = 0;
uint8_t green = 0;
uint8_t blue = 0;
uint8_t actMin = 0;
uint8_t prevMin = 63;
int lightBarValue = 0;
boolean lightBarState = false;

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel.  Avoid connecting
// on a live circuit...if you must, connect GND first.

// rtc
//const int  cs=8; //chip select

// where is de leddisplay
LedControl lc1 = LedControl(A2, A1, A0, 1);
// A2 -> pin  1 op max7219 din
// A1 -> pin 13 op max7219 clock
// A0 -> pin 12 op max7219 cs load

//start rotary encoder variables
enum PinAssignments {
  encoderPinA = 2,
  encoderPinB = 3,
  clearButtonPin = A3,
};

volatile int encoderPos = 0;
int lastReportedPos = 1;
int encoderPosPrev = 0;

static boolean rotating = false;    // debounce management

boolean A_set = false;
boolean B_set = false;
boolean C_set = false;
//end rotary encoder variables ###

uint8_t state = 0;
uint8_t menuLevel = 0;
uint8_t menuLevelState = 0;
uint8_t menuSize = 2;

uint8_t menuTopSize = 2;
enum menuTop {
  clockRun,
  selectMenu
};

// menu level 1
uint8_t menuAlarmSize = 4;
char* menuAlarmString[] = { "alarm","uur","alarm","minuut","alarm","aan/uit","klok","run" };
uint8_t menuAlarmTeller[] = {5,3,5,6,5,7,4,3};
enum menuAlarm {
  alarmAdjustHour,
  alarmAdjustMinute,
  alarmAanUit,
  return1up
};
// menu level 2
uint8_t menuClockSize = 6;
enum menuClock {
  clockAdjustHour,
  clockAdjustMinute,
  clockAdjustDay,
  clockAdjustMonth,
  clockAdjustYear,
  return1up2
};
// menu level 3
//uint8_t menuDemoSize = 1;
//enum menuDemo {
//  showDemo
//};

boolean displayClear = false;
boolean displayCleared = false;

Switch clockAlarmAdjust = Switch(clearButtonPin, INPUT_PULLUP, HIGH);
Switch alarmButton = Switch(alarmPin, INPUT_PULLUP, HIGH);

uint16_t onDelay = 5;		// this is the on delay in milliseconds, if there is no on delay, the erase will be too fast to clean up the screen.

time_t lastaction = 0;

/* to blink or not to blink */
boolean dp = true;
/*
void halfSecondBlink();
void displayTimeLC();
void displayTime();
void doEncoderA();
void doEncoderB();
*/

/* soundstuff */
uint8_t mp3 = 1;
boolean mp3playing = false;

void setup() {
  /*debug stuff */
//  Serial.begin(9600);
  Serial.begin(38400);

  /* sync to DS3231 */
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  if (timeStatus() != timeSet) {
    Serial.println("Unable to sync with the RTC");
    // set default time
    setTime(10, 10, 30, 10, 10, 2016);
  } else {
    Serial.println("RTC has set the system time");
  }

  
  /* setup encoder */
  pinMode(encoderPinA, INPUT_PULLUP);
  pinMode(encoderPinB, INPUT_PULLUP);
  //pinMode(clearButton, INPUT);
  // encoder pin on interrupt 0 (pin 3)
  attachInterrupt(0, doEncoderA, CHANGE);
  // encoder pin on interrupt 1 (pin 4)
  attachInterrupt(1, doEncoderB, CHANGE);
  /* end setup encoder */

  /* adafruit neopixel */
  strip.begin();
  strip.show(); // Initialize all pixels to off

  /* Setup microview */
  uView.begin();	// begin of MicroView
  uView.clear(ALL);	// erase hardware memory inside the OLED controller
  uView.display();	// display the content in the buffer memory, by default it is the MicroView logo
  delay(700);
  uView.clear(PAGE);	// erase the memory buffer, when next uView.display() is called, the OLED will be cleared.


  alarm1Time = SECS_PER_HOUR * 10 + SECS_PER_MIN * 11;
  alarmID = Alarm.alarmRepeat(alarm1Time,lightBarOn);
  Alarm.disable(alarmID);
  AlarmOnOff = false;

  // setup ledarray
  lc1.shutdown(0, false);
  lc1.setIntensity(0, 15);
  lc1.clearDisplay(0);

  // timer (halfsecond)
  Timer1.initialize(500000UL);
  Timer1.attachInterrupt(halfSecondBlink);

  clocksplash();
}

void loop() {
/*
  Serial.print(alarmState);
  Serial.print(" ");
  Serial.print(lightBarState);
  Serial.print(" ");

  Serial.println();
 
  */
  rotating = true;  // reset the debouncer

  Alarm.delay(0); // don't wait

  if (Serial.available() > 0) {
    if(Serial.read() == 'X') {
      mp3 = mp3 > 17 ? 1 : ++mp3;
      Serial.print('v');
      Serial.print(17-mp3,BIN);
      Serial.print("T");
      Serial.print(mp3);
      mp3playing = true;
    }
  }

  clockAlarmAdjust.poll();
  alarmButton.poll();
  
  if ( clockAlarmAdjust.released() ) {
    state++;
    encoderPos = 0;
    state %= menuSize;
    menuLevel = menuLevelState;
    displayClear = !displayCleared;
    lastaction = now();
  }
  if ( alarmButton.released() ) {
    alarmState = false;
    lightBarOff();
    Alarm.free(alarmID);
    alarmID = Alarm.alarmRepeat(alarm1Time,lightBarOn);
    uView.invert(false);
  }
/*  
  if(alarmState == true ) {   // && AlarmOnOff == true ){
//    displayClear = !displayCleared;
  }
*/  
/*
  if ( alarmState == true) {
    displayClear = !displayCleared;
    lightBarOff();
    uView.invert(false);
    alarmState = false;
  }
*/
  lightBar(lightBarState);

  if ( (now() - lastaction) > 30 ) {
    lastaction = now();
    state = 0;
    menuLevel = 0;
    displayClear = !displayCleared;
  }

  uView.setFontType(0);     // set font type 0, please see declaration in MicroView.cpp
  uView.setCursor(0, 0);
  switch (menuLevel) {
  case 0:
    menuSize = 2;
    switch (state) {
    case clockRun:
      displayTime();
      displayTimeLC();
      break;
    case selectMenu:
      showMenuLevel();
      break;
    default:
      displayTime();
      displayTimeLC();
    };
    break;
  case 1: // set alarm
    menuSize = menuAlarmSize;
    switch (state) {
    case alarmAdjustHour:
      lastaction = now();
      alarm1Time += SECS_PER_HOUR * encoderPos;
      Alarm.free(alarmID);
      alarmID = Alarm.alarmRepeat(alarm1Time,lightBarOn);
      encoderPos = 0;
      displayAdjust("Alarm",5,"uur",3);
      displayAlarmLC();
      break;
    case alarmAdjustMinute:
      lastaction = now();
      alarm1Time += SECS_PER_MIN * encoderPos;
      Alarm.free(alarmID);
      alarmID = Alarm.alarmRepeat(alarm1Time,lightBarOn);
      encoderPos = 0;
      displayAdjust("Alarm",5,"minuut",6);
      displayAlarmLC();
      break;
    case alarmAanUit:
      lastaction = now();
      encoderPos = constrain(encoderPos,0,1);
      if ( encoderPos == 0 ){
        displayAdjust("AAN",3,"   ",3);
        AlarmOnOff = true;
        Alarm.enable(alarmID);
      } else {
        displayAdjust("   ",3,"UIT",3);
        AlarmOnOff = false;
        Alarm.disable(alarmID);
      }
      displayAlarmLC();
      break;
    case return1up:
      menuLevel = 0;
      state = 0;
      displayTime();
      displayTimeLC();
      lastaction = now();
    default:
      menuLevelState = 0;
      displayTime();
      displayTimeLC();
      lastaction = now();
    };
    break;
  case 2: // set the clock
    menuSize = 6;
    switch(state){
    case clockAdjustHour:
      RTC.set(now());
      adjustTime( SECS_PER_HOUR * encoderPos);
      encoderPos = 0;
      displayAdjust("Klok",4,"uur",3);
      displayTimeLC();
      lastaction = now();
      break;
    case clockAdjustMinute:
      RTC.set(now());
      adjustTime( SECS_PER_MIN * encoderPos);
      encoderPos = 0;
      displayAdjust("Klok",4,"Minuut",6);
      displayTimeLC();
      lastaction = now();
      break;
    case clockAdjustDay:
      adjustTime( SECS_PER_DAY * encoderPos);
      RTC.set(now());
      encoderPos = 0;
      displayAdjust("Klok",4,"Dag",3);
      displayDateLC(day());
      lastaction = now();
      break;
    case clockAdjustMonth:
      RTC.set(now());
      adjustTime( SECS_PER_DAY * daysInMonth() * encoderPos);
      encoderPos = 0;
      displayAdjust("Klok",4,"Maand",5);
      displayDateLC(month());
      lastaction = now();
      break;
    case clockAdjustYear:
      RTC.set(now());
      adjustTime( SECS_PER_YEAR * encoderPos);
      encoderPos = 0;
      displayAdjust("Klok",4,"Jaar",4);
      displayDateLC(year());
      lastaction = now();
      break;
    case return1up2:
      menuLevel = 0;
      state = 0;
      displayTime();
      displayTimeLC();
      lastaction = now();
      RTC.set(now());
    default:
      menuLevelState = 0;
      displayTime();
      displayTimeLC();
      lastaction = now();
    }
    break;
  default:
    displayTime();
    displayTimeLC();
  }
}


/* blink double point ever half second */
/* interrupt routine */
void halfSecondBlink() {
  dp = !dp;
}

/* neopixel display control */
void lightBar(bool lightBarState) {
  if ( lightBarState == true ) {
    int actMin = second();
    if ( actMin != prevMin ) {
      prevMin = actMin;
      if ( actMin % 10 == 0) {   //10
        lightBarValue += 1;
        lightBarValue = min(lightBarValue, 255);
        red = (int)sqrt(lightBarValue - 1 ) * 16;
        green = 254 - (int)(sqrt(255-lightBarValue)*16);   // 256
        blue  = 252 - (int)(log10(256-lightBarValue)*105);  // 256
/*
        lightBarValue = min(lightBarValue, 1024);
        red   = min(lightBarValue, 255);
        green = min(lightBarValue / 8, 255);
        blue  = min(lightBarValue / 16, 255);
*/
        Serial.print(lightBarValue);
        Serial.print("  ");
        Serial.print(red);
        Serial.print("  ");
        Serial.print(green);
        Serial.print("  ");
        Serial.print(blue);
        Serial.print("  ");
        Serial.println();
        for ( int px = 0 ; px < Pixels; px++) { strip.setPixelColor(px, red,green,blue); }
      }
    }
  } else { 
    for ( int px = 0 ; px < Pixels; px++) { strip.setPixelColor(px, 0,0,0); }
  }
  strip.show();
}

void lightBarOn() {
  lightBarState = true;
  lightBarValue = 0;
  uView.invert(true);
  Serial.print("v");
  Serial.print(17-mp3,BIN);
  Serial.println();
  Serial.print("T");
  Serial.print(mp3);
  Serial.println();
  mp3playing = true;
}
void lightBarOff() {
  lightBarState = false;
  lightBarValue = 0;
  uView.invert(false);
  Serial.print("O");
  Serial.println();
  mp3playing = false;
}

/* encoder */
// Interrupt on A changing state
void doEncoderA() {
  // debounce
  if ( rotating ) delay (1);  // wait a little until the bouncing is done
  // Test transition, did things really change?
  if ( digitalRead(encoderPinA) != A_set ) { // debounce once more
    A_set = !A_set;
    // adjust counter + if A leads B
    if ( A_set && !B_set ) {
      encoderPos += 1;
    }
    rotating = false;  // no more debouncing until loop() hits again
  }
}

// Interrupt on B changing state
void doEncoderB() {
  if ( rotating ) delay (1);
  if ( digitalRead(encoderPinB) != B_set ) {
    B_set = !B_set;
    //  adjust counter - 1 if B leads A
    if ( B_set && !A_set ) {
      encoderPos -= 1;
    }
    rotating = false;
  }
}

long unsigned int daysInMonth() {
  switch (month()) {
    case (1):
      return 31;
      break;
    case (2):
      if (year() / 4) {
        return 29;
      } else {
        return 28;
      }
      break;
    case (3):
      return 31;
      break;
    case (4):
      return 30;
      break;
    case (5):
      return 31;
      break;
    case (6):
      return 30;
      break;
    case (7):
      return 31;
      break;
    case (8):
      return 31;
      break;
    case (9):
      return 30;
      break;
    case (10):
      return 31;
      break;
    case (11):
      return 30;
      break;
    case (12):
      return 31;
      break;
  }
}

void showMenuLevel() {
  if ( displayClear != displayCleared ) {
    uView.clear(PAGE);
    uView.display();
    displayClear = displayCleared;
    encoderPos = 1;
    encoderPosPrev = 0;
    menuSize = menuAlarmSize;
  }
  if ( encoderPosPrev != encoderPos ) {
    lastaction = now();
    encoderPosPrev = encoderPos;
//    if ( encoderPos > menuTopSize ) { encoderPos = 0; };
//    if ( encoderPos < 1 ) { encoderPos = menuTopSize; };
    encoderPos = constrain(encoderPos,0,menuTopSize);
    uint8_t st;
    uint8_t clrfg = 0;
    uint8_t clrbg = NORM;
    char* level1string[] = { "Set","Alarm","Klok" };
    int level1teller[] = {3,5,4};
    
    uView.setFontType(1);     // set font type 0, please see declaration in MicroView.cpp

    for ( uint8_t x = 0; x <= menuTopSize; x++) {
      menuLevelState = encoderPos;
      if ( encoderPos == x ) { clrfg = BLACK; } else { clrfg = WHITE; };
      st = (LCDWIDTH - (level1teller[x] * uView.getFontWidth())) / 2;
      for ( uint8_t y = 0; y < level1teller[x]; y++ ) {
        uView.drawChar(st, x * uView.getFontHeight(), level1string[x][y], clrfg, clrbg);
        st += uView.getFontWidth();
      }
    }
    uView.display();
  }
}

void displayAdjust(const char* messageR1, int messageLengthR1, const char* messageR2, int messageLengthR2) {
  if ( displayClear != displayCleared ) {
    uView.clear(PAGE);
    uView.display();
    displayClear = displayCleared;
  }
  uView.setFontType(1);     // set font type 0, please see declaration in MicroView.cpp
  messageLengthR1 *= uView.getFontWidth();
  messageLengthR1 = max(0, (LCDWIDTH - messageLengthR1) / 2);
  uView.setCursor(messageLengthR1, 8);
  uView.print(messageR1);
  messageLengthR2 *= uView.getFontWidth();
  messageLengthR2 = max(0, (LCDWIDTH - messageLengthR2) / 2);
  uView.setCursor(messageLengthR2, 24);
  uView.print(messageR2);

  uView.setFontType(0);
  uView.setCursor(0,0);
  uView.print(hour());
  uView.print(":");
  uView.print(minute());
  uView.print(":");
  uView.print(second());
  uView.print(" ");
  uView.setCursor(0,uView.getLCDHeight() - uView.getFontHeight());
  uView.print(day());
  uView.print("/");
  uView.print(month());
  uView.print("/");
  uView.print(year());
  uView.print(" ");


  uView.display();
}

void displayTime() {
  if ( displayClear != displayCleared ) {
    uView.clear(PAGE);
    uView.display();
    displayClear = displayCleared;
  }
  uView.setFontType(0);     // set font type 0, please see declaration in MicroView.cpp
  if ( AlarmOnOff == true) {
    time_t showalarm = 0;
    showalarm = Alarm.read(alarmID);
    uView.setCursor(0, 0);
    uView.setFontType(0);
    uView.print("Alarm");
    uView.print(hour(showalarm) / 10);
    uView.print(hour(showalarm) % 10);
    uView.print(":");
    uView.print((minute(showalarm) / 10));
    uView.print((minute(showalarm) % 10));
    uViewdisplayClock();
    /*
    uView.print("   Alarm   ");
    uView.setFontType(2);
    uView.setCursor((LCDWIDTH - (uView.getFontWidth()*4))/2,(LCDHEIGHT - (uView.getFontHeight()))/2);
    uView.print(hour(showalarm) / 10);
    uView.print(hour(showalarm) % 10);
    uView.print((minute(showalarm) / 10));
    uView.print((minute(showalarm) % 10));
    */
  } else {
    uView.setCursor(2, 0);
    if ( day() < 10 ) {
      uView.print("0");
    }
    uView.print(day());
    uView.print("/");
    //uView.print(monthShortStr(month()));
    uView.print(month());
    uView.print("/");
    uView.print(year());
    uViewdisplayClock();
  }
  
  uView.display();      // display the memory buffer drawn

}

void uViewdisplayClock() {
  /* show me the clockface */
  #define clocksize 18 //24
  uint8_t clocksizeX = uView.getLCDWidth() / 2;
  uint8_t clocksizeY = uView.getLCDHeight() / 2 + 5;
  
  static uint8_t x0, y0, x1, y1;
  static float degresshour, degressmin, degresssec, hourx, houry, minx, miny, secx, secy;
  uView.line(clocksizeX, clocksizeY, clocksizeX + hourx, clocksizeY + houry, BLACK, NORM);
  uView.line(clocksizeX, clocksizeY, clocksizeX + minx, clocksizeY + miny, BLACK, NORM);
  uView.line(clocksizeX, clocksizeY, clocksizeX + secx, clocksizeY + secy, BLACK, NORM);

  //print hourpoints
  for ( uint8_t hx = 0; hx < 12; hx++) {
    degresshour = (((hx * 360) / 12) + 270) * (PI / 180);
    hourx = cos(degresshour) * clocksize;
    houry = sin(degresshour) * clocksize;
    uView.pixel(clocksizeX + hourx, clocksizeY + houry);
  }

  degresshour = (((hour() * 360) / 12) + 270) * (PI / 180);
  degressmin = (((minute() * 360) / 60) + 270) * (PI / 180);
  degresssec = (((second() * 360) / 60) + 270) * (PI / 180);

  hourx = cos(degresshour) * (clocksize / 1.6);
  houry = sin(degresshour) * (clocksize / 1.6);

  minx = cos(degressmin) * (clocksize / 1.3);
  miny = sin(degressmin) * (clocksize / 1.3);

  secx = cos(degresssec) * (clocksize / 1.1);
  secy = sin(degresssec) * (clocksize / 1.1);

  uView.line(clocksizeX, clocksizeY, clocksizeX + hourx, clocksizeY + houry, WHITE, XOR);
  uView.line(clocksizeX, clocksizeY, clocksizeX + minx, clocksizeY + miny, WHITE, XOR);
  uView.line(clocksizeX, clocksizeY, clocksizeX + secx, clocksizeY + secy, WHITE, XOR);

//  if (timeStatus() != timeSet) {
  if ( RTC.get() == 0 ) {
    uView.setFontType(0);
    uView.setCursor(0,uView.getLCDHeight() - uView.getFontHeight());
    uView.print("x");
  }

  uView.display();
}

/* lcd display */
void displayTimeLC() {
  lc1.setDigit(0, 0, hour() / 10, false);
  lc1.setDigit(0, 1, hour() % 10, dp);
  lc1.setDigit(0, 2, (minute() / 10), dp);
  lc1.setDigit(0, 3, (minute() % 10), false);
}
void displayAlarmLC() {
  time_t showalarm = Alarm.read(alarmID);
  lc1.setDigit(0, 0, hour(showalarm) / 10, false);
  lc1.setDigit(0, 1, hour(showalarm) % 10, dp);
  lc1.setDigit(0, 2, (minute(showalarm) / 10), dp);
  lc1.setDigit(0, 3, (minute(showalarm) % 10), false);
}

void displayDateLC(int dateToShow) {
  int n[] = {' ', ' ', ' ', ' '};
  if ( dateToShow >= 1000 ) {
    n[0] = dateToShow / 1000;
    dateToShow %= 1000;
    n[1] = 0;
    n[2] = 0;
    n[3] = 0;
  }
  if ( dateToShow >=  100 ) {
    n[1] = dateToShow /  100;
    dateToShow %=  100;
    n[2] = 0;
    n[3] = 0;
  }
  if ( dateToShow >=   10 ) {
    n[2] = dateToShow /   10;
    dateToShow %=   10;
    n[3] = 0;
  }
  if ( dateToShow  <   10 ) {
    n[3] = dateToShow %   10;
  }
  for ( uint8_t x = 0; x < 4; x++) {
    lc1.setChar(0, x, n[x], false);
  }
}

void clocksplash(){
  unsigned long int c = 1;
  for(int x = 0; x < Pixels; x++) {
    strip.setPixelColor(x,c);
    strip.show();
    c = c << 1;
    c %= 16777215;
    delay(15);
  }
  for(int x = 0; x < Pixels; x++) {
    strip.setPixelColor(x,0);
    strip.show();
  }
}