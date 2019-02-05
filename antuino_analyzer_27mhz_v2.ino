#include <Wire.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>

//#define XTAL_FREQ 24996900l;             // Frequency of Quartz-Oszillator
uint32_t xtal_freq_calibrated = 27000000l;
#define MASTER_CAL 0
#define LAST_FREQ 4
#define OPEN_HF 8
#define OPEN_VHF 12
#define OPEN_UHF 16

#define SI_CLK0_CONTROL  16      // Register definitions
#define SI_CLK1_CONTROL 17
#define SI_CLK2_CONTROL 18


unsigned long f = 10000000l;
#define IF_FREQ  (24991000l)
#define MODE_ANTENNA_ANALYZER 0
#define MODE_MEASUREMENT_RX 1
#define MODE_NETWORK_ANALYZER 2
unsigned long mode = MODE_ANTENNA_ANALYZER;

LiquidCrystal lcd(8,9,10,11,12,13);
char b[32], c[32], buff[32], serial_in[32];
int return_loss;
unsigned long frequency = 10000000l;
unsigned long fromFrequency=14150000;
unsigned long toFrequency=30000000;
//int openReading = 93; // in dbm
int openHF = 96;
int openVHF = 96;
int openUHF = 68;

int dbmOffset = -114;

int menuOn = 0;
unsigned long timeOut = 0;

/* for reading and writing from serial port */
unsigned char serial_in_count = 0;

void active_delay(int delay_by){
  unsigned long timeStart = millis();

  while (millis() - timeStart <= delay_by) {
      //Background Work      
  }
}

int calibrateClock(){
  int knob = 0;
  int32_t prev_calibration;


  //keep clear of any previous button press
  while (btnDown())
    active_delay(100);
  active_delay(100);

  prev_calibration = xtal_freq_calibrated;
  xtal_freq_calibrated = 27000000l;

  si5351aSetFrequency_clk1(10000000l);  
  ltoa(xtal_freq_calibrated - 27000000l, c, 10);
  printLine2(c);

  while (!btnDown())
  {
    knob = enc_read();

    if (knob > 0)
      xtal_freq_calibrated += 10;
    else if (knob < 0)
      xtal_freq_calibrated -= 10;
    else 
      continue; //don't update the frequency or the display

    si5351aSetFrequency_clk1(10000000l);  
      
    ltoa(xtal_freq_calibrated - 27000000l, c, 10);
    printLine2(c);     
  }

  printLine2("Calibration set!");
  EEPROM.put(MASTER_CAL, xtal_freq_calibrated);

  while(btnDown())
    active_delay(50);
  active_delay(100);
}


#define ENC_A (A0)
#define ENC_B (A1)
#define FBUTTON (A2)
#define BACK_LIGHT   (A3)
#define DBM_READING (A6)


const int PROGMEM vswr[] = {
999,
174,
87,
58,
44,
35,
30,
26,
23,
21,
19,
18,
17,
16,
15,
14,
14,
13,
13,
12,
12,
12,
12,
11,
11,
11,
11,
1,
10,
10,
10 
};

void printLine1(char *c){
    lcd.setCursor(0, 0);
    lcd.print(c);
}

void printLine2(char *c){
  lcd.setCursor(0, 1);
  lcd.print(c);
}



int enc_prev_state = 3;

//returns true if the button is pressed
int btnDown(){
  if (digitalRead(FBUTTON) == HIGH)
    return 0;
  else
    return 1;
}

void resetTimer(){
  //push the timer to the next
  timeOut = millis() + 10000l;
  digitalWrite(BACK_LIGHT, HIGH);
}


void checkTimeout(){
  unsigned long last_freq;

  if (timeOut > millis())
    return;
  digitalWrite(BACK_LIGHT, LOW);
  EEPROM.get(LAST_FREQ, last_freq);
  if (last_freq != frequency)
    EEPROM.put(LAST_FREQ, frequency);
}

byte enc_state (void) {
    return (analogRead(ENC_A) > 500 ? 1 : 0) + (analogRead(ENC_B) > 500 ? 2: 0);
}

int enc_read(void) {
  int result = 0; 
  byte newState;
  int enc_speed = 0;
  
  long stop_by = millis() + 50;
  
  while (millis() < stop_by) { // check if the previous state was stable
    newState = enc_state(); // Get current state  
    
    if (newState != enc_prev_state)
      delay (1);
    
    if (enc_state() != newState || newState == enc_prev_state)
      continue; 
    //these transitions point to the encoder being rotated anti-clockwise
    if ((enc_prev_state == 0 && newState == 2) || 
      (enc_prev_state == 2 && newState == 3) || 
      (enc_prev_state == 3 && newState == 1) || 
      (enc_prev_state == 1 && newState == 0)){
        result--;
      }
    //these transitions point o the enccoder being rotated clockwise
    if ((enc_prev_state == 0 && newState == 1) || 
      (enc_prev_state == 1 && newState == 3) || 
      (enc_prev_state == 3 && newState == 2) || 
      (enc_prev_state == 2 && newState == 0)){
        result++;
      }
    enc_prev_state = newState; // Record state for next pulse interpretation
    enc_speed++;
    active_delay(1);
  }
  return(result);
}

// this builds up the top line of the display with frequency and mode
void updateDisplay() {
  int vswr_reading;
  // tks Jack Purdum W8TEE
  // replaced fsprint commmands by str commands for code size reduction

  memset(c, 0, sizeof(c));
  memset(b, 0, sizeof(b));

  ultoa(frequency, b, DEC);


  //one mhz digit if less than 10 M, two digits if more

   if (frequency >= 100000000l){
    strncat(c, b, 3);
    strcat(c, ".");
    strncat(c, &b[3], 3);
    strcat(c, ".");
    strncat(c, &b[6], 3);
  }
  else if (frequency >= 10000000l){
    strcpy(c, " ");
    strncat(c, b, 2);
    strcat(c, ".");
    strncat(c, &b[2], 3);
    strcat(c, ".");
    strncat(c, &b[5], 3);
  }
  else {
    strcpy(c, "  ");
    strncat(c, b, 1);
    strcat(c, ".");
    strncat(c, &b[1], 3);    
    strcat(c, ".");
    strncat(c, &b[4], 3);
  }
  if (mode == MODE_ANTENNA_ANALYZER)
    strcat(c, "  ANT");
  else if (mode == MODE_MEASUREMENT_RX)
    strcat(c, "  MRX");
  else if (mode == MODE_NETWORK_ANALYZER)
    strcat(c, "  SNA");
  printLine1(c);

  if (mode == MODE_ANTENNA_ANALYZER){
    return_loss = openReading(frequency) - analogRead(DBM_READING)/5;
    if (return_loss > 30)
       return_loss = 30;
    if (return_loss < 0)
       return_loss = 0;
    
    vswr_reading = pgm_read_word_near(vswr + return_loss);
    sprintf (c, "%ddb VSWR=%d.%01d", return_loss, vswr_reading/10, vswr_reading%10);
  }else if (mode == MODE_MEASUREMENT_RX){
    sprintf(c, "%d dbm         ", analogRead(DBM_READING)/5 + dbmOffset);
  }
  else if (mode == MODE_NETWORK_ANALYZER) {
    sprintf(c, "%d dbm         ", analogRead(DBM_READING)/5 + dbmOffset);  
  }
  printLine2(c);
}


long prev_freq = 0;
void takeReading(long newfreq){
  long local_osc;

  if (newfreq < 20000l)
      newfreq = 20000l;
  if (newfreq < 150000000l)
  {
    if (newfreq < 50000000l)
      local_osc = newfreq + IF_FREQ;
    else
      local_osc = newfreq - IF_FREQ;
  } else {
    newfreq = newfreq / 3;
    local_osc = newfreq - IF_FREQ/3;
  }

  if (prev_freq != newfreq){
    switch(mode){
    case MODE_MEASUREMENT_RX:
      si5351aSetFrequency_clk2(local_osc);    
    break;
    case MODE_NETWORK_ANALYZER:
      si5351aSetFrequency_clk2(local_osc);  
      si5351aSetFrequency_clk0(newfreq);
      Serial.print(local_osc);
      Serial.print(' ');
      Serial.println(newfreq);
    break;
    default:
      si5351aSetFrequency_clk2(local_osc);  
      si5351aSetFrequency_clk1(newfreq);
    }      
    prev_freq = newfreq;
  }     
}

void setup() {
  lcd.begin(16, 2);
  Wire.begin();
  Serial.begin(9600);
  Serial.flush();  
  Serial.println("i Antuino v1.01");
  analogReference(DEFAULT);

  unsigned long last_freq = 0;
  EEPROM.get(MASTER_CAL, xtal_freq_calibrated);
  EEPROM.get(LAST_FREQ, last_freq);
  EEPROM.get(OPEN_HF, openHF);
  EEPROM.get(OPEN_VHF, openVHF);
  EEPROM.get(OPEN_UHF, openUHF);

  if (0< last_freq && last_freq < 500000000l)
      frequency = last_freq;
    
  if (xtal_freq_calibrated < 26900000l || xtal_freq_calibrated > 27100000l)
    xtal_freq_calibrated = 27000000l;

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(FBUTTON, INPUT_PULLUP);
  pinMode(BACK_LIGHT, OUTPUT);
  
  digitalWrite(BACK_LIGHT, LOW);  
  printLine1("Antuino v1.1");
  delay(2000);  
  digitalWrite(BACK_LIGHT, HIGH);

  if (btnDown()){
    calibrateClock();
  }
  
  si5351aOutputOff(SI_CLK0_CONTROL);
  takeReading(frequency);
  updateDisplay();
}

int menuBand(int btn){
  int knob = 0;
  int band, prev = 0, r;
  unsigned long offset;
  unsigned long prev_freq;


  if (!btn){
   printLine2("Band Select    \x7E");
   return;
  }

  printLine2("Band Select:    ");
  //wait for the button menu select button to be lifted)
  while (btnDown())
    active_delay(50);
  active_delay(50);    

  while(!btnDown()){

    prev_freq = frequency;
    knob = enc_read();
    checkTimeout();
    
    if (knob != 0){

      resetTimer();     

      if (knob < 0 && frequency > 3000000l)
        frequency -= 250000l;
      if (knob > 0 && frequency < 500000000l)
        frequency += 250000l;

      if (prev_freq <= 150000000l && frequency > 150000000l)
        frequency = 350000000l;
      if (prev_freq >= 350000000l && frequency < 350000000l)
        frequency = 149999000l; 

       
      takeReading(frequency);
      updateDisplay();
      printLine2("Band Select");
    }
    else{
      r = analogRead(DBM_READING);
      if (r != prev){
        takeReading(frequency);
        updateDisplay();
        prev = r;    
      }
    }
    active_delay(20);
  }

  menuOn = 0;
  while(btnDown())
    active_delay(50);
  active_delay(50);
  
  printLine2("");
  updateDisplay();
}


int tuningClicks = 0;
int tuningSpeed = 0;
void doTuning(){
  int s;
  unsigned long prev_freq;

  s = enc_read();

  if (s < 0 && tuningClicks > 0)
    tuningClicks = s;
  else if (s > 0 && tuningClicks < 0)
    tuningClicks = s;
  else
    tuningClicks += s;

  tuningSpeed = (tuningSpeed * 4 + s)/5;
  //Serial.print(tuningSpeed);
  //Serial.print(",");
  //Serial.println(tuningClicks);
  if (s != 0){
    resetTimer();
    prev_freq = frequency;

    if (s > 4 && tuningClicks > 100)
      frequency += 200000l;
    else if (s > 2)
      frequency += 1000l;
    else if (s > 0)
      frequency +=  20l;
    else if (s > -2)
      frequency -= 20l;
    else if (s > -4)
      frequency -= 1000l;
    else if (tuningClicks < -100)
      frequency -= 200000l;

    takeReading(frequency);
    updateDisplay();
  }
}

void doTuning2(){
  int s;
  unsigned long prev_freq;

  s = enc_read();

  if (s < 0 && tuningClicks > 0)
    tuningClicks = s;
  else if (s > 0 && tuningClicks < 0)
    tuningClicks = s;
  else
    tuningClicks += s;

  tuningSpeed = (tuningSpeed * 4 + s)/5;
//  Serial.print(tuningSpeed);
//  Serial.print(",");
//  Serial.println(tuningClicks);
  if (s != 0){
    resetTimer();
    prev_freq = frequency;

    if (tuningSpeed >= 5 && tuningClicks > 100)
      frequency += 10000000l;
    else if (tuningSpeed== 4 && tuningClicks > 100)
      frequency += 1000000l;
    else if (tuningSpeed == 3)
      frequency += 100000l;
    else if (tuningSpeed == 2)
      frequency += 10000l;
    else if (tuningSpeed == 1)
      frequency += 1000l;
    else if (tuningSpeed == 0 && s > 0)
      frequency += 100l;
    else if (tuningSpeed == 0 && s < 0)
      frequency -= 100l;    
    else if (tuningSpeed == -1)
      frequency -= 1000l;
    else if (tuningSpeed == -2)
      frequency -= 10000;
    else if (tuningSpeed == -3)
      frequency -= 100000l;
    else if (tuningSpeed== -4 && tuningClicks < -100 && frequency > 1000000l)
      frequency -= 1000000l;
    else if (tuningSpeed <= -5 && tuningClicks < -100 && frequency > 10000000l)
      frequency -= 10000000l;

    if (frequency < 100000l)
      frequency = 100000l;
    if (frequency > 450000000l)
      frequency = 450000000l;
    if (tuningSpeed < 0 && frequency < 350000000l && frequency > 150000000l)
      frequency = 149999000l;
    if (tuningSpeed > 0 && frequency > 150000000l && frequency < 350000000l)
      frequency  = 350000000l;
    takeReading(frequency);
    updateDisplay();
  }
}



int menuSelectAntAnalyzer(int btn){
  if (!btn){
   printLine2("Ant. Analyzer  \x7E");
  }
  else {
    mode = MODE_ANTENNA_ANALYZER;
    printLine2("Ant. Analyzer!   ");
    active_delay(500);
    printLine2("");
    menuOn = 0;

    //switch off just the tracking source
    si5351aOutputOff(SI_CLK0_CONTROL);
    takeReading(frequency);
    updateDisplay();
  }
}

int readOpen(unsigned long f){
  int i, r;

  takeReading(f);
  delay(100);
  r = 0;
  for (i = 0; i < 10; i++){
    r += analogRead(DBM_READING)/5;
    delay(50);
  }
  sprintf(b, "%ld: %d  ", f, r/10);
  printLine2(b);
  delay(1000);
  
  return r/10;
}

int menuCalibrate2(int btn){
  if (!btn){
   printLine2("Calibrate SWR  \x7E");
  }
  else {
    printLine1("Disconnect ant &");
    printLine2("Click to Start..");

    //wait for the button to be raised up
    while(btnDown())
      active_delay(50);
    active_delay(50);  //debounce

    //wait for a button down
    while(!btnDown())
      active_delay(50);

    printLine1("Calibrating.....");
    
    int i, r;
    mode = MODE_ANTENNA_ANALYZER;
    printLine2("");
    delay(100);
    r = readOpen(20000000l);
    EEPROM.put(OPEN_HF, r);
    r = readOpen(140000000l);
    EEPROM.put(OPEN_VHF, r);
    r = readOpen(440000000l);    EEPROM.put(OPEN_UHF, r);
    
    menuOn = 0;
   
    printLine1("Calibrating.....");
    printLine2("Done!           ");
    delay(1000);
    
    //switch off just the tracking source
    si5351aOutputOff(SI_CLK0_CONTROL);
    takeReading(frequency);
    updateDisplay();
  }
}



int menuSelectMeasurementRx(int btn){
  if (!btn){
   printLine2("Measurement RX \x7E");
  }
  else {
    mode = MODE_MEASUREMENT_RX;
    printLine2("Measurement RX!");    
    active_delay(500);
    printLine2("");
    menuOn = 0;

    //only allow the local oscillator to work
    si5351aOutputOff(SI_CLK0_CONTROL);
    si5351aOutputOff(SI_CLK1_CONTROL);    
    takeReading(frequency);
    updateDisplay();
  }
}

int menuSelectNetworkAnalyzer(int btn){
  if (!btn){
   printLine2("SNA            \x7E");   
  }
  else {
    mode = MODE_NETWORK_ANALYZER;
    printLine2("SNA!           ");    
    active_delay(500);
    printLine2("");
    menuOn = 0;

    //switch off the clock2 that drives the return loss bridge
    si5351aOutputOff(SI_CLK1_CONTROL);        
    takeReading(frequency);
    updateDisplay();
  }
}

void menuExit(int btn){

  if (!btn){
      printLine2("Exit Menu      \x7E");
  }
  else{
      printLine2("Exiting...");
      active_delay(500);
      printLine2("");
      updateDisplay();
      menuOn = 0;
  }
}

void doMenu(){
  int select=0, i,btnState;

  //wait for the button to be raised up
  while(btnDown())
    active_delay(50);
  active_delay(50);  //debounce
  
  menuOn = 2;
  
  while (menuOn){
    i = enc_read();
    btnState = btnDown();
    
    checkTimeout();

    if (i != 0)
        resetTimer();
        
    if (select + i < 60)
      select += i;
 
    if (i < 0 && select - i >= 0)
      select += i;      //caught ya, i is already -ve here, so you add it

    if (select < 10)
      menuBand(btnState);
    else if (select < 20)
      menuSelectAntAnalyzer(btnState);
    else if (select < 30)
      menuSelectMeasurementRx(btnState);
    else if (select < 40)
      menuSelectNetworkAnalyzer(btnState);
    else if (select < 50)
      menuCalibrate2(btnState);
    else
      menuExit(btnState);
  }

  //debounce the button
  while(btnDown())
    active_delay(50);
  active_delay(50);
}

void checkButton(){
  int i, t1, t2, knob, new_knob;

  //only if the button is pressed
  if (!btnDown())
    return;
  active_delay(50);
  if (!btnDown()) //debounce
    return;

  doMenu();
  //wait for the button to go up again
  while(btnDown())
    active_delay(10);
  active_delay(50);//debounce
}

int openReading(unsigned long f){
  if (f < 60000000l)
    return openHF;
  else if (f < 150000000l)
    return openVHF;
  else
    return openUHF;
}

void readDetector(unsigned long f){
  int i = openReading(f) - analogRead(DBM_READING)/5;
  sprintf(c, "d%d\n", i);
  Serial.write(c);
}

void doSweep(){
  unsigned long x, stepSize;
  int reading, vswr_reading;

  stepSize = (toFrequency - fromFrequency) / 300;
  Serial.write("begin\n");
  for (x = fromFrequency; x < toFrequency; x = x + stepSize){
    takeReading(x);
    delay(10);
    reading = openReading(x) - analogRead(DBM_READING)/5;
    if (mode == MODE_ANTENNA_ANALYZER){
      if (reading < 0)
        reading = 0;
      vswr_reading = pgm_read_word_near(vswr + reading);
      sprintf (c, "r:%ld:%d:%d\n", x, reading, vswr_reading);
    }else
      sprintf(c, "r:%ld:%d\n", x, analogRead(DBM_READING)/5 + dbmOffset);
    Serial.write(c);
  }
  Serial.write("end\n");
}

char *readNumber(char *p, unsigned long *number){
  *number = 0;

  sprintf(c, "#%s", p);
  while (*p){
    char c = *p;
    if ('0' <= c && c <= '9')
      *number = (*number * 10) + c - '0';
    else 
      break;
     p++;
  }
  return p;
}

char *skipWhitespace(char *p){
  while (*p && (*p == ' ' || *p == ','))
    p++;
  return p;
} 

/* command 'h' */
void sendStatus(){
  Serial.write("helo v1\n");
  sprintf(c, "from %ld\n", fromFrequency);
  Serial.write(c);
   
  sprintf(c, "to %ld\n", toFrequency);
  Serial.write(c);

  sprintf(c, "mode %ld\n", mode);
  Serial.write(c);

}

void parseCommand(char *line){
  unsigned long param = 0;
  char *p = line;
  char command;

  while (*p){
    p = skipWhitespace(p);
    command = *p++;
    
    switch (command){
      case 'f' : //from - start frequency
        p = readNumber(p, &fromFrequency);
        takeReading(fromFrequency);
        break;
      case 'm':
        p = readNumber(p, &mode);
        updateDisplay();
        break;
      case 't':
        p = readNumber(p, &toFrequency);
        break;
      case 'v':
        sendStatus();
        break;
      case 'g':
         doSweep();
         break;
      case 'r':
         readDetector(frequency);
         break;        
      case 'i': /* identifies itself */
        Serial.write("iAntuino 1.1\n");
        break;
    }
  } /* end of the while loop */
}

void acceptCommand(){
  int inbyte = 0;
  inbyte = Serial.read();
  
  if (inbyte == '\n'){
    parseCommand(serial_in);    
    serial_in_count = 0;    
    return;
  }
  
  if (serial_in_count < sizeof(serial_in)){
    serial_in[serial_in_count] = inbyte;
    serial_in_count++;
    serial_in[serial_in_count] = 0;
  }
}

int prev = 0;
void loop() {

  doTuning2();
  checkButton();

  checkTimeout();
  if (Serial.available()>0)
    acceptCommand();    

  delay(50);
  int r = analogRead(DBM_READING);
  if (r != prev){
    takeReading(frequency);
    updateDisplay();
    prev = r;
  }
}


