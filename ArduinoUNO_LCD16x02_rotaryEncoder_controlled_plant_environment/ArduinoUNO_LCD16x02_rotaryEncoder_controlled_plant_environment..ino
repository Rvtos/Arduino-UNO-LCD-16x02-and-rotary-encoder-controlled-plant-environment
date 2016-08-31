//Libraries
#include "DHT.h"
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
//DHT11 or any other model number is an air humidity and temperature sensor:
#define DHTPIN 5 // if using different pin change here
#define DHTTYPE DHT11 // if using different sensor change here       
DHT dht(DHTPIN, DHTTYPE);
// some other similar stuff:
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE); // I2C address is 0x27, migh need to change that and/or other arguments
unsigned long StartTime = 0;                                // variable for environment data
const unsigned int DataUpdateInterval = 10000; // 10 seconds,  variable for environment data 
const unsigned int RHsPowerTime = 500; // 0.5 seconds,         variable for environment data
unsigned char EEPROMAddress = 0; // EEPROM is used for saving user input beyond the powering off of the Arduino

struct Limens // got this from Seeed studio code
{
    unsigned char DHTHumidity_Hi      = 60;
    unsigned char DHTHumidity_Low     = 40;
    unsigned char DHTTemperature_Hi   = 30;
    unsigned char DHTTemperature_Low  = 15;
    unsigned char SoilHum             = 20;
    unsigned char _SoilHum_Low        = 0;
    unsigned char _SoilHum_Hi         = 255;
    unsigned char WaterVolume         = 2;
    unsigned char LightDaily          = 10;
}; // the above Limens values are replaced with the ones stored in the EEPROM in the setup, therefore immediately after power on of Arduino
typedef struct Limens WorkingLimens;
WorkingLimens SystemLimens;

char m0r1[]="    NOTRANJI";
char m0r2[]="    VRT_1.0";
char m1r1[]="RHa[%]";
char m1r2[]="Ta[*C]";
char m2r1[]="RHa lo";
char m2r2[]="RHa hi";
char m3r1[]="Ta lo";
char m3r2[]="Ta hi";
char m4r1[]="light/day[h]";
char m4r2[]="lightOn[h]";
char m5r1[]="RHs[%]";
char m5r2[]="waterVol[dL]";
char m6r1[]="calibrate RHs";
char m6r2[]="sensor?";
char m6r3[]="dry";
char m6r4[]="wet";
// above are constant strings used on the LCD 
const byte LightRelayPin = 6;            // the relay controlling the lights, inversed (LOW triggers it, HIGH is NormallyConnected)
const byte HumidifierRelayPin = 7;       // the relay controlling the humidifier, inversed (if yours are not you have to check all the code)
const byte IrrigationRelayPin = 10;      // the relay controlling the water pump or whatever, inversed
const byte VentTransistorPin = 9;        // I use 2N2222 with a 12V 150mA vent for now
const byte RHsPin = A0;                  // pin for reading the relative soil humidity
const byte RHsPower = 13;                // pin for enabling the above reading, so the consumption is smaller
byte RHsPowerFlag = 0;                   // some flag connected with the above two
volatile byte modeVal2 = 0;              // some flag connected to changing the user input values
// some light stuff
unsigned long _leftTillLight = 0;
unsigned long lastTimeOfUpdate = 0;
const unsigned long OneHour = 3600000;   // one hour in miliseconds
unsigned long _lightRise = 0;
unsigned long _lightDaily = 0;
float _lightOn = 0;
int lightOn = 0;
// some air variables
float DHTHumidity    = 0;
float DHTTemperature = 0;
unsigned int DHTHumidity_ = 0;
unsigned int DHTTemperature_ = 0;
byte _fanPower = 0;
byte fanPower = 25;
byte airHumidify = 0;
// some irrigation stuff
int soilHum;
int _soilHum;
const unsigned long waterTimeOut = 120000;
const unsigned int msPERdL = 2000;
unsigned long waterTime;
byte watering = 0;
byte startYN = 0;
unsigned long startWatering = 0;

// Rotary encoder declarations
static int pinA = 2; // Our first hardware interrupt pin is digital pin 2
static int pinB = 3; // Our second hardware interrupt pin is digital pin 3
volatile byte aFlag = 0; // let's us know when we're expecting a rising edge on pinA to signal that the encoder has arrived at a detent
volatile byte bFlag = 0; // let's us know when we're expecting a rising edge on pinB to signal that the encoder has arrived at a detent (opposite direction to when aFlag is set)
volatile byte encoderPos = 0; //this variable stores our current value of encoder position. Change to int or uin16_t instead of byte if you want to record a larger range than 0-255
volatile byte oldEncPos = 0; //stores the last encoder position value so we can compare to the current reading and see if it has changed (so we know when to print to the serial monitor)
volatile byte reading = 0; //somewhere to store the direct values we read from our interrupt pins before checking to see if we have moved a whole detent
// Button reading, including debounce without delay function declarations
const byte buttonPin = 4; // this is the Arduino pin we are connecting the push button to
byte oldButtonState = HIGH;  // assume switch open because of pull-up resistor
const unsigned long debounceTime = 10;  // milliseconds
unsigned long buttonPressTime;  // when the switch last changed state
boolean buttonPressed = 0; // a flag variable
// Menu and submenu/setting declarations
byte Mode = 0;   // This is which menu mode we are in at any given time (top level or one of the submenus)
const byte modeMax = 6; // This is the number of submenus/settings you want
/* Note: you may wish to change settingN etc to int, float or boolean to suit your application. 
 Remember to change "void setADisplayModeLCDn(byte name,*BYTE* setting)" to match and probably add some 
 "modeMax"-type overflow code in the "if(Mode == N && buttonPressed)" section*/

void setup() {
  //Rotary encoder section of setup
  Serial.begin(9600);
  pinMode(pinA, INPUT_PULLUP); // set pinA as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  pinMode(pinB, INPUT_PULLUP); // set pinB as an input, pulled HIGH to the logic voltage (5V or 3.3V for most cases)
  attachInterrupt(0,PinA,RISING); // set an interrupt on PinA, looking for a rising edge signal and executing the "PinA" Interrupt Service Routine (below)
  attachInterrupt(1,PinB,RISING); // set an interrupt on PinB, looking for a rising edge signal and executing the "PinB" Interrupt Service Routine (below)
  pinMode (buttonPin, INPUT_PULLUP); // setup the button pin
  pinMode(RHsPin, INPUT);           
  pinMode(RHsPower, OUTPUT);         
  dht.begin();  
  lcd.begin(16,2);
  lcd.setCursor(0,0);
  lcd.print(m0r1);
  lcd.setCursor(0,1);
  lcd.print(m0r2); 
  
  if (EEPROM.read(EEPROMAddress) == 0xff) { /* The First time power on to write the default data to EEPROM */
      EEPROM.write(EEPROMAddress,0x00);
      EEPROM.write(++EEPROMAddress,SystemLimens.DHTHumidity_Hi);
      EEPROM.write(++EEPROMAddress,SystemLimens.DHTHumidity_Low);
      EEPROM.write(++EEPROMAddress,SystemLimens.DHTTemperature_Hi);
      EEPROM.write(++EEPROMAddress,SystemLimens.DHTTemperature_Low);
      EEPROM.write(++EEPROMAddress,SystemLimens.SoilHum);
      EEPROM.write(++EEPROMAddress,SystemLimens._SoilHum_Low);
      EEPROM.write(++EEPROMAddress,SystemLimens._SoilHum_Hi);      
      EEPROM.write(++EEPROMAddress,SystemLimens.WaterVolume);   
      EEPROM.write(++EEPROMAddress,SystemLimens.LightDaily);
  } 
  else {
      EEPROMAddress++;
      SystemLimens.DHTHumidity_Hi     = EEPROM.read(EEPROMAddress++);
      SystemLimens.DHTHumidity_Low    = EEPROM.read(EEPROMAddress++);
      SystemLimens.DHTTemperature_Hi  = EEPROM.read(EEPROMAddress++);
      SystemLimens.DHTTemperature_Low = EEPROM.read(EEPROMAddress++);
      SystemLimens.SoilHum            = EEPROM.read(EEPROMAddress++);
      SystemLimens._SoilHum_Low       = EEPROM.read(EEPROMAddress++);
      SystemLimens._SoilHum_Hi        = EEPROM.read(EEPROMAddress++);
      SystemLimens.WaterVolume        = EEPROM.read(EEPROMAddress++);        
      SystemLimens.LightDaily         = EEPROM.read(EEPROMAddress++);
  }
  _lightDaily = SystemLimens.LightDaily * 3600000;
  pinMode(LightRelayPin, OUTPUT); // Relay controlling : Lights
  pinMode(HumidifierRelayPin, OUTPUT); //                     Humidifier
  pinMode(IrrigationRelayPin, OUTPUT); //                     Water Pump
  pinMode(VentTransistorPin,OUTPUT); //                 Ventilation 
}

void loop() {
  CheckChangeWaterPump(); // is run constantly and completely controls the output for irrigation
  rotaryMenu();           // is run constantly and controls most of the LCD / rotary encored menu       
  if ((millis() - StartTime > DataUpdateInterval - RHsPowerTime) && (millis() - StartTime < DataUpdateInterval)) {
    digitalWrite(RHsPower,HIGH);
  }
  else if (millis() - StartTime > DataUpdateInterval) {
     StartTime      = millis();
     DHTHumidity    = dht.readHumidity();
     DHTHumidity_   = (int) DHTHumidity;
     DHTTemperature = dht.readTemperature();
     DHTTemperature_ = (int) DHTTemperature;
     _soilHum = analogRead(RHsPin) / 4;
     _soilHum = constrain(_soilHum,SystemLimens._SoilHum_Low,SystemLimens._SoilHum_Hi);
     soilHum = map(_soilHum,SystemLimens._SoilHum_Low,SystemLimens._SoilHum_Hi,0,100);
     digitalWrite(RHsPower,LOW);
     CheckChangeLights();
     CheckChangeVents();
     CheckTurnOnAirHumidifier();
  }
  if (((millis()- StartTime) > 5000) && airHumidify) {
    airHumidify = 0;
    digitalWrite(HumidifierRelayPin,HIGH);
  }
}

void rotaryMenu() {
  if(oldEncPos != encoderPos) {
    if (Mode == 0) {
      if (encoderPos == 0) {
        if (oldEncPos == 6 && RHsPowerFlag) {
          RHsPowerFlag = 0;
          digitalWrite(RHsPower,LOW);  
        }
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print(m0r1);
        lcd.setCursor(0,1);
        lcd.print(m0r2); 
      }
      if (encoderPos == 1) {
        DisplayModeLCD(m1r1,m1r2,DHTHumidity_,DHTTemperature_);
      }
      else if (encoderPos == 2) {
        DisplayModeLCD(m2r1,m2r2,SystemLimens.DHTHumidity_Low,SystemLimens.DHTHumidity_Hi);  
      }
      else if (encoderPos == 3) {
        DisplayModeLCD(m3r1,m3r2,SystemLimens.DHTTemperature_Low,SystemLimens.DHTTemperature_Hi);
      }
      else if (encoderPos == 4) {
        DisplayModeLCD(m4r1,m4r2,SystemLimens.LightDaily,lightOn);
      }
      else if (encoderPos == 5) {
        DisplayModeLCD(m5r1,m5r2,soilHum,SystemLimens.WaterVolume);
        if (oldEncPos == 6 && RHsPowerFlag) {
          RHsPowerFlag = 0;
          digitalWrite(RHsPower,LOW);
        }
      }
      else if (encoderPos == 6) {
        DisplayModeLCD(m6r1,m6r2,-27,-27);
        RHsPowerFlag = 1;
        digitalWrite(RHsPower,HIGH);
      }
    }
    if (Mode == 2 && !modeVal2) {
      if ((encoderPos > SystemLimens.DHTHumidity_Hi - 4) || (encoderPos < 1)) {
        encoderPos = oldEncPos;
      }
      ChangeValue(encoderPos,0);
    }
    else if (Mode == 2 && modeVal2) {
      if ((encoderPos < SystemLimens.DHTHumidity_Low + 4) || (encoderPos > 99)) {
        encoderPos = oldEncPos;
      }
      ChangeValue(encoderPos,1);
    }
    else if (Mode == 3 && !modeVal2) {
      if ((encoderPos > SystemLimens.DHTTemperature_Hi - 2) || (encoderPos < 1)) {
        encoderPos = oldEncPos;
      }
      ChangeValue(encoderPos,0);
    }
    else if (Mode == 3 && modeVal2) {
      if ((encoderPos < SystemLimens.DHTTemperature_Low + 2) || (encoderPos > 49)) {
        encoderPos = oldEncPos;
      }
      ChangeValue(encoderPos,1);
    }
    else if (Mode == 4 && !modeVal2) {
      if (encoderPos > 24) {
        encoderPos = oldEncPos;
      }
      ChangeValue(encoderPos,0);
    }
    else if (Mode == 4 && modeVal2) {
      if (encoderPos > 24) {
        encoderPos = oldEncPos;
      }
      ChangeValue(encoderPos,1);
    }
    else if (Mode == 5 && !modeVal2) {
      if ((encoderPos > 99) || (encoderPos == 0)) {
        encoderPos = oldEncPos;
      }
      ChangeValue(encoderPos,0);
    }
    else if (Mode == 5 && modeVal2) {
      if((encoderPos == 255) || (encoderPos < 1)) {
        encoderPos = oldEncPos;
      }
      ChangeValue(encoderPos,1);
    }
    oldEncPos = encoderPos;
  }
  byte buttonState = digitalRead (buttonPin); 
  if (buttonState != oldButtonState){
    if (millis () - buttonPressTime >= debounceTime){
      buttonPressTime = millis ();
      oldButtonState =  buttonState;
      if (buttonState == LOW){
        buttonPressed = 1;
      }
      else {
        buttonPressed = 0;  
      }  
    }  
  }

  if (Mode == 0) {
    if (encoderPos > (modeMax+10)) encoderPos = modeMax;
    else if (encoderPos > modeMax) encoderPos = 0;
    if (buttonPressed){ 
      if (encoderPos != 1) {  
        Mode = encoderPos;
      }
      buttonPressed = 0;
      if (Mode == 2) {
        encoderPos = SystemLimens.DHTHumidity_Low;
        SetValueCursor();
      }
      else if (Mode == 3) {
        encoderPos = SystemLimens.DHTTemperature_Low;
        SetValueCursor();
      }
      else if (Mode == 4) {
        encoderPos = SystemLimens.LightDaily;
        SetValueCursor();
      }
      else if (Mode == 5) {
        encoderPos = SystemLimens.SoilHum;
        lcd.setCursor(3,0);
        lcd.print(" lo");
        SetValueCursor();
      }
      else if (Mode == 6) {
        lcd.clear();
        lcd.print(m6r3);
      }
    }
  }
  if (Mode == 2 && buttonPressed && !modeVal2){
    buttonPressed = 0;
    modeVal2 = 1; 
    SystemLimens.DHTHumidity_Low = encoderPos;
    EEPROM.update(2,SystemLimens.DHTHumidity_Low);
    SetValueCursor();
    encoderPos = SystemLimens.DHTHumidity_Hi;  
  }
  else if (Mode == 2 && buttonPressed && modeVal2) {
    buttonPressed = 0;
    modeVal2 = 0;
    SystemLimens.DHTHumidity_Hi = encoderPos;
    EEPROM.update(1,SystemLimens.DHTHumidity_Hi);
    SetValueCursor(); 
    Mode = 0;
    encoderPos = 2;
  }
  else if (Mode == 3 && buttonPressed && !modeVal2){
    buttonPressed = 0;
    modeVal2 = 1;    
    SystemLimens.DHTTemperature_Low = encoderPos;
    EEPROM.update(4,SystemLimens.DHTTemperature_Low);
    SetValueCursor();
    encoderPos = SystemLimens.DHTTemperature_Hi;  
  }
  else if (Mode == 3 && buttonPressed && modeVal2) {
    buttonPressed = 0;
    modeVal2 = 0;
    SystemLimens.DHTTemperature_Hi = encoderPos;
    EEPROM.update(3,SystemLimens.DHTTemperature_Hi);
    SetValueCursor();
    Mode = 0;
    encoderPos = 3;
  }
  else if (Mode == 4 && buttonPressed && !modeVal2){
    buttonPressed = 0;
    modeVal2 = 1; 
    SystemLimens.LightDaily = encoderPos;
    _lightDaily = SystemLimens.LightDaily * 3600000;
    EEPROM.update(9,SystemLimens.LightDaily);
    SetValueCursor();
    encoderPos = 0;  
  }
  else if (Mode == 4 && buttonPressed && modeVal2){
    buttonPressed = 0;
    modeVal2 = 0; 
    _leftTillLight = encoderPos * OneHour;
    lastTimeOfUpdate = millis();
    lightOn = encoderPos;
    SetValueCursor();
    Mode = 0;
    encoderPos = 4;  
  }
  else if (Mode == 5 && buttonPressed && !modeVal2){
    buttonPressed = 0;
    modeVal2 = 1;
    SystemLimens.SoilHum = encoderPos;
    EEPROM.update(5,SystemLimens.SoilHum);
    SetValueCursor();
    lcd.setCursor(3,0);
    lcd.print("[%]");
    lcd.setCursor(8,1);
    lcd.print("    ");
    encoderPos = SystemLimens.WaterVolume;
  }
  else if (Mode == 5 && buttonPressed && modeVal2) {
    buttonPressed = 0;
    modeVal2 = 0;
    SystemLimens.WaterVolume = encoderPos;
    EEPROM.update(8,SystemLimens.WaterVolume);
    lcd.setCursor(8,1);
    lcd.print("[dL]");    
    SetValueCursor();
    Mode = 0;
    encoderPos = 5;
  }
  else if (Mode == 6 && buttonPressed && !modeVal2) {
    buttonPressed = 0;
    modeVal2 = 1;
    SystemLimens._SoilHum_Low = analogRead(RHsPin) / 4;
    EEPROM.update(6,SystemLimens._SoilHum_Low);
    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("wet");
  }
  else if (Mode == 6 && buttonPressed && modeVal2) {
    buttonPressed = 0;
    modeVal2 = 0;
    SystemLimens._SoilHum_Hi = analogRead(RHsPin) / 4;
    EEPROM.update(7,SystemLimens._SoilHum_Hi);
    Mode = 0;
    encoderPos = 0;
  }
} 

//Rotary encoder interrupt service routine for one encoder pin
void PinA(){
  cli(); //stop interrupts happening before we read pin values
  reading = PIND & 0xC; // read all eight pin values then strip away all but pinA and pinB's values
  if(reading == B00001100 && aFlag) { //check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
    encoderPos --; //decrement the encoder's position count
    bFlag = 0; //reset flags for the next turn
    aFlag = 0; //reset flags for the next turn
  }
  else if (reading == B00000100) bFlag = 1; //signal that we're expecting pinB to signal the transition to detent from free rotation
  sei(); //restart interrupts
}

//Rotary encoder interrupt service routine for the other encoder pin
void PinB(){
  cli(); //stop interrupts happening before we read pin values
  reading = PIND & 0xC; //read all eight pin values then strip away all but pinA and pinB's values
  if (reading == B00001100 && bFlag) { //check that we have both pins at detent (HIGH) and that we are expecting detent on this pin's rising edge
    encoderPos ++; //increment the encoder's position count
    bFlag = 0; //reset flags for the next turn
    aFlag = 0; //reset flags for the next turn
  }
  else if (reading == B00001000) aFlag = 1; //signal that we're expecting pinA to signal the transition to detent from free rotation
  sei(); //restart interrupts
}
// end of sketch!

void DisplayModeLCD(char v1[],char v2[],int val1,int val2) 
{
  lcd.clear();
  char val1s[4];
  char val2s[4];
  sprintf(val1s,"%d",val1);
  sprintf(val2s,"%d",val2);
  lcd.home();
  lcd.print(v1);
  if (val1 != -27) {
    lcd.setCursor(13,0);
    lcd.print(val1s);
  }
  lcd.setCursor(0,1);
  lcd.print(v2);
  if (val2 != -27) {
    lcd.setCursor(13,1);
    lcd.print(val2s);
  }
}

void ChangeValue(int val,byte r) //ChangeValue
{
  char str[4];
  lcd.setCursor(13,r);
  lcd.print("   ");
  sprintf(str,"%d",val);
  lcd.setCursor(13,r);
  lcd.print(str);
}

void SetValueCursor() // the ">" sign when changing values 
{
  if (modeVal2 == 0) {
    lcd.setCursor(12,0);
    lcd.print(">");
  }
  else {
    lcd.setCursor(12,0);
    lcd.print(" ");
    lcd.setCursor(12,1);
    lcd.print(">");
  }
}

void CheckChangeLights()
{
  if (_leftTillLight > 0) {
    if (millis() >= _leftTillLight + lastTimeOfUpdate) {
      _leftTillLight = 0;
      _lightRise = millis();
    }
    digitalWrite(LightRelayPin,HIGH);
    _lightOn=(_leftTillLight+lastTimeOfUpdate-millis())/(float)OneHour;
    lightOn = (int) _lightOn;
    if ((_lightOn - lightOn) >= 0.5) {
      lightOn++; 
    }
    lightOn = -lightOn;
  }
  else {
    if (millis() >= _lightDaily + _lightRise) {
      if (millis() - _lightRise >= 24 * OneHour) {
        _lightRise = millis();
      }
      digitalWrite(LightRelayPin,HIGH);
      _lightOn=-(millis()-_lightRise-(24*OneHour))/(float)OneHour;
      lightOn = (int) _lightOn;
      if ((_lightOn - lightOn) >= 0.5) {
        lightOn++;
      }
      lightOn = -lightOn;
    }
    else {
      digitalWrite(LightRelayPin,LOW); 
      _lightOn=(millis()-_lightRise)/(float)OneHour;
      lightOn = (int) _lightOn;
      if ((_lightOn - lightOn) >= 0.5) {
        lightOn++; 
      }  
    }
  }
}

void CheckChangeVents()
{   
    _fanPower= constrain(DHTTemperature_, SystemLimens.DHTTemperature_Low, SystemLimens.DHTTemperature_Hi);
    fanPower = map(_fanPower, SystemLimens.DHTTemperature_Low, SystemLimens.DHTTemperature_Hi, 25, 255);    
    if (DHTHumidity_ < SystemLimens.DHTHumidity_Low + 3) {
      fanPower = 25;
    }
    else if (DHTHumidity_ > SystemLimens.DHTHumidity_Hi) {
      fanPower = 255;
    }
    analogWrite(VentTransistorPin,fanPower);
}

void CheckTurnOnAirHumidifier()
{
  if ((DHTHumidity_ < SystemLimens.DHTHumidity_Low + 2) && !airHumidify) {
    airHumidify = 1;
    digitalWrite(HumidifierRelayPin, LOW);
  }
}

void CheckChangeWaterPump() 
{ 
  waterTime = SystemLimens.WaterVolume * msPERdL;
  if (watering) {
    digitalWrite(IrrigationRelayPin, LOW);
    if (startYN) {
      startWatering = millis();
      startYN = 0;
    }
    else if (millis() > startWatering + waterTime){ 
      watering = 0;
    }
  }
  else if (startYN && (soilHum <= SystemLimens.SoilHum) && (soilHum > 0)) {
    watering = 1;
  }  
  else if (millis() > startWatering + waterTimeOut) {
    startYN = 1;
  }
  else {
    digitalWrite(IrrigationRelayPin, HIGH);
  }
}
