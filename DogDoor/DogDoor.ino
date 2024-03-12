#include <WiFiS3.h>
#include <quickping.h>
#include <quickping_wifi.h>
#include <quickping_http.h>

struct Action
{
  int direction;
  int duration; // ms to run
  int power;
};

const int BRAKE_PIN = 9;
const int DIRECTION_PIN = 12;
const int LIMIT_OPEN_PIN = 2;
const int PWM_PIN = 3;
const int CURRENT_PIN = A0;
#define OPEN_DIRECTION LOW
#define CLOSE_DIRECTION HIGH

unsigned long stopMotorAt = 0;

// STATE OF MOTOR
bool stateIsDirty = false;
char *MOVING = "MOVING";
char *CLOSED = "CLOSED";
char *OPEN = "OPEN";
char *STOPPED = "STOPPED";

char motorDirection = OPEN_DIRECTION;
unsigned char motorPower = 0;

QuickPing quickPing;
QuickPingState state;

QuickPingState *getState()
{
  state.clear(motorPower > 0 ? 1 : isOpen() ? 2
                                            : 0);
  char *tmp = motorPower > 0 ? MOVING : isOpen() ? OPEN
                                                 : CLOSED;
  state.addValue("state", tmp);
  state.addValue("direction", motorDirection);
  state.addValue("power", motorPower);
  Serial.print("STATE: ");
  Serial.println(stte.getString());
  return &state;
}

void setMotorPower(int power)
{
  stateIsDirty = true;
  motorPower = power;
  analogWrite(PWM_PIN, motorPower);
}

void onLimitOpen()
{
  setMotorPower(0);
  stopMotorAt = 0;
}

void setup()
{
  state.clear();
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }

  pinMode(DIRECTION_PIN, OUTPUT);
  pinMode(PWM_PIN, OUTPUT);
  pinMode(BRAKE_PIN, OUTPUT);
  pinMode(LIMIT_OPEN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(LIMIT_OPEN_PIN), onLimitOpen, HIGH);

  WiFiServer wifiServer(80);

  // TRY TO READ CONFIG
  Serial.println("[QP] RUNNING");

  quickPing.loadConfig(&wifiServer);
  quickPing.run(&wifiServer);
}

bool isOpen()
{
  return digitalRead(LIMIT_OPEN_PIN);
}

void setMotor(Action *a)
{
  // NOTE: I'm not sure if we want to use the break.
  if (a->power == 0)
  {
    digitalWrite(BRAKE_PIN, HIGH);
    return;
  }
  else
  {
    digitalWrite(BRAKE_PIN, LOW);
  }

  motorDirection = a->direction;

  Serial.print("SETTING MOTOR: \n\tPOWER: ");
  Serial.print(a->power);
  Serial.print("\n\tROTATION: ");
  Serial.println(motorDirection);
  digitalWrite(DIRECTION_PIN, motorDirection);
  setMotorPower(a->power);

  if (a->duration > 0)
  {
    stopMotorAt = millis() + a->duration * 50; // scales to allow for up to 10s
  }
  else
  {
    stopMotorAt = 0;
  }
}

void stop()
{
  Serial.println("[POTAMUS] STOPPING");
  stopMotorAt = 0;
  setMotorPower(0);
}

void open()
{
  Serial.println("[POTAMUS] OPENING");

  Action a = {
    direction : OPEN_DIRECTION,
    duration : 0,
    power : 255,
  };
  setMotor(&a);
}

void close()
{
  Serial.println("[POTAMUS] CLOSING");

  Action a = {
    direction : CLOSE_DIRECTION,
    duration : 85,
    power : 100,
  };
  setMotor(&a);
}

void loop()
{
  if (stopMotorAt > 0 && stopMotorAt < millis())
  {
    stop();
  }
  if (stateIsDirty)
  {
    quickPing.sendPing(getState());
    stateIsDirty = false;
  }
  else
  {
    Serial.println("[POTAMUS] LOOPING");
    QuickPingMessage *message = quickPing.loop(getState());
    if (message == NULL || message->action == 'P')
    {
      free(message);
      return;
    }
    switch (message->action)
    {
    case 'S':
      Serial.println("[POTAMUS] STOPPING");
      stop();
      break;
    case 'O':
      Serial.println("[POTAMUS] OPENING");
      open();
      break;
    case 'C':
      Serial.println("[POTAMUS] CLOSING");
      close();
      break;
    }
    free(message);
  }
}
