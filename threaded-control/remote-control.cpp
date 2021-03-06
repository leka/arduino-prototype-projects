#include <Arduino.h>
#include "ChibiOS_AVR.h"
//#include <string.h>
#include <Wire.h>
//#include "Moti.h"

#include <Adafruit_NeoPixel.h>

#include <ctype.h>

#include <QueueArray.h>

#include "LedPixel.h"

//Predefining game functions
void rainbowCycle(uint8_t wait);
void rainbowSequence(uint8_t wait);
uint32_t Wheel(byte WheelPos);
void theaterChase(uint32_t c, uint8_t wait);
void colorLookHere(uint32_t c, uint8_t wait);
void colorWipeRight(uint32_t c, uint8_t wait);
void colorWipeLeft(uint32_t c, uint8_t wait);
void clearColorBelt(void);
void clearColorEars(void);

void mainThread(void);
//Definining Pin types for the device
#define Serio_BT Serial1
#define Serio_USB Serial
//LEDS
#define PIN_LED_BELT   46
#define PIN_LED_EARS   8
#define NUMPIXELS      20

Adafruit_NeoPixel pixel_belt = Adafruit_NeoPixel(NUMPIXELS, PIN_LED_BELT);
Adafruit_NeoPixel pixel_ears = Adafruit_NeoPixel(2,PIN_LED_EARS);



// Motor Pins TODO:define/explain theses pins
//M1 corresponds to the Left Motor's direction
//M2 corresponds to the right Motor's direction
//Mn value is LL_LOW for reverse & LL_HIGH for forward.
//Respectively the E values represent strength applied
int E1 = 5;
int M1 = 4;
int E2 = 6;
int M2 = 7;

//Action Flags
volatile bool FLAG_GAMES = false;
volatile bool FLAG_MOTOR = false;
volatile bool FLAG_LEDS = false;



//REVIEW:verify utility of theses verifications characters
char validChars[]={'N','L','R','B','O','H'};
int size_validChars = (int)(sizeof(validChars) / sizeof(validChars[0]));

char listStarterChar[] = {'<','['};
int size_listStarterChar = (int)(sizeof(listStarterChar) / sizeof(listStarterChar[0]));

char listFinisherChar[] = {'>',']'};
int size_listFinisherChar = (int)(sizeof(listFinisherChar) / sizeof(listFinisherChar[0]));

//REVIEW: string orders might need different encapsulation

char command[256] = {0};
char order[256] = {0};

QueueArray <LedPixel> led_command_queue;
// String command = "";
// String order = "";

// Mutex for atomic access to data.
MUTEX_DECL(serialMutex);

//Verification functions:
bool isStarterChar(char tested_char){
  int i = 0;
  for(i=0;i<size_listStarterChar;i++){
    if(listStarterChar[i] == tested_char){
      return true;
    }
  }
  return false;
}

bool isFinisherChar(char tested_char){
  int i = 0;
  for(i=0;i<size_listFinisherChar;i++){
    if(listFinisherChar[i] == tested_char){
      return true;
    }
  }
  return false;
}

void printOrder(){
  chMtxLock(&serialMutex);
  Serio_BT.println(order);
  chMtxUnlock();
}


//------------------------------------------------------------------------------
// thread 1 - Capture Serial data, and parse commands - normal Prio
// 64 byte stack beyond task switch and interrupt needs
//static THD_WORKING_AREA(waThread1, 64);
static WORKING_AREA(waThread1, 256);

static msg_t Thread1(void* arg) {

    // String tempString = "tmpStr";
    // String tempOrder = "yo";
    char tempString[5] = {0};
    char tempOrder[50] = "";

    int led_ID;
    int led_R;
    int led_G;
    int led_B;

    LedPixel tempLed;

    //delay(50);

    while (!chThdShouldTerminate()) {

      while(Serio_BT.available()){

        char caracter = (char) Serio_BT.read();

        chMtxLock(&serialMutex);
        tempString[0] = caracter;
        chMtxUnlock();

        Serio_BT.print("inThd1-TempOrder:");
        Serio_BT.println(tempOrder);
        // Serio_BT.print("inThd1-Command:");
        // Serio_BT.println(command);
        //Serio_BT.print("inThd1-Command:");
        //Serio_BT.println(tempOrder);

        if (isFinisherChar(caracter)){

          if(caracter == ']'){
            //do motor command actions
            strcat(tempOrder,tempString);
            strcpy(command,tempOrder);

            //Serio_BT.print("inThd1-Command:");
            //Serio_BT.println(command);

            if(!FLAG_MOTOR){
              FLAG_MOTOR = true;
            }
          } else if (caracter == '>'){
            //do game order actions
            strcat(tempOrder,tempString);
            // Serio_BT.print("tempOrder size:");
            // Serio_BT.println(strlen(tempOrder));
            // Serio_BT.print("tempOrder[1]:");
            // Serio_BT.println(tempOrder[1]);


            if(strlen(tempOrder) == 18 && tempOrder[1] == 'L'){
                Serio_BT.print("In Led Decoding:");
                //Insert values to LED Fifo
                led_ID = strtol(strtok (&tempOrder[2]," ,>"),NULL,10);
                led_R = strtol(strtok (NULL," ,>"),NULL,10);
                led_G = strtol(strtok (NULL," ,>"),NULL,10);
                led_B = strtol(strtok (NULL," ,>"),NULL,10);

                Serio_BT.println(led_ID);
                Serio_BT.println(led_R);


                tempLed.set_id(led_ID);
                tempLed.set_color(pixel_belt.Color(led_R,led_G,led_B));


                //Queue values into LED Queue.
                led_command_queue.push(tempLed);


                if(!FLAG_LEDS) FLAG_LEDS = true;

            } else {

              strcpy(order,tempOrder);
              if(!FLAG_GAMES) FLAG_GAMES = true;
            }
          //End of distinction between Games & LEDS.
          }

        } else if(isStarterChar(caracter)){
          strcpy(tempOrder,tempString);
          //Serio_BT.print("inThd1-Starter,temp:");
          //Serio_BT.println(tempOrder);
          // Serio_BT.print("inThd1-Starter:");
          // Serio_BT.println(command);

        } else if (isupper(caracter)){
          strcat(tempOrder,tempString);
          //Serio_BT.print("inThd1-Upper,temp:");
          //Serio_BT.println(tempOrder);

        } else if (isdigit(caracter)){
          strcat(tempOrder,tempString);
          //Serio_BT.print("inIsDigit:");
          //Serio_BT.println(tempOrder);

        } else if (caracter == ',' || caracter == '+' || caracter == '-'){
          strcat(tempOrder,tempString);
        }
        else {
          memset(tempOrder,0,strlen(tempOrder));
          memset(tempString,0,strlen(tempString));
        }

      }

      //REVIEW: mutexes maybe not necessary
      // // Lock access to data.
      // chMtxLock(&dataMutex);
      // order += tempChar;
      // // Unlock data access.
      // chMtxUnlock(&dataMutex);

      // Sleep for 50 milliseconds.
      //chThdSleepMilliseconds(20);

  }
  return (msg_t)0;
}
//------------------------------------------------------------------------------
// thread 2 - Verify raised flags and execute apporpriate orders
// 100 byte stack beyond task switch and interrupt needs
//static THD_WORKING_AREA(waThread2, 100);
static WORKING_AREA(waThread2, 256);

static msg_t Thread2(void* arg) {

  // print count every second
  LedPixel tempLed;

  while (!chThdShouldTerminate()) {

    if(FLAG_GAMES){
      FLAG_GAMES = false;
      //Serio_BT.print("inThd2-Order:");
      //Serio_BT.println(order);
      //if(strlen(order)>0) || order[1];

      if(strlen(order)>0){
        switch (order[1]) {
          case 'N':
          //Serio_USB.println("inThd2-case: N");
          rainbowSequence(20);
          break;
          case 'L':
          colorWipeLeft(pixel_belt.Color(200,200,200), 50);
          //Serio_USB.println("inThd2-case: L");
          break;
          case 'R':
          colorWipeRight(pixel_belt.Color(200,200,200), 50);
          //Serio_USB.println("inThd2-case: R");
          break;
          case 'O':
          //Serio_USB.println("inCaseO");
          theaterChase(pixel_belt.Color(200,200,200),50);
          break;
          case 'H':
          //Serio_USB.println("inCaseH");
          colorLookHere(pixel_belt.Color(200,200,200),50);
          break;
        }
      }

      memset(order, 0, sizeof(order));

      clearColorBelt();
      //order = "";
    }

    if(FLAG_LEDS){
      tempLed = led_command_queue.pop();

      Serio_BT.println("AfterPop");
      Serio_BT.println(tempLed.get_id());
      Serio_BT.println(tempLed.get_color());



      pixel_belt.setPixelColor(tempLed.get_id(),tempLed.get_color());
      pixel_belt.show();

      if(led_command_queue.isEmpty()){
        FLAG_LEDS = false;
      }
    }
    // Sleep for 20 milliseconds.
    chThdSleepMilliseconds(20);
  }
  return (msg_t)0;
}
//------------------------------------------------------------------------------
// TODO: thread 3 - Mouvement Thread - Applies mouvement from order to the wheels
// 64 byte stack beyond task switch and interrupt needs
static WORKING_AREA(waThread3, 256);

static msg_t Thread3(void* arg) {

  int leftMotor=0;
  int rightMotor=0;
  // Flash led every 200 ms.
  while (!chThdShouldTerminate()) {

    if(FLAG_MOTOR){
      FLAG_MOTOR = false;

      leftMotor = strtol(strtok (command," ,[]"),NULL,10);
      rightMotor = strtol(strtok (NULL," ,[]"),NULL,10);

      //Serio_BT.println("Left and right Motor:");
      //Serio_BT.println(leftMotor);
      //Serio_BT.println(rightMotor);

      //M1 corresponds to the Left Motor's direction
      //M2 corresponds to the right Motor's direction
      //Mn value is LOW for reverse & HIGH for forward.
      //Respectively the E values represent strength applied

      //REVIEW change 255 values for constant variable named max speed.
      //Setting the Left motor values
      if (leftMotor>=0 && leftMotor < 255){
        digitalWrite(M1,HIGH);
        analogWrite(E1, abs(leftMotor));

      } else if (leftMotor > 255){
        digitalWrite(M1,HIGH);
        analogWrite(E1, 255);

      } else if (leftMotor < 0 && leftMotor > -255) {

        digitalWrite(M1,LOW);
        analogWrite(E1, abs(leftMotor));

      } else if (leftMotor < -255) {
        digitalWrite(M1,LOW);
        analogWrite(E1, 255);
      }

      //Right motor
      if (rightMotor >= 0 && rightMotor < 255){

        digitalWrite(M2,HIGH);
        analogWrite(E2, abs(rightMotor));

      } else if (rightMotor > 255){
        digitalWrite(M2,HIGH);
        analogWrite(E2, 255);
      } else if (rightMotor < 0 && rightMotor > -255) {

        digitalWrite(M2,LOW);
        analogWrite(E2, abs(rightMotor));

      } else if (leftMotor < -255) {
        digitalWrite(M2,LOW);
        analogWrite(E2, 255);
      }



      memset(command, 0, sizeof(command));

    }
    // Sleep for 150 milliseconds.
    chThdSleepMilliseconds(5);
  }
}
//------------------------------------------------------------------------------
// TODO:thread 4 - SensorData record/send Thread -
// 64 byte stack beyond task switch and interrupt needs
// static THD_WORKING_AREA(waThread4, 256);
//
// static THD_FUNCTION(Thread4 ,arg) {
//   pinMode(LED_PIN, OUTPUT);
//
//   // Flash led every 200 ms.
//   while (1) {
//     // Turn LED on.
//     digitalWrite(LED_PIN, HIGH);
//
//     // Sleep for 50 milliseconds.
//     chThdSleepMilliseconds(50);
//
//     // Turn LED off.
//     digitalWrite(LED_PIN, LOW);
//
//     // Sleep for 150 milliseconds.
//     chThdSleepMilliseconds(150);
//   }
// }

//****************************************************************************//
// Main thread and Arduino setup

void setup(){


  pinMode(M1, OUTPUT);
  pinMode(M2, OUTPUT);
  digitalWrite(M1,HIGH);
  digitalWrite(M2, HIGH);
  analogWrite(E1, 0);
  analogWrite(E2, 0);

  //Initialising Communication
  Serio_USB.begin(115200);
  Serio_BT.begin(115200);
  Wire.begin();


  //Initialising led belt.
  pixel_belt.begin();
  pixel_ears.begin();
  led_command_queue.setPrinter(Serio_BT);

  // wait for USB Serial
  while (!Serio_USB) {}
  // read any input
  delay(200);
  while (Serio_USB.read() >= 0) {}

  //switch all leds off
  pixel_belt.show();
  pixel_ears.show();

  chBegin(mainThread);
  // chBegin never returns, main thread continues with mainThread()
  while(1) {}

}

// main thread runs at NORMALPRIO
void mainThread() {

  // start blink thread
  (void)chThdCreateStatic(waThread1, sizeof(waThread1),
                          NORMALPRIO, Thread1, NULL);
  // start print thread
  (void)chThdCreateStatic(waThread2, sizeof(waThread2),
                          NORMALPRIO, Thread2, NULL);

  (void)chThdCreateStatic(waThread3, sizeof(waThread3),
                          NORMALPRIO, Thread3, NULL);

  // increment counter
  while (1) {
    // must insure increment is atomic in case of context switch for print
    // should use mutex for longer critical sections
    // noInterrupts();
    // count++;
    // interrupts();
  }
}
//------------------------------------------------------------------------------
void loop() {
 // not used
}


//------------------GAMES-------------------------------------------------------
//TODO: change games into their own files
void clearColorBelt(){
  uint32_t off = pixel_belt.Color(0,0,0);
  for(int i = 0; i < pixel_belt.numPixels();i++){
    pixel_belt.setPixelColor(i,off);
  }
  pixel_belt.show();
}
void clearColorEars(){
  pixel_ears.setPixelColor(1,0,0,0);
  pixel_ears.setPixelColor(2,0,0,0);

  pixel_ears.show();
}

void colorWipeLeft(uint32_t c, uint8_t wait) {
  uint32_t off = pixel_belt.Color(0,0,0);

  uint8_t trailSize = 5;

  for(uint16_t i=0; i<pixel_belt.numPixels(); i++) {
    pixel_belt.setPixelColor(i, c);
    if((i-trailSize)>=0) pixel_belt.setPixelColor(i-trailSize,off);
    pixel_belt.show();
    delay(wait);
  }
}

void colorWipeRight(uint32_t c, uint8_t wait) {
  uint32_t off = pixel_belt.Color(0,0,0);
  uint16_t max_pixels = pixel_belt.numPixels();

  uint8_t trailSize = 5;

  for(uint16_t i=0; i<=max_pixels; i++) {
    pixel_belt.setPixelColor(max_pixels-i, c);
    if(i>=trailSize)pixel_belt.setPixelColor(max_pixels-i+trailSize,off);
    pixel_belt.show();
    delay(wait);
  }
}

void colorLookHere(uint32_t c, uint8_t wait){
  uint32_t off = pixel_belt.Color(0,0,0);
  uint16_t max_pixels = pixel_belt.numPixels();
  uint16_t half_max_pixels = max_pixels/2;

  uint8_t trailSize = 3;

  for(uint16_t i=0;i<=half_max_pixels;i++){
    pixel_belt.setPixelColor(half_max_pixels-i,c);
    pixel_belt.setPixelColor(half_max_pixels+i,c);

    if((i-trailSize)>=0) pixel_belt.setPixelColor(half_max_pixels+i-trailSize,off) ;
    if(i>=trailSize) pixel_belt.setPixelColor(half_max_pixels-i+trailSize,off);
    pixel_belt.show();
    delay(wait);
  }
}

void theaterChase(uint32_t c, uint8_t wait) {
  for (int j=0; j<2; j++) {  //do 10 cycles of chasing
    for (int q=0; q < 3; q++) {
      for (uint16_t i=0; i < pixel_belt.numPixels(); i=i+3) {
        pixel_belt.setPixelColor(i+q, c);    //turn every third pixel on
      }
      pixel_belt.show();

      delay(wait);

      for (uint16_t i=0; i < pixel_belt.numPixels(); i=i+3) {
        pixel_belt.setPixelColor(i+q, 0);        //turn every third pixel off
      }
    }
  }
}

uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return pixel_belt.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return pixel_belt.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  }
  WheelPos -= 170;
  return pixel_belt.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
}

void rainbowSequence(uint8_t wait){
  uint16_t i, j;


    for(j=0; j<256; j++) {
      for(i=0; i<pixel_belt.numPixels(); i++) {
        pixel_belt.setPixelColor(i, Wheel((i+j) & 255));
      }
      pixel_belt.show();
      delay(wait);
    }
}

void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256*5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< pixel_belt.numPixels(); i++) {
      pixel_belt.setPixelColor(i, Wheel(((i * 256 / pixel_belt.numPixels()) + j) & 255));
    }
    pixel_belt.show();
    delay(wait);
  }
}
