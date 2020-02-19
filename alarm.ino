#include <DS3232RTC.h>                      // library for real time clock 
#include <TimeLib.h>                        // The Time library adds timekeeping functionality to Arduino with or without external timekeeping hardware
#include <Wire.h>                           // for analog pins and for I2C (Inter Integrated Circuit), serial communication systems used between integrated circuits
#include <LiquidCrystal_I2C.h>              // library used to manage the display lcd
#include <Servo.h>                          // library required for the sg90 servo motor

LiquidCrystal_I2C lcd(0x27, 20, 4);         // Set the LCD address to 0x27 for a 20 chars and 4 line display

const int buzzerPin = 10;                   // pin for buzzer
int triggerPort = 11;                       // pin for the output module HC-SR
int echoPort = 12;                          // pin for the input module HC-SR

const int resetButtonPin = 6;               // pins dedicated to the keys
const int startStopButtonPin = 7;           // pins dedicated to the keys
const int downButtonPin = 8;                // pins dedicated to the keys
const int upButtonPin = 9;                  // pins dedicated to the keys

Servo servo;                                // name of the servo
int angle = 10;                             // variable needed to set the angle of the servo

int setupHours = 0;                         // How many hours will count down when started
int setupMinutes = 0;                       // How many minutes will count down when started
int setupSeconds = 0;                       // How many seconds will count down when started
time_t setupTime = 0;

int currentHours = 0;                       // variable required for calculating hours
int currentMinutes = 0;                     // variable required for calculating minutes
int currentSeconds = 0;                     // variable required for calculating seconds
time_t currentTime = 0;
time_t startTime = 0;

int resetButtonState = LOW;
long resetButtonLongPressCounter = 0;
int startStopButtonState = LOW;
int upButtonState = LOW;
int downButtonState = LOW;
int resetButtonPrevState = LOW;
int startStopButtonPrevState = LOW;
int upButtonPrevState = LOW;
int downButtonPrevState = LOW;
bool resetButtonPressed = false;
bool resetButtonLongPressed = false;
bool startStopButtonPressed = false;
bool upButtonPressed = false;
bool downButtonPressed = false;

const int MODE_IDLE = 0;                  // configurations required for the switch case
const int MODE_SETUP = 1;
const int MODE_RUNNING = 2;
const int MODE_RINGING = 3;

int currentMode = MODE_IDLE;              // 0=idle 1=setup 2=running 3=ringing
                                                    // Power up --> idle
                                                    // Reset --> idle
                                                    //  Start/Stop --> start or stop counter
                                                    //  Up / Down --> NOP
                                                    // Reset (long press) --> enter setup
                                                    //   Start/Stop --> data select
                                                    //   Up --> increase current data value
                                                    //   Down --> decrease current data value
                                                    //   Reset --> exit setup (idle)

int dataSelection = 0;                    // Currently selected data for edit (setup mode, changes with Start/Stop)
                                          // 0=hours (00-99) 1=minutes (00-59) 2=seconds (00-59)

void setup() {                            // installation code to run only once
  servo.attach(13);                       // pin to drive the servo
  servo.write(angle);                     // set servo to initial point
  pinMode(triggerPort, OUTPUT);           // setting pin for HC-SR04 sensor
  pinMode(echoPort, INPUT);               // setting pin for HC-SR04 sensor
  lcd.begin();       // Initializes the interface to the LCD screen, and specifies the dimensions of the display. begin() needs to be called before any other LCD library commands.
  pinMode(resetButtonPin, INPUT);         // setting the reset pin as input
  pinMode(startStopButtonPin, INPUT);     // setting the startStop pin as input
  pinMode(upButtonPin, INPUT);            // setting the up pin as input
  pinMode(downButtonPin, INPUT);          // setting the down pin as input
  pinMode(buzzerPin, OUTPUT);             // setting the buzzerPin previously declared at 10 as output
  pinMode(4, OUTPUT);                     // setting the pin for the red led
  pinMode(3, OUTPUT);                     // setting the pin for the green led
}

void loop() {        // start of the loop function which has the task of providing the board with all the information relating to the program information and the necessary commands

  startStopButtonPressed = false;         // setting of the boolean variables needed by the keys
  upButtonPressed = false;
  downButtonPressed = false;
  resetButtonPressed = false;

  /*
   * Reset button management
   */

  resetButtonLongPressed = false;
  resetButtonState = digitalRead(resetButtonPin);
  if (resetButtonState != resetButtonPrevState) {       // control on the state of the pressure of the key
    resetButtonPressed = resetButtonState == HIGH;
    resetButtonPrevState = resetButtonState;
  } else // Long press management...
  {
    if (resetButtonState == HIGH) {                     // control on how long the reset button is pressed, if the button has been pressed
      resetButtonLongPressCounter++;                    // start the increment of the variable and if it reaches 100 then change the configurations
      if (resetButtonLongPressCounter == 100) {         
        resetButtonPressed = false;
        resetButtonLongPressed = true;
        resetButtonLongPressCounter = 0;
      }
    } else {                                            // otherwise if the count has not been reached, the counter resets and consequently also the status of the key
      resetButtonLongPressCounter = 0;
      resetButtonPressed = false;
      resetButtonLongPressed = false;
    }
  }

  /*
   * Start/Stop button management                            check the status of the start/stop button
   */
  startStopButtonPressed = false;
  startStopButtonState = digitalRead(startStopButtonPin);  // A digitalRead is issued on the relevant pin and the result is compared with a previously read value
  if (startStopButtonState != startStopButtonPrevState) {  // if something has changed, the new value is stored for future reference and the static variable "ButtonPressed" 
    startStopButtonPressed = startStopButtonState == HIGH; // bool is set to true if the button is pressed
    startStopButtonPrevState = startStopButtonState;
  }

  /*
   * Down button management                                   check on the status of the down button
   */
  downButtonPressed = false;
  downButtonState = digitalRead(downButtonPin);
  if (downButtonState != downButtonPrevState) {
    downButtonPressed = downButtonState == HIGH;
    downButtonPrevState = downButtonState;
  }

  /*
   * Up button management                                     check on the status of the up button 
   */
  upButtonPressed = false;
  upButtonState = digitalRead(upButtonPin);
  if (upButtonState != upButtonPrevState) {
    upButtonPressed = upButtonState == HIGH;
    upButtonPrevState = upButtonState;
  }

  /*
   * Mode management
   */
  switch (currentMode) {                // switch case related to mode management 
                    // here we examine the button state triggers ("xxxButtonPressed") and redirect the flow to the new correct state or perform the correct action
  case MODE_IDLE:                       // inactivity status equal to reset
    if (resetButtonPressed) {           // this block of code is able to detect the above prolonged pressure to access the respective modes
      Reset();                       
    }
    if (resetButtonLongPressed) {       //if the reset button is pressed for a long period, it enters the setup mode
      currentMode = MODE_SETUP;
    }
    if (startStopButtonPressed) {
      currentMode = currentMode == MODE_IDLE ? MODE_RUNNING : MODE_IDLE;    // if the value of mode idle is true then mode running is evaluated and mode idle is ignored
      if (currentMode == MODE_RUNNING) {                                    // while if mode idle is evaluated as false mode idle is evaluated and mode running ignored
        // STARTING TIMER!
        startTime = now();
      }
    }
    break;

  case MODE_SETUP:                                                           // setup mode
    if (resetButtonPressed) {
      // Exit setup mode
      setupTime = setupSeconds + (60 * setupMinutes) + (3600 * setupHours);  // set the setupTimer variable
      currentHours = setupHours;
      currentMinutes = setupMinutes;                                         // setting of variables to count the time
      currentSeconds = setupSeconds;
      dataSelection = 0;                                                     // variable required to switch from one time mode (min and sec hours) to another
      currentMode = MODE_IDLE;                                               // the current mode remains idle
    }
    if (startStopButtonPressed) {                                            // if the startstop button has been pressed it increases the selection mode of hours min and seconds
      // Select next data to adjust
      dataSelection++;
      if (dataSelection == 3) {                                              // while if this reaches a certain tot it starts again
        dataSelection = 0;
      }
    }
    if (downButtonPressed) {       //if the down button has been pressed according to the time mode it is in, it decreases (hours, min and sec) or starts again               
      switch (dataSelection) {
      case 0: // hours
        setupHours--;
        if (setupHours == -1) {
          setupHours = 99;
        }
        break;
      case 1: // minutes
        setupMinutes--;
        if (setupMinutes == -1) {
          setupMinutes = 59;
        }
        break;
      case 2: // seconds
        setupSeconds--;
        if (setupSeconds == -1) {
          setupSeconds = 59;
        }
        break;
      }
    }
    if (upButtonPressed) {        //if the up button has been pressed according to the time mode it is in, it increases (hours, min and sec) or starts again 
      switch (dataSelection) {
      case 0: // hours
        setupHours++;
        if (setupHours == 100) {
          setupHours = 0;
        }
        break;
      case 1: // minutes
        setupMinutes++;
        if (setupMinutes == 60) {
          setupMinutes = 0;
        }
        break;
      case 2: // seconds
        setupSeconds++;
        if (setupSeconds == 60) {
          setupSeconds = 0;
        }
        break;
      }
    }
    break;

  case MODE_RUNNING:               //running mode, if the startstop button or the reset button are pressed, it returns to the idle state
    if (startStopButtonPressed) {
      currentMode = MODE_IDLE;
    }
    if (resetButtonPressed) {
      Reset();
      currentMode = MODE_IDLE;
    }
    break;

  case MODE_RINGING:                //ringing if one of the 4 buttons has been pressed, the program goes into the inactivity state
    if (resetButtonPressed || startStopButtonPressed || downButtonPressed || upButtonPressed) {
      currentMode = MODE_IDLE;
    }
    break;
  }

  switch (currentMode) {
  case MODE_IDLE:
  case MODE_SETUP:
    // NOP
    break;
  case MODE_RUNNING:
    currentTime = setupTime - (now() - startTime);
    if (currentTime <= 0) {
      currentMode = MODE_RINGING;
    }
    break;
  case MODE_RINGING:
    analogWrite(buzzerPin, 0);
    break;
  }

  lcd.setCursor(0, 0);
  switch (currentMode) {
  case MODE_IDLE:
    lcd.print("Allarme a tempo");
    lcd.setCursor(0, 1);
    lcd.print(currentHours);
    lcd.print(" ");
    lcd.print(currentMinutes);
    lcd.print(" ");
    lcd.print(currentSeconds);
    lcd.print(" ");
    break;
  case MODE_SETUP:                  // setup case 
    lcd.print("Setup mode: ");
    switch (dataSelection) {        // another internal switch to select the hours, minutes and seconds
    case 0:
      lcd.print("ORE ");
      break;
    case 1:
      lcd.print("MIN");
      break;
    case 2:
      lcd.print("SEC");
      break;
    }
    lcd.setCursor(0, 1);
    lcd.print(setupHours);
    lcd.print(" ");
    lcd.print(setupMinutes);
    lcd.print(" ");
    lcd.print(setupSeconds);
    lcd.print(" ");
    break;

  case MODE_RUNNING:                         // CASE OF COUNTDOWN, the display of min and sec hours is shown on the display
    lcd.print("Tempo Rimanente");
    lcd.setCursor(0, 1);
    if (hour(currentTime) < 10) lcd.print("0");
    lcd.print(hour(currentTime));
    lcd.print(":");
    if (minute(currentTime) < 10) lcd.print("0");
    lcd.print(minute(currentTime));
    lcd.print(":");
    if (second(currentTime) < 10) lcd.print("0");
    lcd.print(second(currentTime));
    delay(10);    
    break;
  
  case MODE_RINGING:                          // OPERATING CASE  
  //andata servo
  for(angle = 10; angle < 180; angle++)       // cycle for the movement of the servo in the first half of its run
  {     

  digitalWrite(triggerPort, LOW);             // start of functioning of the HC-SR04 sensor with respective calibration
  digitalWrite(triggerPort, HIGH);            // sends a 10 microsec pulse on trigger
  delayMicroseconds(10);
  digitalWrite(triggerPort, LOW);

  long duration = pulseIn(echoPort, HIGH);   //calculation and configuration of the distance required for the HC-SR04 sensor
  long r = 0.034 * duration / 2;
       
    if (r <= 9) {                         // condition on distance r less than 10 cm (case of buzzer activation)
      digitalWrite(4, HIGH);              // in this case the green and red LEDs are inverted
      digitalWrite(3, LOW);
      lcd.setCursor(0, 0);                // set the cursor to write on the display in the character present in the column and row in position 0 and 0
      lcd.print("     ZONA ROSSA");
   //   lcd.setCursor(0, 1);                // set the cursor to write on the display in the character present in the second line and the first character
   //   lcd.print("  INFERIORE A 10 CM ");
     lcd.setCursor(0, 1);
     lcd.print(" INTRUSO RIVELATO");
      tone(10, 2000, 300);                // signal for pin 10 of the buzzer, activated by the respective method associated with frequency and duration
    }

    if (r <= 200 && r > 9) {              // condition on distance r in the range between 10 and 200
      digitalWrite(3, HIGH);              // in this case the green LED lights up and the red one remains off
      digitalWrite(4, LOW);
      lcd.setCursor(0, 0);                // set the cursor to write on the display in the character present in the column and row in position 0 and 0
      lcd.print("    ZONA BIANCA");
   //   lcd.setCursor(0, 1);                // set the cursor to write on the display in the character present in the second line and the first character
   //   lcd.print("  SUPERIORE A 10 CM ");
     lcd.setCursor(0, 1);
      lcd.print(" NESSUN INTRUSO");
    }
                               
    servo.write(angle);    // servo command necessary to perform the rotation, the argument is in fact the rotation angle that increases and decreases according to the case                  
  } 

  // servo return
  for(angle = 180; angle > 10; angle--)       // cycle for the movement of the servo in the second half of its run
  {        
  
  digitalWrite(triggerPort, LOW);             // start of operation of the HC-SR04 sensor with respective calibration
  digitalWrite(triggerPort, HIGH);            // send a 10 microsec pulse on trigger
  delayMicroseconds(10);
  digitalWrite(triggerPort, LOW);

  long duration = pulseIn(echoPort, HIGH);    // calculation and configuration of the distance required for the HC-SR04 sensor
  long r = 0.034 * duration / 2;
     
      if (r <= 200 && r > 9) {                 // condition on distance r in the range between 10 and 200
      digitalWrite(3, HIGH);                   // in this case the green LED lights up and the red one remains off
      digitalWrite(4, LOW);
      lcd.setCursor(0, 0);                     // set the cursor to write on the display in the character present in the column and row in position 0 and 0
      lcd.print("    ZONA BIANCA");
    //  lcd.setCursor(0, 1);                     // set the cursor to write on the display in the character present in the second line and the first character
    //  lcd.print("  SUPERIORE A 10 CM ");
     lcd.setCursor(0, 1);
      lcd.print(" NESSUN INTRUSO");
    }    
    
    if (r <= 9) {                              // condition on distance r less than 10 cm (case of buzzer activation)
      digitalWrite(4, HIGH);                   // in this case the green and red LEDs are inverted
      digitalWrite(3, LOW);
      lcd.setCursor(0, 0);                     // set the cursor to write on the display in the character present in the column and row in position 0 and 0
      lcd.print("     ZONA ROSSA");
   //   lcd.setCursor(0, 1);                     // set the cursor to write on the display in the character present in the second line and the first character
   //   lcd.print("  INFERIORE A 10 CM ");
   lcd.setCursor(0, 1);
      lcd.print(" INTRUSO RIVELATO");
      tone(10, 2000, 300);                     // signal for pin 10 of the buzzer, activated by the respective method associated with frequency and duration
    } 
                     
    servo.write(angle);  //servo command needed to rotate, the argument is in fact the rotation angle which increases and decreases according to the case
  } 

  }
}

void Reset() {
  currentMode = MODE_IDLE;
  currentHours = setupHours;
  currentMinutes = setupMinutes;
  currentSeconds = setupSeconds;
}
