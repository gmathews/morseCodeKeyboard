// Constants
const byte kModePin = 0;
const byte kKeyPin = 1;
const byte kLedPin = 2;
// Configuration constants
const byte kMinModeSwitchPressCycles = 10;
const int kSampleHz = 100;
const unsigned short kMaxTimerValue = 65535;
// Define length of elements
const byte kLengthDot = 1;
const byte kLengthDash = 3;
const byte kLengthGapElement = 1;
const byte kLengthGapLetter = 3;
const byte kLengthGapWord = 7;

// State variables
bool fullKeyboardMode;
unsigned short modeTimer;
unsigned short keyPressedTimer;
unsigned short keyReleasedTimer;
unsigned short averageElementSize;
bool allowSpaceInserted;
// The size of each element
byte elements[8];

void resetStateOnModeSwitch() {
    modeTimer = 0;
    keyPressedTimer = 0;
    keyReleasedTimer = 1; // Make sure we don't trigger key released logic
    allowSpaceInserted = false;
    clearElements(elements);
    averageElementSize = 24; // Hard code for now
    // Make sure we don't get stuck
    Keyboard.releaseAll();
}

// This runs once on startup
void setup() {
    // Setup our hardware
    Serial.begin(38400);
    pinMode(kModePin, INPUT_PULLUP);
    pinMode(kKeyPin, INPUT_PULLUP);
    pinMode(kLedPin, OUTPUT);
    Keyboard.begin();

    // Setup our mode before resetting everything
    fullKeyboardMode = true;
    resetStateOnModeSwitch();
    indicateMode();
}

// This runs repeatedly
void loop() {
    if (digitalRead(kModePin) == LOW) {
        updateTimer(&modeTimer);
    } else if (modeTimer > kMinModeSwitchPressCycles) {
        // Switch mode
        fullKeyboardMode = !fullKeyboardMode;
        // Reset State
        resetStateOnModeSwitch();
        // Let people know our mode
        indicateMode();
    }

    if (fullKeyboardMode) {
        fullKeyboardUpdate();
    } else {
        practiceKeyboardUpdate();
    }

    // Don't sample all the time, take a break
    delay(1000 / kSampleHz);
}

void fullKeyboardUpdate() {
    if (digitalRead(kKeyPin) == LOW) {
        // Key down
        if (keyPressedTimer == 0) {
            // Visual indicator
            digitalWrite(kLedPin, HIGH);
            // If the last thing printed wasn't a prosign, add a space
            if(allowSpaceInserted && keyReleasedTimer > kLengthGapWord * averageElementSize){
                Keyboard.print(' ');
            }
            keyReleasedTimer = 0;
        }
        updateTimer(&keyPressedTimer);
    } else {
        // Key up
        if (keyReleasedTimer == 0) {
            // Turn off visual indicator
            digitalWrite(kLedPin, LOW);

            // Update our buffer with the element we made
            updateElements(elements, keyPressedTimer);
            // Reset relevant data
            keyPressedTimer = 0;
        }

        if (keyReleasedTimer > kLengthGapLetter * averageElementSize){
            char key = getLetter(elements);
            allowSpaceInserted = isControl(key);
            Keyboard.print(key);

            // Debug what happened
            Serial.write(key);
        }
        updateTimer(&keyReleasedTimer);
    }
}

void clearElements(byte elements[]){
}
void updateElements(byte elements[], unsigned short keyPressTime){
}
char getLetter(byte elements[]){
    return 'a';
}

void practiceKeyboardUpdate() {
    if (digitalRead(kKeyPin) == LOW) {
        // Key down
        Keyboard.press(MODIFIERKEY_SHIFT);
        updateTimer(&keyPressedTimer);
    } else if (keyPressedTimer > 0) {
        // Key up
        Keyboard.release(MODIFIERKEY_SHIFT);
        keyPressedTimer = 0;
    }
}

void indicateMode() {
    String modeStr = fullKeyboardMode ? "full keyboard" : "practice";
    Serial.println("Entered " + modeStr + " mode.");
    // Indicate our mode
    digitalWrite(kLedPin, !fullKeyboardMode);
}

// Make sure we don't wrap around
void updateTimer(unsigned short *timer) {
    if (*timer < kMaxTimerValue) {
        (*timer)++;
    }
}
