#include <Bluepad32.h>
#include <ESP32Servo.h>

// ============================================================
// PIN DEFINITIONS
// ============================================================

#define SERVO_PIN 12
#define ESC_PIN   13

// ============================================================
// SERVO / ESC
// ============================================================

Servo steeringServo;
Servo esc;

// ============================================================
// CONTROLLER
// ============================================================

ControllerPtr myController = nullptr;

// Left-stick deadzone
const int DEADZONE = 30;

// ============================================================
// PWM SETTINGS
// ============================================================

const int NEUTRAL_US = 1500;

const int ESC_FORWARD_MAX_US = 2000;
const int ESC_REVERSE_MAX_US = 1000;

const int SERVO_LEFT_US   = 1000;
const int SERVO_CENTER_US = 1500;
const int SERVO_RIGHT_US  = 2000;

// ============================================================
// DEADZONE
// ============================================================

int applyDeadzone(int value, int deadzone) {

  if (abs(value) < deadzone) {
    return 0;
  }

  return value;
}

// ============================================================
// CONTROLLER CONNECTED
// ============================================================

void onConnectedController(ControllerPtr ctl) {

  Serial.println("Controller connected!");

  myController = ctl;

  // Start safely
  steeringServo.writeMicroseconds(SERVO_CENTER_US);
  esc.writeMicroseconds(NEUTRAL_US);
}

// ============================================================
// CONTROLLER DISCONNECTED
// ============================================================

void onDisconnectedController(ControllerPtr ctl) {

  Serial.println("Controller disconnected!");

  myController = nullptr;

  // Safety:
  // Centre steering and immediately command neutral throttle
  steeringServo.writeMicroseconds(SERVO_CENTER_US);
  esc.writeMicroseconds(NEUTRAL_US);
}

// ============================================================
// SETUP
// ============================================================

void setup() {

  Serial.begin(115200);

  Serial.println("Starting RC car...");

  // ----------------------------------------------------------
  // Set BOTH servo and ESC to standard RC 50 Hz
  //
  // We KNOW the ESC accepts:
  //
  // 50 Hz
  // 1500 us neutral
  //
  // ----------------------------------------------------------

  steeringServo.setPeriodHertz(50);
  esc.setPeriodHertz(50);

  // Attach PWM outputs
  steeringServo.attach(
    SERVO_PIN,
    SERVO_LEFT_US,
    SERVO_RIGHT_US
  );

  esc.attach(
    ESC_PIN,
    ESC_REVERSE_MAX_US,
    ESC_FORWARD_MAX_US
  );

  // ----------------------------------------------------------
  // SAFE STARTUP
  // ----------------------------------------------------------

  steeringServo.writeMicroseconds(SERVO_CENTER_US);

  // IMPORTANT:
  // 1500 us is neutral for our car ESC
  esc.writeMicroseconds(NEUTRAL_US);

  Serial.println("Sending 1500 us neutral to ESC...");

  // Give ESC time to recognise neutral and arm
  delay(3000);

  // ----------------------------------------------------------
  // BLUEPAD32
  // ----------------------------------------------------------

  BP32.setup(
    &onConnectedController,
    &onDisconnectedController
  );

  // Leave this commented once the PS4 controller is paired.
  //
  // Uncomment temporarily if you need to clear old pairings:
  //
  // BP32.forgetBluetoothKeys();

  Serial.println("ESC should now be armed.");
  Serial.println("Waiting for PS4 controller...");
}

// ============================================================
// LOOP
// ============================================================

void loop() {

  // Process Bluetooth/controller events
  BP32.update();

  // ==========================================================
  // CONTROLLER CONNECTED
  // ==========================================================

  if (myController && myController->isConnected()) {

    // --------------------------------------------------------
    // STEERING
    // Left stick X axis
    // Approximately -512 -> +512
    // --------------------------------------------------------

    int steerRaw =
      applyDeadzone(myController->axisX(), DEADZONE);

    // Constrain just in case
    steerRaw = constrain(steerRaw, -512, 512);

    int steerUs = map(
      steerRaw,
      -512,
      512,
      SERVO_LEFT_US,
      SERVO_RIGHT_US
    );

    steeringServo.writeMicroseconds(steerUs);

    // --------------------------------------------------------
    // THROTTLE
    //
    // R2 = Forward
    // L2 = Reverse
    //
    // Neutral = 1500 us
    // --------------------------------------------------------

    int r2 = myController->throttle();
    int l2 = myController->brake();

    // Keep values in expected range
    r2 = constrain(r2, 0, 1023);
    l2 = constrain(l2, 0, 1023);

    int throttleUs = NEUTRAL_US;

    // Small trigger deadzone to prevent accidental movement
    const int TRIGGER_DEADZONE = 20;

    // --------------------------------------------------------
    // FORWARD
    // --------------------------------------------------------

    if (r2 > TRIGGER_DEADZONE &&
        l2 <= TRIGGER_DEADZONE) {

      throttleUs = map(
        r2,
        TRIGGER_DEADZONE,
        1023,
        NEUTRAL_US,
        ESC_FORWARD_MAX_US
      );
    }

    // --------------------------------------------------------
    // REVERSE
    // --------------------------------------------------------

    else if (l2 > TRIGGER_DEADZONE &&
             r2 <= TRIGGER_DEADZONE) {

      throttleUs = map(
        l2,
        TRIGGER_DEADZONE,
        1023,
        NEUTRAL_US,
        ESC_REVERSE_MAX_US
      );
    }

    // --------------------------------------------------------
    // BOTH TRIGGERS / NO TRIGGER
    //
    // Treat as neutral for safety.
    // --------------------------------------------------------

    else {

      throttleUs = NEUTRAL_US;
    }

    // --------------------------------------------------------
    // KILL SWITCH
    //
    // Cross / X button
    // --------------------------------------------------------

    if (myController->a()) {

      throttleUs = NEUTRAL_US;
    }

    // Final safety clamp
    throttleUs = constrain(
      throttleUs,
      ESC_REVERSE_MAX_US,
      ESC_FORWARD_MAX_US
    );

    // Send signal to ESC
    esc.writeMicroseconds(throttleUs);

    // --------------------------------------------------------
    // SERIAL MONITOR
    // --------------------------------------------------------

    Serial.printf(
      "Steer: %4d us | R2: %4d | L2: %4d | ESC: %4d us\n",
      steerUs,
      r2,
      l2,
      throttleUs
    );
  }

  // ==========================================================
  // NO CONTROLLER
  // ==========================================================

  else {

    // Continually maintain safe outputs

    steeringServo.writeMicroseconds(SERVO_CENTER_US);

    // Crucially, keep sending the SAME 1500 us neutral signal
    // that we know successfully arms the ESC.
    esc.writeMicroseconds(NEUTRAL_US);
  }

  // 20 ms loop interval = 50 Hz
  delay(20);
}