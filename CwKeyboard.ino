#include <Bounce.h>

// Constants
/* #define DEBUG_PRINTS */
/* #define RAM_BENCHMARK */
const byte kModePin = 0;
const byte kKeyPin = 1;
const byte kLedPin = 2;
const byte kSpeedPin = 23;
// Configuration constants
const byte kMinModeSwitchPressCycles = 10;
const int kSampleHz = 500;
const unsigned short kMaxTimerValue = 65535;
// NOTE: The first byte is used to track our used capacity
const byte kNumberOfElementsToTrack = 9;
// Used to clear the current letter if there is an error
const char kEraseLetter = 0x18;
// Define length of elements
const byte kLengthDot = 1;
const byte kLengthDash = 3;
const byte kLengthGapElement = 1;
const byte kLengthGapLetter = 3;
const byte kLengthGapWord = 7;
// +/- percentage that an element can be off by and still count
const float kElementSizeTolerance = 0.25;
const short kMaxSpeedPinValue = 3200;
const short kMinSpeedPinValue = 0;
const byte kMinimumElementSize = 10;
const byte kMaxElementSize = 50;

// State variables
bool fullKeyboardMode;
unsigned short modeTimer;
unsigned short keyPressedTimer;
unsigned short keyReleasedTimer;
// Used for LED indicator if releasing would cause a dot/dash
bool beatIndicator;
bool allowSpaceInserted;
// The size of each element
unsigned short elements[kNumberOfElementsToTrack];
// Used to determine what is a dash, dot, etc...
unsigned short averageElementSize;
unsigned short elementTolerance;

// Track the status of the button and handle debounce
bool keyPressed;
Bounce pushbutton = Bounce(kKeyPin, 10);

void resetStateOnModeSwitch() {
    modeTimer = 0;
    keyPressedTimer = 0;
    keyReleasedTimer = 0;
    beatIndicator = false;
    allowSpaceInserted = false;
    clearElements(elements);
    keyPressed = false;
    // Make sure we don't get stuck
    Keyboard.releaseAll();
    // Turn off LED
    digitalWrite(kLedPin, LOW);
}

// This runs once on startup
void setup() {
    // Setup our hardware
#ifdef DEBUG_PRINTS
    Serial.begin(38400);
#endif
    analogReadResolution(12);
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
    // See how much ram we have
#ifdef RAM_BENCHMARK
    short freeRam = freeMemory();
    // Only bother if this isn't a normal value
    if (freeRam != 1984) {
        Serial.print("free ram: ");
        Serial.println(freeRam);
    }
#endif
}

unsigned short calculateSpeed() {
    float speedPinValue = analogRead(kSpeedPin);
    unsigned short speedToElementSize = kMinimumElementSize + ((speedPinValue / (kMaxSpeedPinValue - kMinSpeedPinValue)) * (kMaxElementSize - kMinimumElementSize));
    // Adjust for our kSampleHz
    speedToElementSize *= (kSampleHz * 10) / 1000;
#ifdef DEBUG_PRINTS
    if (speedToElementSize != averageElementSize) {
        Serial.print("speedPin value: ");
        Serial.print(speedPinValue);
        Serial.print(" element size: ");
        Serial.println(speedToElementSize);
    }
#endif
    return speedToElementSize;
}

void fullKeyboardUpdate() {
    // Set averageElementSize to kSpeedPin's analog value
    averageElementSize = calculateSpeed();
    elementTolerance = averageElementSize * kElementSizeTolerance;

    if (pushbutton.update()) {
        // Key down
        if (pushbutton.fallingEdge()) {
            keyPressed = true;
            // If the last thing printed wasn't a prosign, add a space
            if (allowSpaceInserted && isSpace(keyReleasedTimer)) {
                Keyboard.print(' ');
#ifdef DEBUG_PRINTS
                // Debug what happened
                Serial.write(' ');
#endif
            }
            allowSpaceInserted = false;
            keyReleasedTimer = 0;
            // Key up
        } else if (pushbutton.risingEdge()) {
            keyPressed = false;
            // Turn off visual indicator
            digitalWrite(kLedPin, LOW);

            // Update our buffer with the element we made
            updateElements(elements, keyPressedTimer);
            // Reset relevant data
            keyPressedTimer = 0;
        }
    }
    if (keyPressed) {
        updateTimer(&keyPressedTimer);

        // Visual indicator that right now is a good time to release if you want a dot or a dash
        if (!beatIndicator && (isDot(keyPressedTimer, false) || isDash(keyPressedTimer, false))) {
            digitalWrite(kLedPin, HIGH);
            beatIndicator = true;
        } else if (beatIndicator) {
            digitalWrite(kLedPin, LOW);
            beatIndicator = false;
        }
    } else {
        updateTimer(&keyReleasedTimer);
        if (isLetterGap(keyReleasedTimer)) {
            char key = getLetter(elements);
            if (key) {
                clearElements(elements);
                if (key != kEraseLetter) {
                    allowSpaceInserted = isPrintable(key);
                    Keyboard.print(key);

#ifdef DEBUG_PRINTS
                    // Debug what happened
                    Serial.write(key);
#endif
                }
            }
        }
    }
    // Don't sample all the time, take a break
    // TODO: maybe switch to dynamic frame rate?
    delay(1000 / kSampleHz);
}

void clearElements(unsigned short elements[]) {
    // We can clear everything by clearing the capacity
    elements[0] = 0;
}

void updateElements(unsigned short elements[], unsigned short keyPressTime) {
    // Get our next available index
    unsigned short currentCapacity = elements[0];
    // First spot is used to track current capacity
    if (currentCapacity < kNumberOfElementsToTrack - 1) {
        currentCapacity++;
        elements[currentCapacity] = keyPressTime;
        elements[0] = currentCapacity;
    }
}

bool isLengthWithinTolerance(unsigned short length, unsigned short desiredLength) {
    // Require minimum length to encourage good keying
    return length > (desiredLength * averageElementSize - elementTolerance) && length < (desiredLength * averageElementSize + elementTolerance);
}

bool isDot(unsigned short length) {
    return isDot(length, true);
}
bool isDot(unsigned short length, bool print) {
#ifdef DEBUG_PRINTS
    if (print) {
        Serial.print(". Desired: ");
        Serial.print(kLengthDot * averageElementSize);
        Serial.print(" +/- ");
        Serial.println(elementTolerance);
        Serial.print(" Actual: ");
        Serial.println(length);
    }
#endif
    return isLengthWithinTolerance(length, kLengthDot);
}

bool isDash(unsigned short length) {
    return isDash(length, true);
}
bool isDash(unsigned short length, bool print) {
#ifdef DEBUG_PRINTS
    if (print) {
        Serial.print("- Desired: ");
        Serial.print(kLengthDash * averageElementSize);
        Serial.print(" +/- ");
        Serial.println(elementTolerance);
        Serial.print(" Actual: ");
        Serial.println(length);
    }
#endif
    return isLengthWithinTolerance(length, kLengthDash);
}

bool isLetterGap(unsigned short length) {
    return isLengthWithinTolerance(length, kLengthGapLetter);
}

bool isSpace(unsigned short length) {
    return isLengthWithinTolerance(length, kLengthGapWord);
}

char getLetter(unsigned short elements[]) {
    unsigned short currentCapacity = elements[0];
    if (currentCapacity < 1) {
        return 0; // Nothing yet
    }
    // Morse tree
    if (currentCapacity == 1) {
        //.
        if (isDot(elements[1]))
            return 'e';
        //-
        else if (isDash(elements[1]))
            return 't';
        else return kEraseLetter;
    } else if (currentCapacity == 2) {
        //..
        if (isDot(elements[1]) && isDot(elements[2]))
            return 'i';
        //.-
        if (isDot(elements[1]) && isDash(elements[2]))
            return 'a';
        //--
        else if (isDash(elements[1]) && isDash(elements[2]))
            return 'm';
        //-.
        else if (isDash(elements[1]) && isDot(elements[2]))
            return 'n';
        else return kEraseLetter;
    } else if (currentCapacity == 3) {
        //...
        if (isDot(elements[1]) && isDot(elements[2]) && isDot(elements[3]))
            return 's';
        //..-
        if (isDot(elements[1]) && isDot(elements[2]) && isDash(elements[3]))
            return 'u';
        //.-.
        if (isDot(elements[1]) && isDash(elements[2]) && isDot(elements[3]))
            return 'r';
        //.--
        if (isDot(elements[1]) && isDash(elements[2]) && isDash(elements[3]))
            return 'w';
        //--.
        else if (isDash(elements[1]) && isDash(elements[2]) && isDot(elements[3]))
            return 'g';
        //---
        else if (isDash(elements[1]) && isDash(elements[2]) && isDash(elements[3]))
            return 'o';
        //-..
        else if (isDash(elements[1]) && isDot(elements[2]) && isDot(elements[3]))
            return 'd';
        //-.-
        else if (isDash(elements[1]) && isDot(elements[2]) && isDash(elements[3]))
            return 'k';
        else return kEraseLetter;
    } else if (currentCapacity == 4) {
        //....
        if (isDot(elements[1]) && isDot(elements[2]) && isDot(elements[3]) && isDot(elements[4]))
            return 'h';
        //...-
        if (isDot(elements[1]) && isDot(elements[2]) && isDot(elements[3]) && isDash(elements[4]))
            return 'v';
        //..-.
        if (isDot(elements[1]) && isDot(elements[2]) && isDash(elements[3]) && isDot(elements[4]))
            return 'f';
        //.-..
        if (isDot(elements[1]) && isDash(elements[2]) && isDot(elements[3]) && isDot(elements[4]))
            return 'l';
        //.--.
        if (isDot(elements[1]) && isDash(elements[2]) && isDash(elements[3]) && isDot(elements[4]))
            return 'p';
        //.---
        if (isDot(elements[1]) && isDash(elements[2]) && isDash(elements[3]) && isDash(elements[4]))
            return 'j';
        //--..
        else if (isDash(elements[1]) && isDash(elements[2]) && isDot(elements[3]) && isDot(elements[4]))
            return 'z';
        //--.-
        else if (isDash(elements[1]) && isDash(elements[2]) && isDot(elements[3]) && isDash(elements[4]))
            return 'q';
        //-...
        else if (isDash(elements[1]) && isDot(elements[2]) && isDot(elements[3]) && isDot(elements[4]))
            return 'b';
        //-..-
        else if (isDash(elements[1]) && isDot(elements[2]) && isDot(elements[3]) && isDash(elements[4]))
            return 'x';
        //-.-.
        else if (isDash(elements[1]) && isDot(elements[2]) && isDash(elements[3]) && isDot(elements[4]))
            return 'c';
        //-.--
        else if (isDash(elements[1]) && isDot(elements[2]) && isDash(elements[3]) && isDash(elements[4]))
            return 'y';
        else return kEraseLetter;
    } else if (currentCapacity == 5) {
        //.....
        if (isDot(elements[1]) && isDot(elements[2]) && isDot(elements[3]) && isDot(elements[4]) && isDot(elements[5]))
            return '5';
        //....-
        if (isDot(elements[1]) && isDot(elements[2]) && isDot(elements[3]) && isDot(elements[4]) && isDash(elements[5]))
            return '4';
        //...--
        if (isDot(elements[1]) && isDot(elements[2]) && isDot(elements[3]) && isDash(elements[4]) && isDash(elements[5]))
            return '3';
        //..---
        if (isDot(elements[1]) && isDot(elements[2]) && isDash(elements[3]) && isDash(elements[4]) && isDash(elements[5]))
            return '2';
        //.-.-.
        if (isDot(elements[1]) && isDash(elements[2]) && isDot(elements[3]) && isDash(elements[4]) && isDot(elements[5]))
            return '\n';
        //.----
        if (isDot(elements[1]) && isDash(elements[2]) && isDash(elements[3]) && isDash(elements[4]) && isDash(elements[5]))
            return '1';
        //-....
        else if (isDash(elements[1]) && isDot(elements[2]) && isDot(elements[3]) && isDot(elements[4]) && isDot(elements[5]))
            return '6';
        //--...
        else if (isDash(elements[1]) && isDash(elements[2]) && isDot(elements[3]) && isDot(elements[4]) && isDot(elements[5]))
            return '7';
        //---..
        else if (isDash(elements[1]) && isDash(elements[2]) && isDash(elements[3]) && isDot(elements[4]) && isDot(elements[5]))
            return '8';
        //----.
        else if (isDash(elements[1]) && isDash(elements[2]) && isDash(elements[3]) && isDash(elements[4]) && isDot(elements[5]))
            return '9';
        //-----
        else if (isDash(elements[1]) && isDash(elements[2]) && isDash(elements[3]) && isDash(elements[4]) && isDash(elements[5]))
            return '0';
        else return kEraseLetter;
    } else if (currentCapacity == 8) {
        //........(Backspace)
        for (unsigned short i = 0; i < currentCapacity; i++) {
            if (!isDot(elements[i + 1])) {
                return kEraseLetter;
            }
        }
        return 0x08; // Backspace
    }
    // Bad char
    return kEraseLetter;
}

void practiceKeyboardUpdate() {
    averageElementSize = calculateSpeed();
    unsigned short timeToKeepLedOn = averageElementSize * kLengthDot;
    short timeToKeepLedOff = averageElementSize * kLengthDash;
    if (pushbutton.update()) {
        // Key down
        if (pushbutton.fallingEdge()) {
            keyPressed = false;
            Keyboard.press(MODIFIERKEY_SHIFT);
            // Key up
        } else if (pushbutton.risingEdge()) {
            keyPressed = false;
            Keyboard.release(MODIFIERKEY_SHIFT);
        }
    }
    // Use this for the speed indicator
    updateTimer(&keyPressedTimer);
    if (!beatIndicator && keyPressedTimer > timeToKeepLedOff) {
        digitalWrite(kLedPin, HIGH);
        beatIndicator = true;
    } else if (keyPressedTimer > timeToKeepLedOff + timeToKeepLedOn) {
        digitalWrite(kLedPin, LOW);
        keyPressedTimer = 0;
        beatIndicator = false;
    }
    // Don't sample all the time, take a break
    delay(1000 / kSampleHz);
}

void indicateMode() {
#ifdef DEBUG_PRINTS
    String modeStr = fullKeyboardMode ? "full keyboard" : "practice";
    Serial.println("Entered " + modeStr + " mode.");
#endif
    // Indicate our mode
    /* digitalWrite(kLedPin, !fullKeyboardMode); */
}

// Make sure we don't wrap around
void updateTimer(unsigned short *timer) {
    if (*timer < kMaxTimerValue) {
        (*timer)++;
    }
}

#ifdef RAM_BENCHMARK
#ifdef __arm__
// should use uinstd.h to define sbrk but Due causes a conflict
extern "C" char* sbrk(int incr);
#else  // __ARM__
extern char *__brkval;
#endif  // __arm__
int freeMemory() {
    char top;
#ifdef __arm__
    return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
    return &top - __brkval;
#else  // __arm__
    return __brkval ? &top - __brkval : &top - __malloc_heap_start;
#endif  // __arm__
}
#endif
