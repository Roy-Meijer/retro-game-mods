#define PIN_READ( pin )  (PIND&(1<<(pin)))
//number op nops for approx 1 us, this may be different for other arduino board. we have used arduino nano
#define MICROSECOND_NOPS "nop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\nnop\n"

//connect to reset pin of n64
//NEVER make N64_RESET_PIN HIGH, it can damage your console
#define N64_RESET_PIN        3

//connect to controller player 1 pin of n64
#define N64_CONTROLLER_PIN   2
#define N64_PREFIX           9
#define N64_BITCOUNT        32

//buttons of which appear in data protocol
//if you want to use different buttons, see http://www.qwertymodo.com/hardware-projects/n64/n64-controller
#define BUTTON_L            10
#define BUTTON_R            11
#define BUTTON_CDOWN        13

//wait 20000 ms = 20 s before resetting if console crashed
#define RESET_WAIT       20000
//counter values for approx 1 ms, this may be different for other arduino board. we have used arduino nano
#define ONE_MS (uint32_t)(1880)

// Declare some space to store the bits we read from a controller.
unsigned char rawData[ N64_PREFIX + N64_BITCOUNT + 1];

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// General initialization, just sets all pins to input and starts serial communication.
void setup() {
  PORTD = 0x00;
  DDRD  = 0x00;
  PORTC = 0xFF;
  DDRC  = 0x00;
  pinMode(N64_RESET_PIN, INPUT);
  //NEVER make N64_RESET_PIN HIGH, it can damage your console
  digitalWrite(N64_RESET_PIN, LOW);
  //can be used for debugging
  Serial.begin( 115200 );
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Performs a read cycle from one of Nintendo's one-wire interface based controllers.
// This includes the N64 and the Gamecube.
//     pin  = Pin index on Port D where the data wire is attached.
//     bits = Number of bits to read from the line.
template< unsigned char pin > uint8_t read_oneWire( unsigned char bits ) {
  unsigned char rawDataIdx = 0;
  uint32_t counter = 0;

  read_loop:

  // Wait for the line to go high then low.
  counter = 0;
  while ( !PIN_READ(pin) ) {
    counter++;
    //if we have to wait longer than RESET_WAIT ms
    if (counter > RESET_WAIT * ONE_MS) {
      //console has crashed
      return 0;
    }
  }
  counter = 0;
  while ( PIN_READ(pin) ) {
    counter++;
    //if we have to wait longer than RESET_WAIT ms
    if (counter > RESET_WAIT * ONE_MS) {
      //console has crashed
      return 0;
    }
  }

  // Wait ~2us between line reads
  asm volatile( MICROSECOND_NOPS );

  // Read a bit from the line and store as a byte in "rawData"
  rawData[rawDataIdx] = PIN_READ(pin);
  if (rawDataIdx <  N64_PREFIX + N64_BITCOUNT ) {
    rawDataIdx++;
  }
  //read success
  if ( --bits == 0 ) return 1;

  goto read_loop;
}

//this function resets the n64
inline void n64_reset() {
  //make reset pin floating (if it is not already floating)
  pinMode(N64_RESET_PIN, INPUT);
  //make output of reset pin low
  ///after this line the pin remains floating, but we are making it low already so that we do not
  ///accidentilly send 5V to reset pin when we stop make it floating
  //NEVER make N64_RESET_PIN HIGH, it can damage your console
  digitalWrite(N64_RESET_PIN, LOW);
  //make reset pin output, so it becomes low
  pinMode(N64_RESET_PIN, OUTPUT);
  //wait 100 ms
  delay(100);
  //make reset pin floating
  pinMode(N64_RESET_PIN, INPUT);
  //wait at least 5 seconds before it can resets again
  delay(5000);
}

//read controller and potentially reset
void loop() {
  uint8_t dataInTime = 0;
  static int pressState = 0;

  noInterrupts();
  dataInTime = read_oneWire< N64_CONTROLLER_PIN >( N64_PREFIX + N64_BITCOUNT );
  interrupts();  
  
  //check if controller data is received within expected time frame
  if (dataInTime) {
      bool nothingPressed = true;
      bool nothingElsePressed = true;
      bool somethingElsePressed = false;

      switch (pressState) {
        //check if nothing is pressed
        case 0:
          for (int i = N64_PREFIX; i < N64_BITCOUNT; i++) {
            if (rawData[i]) {
              nothingPressed = false;
            }
          }
          if (nothingPressed) {
            pressState = 1;
            Serial.println("Nothing pressed");
          }
          break;
        //check if correct button combination is pressed
        case 1:
          //if L, R and CDOWN are pressed
          if (rawData[N64_PREFIX + BUTTON_L] && rawData[N64_PREFIX + BUTTON_R] && rawData[N64_PREFIX + BUTTON_CDOWN]) {
            for (int i = N64_PREFIX; i < N64_PREFIX + N64_BITCOUNT; i++) {
              //check if no other buttons are pressed
              if (i - N64_PREFIX != BUTTON_L && i - N64_PREFIX != BUTTON_R && i - N64_PREFIX != BUTTON_CDOWN) {
                if (rawData[i]) {
                  //there is also another button pressed
                  nothingElsePressed = false;
                }
              }
            }
            //go to next state if L, R and CDOWN are pressed and no other button is pressed
            if (nothingElsePressed) {
              pressState = 2;
              Serial.println("Reset btn combo");
            } else {
              for (int i = N64_PREFIX; i < N64_PREFIX + N64_BITCOUNT; i++) {
                //check if some button(s) are pressed which are not the combination of only L, R and CDOWN
                if (rawData[i]) {
                  somethingElsePressed = true;
                }
              }
              //if incorrect buttons are pressed
              if (somethingElsePressed) {
                //incorrect combination, go back to state 0 again and check if nothing is pressed
                pressState = 0;
                Serial.println("Too much pressed");
              }
            }
          //if only one or two of the three buttons are pressed
          } else if (rawData[N64_PREFIX + BUTTON_L] || rawData[N64_PREFIX + BUTTON_R] || rawData[N64_PREFIX + BUTTON_CDOWN]) {
            for (int i = N64_PREFIX; i < N64_PREFIX + N64_BITCOUNT; i++) {
              if (i - N64_PREFIX != BUTTON_L && i - N64_PREFIX != BUTTON_R && i - N64_PREFIX != BUTTON_CDOWN) {
                if (rawData[i]) {
                  //check if some other button is also pressed
                  somethingElsePressed = true;
                }
              }
            }
            //if incorrect buttons are pressed
            if (somethingElsePressed) {
              //incorrect combination, go back to state 0 again and check if nothing is pressed
              pressState = 0;
              Serial.println("Too much pressed");
            } else {
              //only one or two of L, R and CDOWN are pressed and nothing else is pressed. we can keep waiting in the current state
              Serial.println("Almost reset");
            }
          //L, R and CDOWN are not pressed
          } else {
            for (int i = N64_PREFIX; i < N64_PREFIX + N64_BITCOUNT; i++) {
              if (rawData[i]) {
                //check if some other button is also pressed
                somethingElsePressed = true;
              }
            }
            if (somethingElsePressed) {
              //incorrect combination, go back to state 0 again and check if nothing is pressed
              pressState = 0;
              Serial.println("Wrong btn combo");
            }
            //else ... wait in current state
          }
          break;
        case 2:
          //go back wainting for no button presses
          pressState = 0;
          //reset n64
          n64_reset();
          break;
      }
  //if no controller data has been received within expected time frame, it is assumed the n64 has crashed, so it can be resetted
  } else {
    Serial.println("Reset crash");
    n64_reset();
  }
}
