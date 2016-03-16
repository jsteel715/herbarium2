    
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
unsigned long modMillis;
byte _status;
unsigned int H_dat, T_dat, date_dat;
float RH, T_C;
long date[9];
bool humidify = true; 

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
    //Time Control Variable
    modMillis = (millis() % 1000);
    
    //Get Temp and RH data from HIH-6130
    _status = get_humidity_temperature(&H_dat, &T_dat);
    RH = (float) H_dat * 6.10e-3;
    T_C = (float) T_dat * 1.007e-2 - 40.0;
    //Get Date from DS-1307
    get_date(date);
    
    //Moisture Control
    moistureControl(&moistureSense);
    
    //Temp & Humidity Control
    climateControl(&RH, &T_C);
      
    //Light Control
    lightControl(date, &T_C);
    
    //Begin Serial Diagnostics. Verify HIH-6130 Mode and RTC Clock Output
    
    if (modMillis == 0){
      printHumidityAndTemp(_status, RH, T_C);
      printDate(date);
      //End Serial Diagnostics
      }
   }
}

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

  byte second =      05; //0-59
  byte minute =      51; //0-59
  byte hour =        15; //0-23
  byte weekDay =     1; //1-7
  byte monthDay =    13; //1-31
  byte month =       3; //1-12
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


void get_date(long element[]){
  // Reset the register pointer
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 7);

  element[0] = bcdToDec(Wire.read());
  element[1] = bcdToDec(Wire.read());
  element[2] = bcdToDec(Wire.read() & 0b111111); //24 hour time
  element[3] = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
  element[4] = bcdToDec(Wire.read());
  element[5] = bcdToDec(Wire.read());
  element[6] = bcdToDec(Wire.read());
  }

void printDate(long dateArray[]){
  //print the date EG   3/1/11 23:59:59
  Serial.print(dateArray[5]);
  Serial.print("/");
  Serial.print(dateArray[4]);
  Serial.print("/");
  Serial.print(dateArray[6]);
  Serial.print(" ");
  Serial.print(dateArray[2]);
  Serial.print(":");
  Serial.print(dateArray[1]);
  Serial.print(":");
  Serial.println(dateArray[0]);
  Serial.print("Day of Year:");
  Serial.println(dateArray[7]);
  Serial.print("Time in Secs:");
  Serial.println(dateArray[8]);

}

void printHumidityAndTemp(byte stat, float relHum, float temp){
  switch(stat){
    case 0:  Serial.println("Normal.");
      break;
       
    case 1:  Serial.println("Stale Data.");
      break;
          
    case 2:  Serial.println("In command mode.");
      break;
          
    default: Serial.println("Diagnostic."); 
     break; 
    }          
    
  print_float(relHum, 1);
  Serial.print("  ");
  print_float(temp, 2);
  Serial.println();
  }  

void moistureControl(const int *moistureSense){
  int rainSensor = digitalRead(*moistureSense);
  
  if (rainSensor == 0){
    digitalWrite(pump, HIGH);
      }
    else {
      digitalWrite(pump, LOW);
      }
  }

void climateControl(float *Rel_Hum, float *Temp_Celcius){
  if(*Temp_Celcius > 32) {
      digitalWrite(safeFan, HIGH);
      }
    else {
      digitalWrite(safeFan, LOW);
      }
  }
  
void humidControl (float *Rel_Hum){
  if(*Rel_Hum > 80) {
     digitalWrite(humidFan, HIGH);
  }
  else {
    digitalWrite(humidFan, LOW);
  }
}
  
void lightControl (long pdata[], float *T_C){
  int secsPerDayOffset = 60;
  int baseSunrise      = 25200;
  long baseSunset      = 61200;
// int seconds         = pdata[0];
// int minutes         = pdata[1];
//int hours            = pdata[2];
  long day             = pdata[4];
//int month            = pdata[5];
  long timeInSecs      = (pdata[0] + (pdata[1]*60) + (pdata[2]*3600));
  int dayOfYear        = 0;
  long sunrise         = 0;
  long sunset          = 0;
  pdata[8]             = timeInSecs;
  
  //Calculate Day Of Year
  switch(pdata[5]){
    case 1:  dayOfYear = day;
             pdata[7] = dayOfYear;
      break;
        
    case 2:  dayOfYear = (day + 31);
             pdata[7] = dayOfYear;
      break;
        
    case 3:  dayOfYear = (day + 59);
             pdata[7] = dayOfYear;
      break;
        
    case 4:  dayOfYear = (day + 90);
             pdata[7] = dayOfYear;  
      break;
        
    case 5:  dayOfYear = (day + 120);
             pdata[7] = dayOfYear;
      break;
        
    case 6:  dayOfYear = (day + 151);
             pdata[7] = dayOfYear;
      break;
        
    case 7:  dayOfYear = (day + 181);
             pdata[7] = dayOfYear;
      break;
        
    case 8:  dayOfYear = (day + 212);
             pdata[7] = dayOfYear;      
      break;
        
    case 9:  dayOfYear = (day + 243);
             pdata[7] = dayOfYear;
      break;
        
    case 10:  dayOfYear = (day + 273);
              pdata[7] = dayOfYear;
      break;
        
    case 11:  dayOfYear = (day + 304);
              pdata[7] = dayOfYear;
      break;
        
    case 12:  dayOfYear = (day + 334);
              pdata[7] = dayOfYear;
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

  //Keep lights off if overheating
  if (*T_C >= 34){
    digitalWrite(lights, HIGH); 
  }
  else if ((timeInSecs >= sunrise) && (timeInSecs <= sunset)){
    digitalWrite(lights, LOW);
  
  }
  else {
    digitalWrite(lights, HIGH);
  }  
 } 
