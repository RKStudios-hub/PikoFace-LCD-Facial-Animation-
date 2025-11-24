#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// -------------------------------
// Custom chars (patterns)
// -------------------------------

// Eyes
byte eyeCenter[8] = {
  B00000, B01110, B10001, B10101,
  B10101, B10001, B01110, B00000
};

byte eyeLeft[8] = {
  B00000, B01110, B10001, B10000,
  B10000, B10001, B01110, B00000
};

byte eyeRight[8] = {
  B00000, B01110, B10001, B00001,
  B00001, B10001, B01110, B00000
};

byte eyeHappy[8] = {
  B00000, B00000, B10001, B10101,
  B10101, B10001, B01110, B00000
};

byte eyeAngryLeft[8] = {
  B00000, B01000, B10000, B10100,
  B10100, B10000, B01110, B00000
};

byte eyeAngryRight[8] = {
  B00000, B00010, B00001, B00101,
  B00101, B00001, B01110, B00000
};

byte eyeClosed[8] = {
  B00000, B00000, B00000, B11111,
  B00000, B00000, B00000, B00000
};

byte eyeWinkLeft[8] = {
  B00000, B00000, B10001, B11111,
  B10001, B00000, B00000, B00000
};

byte eyeWinkRight[8] = {
  B00000, B00000, B10001, B10001,
  B11111, B00000, B00000, B00000
};

byte eyeTearLeft[8] = {
  B00000, B01110, B10001, B10101,
  B10101, B10001, B01110, B00100
};

byte eyeTearRight[8] = {
  B00000, B01110, B10001, B10101,
  B10101, B10001, B01110, B00010
};

byte starEye[8] = {
  B00100, B01110, B10101, B01110,
  B00100, B00000, B00000, B00000
};

byte sweatDrop[8] = {
  B00000, B00100, B00100, B00100,
  B00010, B00000, B00000, B00000
};

// Blush
byte blush[8] = {
  B00000, B00000, B00000, B00100,
  B00100, B00000, B00000, B00000
};

byte blushBright[8] = {
  B00000, B00100, B01110, B11111,
  B01110, B00100, B00000, B00000
};

// Mouth shapes (slot 3 is reserved for mouth)
byte mouth_M[8] = {
  B00000, B00000, B00000, B01110,
  B01110, B00000, B00000, B00000
};

byte mouth_E[8] = {
  B00000, B00000, B01010, B01110,
  B01010, B00000, B00000, B00000
};

byte mouth_A[8] = {
  B00000, B00000, B00100, B01110,
  B01110, B00100, B00000, B00000
};

byte mouth_O[8] = {
  B00000, B00000, B00100, B01010,
  B01010, B00100, B00000, B00000
};

byte mouth_U[8] = {
  B00000, B00000, B00000, B01010,
  B00100, B00000, B00000, B00000
};

// -------------------------------
// State variables
// -------------------------------
char currentMouth = 'M';  // displayed mouth char (whatever Python sends)
unsigned long mouthLastUpdate = 0;
unsigned long mouthIdleTimeout = 1200; // ms — if no mouth frames for a while, return to M

// Eye modes
enum EyeMode { E_CENTER, E_LEFT, E_RIGHT, E_HAPPY, E_ANGRY, E_SURPRISED };
EyeMode eyeMode = E_CENTER;

// Head tilt (confused)
int headTilt = 0;     // -1, 0, +1
bool confusedMode = false;

// Blink/wink
bool blinking = false;
unsigned long blinkStart = 0;
int blinkDuration = 80;       // faster blink: 60-100ms

bool winkActive = false;
unsigned long winkStart = 0;
int winkDuration = 110;       // faster wink
bool winkLeft = false;

// Tears / sweat / stars
bool tearsOn = false;
unsigned long tearsStart = 0;

bool starEyes = false;
unsigned long starStart = 0;
int starDuration = 700; // ms

bool sweatDropOneShot = false;
unsigned long sweatStart = 0;
int sweatDuration = 700; // ms

// Blush
bool showBlush = false;
bool heartbeatBlush = false;
unsigned long blushStart = 0;
int blushPulseStage = 0;
unsigned long lastBlushCheck = 0;

// Random timers (tuned)
unsigned long lastEyeChange = 0;
unsigned long nextEyeChange = 3000; // 3-5s idle eye movement
unsigned long lastBlink = 0;
unsigned long nextBlink = 2000;     // faster blink 2-4s
unsigned long lastBlushCheckRand = 0;
unsigned long nextBlushCheck = 5000; // check blush every ~5-9s

// Random seed pin
#define RAND_PIN A0

// -------------------------------
// Helpers: dynamic char load
// slots mapping: 0 = left eye, 1 = right eye, 2 = blush, 3 = mouth, 4 = temp, 5 = temp2
// -------------------------------
void loadEyes(byte *leftPattern, byte *rightPattern) {
  lcd.createChar(0, leftPattern);
  lcd.createChar(1, rightPattern);
}

void loadBlush(bool bright) {
  if (bright) lcd.createChar(2, blushBright);
  else lcd.createChar(2, blush);
}

void loadMouthChar(char p) {
  // map char to available mouth patterns. If you later extend with more frames,
  // add them and map here. For now we support M, E, A, O, U
  if (p == 'M') lcd.createChar(3, mouth_M);
  else if (p == 'E') lcd.createChar(3, mouth_E);
  else if (p == 'A') lcd.createChar(3, mouth_A);
  else if (p == 'O') lcd.createChar(3, mouth_O);
  else if (p == 'U') lcd.createChar(3, mouth_U);
  else lcd.createChar(3, mouth_M); // fallback
}

void loadTemp(byte *pattern, uint8_t slot = 4) {
  lcd.createChar(slot, pattern);
}

// -------------------------------
// Draw face function (composes current face)
// -------------------------------
void drawFace() {
  lcd.clear();

  int leftX = 4 + headTilt;
  int rightX = 11 + headTilt;
  int mouthX = 7 + headTilt;

  // choose eye patterns
  byte *lpat = eyeCenter;
  byte *rpat = eyeCenter;

  if (tearsOn) {
    // teary variants
    lpat = eyeTearLeft;
    rpat = eyeTearRight;
  } else if (starEyes) {
    lpat = starEye;
    rpat = starEye;
  } else if (blinkActive()) {
    lpat = eyeClosed;
    rpat = eyeClosed;
  } else if (winkActive) {
    if (winkLeft) {
      lpat = eyeWinkLeft;
      rpat = (eyeMode == E_RIGHT ? eyeRight : eyeCenter);
    } else {
      lpat = (eyeMode == E_LEFT ? eyeLeft : eyeCenter);
      rpat = eyeWinkRight;
    }
  } else {
    // default mapping based on mode
    switch (eyeMode) {
      case E_CENTER: lpat = eyeCenter; rpat = eyeCenter; break;
      case E_LEFT:   lpat = eyeLeft;   rpat = eyeRight;  break;
      case E_RIGHT:  lpat = eyeLeft;   rpat = eyeRight;  break;
      case E_HAPPY:  lpat = eyeHappy;  rpat = eyeHappy;  break;
      case E_ANGRY:  lpat = eyeAngryLeft; rpat = eyeAngryRight; break;
      case E_SURPRISED: lpat = eyeCenter; rpat = eyeCenter; break;
      default: lpat = eyeCenter; rpat = eyeCenter; break;
    }
  }

  // load chosen chars into slots 0 & 1
  loadEyes(lpat, rpat);

  // ensure mouth slot is loaded
  loadMouthChar(currentMouth);

  // draw left eye
  lcd.setCursor(leftX, 0);
  lcd.write(byte(0));

  // draw right eye
  lcd.setCursor(rightX, 0);
  lcd.write(byte(1));

  // blush
  if (showBlush) {
    loadBlush(heartbeatBlush && (blushPulseStage % 2 == 1));
    lcd.setCursor(2, 1);
    lcd.write(byte(2));
    lcd.setCursor(13, 1);
    lcd.write(byte(2));
  }

  // sweat one-shot (top-right)
  if (sweatDropOneShot) {
    loadTemp(sweatDrop, 4);
    lcd.setCursor(rightX + 2, 0);
    lcd.write(byte(4));
  }

  // tears show under eyes (use temp slots)
  if (tearsOn) {
    loadTemp(eyeTearLeft, 4);
    lcd.setCursor(leftX, 1);
    lcd.write(byte(4));
    loadTemp(eyeTearRight, 5);
    lcd.setCursor(rightX, 1);
    lcd.write(byte(5));
  }

  // star eyes override (draw stars in eye slots)
  if (starEyes) {
    loadTemp(starEye, 4);
    lcd.setCursor(leftX, 0);
    lcd.write(byte(4));
    lcd.setCursor(rightX, 0);
    lcd.write(byte(4));
  }

  // draw mouth
  lcd.setCursor(mouthX, 1);
  lcd.write(byte(3));
}

// helper to check blink
bool blinkActive() {
  if (blinking && (millis() - blinkStart < (unsigned long)blinkDuration)) return true;
  return false;
}

// -------------------------------
// Random behavior engine (non-blocking)
// Tuned timings: faster blinks, slower blush fade
// -------------------------------
void scheduleNextEyeChange() {
  nextEyeChange = random(3000, 5000);
  lastEyeChange = millis();
}

void scheduleNextBlink() {
  nextBlink = random(2000, 4000);
  lastBlink = millis();
}

void scheduleNextBlushCheck() {
  nextBlushCheck = random(4000, 9000);
  lastBlushCheckRand = millis();
}

void randomBehaviorTick() {
  unsigned long now = millis();

  // eyes movement
  if (now - lastEyeChange >= nextEyeChange) {
    if (!tearsOn && !starEyes && !winkActive) {
      int pick = random(0, 10);
      if (pick < 3) eyeMode = E_CENTER;
      else if (pick < 5) eyeMode = E_LEFT;
      else if (pick < 7) eyeMode = E_RIGHT;
      else if (pick < 8) eyeMode = E_HAPPY;
      else if (pick < 9) eyeMode = E_ANGRY;
      else eyeMode = E_SURPRISED;
    }
    scheduleNextEyeChange();
    drawFace();
  }

  // blinking (faster)
  if (now - lastBlink >= nextBlink) {
    blinking = true;
    blinkStart = now;
    scheduleNextBlink();
    drawFace();
  }
  if (blinking && (now - blinkStart >= (unsigned long)blinkDuration)) {
    blinking = false;
    drawFace();
  }

  // random wink (rare)
  if (!winkActive && random(0, 1000) < 8) {
    winkActive = true;
    winkLeft = (random(0,2) == 0);
    winkStart = now;
    drawFace();
  }
  if (winkActive && (now - winkStart >= (unsigned long)winkDuration)) {
    winkActive = false;
    drawFace();
  }

  // blush check (slower fade, but more often if you asked earlier)
  if (now - lastBlushCheckRand >= nextBlushCheck) {
    // ~25% chance every check
    if (!showBlush && random(0,100) < 25) {
      showBlush = true;
      blushStart = now;
      heartbeatBlush = (random(0,100) < 45); // ~45% of blushes heartbeat
      blushPulseStage = 0;
      drawFace();
    }
    scheduleNextBlushCheck();
  }

  // blush pulsing & fade (slower fade)
  if (showBlush) {
    if (heartbeatBlush) {
      if (((now - blushStart) / 240) > (unsigned long)blushPulseStage) {
        blushPulseStage++;
        drawFace();
      }
    }
    // hide blush after 900 - 1800 ms (slower fade)
    if (now - blushStart > (unsigned long)random(900, 1800)) {
      showBlush = false;
      heartbeatBlush = false;
      drawFace();
    }
  }

  // random tears
  if (!tearsOn && random(0, 20000) < 8) { // rare
    tearsOn = true;
    tearsStart = now;
    drawFace();
  }
  if (tearsOn && (now - tearsStart > (unsigned long)random(1200, 3600))) {
    tearsOn = false;
    drawFace();
  }

  // star eyes (rare)
  if (!starEyes && random(0, 20000) < 8) {
    starEyes = true;
    starStart = now;
    drawFace();
  }
  if (starEyes && (now - starStart > (unsigned long)starDuration)) {
    starEyes = false;
    drawFace();
  }

  // sweat drop one-shot
  if (!sweatDropOneShot && random(0, 20000) < 10) {
    sweatDropOneShot = true;
    sweatStart = now;
    drawFace();
  }
  if (sweatDropOneShot && (now - sweatStart > (unsigned long)sweatDuration)) {
    sweatDropOneShot = false;
    drawFace();
  }

  // confused head-tilt occasional nudge
  if (confusedMode && random(0, 1000) < 10) {
    headTilt = (random(0,2) == 0) ? -1 : 1;
    drawFace();
  }
  if (!confusedMode && headTilt != 0 && random(0,1000) < 6) {
    headTilt = 0;
    drawFace();
  }
}

// -------------------------------
// Serial command handler
// -------------------------------
void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();

    // Treat lowercase as uppercase for convenience
    if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';

    // Mouth frames (Python sends single-char frames)
    if (c == 'M' || c == 'E' || c == 'A' || c == 'O' || c == 'U') {
      currentMouth = c;
      mouthLastUpdate = millis();
      loadMouthChar(currentMouth);
      drawFace();
      continue;
    }

    // If Python wants to send a different mouth frame not in the set above,
    // add mapping in loadMouthChar and add condition here.

    // other expression commands:
    switch (c) {
      case 'N': // neutral reset
        eyeMode = E_CENTER;
        showBlush = false;
        tearsOn = false;
        starEyes = false;
        confusedMode = false;
        headTilt = 0;
        drawFace();
        break;

      case 'L': // look left
        eyeMode = E_LEFT; drawFace(); break;
      case 'R': // look right
        eyeMode = E_RIGHT; drawFace(); break;
      case 'H': // happy
        eyeMode = E_HAPPY; drawFace(); break;
      case 'G': // grumpy / angry
        eyeMode = E_ANGRY; drawFace(); break;
      case 'S': // surprised
        eyeMode = E_SURPRISED; drawFace(); break;
      case 'W': // wink
        winkActive = true;
        winkLeft = (random(0,2)==0);
        winkStart = millis();
        drawFace();
        break;

      case 'B': // heartbeat blush (immediate)
        showBlush = true;
        heartbeatBlush = true;
        blushStart = millis();
        blushPulseStage = 0;
        drawFace();
        break;

      case 'T': // toggle tears
        tearsOn = !tearsOn;
        if (tearsOn) tearsStart = millis();
        drawFace();
        break;

      case 'Y': // sweat one-shot
        sweatDropOneShot = true;
        sweatStart = millis();
        drawFace();
        break;

      case 'X': // star eyes
        starEyes = true;
        starStart = millis();
        drawFace();
        break;

      case 'C': // confused toggle
        confusedMode = !confusedMode;
        if (confusedMode) headTilt = -1; else headTilt = 0;
        drawFace();
        break;

      default:
        // unknown commands ignored
        break;
    }
  }
}

// -------------------------------
// Setup & Loop
// -------------------------------
void setup() {
  lcd.init();
  lcd.backlight();

  // Preload a few slots to avoid flicker
  lcd.createChar(0, eyeCenter);
  lcd.createChar(1, eyeCenter);
  lcd.createChar(2, blush);
  lcd.createChar(3, mouth_M);

  Serial.begin(9600);

  randomSeed(analogRead(RAND_PIN));

  scheduleNextEyeChange();
  scheduleNextBlink();
  scheduleNextBlushCheck();

  drawFace();
}

void loop() {
  // handle incoming serial frames & commands
  handleSerial();

  // return to closed mouth after idle timeout if no new frames from Python
  if (currentMouth != 'M' && (millis() - mouthLastUpdate > mouthIdleTimeout)) {
    currentMouth = 'M';
    loadMouthChar(currentMouth);
    drawFace();
  }

  // run autonomous behavior
  randomBehaviorTick();

  // small sleep — keeps loop responsive and reduces flicker
  delay(30);
}

/*
Serial Command Reference (single char commands):

M,E,A,O,U  -> mouth frames (Python should send these directly for multi-frame lip-sync)
N          -> reset to neutral
L          -> look left
R          -> look right
H          -> happy eyes
G          -> angry/grumpy eyes
S          -> surprised
W          -> wink (auto left/right)
B          -> heartbeat blush (immediate)
T          -> toggle tears on/off
Y          -> sweat one-shot
X          -> star eyes (short)
C          -> toggle confused head-tilt

Notes:
- Python should send single-byte chars for mouth frames in sequence to implement smooth lip-sync.
  E.g. smoother sequence (from Python): b'M', b'E', b'A', b'E', b'M'  (Arduino will display each frame)
- If you want more mouth frames (H1, H2, etc.) you can add additional pattern arrays and map characters to them in loadMouthChar().
- Timings tuned: faster blinks (60–100ms), slower blush fade (0.9–1.8s), mouth idle timeout ~1.2s.
*/
