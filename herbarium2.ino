
#include <Wire.h> //I2C library
#define DS1307_ADDRESS 0x68
byte zero = 0x00; //workaround for issue #527
byte fetch_humidity_temperature(unsigned int *p_Humidity, unsigned int *p_Temperature);
void print_float(float f, int num_digits);
//const int lowWater      = A0;
const int moistureSense = 9;
const int safeFan       = 10;
const int humidFan      = 11;
const int pump          = 12;
const int lights        = 13;
const long printInterval = 1000;
const long pumpRunTime = 120000; //run for 2 minutes.
unsigned long printMillis = 0;
unsigned long pumpMillis = 0;
unsigned long currentMillis;
byte _status;
unsigned int H_dat, T_dat, date_dat;
float RH, T_C;
long date[9];
bool doHumidify = true;
bool doWater = false;
bool hot = false;
bool fault = false;

#define TRUE 1
#define FALSE 0

void setup(void)
{
   pinMode(moistureSense, INPUT_PULLUP);
   pinMode(safeFan, OUTPUT);
   pinMode(humidFan, OUTPUT);
   pinMode(pump, OUTPUT);
   pinMode(lights, OUTPUT);

   Serial.begin(9600);
   Wire.begin();
   pinMode(4, OUTPUT);
   digitalWrite(4, HIGH); // this turns on the HIH3610
   //setDateTime(); //MUST CONFIGURE IN FUNCTION
   //Delay for I2C Initialize?
   delay(5000);
   Serial.println(">>>>>>>>>>>>>>>>>>>>>>>>");  // just to be sure things are working

}

void loop(void){
  //Main Operational Function
  while(1){
    //something bad happened. Go into safe mode.
    if(fault){
      digitalWrite(humidFan, LOW);
      digitalWrite(safeFan, HIGH);
      digitalWrite(lights, HIGH);
    }
    else{
    //Time Control Variable
    currentMillis = millis();
    //Make sure millis() did not roll over. 
    if ((currentMillis < printMillis) || (currentMillis < pumpMillis)) {
      printMillis = currentMillis;
      pumpMillis = currentMillis;
    }
    //Get Temp and RH data from HIH-6130
    _status = get_humidity_temperature(&H_dat, &T_dat);
    RH = (float) H_dat * 6.10e-3;
    T_C = (float) T_dat * 1.007e-2 - 40.0;
    //Get Date from DS-1307
    get_date();

    //Moisture Control
    moistureControl();

    //Temp & Humidity Control
    climateControl();

    //Light Control
    lightControl();

    //Begin Serial Diagnostics. Verify HIH-6130 Mode and RTC Clock Output

    if (currentMillis - printMillis >= printInterval){
      printMillis = currentMillis;
      printHumidityAndTemp();
      printDate();
      //End Serial Diagnostics
    }
   }
  }
}

/*I did not write this, but a pretty cool work around to simplify the function.
* return status but also use globals to "return" humidity and temp data
*/
byte get_humidity_temperature(unsigned int *p_H_dat, unsigned int *p_T_dat)
{
      byte address, Hum_H, Hum_L, Temp_H, Temp_L, _status;
      unsigned int H_dat, T_dat;
      address = 0x27;;
      Wire.beginTransmission(address);
      Wire.endTransmission();
      delay(100);

      Wire.requestFrom((int)address, (int) 4);
      Hum_H = Wire.read();
      Hum_L = Wire.read();
      Temp_H = Wire.read();
      Temp_L = Wire.read();
      Wire.endTransmission();

      _status = (Hum_H >> 6) & 0x03;
      Hum_H = Hum_H & 0x3f;
      H_dat = (((unsigned int)Hum_H) << 8) | Hum_L;
      T_dat = (((unsigned int)Temp_H) << 8) | Temp_L;
      T_dat = T_dat / 4;
      *p_H_dat = H_dat;
      *p_T_dat = T_dat;
      return(_status);
}

void print_float(float f, int num_digits)
{
//    int f_int;
    int pows_of_ten[4] = {1, 10, 100, 1000};
    int multiplier, whole, fract, d, n;

    multiplier = pows_of_ten[num_digits];
    if (f < 0.0)
    {
        f = -f;
        Serial.print("-");
    }
    whole = (int) f;
    fract = (int) (multiplier * (f - (float)whole));

    Serial.print(whole);
    Serial.print(".");

    for (n=num_digits-1; n>=0; n--) // print each digit with no leading zero suppression
    {
         d = fract / pows_of_ten[n];
         Serial.print(d);
         fract = fract % pows_of_ten[n];
    }
}

void setDateTime(){

  byte second =      00; //0-59
  byte minute =      14; //0-59
  byte hour =        21; //0-23
  byte weekDay =     4; //1-7
  byte monthDay =    10; //1-31
  byte month =       11; //1-12
  byte year  =       16; //0-99

  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero); //stop Oscillator

  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write(decToBcd(weekDay));
  Wire.write(decToBcd(monthDay));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));

  Wire.write(zero); //start

  Wire.endTransmission();

}

byte decToBcd(byte val){
// Convert normal decimal numbers to binary coded decimal
  return ( (val/10*16) + (val%10) );
}

byte bcdToDec(byte val)  {
// Convert binary coded decimal to normal decimal numbers
  return ( (val/16*10) + (val%16) );
}


void get_date(){
  // Reset the register pointer
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 7);

  date[0] = bcdToDec(Wire.read());
  date[1] = bcdToDec(Wire.read());
  date[2] = bcdToDec(Wire.read() & 0b111111); //24 hour time
  date[3] = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
  date[4] = bcdToDec(Wire.read());
  date[5] = bcdToDec(Wire.read());
  date[6] = bcdToDec(Wire.read());
  }

void printDate(){
  //print the date EG   3/1/11 23:59:59
  Serial.print(date[5]);
  Serial.print("/");
  Serial.print(date[4]);
  Serial.print("/");
  Serial.print(date[6]);
  Serial.print(" ");
  Serial.print(date[2]);
  Serial.print(":");
  Serial.print(date[1]);
  Serial.print(":");
  Serial.println(date[0]);
  Serial.print("Day of Year:");
  Serial.println(date[7]);
  Serial.print("Time in Secs:");
  Serial.println(date[8]);

}

void printHumidityAndTemp(){
  switch(_status){
    case 0:  Serial.println("Normal.");
      break;

    case 1:  Serial.println("Stale Data.");
      break;

    case 2:  Serial.println("In command mode.");
      break;

    default: Serial.println("Diagnostic.");
     break;
    }

  print_float(RH, 1);
  Serial.print("  ");
  print_float(T_C, 2);
  Serial.println();
  Serial.println();
  Serial.println();
  }

void moistureControl(){
  if (digitalRead(moistureSense) == 0){
    //Pump has not been started
    if (!doWater){
      //record the start time
      pumpMillis = currentMillis;
      //record the pump was started
      doWater = true;
      //start the pump
      digitalWrite(pump, HIGH);
    }
    else {
      //pump was previously started
      //check how long it has been running
      //if it's been running for longer than set time, have a problem.
      if (currentMillis - pumpMillis >= pumpRunTime){
        digitalWrite(pump, LOW);
        fault = true;
        return;
      }
      else {
        digitalWrite(pump, HIGH);
      }
    }
   }
   //our moisture sensor has tripped. make sure the pump runs for set time.
   else { 
    //check how long.
    if ((currentMillis - pumpMillis < pumpRunTime) && doWater){
      digitalWrite(pump, HIGH);
    }
    else {
      //pump has run long enough.
      digitalWrite(pump, LOW);
      doWater = false;
    }
  }
}
void climateControl(){
  //Temp at grow area is about 3 degrees cooler than where the temp sensor is
  if (T_C > 32.5) {
    digitalWrite(safeFan, HIGH);
  }
  else {
    digitalWrite(safeFan, LOW);
  }
  
  // really hot! turn the fans on. about 90F at plant location.
  if(T_C > 33.9) {
    //humidity fan is normally closed
    fault = true;
    return;
  }
  else if (RH >= 75) {
      digitalWrite(humidFan, HIGH);
      doHumidify = false;
  }
  else if (RH > 65){
    //continue adding humidity to the system
    if (doHumidify) {
      digitalWrite(humidFan, LOW);
      digitalWrite(safeFan, LOW);
    }
    //get some fresh air into the system before doHumidifying again
    else if (RH < 68){
      digitalWrite(humidFan, HIGH);
      digitalWrite(safeFan, HIGH);
    }
  }
  else {
    //Humidity less than 70%. Add moisture
    digitalWrite(humidFan, LOW);
    if (hot) {
      digitalWrite(safeFan, HIGH);
    }
    else {
      digitalWrite(safeFan, LOW);
    }
    doHumidify = true;
  } 
}

void lightControl (){
  int secsPerDayOffset = 60;
  int baseSunrise      = 25200;
  long baseSunset      = 61200;
// int seconds         = date[0];
// int minutes         = date[1];
//int hours            = date[2];
  long day             = date[4];
//int month            = date[5];
  long timeInSecs      = (date[0] + (date[1]*60) + (date[2]*3600));
  int dayOfYear        = 0;
  long sunrise         = 0;
  long sunset          = 0;
  date[8]             = timeInSecs;

  //Calculate Day Of Year
  switch(date[5]){
    case 1:  dayOfYear = day;
             date[7] = dayOfYear;
      break;

    case 2:  dayOfYear = (day + 31);
             date[7] = dayOfYear;
      break;

    case 3:  dayOfYear = (day + 59);
             date[7] = dayOfYear;
      break;

    case 4:  dayOfYear = (day + 90);
             date[7] = dayOfYear;
      break;

    case 5:  dayOfYear = (day + 120);
             date[7] = dayOfYear;
      break;

    case 6:  dayOfYear = (day + 151);
             date[7] = dayOfYear;
      break;

    case 7:  dayOfYear = (day + 181);
             date[7] = dayOfYear;
      break;

    case 8:  dayOfYear = (day + 212);
             date[7] = dayOfYear;
      break;

    case 9:  dayOfYear = (day + 243);
             date[7] = dayOfYear;
      break;

    case 10:  dayOfYear = (day + 273);
              date[7] = dayOfYear;
      break;

    case 11:  dayOfYear = (day + 304);
              date[7] = dayOfYear;
      break;

    case 12:  dayOfYear = (day + 334);
              date[7] = dayOfYear;
      break;
    }
  //Set Sunrise and Sunset Variables
  if (dayOfYear < 183){
    sunrise = (baseSunrise - (dayOfYear * secsPerDayOffset));
    sunset  = (baseSunset  + (dayOfYear * secsPerDayOffset));
    }

  else if (dayOfYear == 183){
    sunrise = (baseSunrise - (dayOfYear * secsPerDayOffset)
            - (.5 * secsPerDayOffset));
    sunset  = (baseSunset  + (dayOfYear * secsPerDayOffset)
            + (.5 * secsPerDayOffset));
    }

  else {
    sunrise = (baseSunrise - ((365 - dayOfYear) * secsPerDayOffset));
    sunset  = (baseSunset  + ((365 - dayOfYear) * secsPerDayOffset));
    }

  if ((timeInSecs >= sunrise) && (timeInSecs <= sunset)){
    digitalWrite(lights, LOW);
  }
  else {
    digitalWrite(lights, HIGH);
  }
 }


