#include <Wire.h>

#define XTAL_FREQ 25003500l;             // Frequency of Quartz-Oszillator

#define SI_CLK0_CONTROL  16      // Register definitions
#define SI_CLK1_CONTROL 17
#define SI_CLK2_CONTROL 18
#define SI_SYNTH_PLL_A  26
#define SI_SYNTH_PLL_B  34
#define SI_SYNTH_MS_0   42
#define SI_SYNTH_MS_1   50
#define SI_SYNTH_MS_2   58
#define SI_PLL_RESET    177

#define SI_R_DIV_1    0b00000000      // R-division ratio definitions
#define SI_R_DIV_2    0b00010000
#define SI_R_DIV_4    0b00100000
#define SI_R_DIV_8    0b00110000
#define SI_R_DIV_16   0b01000000
#define SI_R_DIV_32   0b01010000
#define SI_R_DIV_64   0b01100000
#define SI_R_DIV_128    0b01110000

#define SI_CLK_SRC_PLL_A  0b00000000
#define SI_CLK_SRC_PLL_B  0b00100000

char buff[32];

void i2cSendRegister (byte regist, byte value){   // Writes "byte" into "regist" of Si5351a via I2C
  Wire.beginTransmission(96);                       // Starts transmission as master to slave 96, which is the
                                                    // I2C address of the Si5351a (see Si5351a datasheet)
  Wire.write(regist);                               // Writes a byte containing the number of the register
  Wire.write(value);                                // Writes a byte containing the value to be written in the register
  Wire.endTransmission();                           // Sends the data and ends the transmission
}

void si5351aOutputOff(uint8_t clk)
{
  //i2c_init();
  
  i2cSendRegister(clk, 0x80);   // Refer to SiLabs AN619 to see bit values - 0x80 turns off the output stage

  //i2c_exit();
}


void setupPLL(uint8_t pll, uint8_t mult, uint32_t num, uint32_t denom)
{
  uint32_t P1;          // PLL config register P1
  uint32_t P2;          // PLL config register P2
  uint32_t P3;          // PLL config register P3

  P1 = (uint32_t)(128 * ((float)num / (float)denom));
  P1 = (uint32_t)(128 * (uint32_t)(mult) + P1 - 512);
  P2 = (uint32_t)(128 * ((float)num / (float)denom));
  P2 = (uint32_t)(128 * num - denom * P2);
  P3 = denom;

  i2cSendRegister(pll + 0, (P3 & 0x0000FF00) >> 8);
  i2cSendRegister(pll + 1, (P3 & 0x000000FF));
  i2cSendRegister(pll + 2, (P1 & 0x00030000) >> 16);
  i2cSendRegister(pll + 3, (P1 & 0x0000FF00) >> 8);
  i2cSendRegister(pll + 4, (P1 & 0x000000FF));
  i2cSendRegister(pll + 5, ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
  i2cSendRegister(pll + 6, (P2 & 0x0000FF00) >> 8);
  i2cSendRegister(pll + 7, (P2 & 0x000000FF));
}

//
// Set up MultiSynth with integer divider and R divider
// R divider is the bit value which is OR'ed onto the appropriate register, it is a #define in si5351a.h
//
void setupMultisynth(uint8_t synth, uint32_t divider, uint8_t rDiv)
{
  uint32_t P1;          // Synth config register P1
  uint32_t P2;          // Synth config register P2
  uint32_t P3;          // Synth config register P3

  P1 = 128 * divider - 512;
  P2 = 0;             // P2 = 0, P3 = 1 forces an integer value for the divider
  P3 = 1;

  i2cSendRegister(synth + 0,   (P3 & 0x0000FF00) >> 8);
  i2cSendRegister(synth + 1,   (P3 & 0x000000FF));
  i2cSendRegister(synth + 2,   ((P1 & 0x00030000) >> 16) | rDiv);
  i2cSendRegister(synth + 3,   (P1 & 0x0000FF00) >> 8);
  i2cSendRegister(synth + 4,   (P1 & 0x000000FF));
  i2cSendRegister(synth + 5,   ((P3 & 0x000F0000) >> 12) | ((P2 & 0x000F0000) >> 16));
  i2cSendRegister(synth + 6,   (P2 & 0x0000FF00) >> 8);
  i2cSendRegister(synth + 7,   (P2 & 0x000000FF));
}

// This example sets up PLL A
// and MultiSynth 0
// and produces the output on CLK0
//
void si5351aSetFrequency(uint32_t frequency)
{
  uint32_t pllFreq;
  uint32_t xtalFreq = XTAL_FREQ;
  uint32_t l;
  float f;
  uint8_t mult;
  uint32_t num;
  uint32_t denom;
  uint32_t divider;

  divider = 900000000 / frequency;// Calculate the division ratio. 900,000,000 is the maximum internal 
                  // PLL frequency: 900MHz
  if (divider % 2) divider--;   // Ensure an even integer division ratio

  pllFreq = divider * frequency;  // Calculate the pllFrequency: the divider * desired output frequency

  mult = pllFreq / xtalFreq;    // Determine the multiplier to get to the required pllFrequency
  l = pllFreq % xtalFreq;     // It has three parts:
  f = l;              // mult is an integer that must be in the range 15..90
  f *= 1048575;         // num and denom are the fractional parts, the numerator and denominator
  f /= xtalFreq;          // each is 20 bits (range 0..1048575)
  num = f;            // the actual multiplier is  mult + num / denom
  denom = 1048575;        // For simplicity we set the denominator to the maximum 1048575

                  // Set up PLL A with the calculated multiplication ratio
  setupPLL(SI_SYNTH_PLL_A, mult, num, denom);
                  // Set up MultiSynth divider 0, with the calculated divider. 
                  // The final R division stage can divide by a power of two, from 1..128. 
                  // reprented by constants SI_R_DIV1 to SI_R_DIV128 (see si5351a.h header file)
                  // If you want to output frequencies below 1MHz, you have to use the 
                  // final R division stage
  setupMultisynth(SI_SYNTH_MS_0, divider, SI_R_DIV_1);
                  // Reset the PLL. This causes a glitch in the output. For small changes to 
                  // the parameters, you don't need to reset the PLL, and there is no glitch
  i2cSendRegister(SI_PLL_RESET, 0xA0);  
                  // Finally switch on the CLK0 output (0x4F)
                  // and set the MultiSynth0 input to be PLL A
  i2cSendRegister(SI_CLK1_CONTROL, 0x4F | SI_CLK_SRC_PLL_A);
}


void si5351aSetFrequency_clk1(uint32_t frequency)
{
  uint32_t pllFreq;
  uint32_t xtalFreq = XTAL_FREQ;
  uint32_t l;
  float f;
  uint8_t mult;
  uint32_t num;
  uint32_t denom;
  uint32_t divider;

  divider = 900000000 / frequency;// Calculate the division ratio. 900,000,000 is the maximum internal 
                  // PLL frequency: 900MHz
  if (divider % 2) divider--;   // Ensure an even integer division ratio

  pllFreq = divider * frequency;  // Calculate the pllFrequency: the divider * desired output frequency
  //sprintf(buff,"pllFreq: %ld", (long)pllFreq);
  //Serial.println(buff);                

  mult = (pllFreq / xtalFreq);    // Determine the multiplier to get to the required pllFrequency
  l = pllFreq % xtalFreq;     // It has three parts:
  f = l;              // mult is an integer that must be in the range 15..90
  f *= 1048575;         // num and denom are the fractional parts, the numerator and denominator
  f /= xtalFreq;          // each is 20 bits (range 0..1048575)
  num = f;            // the actual multiplier is  mult + num / denom
  denom = 1048575;        // For simplicity we set the denominator to the maximum 1048575

  //sprintf(buff,"mult: %d", (int) mult);
  //Serial.println(buff);
  //sprintf(buff,"num: %ld", (long)num);
  //Serial.println(buff);
  //sprintf(buff,"denom: %d", (long)denom);
  //Serial.println(buff);

                  // Set up PLL A with the calculated multiplication ratio
  setupPLL(SI_SYNTH_PLL_A, mult, num, denom);
                  // Set up MultiSynth divider 0, with the calculated divider. 
                  // The final R division stage can divide by a power of two, from 1..128. 
                  // reprented by constants SI_R_DIV1 to SI_R_DIV128 (see si5351a.h header file)
                  // If you want to output frequencies below 1MHz, you have to use the 
                  // final R division stage
  setupMultisynth(SI_SYNTH_MS_1, divider, SI_R_DIV_1);
                  // Reset the PLL. This causes a glitch in the output. For small changes to 
                  // the parameters, you don't need to reset the PLL, and there is no glitch
  i2cSendRegister(SI_PLL_RESET, 0xA0);  
                  // Finally switch on the CLK0 output (0x4F)
                  // and set the MultiSynth0 input to be PLL A
  i2cSendRegister(SI_CLK1_CONTROL, 0x4F | SI_CLK_SRC_PLL_A);
}


void si5351aSetFrequency_clk2(uint32_t frequency)
{
  uint32_t pllFreq;
  uint32_t xtalFreq = XTAL_FREQ;
  uint32_t l;
  float f;
  uint8_t mult;
  uint32_t num;
  uint32_t denom;
  uint32_t divider;

  divider = 900000000 / frequency;// Calculate the division ratio. 900,000,000 is the maximum internal 
                  // PLL frequency: 900MHz
  if (divider % 2) divider--;   // Ensure an even integer division ratio

  pllFreq = divider * frequency;  // Calculate the pllFrequency: the divider * desired output frequency
  //sprintf(buff,"pllFreq: %ld", (long)pllFreq);
  //Serial.println(buff);                

  mult = (pllFreq / xtalFreq);    // Determine the multiplier to get to the required pllFrequency
  l = pllFreq % xtalFreq;     // It has three parts:
  f = l;              // mult is an integer that must be in the range 15..90
  f *= 1048575;         // num and denom are the fractional parts, the numerator and denominator
  f /= xtalFreq;          // each is 20 bits (range 0..1048575)
  num = f;            // the actual multiplier is  mult + num / denom
  denom = 1048575;        // For simplicity we set the denominator to the maximum 1048575

                  // Set up PLL B with the calculated multiplication ratio
  setupPLL(SI_SYNTH_PLL_B, mult, num, denom);
                  // Set up MultiSynth divider 0, with the calculated divider. 
                  // The final R division stage can divide by a power of two, from 1..128. 
                  // reprented by constants SI_R_DIV1 to SI_R_DIV128 (see si5351a.h header file)
                  // If you want to output frequencies below 1MHz, you have to use the 
                  // final R division stage
  setupMultisynth(SI_SYNTH_MS_2, divider, SI_R_DIV_1);
                  // Reset the PLL. This causes a glitch in the output. For small changes to 
                  // the parameters, you don't need to reset the PLL, and there is no glitch
  i2cSendRegister(SI_PLL_RESET, 0xA0);  
                  // Finally switch on the CLK2 output (0x4F)
                  // and set the MultiSynth0 input to be PLL A
  i2cSendRegister(SI_CLK2_CONTROL, 0x4F | SI_CLK_SRC_PLL_B);
}

#define ENC_A (A0)
#define ENC_B (A1)
#define FBUTTON (A2)
#define PTT   (A3)
#define DBM_READING (A6)

#include <LiquidCrystal.h>

unsigned long f = 10000000l;
#define IF_FREQ  (24995000l)
LiquidCrystal lcd(8,9,10,11,12,13);
char b[32], c[32];
int return_loss;
unsigned long frequency = 10000000l;
int openReading = 72; // in dbm

const int PROGMEM vswr[] = {
1000,
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
125,
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



void active_delay(int delay_by){
  unsigned long timeStart = millis();

  while (millis() - timeStart <= delay_by) {
      //Background Work      
    //checkCAT();
  }
}

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
    strncat(c, b, 2);
    strcat(c, ".");
    strncat(c, &b[2], 3);
    strcat(c, ".");
    strncat(c, &b[5], 3);
  }
  else {
    strncat(c, b, 1);
    strcat(c, ".");
    strncat(c, &b[1], 3);    
    strcat(c, ".");
    strncat(c, &b[4], 3);
  }
  printLine1(c);

  return_loss = openReading - analogRead(DBM_READING)/5;
  if (return_loss > 30)
     return_loss = 30;
  if (return_loss < 0)
     return_loss = 0;
  
  vswr_reading = pgm_read_word_near(vswr + return_loss);
  Serial.println(vswr_reading);
  //sprintf (c, "Rl:%ddb VSWR:%d", return_loss, (int)vswr);
  sprintf (c, "%ddbm VSWR=%d.%01d", return_loss, vswr_reading/10, vswr_reading%10);

  printLine2(c);
}



void takeReading(long newfreq){
  long local_osc;

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
  sprintf(buff, "freq:%ld", newfreq);
  Serial.println(buff);
  
  si5351aSetFrequency_clk2(local_osc);  
  si5351aSetFrequency_clk1(newfreq);     
}


void setup() {
  lcd.begin(16, 2);
  Wire.begin();
  Serial.begin(9600);
  Serial.flush();  
 
  analogReference(DEFAULT);

  //??
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(FBUTTON, INPUT_PULLUP);
  
  
  printLine1("Antuino");
  delay(2000);  
  takeReading(frequency);
  updateDisplay();

  takeReading(frequency);
  // Initialize I2C-communication as master
                                        //    SDA on pin ADC04                                       //    SCL on pin ADC05
//  si5351aSetFrequency_clk2a(150000000l);             // Set TX-Frequency [10,14 MHz]
                      // Intialize park mode
}

int menuBand(){
  int knob = 0;
  int band;
  unsigned long offset;
  unsigned long prev_freq;

  printLine2("Band Select:    ");
  //wait for the button menu select button to be lifted)
  while (btnDown())
    active_delay(50);
  active_delay(50);    

  while(!btnDown()){

    prev_freq = frequency;
    knob = enc_read();
    if (knob != 0){
      /*
      if (band > 3 && knob < 0)
        band--;
      if (band < 30 && knob > 0)
        band++; 
      if (band > 10)
        isUSB = true;
      else
        isUSB = false;
      setFrequency(((unsigned long)band * 1000000l) + offset); */
      if (knob < 0 && frequency > 3000000l)
        frequency -= 1000000l;
      if (knob > 0 && frequency < 500000000l)
        frequency += 1000000l;

      if (prev_freq < 200000000l && frequency > 200000000l)
        frequency = 400000000l;
      if (prev_freq >= 400000000l && frequency < 400000000l)
        frequency = 199000000l; 

        
      takeReading(frequency);
      updateDisplay();
    }
    active_delay(20);
  }

  while(btnDown())
    active_delay(50);
  active_delay(50);
  
  printLine2("");
  updateDisplay();
}


void doTuning(){
  int s;
  unsigned long prev_freq;

  s = enc_read();

  if (s != 0){
    prev_freq = frequency;

    if (s > 4)
      frequency += 100000l;
    else if (s > 2)
      frequency += 20000l;
    else if (s > 0)
      frequency +=  1000l;
    else if (s > -2)
      frequency -= 1000l;
    else if (s > -4)
      frequency -= 20000l;
    else
      frequency -= 100000l;

    takeReading(frequency);
    updateDisplay();
  }
}


void loop() {

  doTuning();
  if (btnDown())
    menuBand();
  delay(100); 

  takeReading(frequency);
  updateDisplay();
}


