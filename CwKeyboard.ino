// Constants
const int kModePin = 0;
const int kKeyPin = 1;
const int kLedPin = 2;
// Configuration constants
const int kMinModeSwitchPressCycles = 10;
const int kSampleHz = 100;
const int kMaxTimerValue = 65535;

// State variables
bool fullKeyboardMode;
unsigned short modeTimer;
unsigned short keyPressedTimer;
unsigned short keyReleasedTimer;

void resetStateOnModeSwitch() {
    modeTimer = 0;
    keyPressedTimer = 0;
    keyReleasedTimer = 1; // Make sure we don't trigger key released logic
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
            keyReleasedTimer = 0;
        }
        updateTimer(&keyPressedTimer);
    } else {
        // Key up
        if (keyReleasedTimer == 0) {
            // Turn off visual indicator
            digitalWrite(kLedPin, LOW);
            keyPressedTimer = 0;

            // TODO: Determine which key to send
            char key = 'a';
            Keyboard.print(key);

            // Debug what happened
            Serial.write(key);
        }
        updateTimer(&keyReleasedTimer);
    }
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

void updateTimer(unsigned short *timer) {
    if (*timer < kMaxTimerValue) {
        (*timer)++;
    }
}
