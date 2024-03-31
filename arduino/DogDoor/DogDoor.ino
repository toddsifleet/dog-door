#include <WiFiS3.h>
#include <quickping.h>

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
char *DOOR_CLOSED = "CLOSED";
char *DOOR_OPEN = "OPEN";
char *STOPPED = "STOPPED";

char motorDirection = OPEN_DIRECTION;
unsigned char motorPower = 0;

QuickPing quickPing;

char *getPingBody()
{
  if (motorPower > 0)
  {
    return MOVING;
  }
  else if (isOpen())
  {
    return DOOR_OPEN;
  }
  else
  {
    return DOOR_CLOSED;
  }
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
  Serial.begin(115200);
  while (!Serial)
  {
    ;
  }

  QuickPingConfig config = {
      // .serverIP = IPAddress(192, 168, 86, 213),
      .serverIP = IPAddress(64, 91, 249, 186),
      .serverPort = 2525,
      .localPort = 2525,
      .uuid = "DEVICE_UUID",
      .ssid = "WIFI_SSID",
      .wifiPassword = "WIFI_PASSWORD",
      .debug = true,
  };

  pinMode(DIRECTION_PIN, OUTPUT);
  pinMode(PWM_PIN, OUTPUT);
  pinMode(BRAKE_PIN, OUTPUT);
  pinMode(LIMIT_OPEN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(LIMIT_OPEN_PIN), onLimitOpen, HIGH);

  Serial.println("[QP] RUNNING");

  quickPing.run(&config);
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
  unsigned long currentMillis = millis();
  // We need to protect the motor from running too long if millis rolls over.
  // so we just panic and stop the motor if millis < 10,000 (10 seconds, bootup time)
  if (stopMotorAt > 0 && (stopMotorAt < currentMillis || currentMillis < 10000))
  {
    stop();
  }
  if (stateIsDirty)
  {
    quickPing.sendPing(getPingBody());
    stateIsDirty = false;
  }
  else
  {
    QuickPingMessage *message = quickPing.loop(getPingBody());
    if (message)
    {
      if (message->method == 'C')
      {
        Serial.print("[POTAMUS] RECEIVED COMMAND:");
        Serial.println(message->body);
        switch (message->body[0])
        {
        case 'S':
          stop();
          break;
        case 'O':
          open();
          break;
        case 'C':
          close();
          break;
        case 'T':
          if (isOpen())
            close();
          else
            open();
          break;
        }
      }
      else
      {
        Serial.print("[POTAMUS] RECEIVED UNKNOWN COMMAND:");
        Serial.print(message->method);
        Serial.print(":");
        Serial.println(message->body);
      }
    }
    free(message);
  }
}
