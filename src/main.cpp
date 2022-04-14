#include <Arduino.h>
#include <Pushbutton.h>
#include <RotaryEncoder.h>
#include "SevenSegmentTM1637.h"
#include "SevenSegmentExtended.h"
#include "SevenSegmentFun.h"
#include <IntervalCounter.h>
#include "playSong.h"

#define ENCODER_DT A2
#define ENCODER_CLK A3
#define ENCODER_BUTTON 4
#define START_BUTTON 6
#define STOP_BUTTON 7
#define DISPLAY_DIO 9
#define DISPLAY_CLK 10
#define BUZZER 11
#define UV_LED 2

Pushbutton encoderButton(ENCODER_BUTTON);
Pushbutton startButton(START_BUTTON);
Pushbutton stopButton(STOP_BUTTON);
RotaryEncoder encoder(ENCODER_CLK, ENCODER_DT, RotaryEncoder::LatchMode::FOUR3);
SevenSegmentFun display(DISPLAY_CLK, DISPLAY_DIO);

IntervalCounter interval(1.0);
IntervalCounter blinkInterval(0.1);

enum STATE
{
  SETUP,
  INITIAL,
  TIMER_EDITABLE,
  TIMER_SET,
  TIMER_RUNNING,
  TIMER_PAUSED,
  SECRET
};

STATE state = INITIAL;
bool blinking = false;
bool showDisplay = true;
long unsigned exposureTime = 0;

void setState(STATE);
void printTime(long unsigned);
void startTimer();
void pauseTimer();
void stopTimer();
void handleSecret();

void handleSecret()
{
  display.scrollingText("SinAn SAkic - nE trAzi je sine", 1);
  display.flush();
  display.printTime(66,66, true);
  playSong(BUZZER);
  setState(INITIAL);
}

void setState(STATE newState)
{
  Serial.print("State: ");
  Serial.print(state);
  Serial.print(" - new state: ");
  Serial.println(newState);

  if (newState != TIMER_RUNNING)
  {
    digitalWrite(UV_LED, LOW);
  }

  if (state == TIMER_EDITABLE)
  {
    showDisplay = true;
    blinking = false;
    printTime(exposureTime);
  }

  state = newState;

  switch (state)
  {
  case SETUP:
    break;
  case INITIAL:
    stopTimer();
    exposureTime = 0;
    printTime(exposureTime);
    display.snake(1);
    break;
  case TIMER_EDITABLE:
    blinking = true;
    break;
  case TIMER_SET:
    break;
  case TIMER_RUNNING:
    startTimer();
    break;
  case TIMER_PAUSED:
    pauseTimer();
    break;
  case SECRET:
    break;
  }
};

int getMinutes(long unsigned totalSeconds)
{
  return (totalSeconds % 3600) / 60;
};

int getSeconds(long unsigned totalSeconds)
{
  return totalSeconds % 60;
};

void printTime(long unsigned seconds)
{
  display.printTime(getMinutes(seconds), getSeconds(seconds), true);
};

void initDisplay()
{
  display.begin();
  display.clear();
  display.setBacklight(100);
  display.snake(3);
  printTime(0);
};

void initBlinkInterval()
{
  blinkInterval.start();
  blinkInterval.onUpdate([&]()
                         {
                                   if (blinking)
                                   {
                                       Serial.println(showDisplay);

                                       showDisplay = !showDisplay;

                                       if (showDisplay)
                                       {
                                           printTime(exposureTime);
                                       }
                                       else
                                       {
                                           display.clear();
                                       }
                                   } });
}

void startTimer()
{
  // Start countdown
  interval.startForCount(exposureTime);
  digitalWrite(UV_LED, HIGH);
  // On interval update
  interval.onUpdate([&]()
                    {
        int currentTime = interval.count();
        printTime(exposureTime - currentTime); });

  // On interval stop
  interval.onStop([&]()
                  {
        exposureTime = 0;
        digitalWrite(UV_LED, LOW);
        setState(INITIAL); });
}

void pauseTimer()
{
  interval.pause();
}

void stopTimer()
{
  interval.stop();
}

void initEncoder()
{
  PCICR |= (1 << PCIE1);
  PCMSK1 |= (1 << PCINT10) | (1 << PCINT11);
}

ISR(PCINT1_vect)
{
  encoder.tick(); // just call tick() to check the state.
}

void readEncoder()
{
  static int position = 0;
  short int newPosition = encoder.getPosition();
  if (newPosition != position)
  {
    short int direction = (int)(encoder.getDirection());
    if (direction == -1)
    {
      if (exposureTime >= 5)
      {
        exposureTime = exposureTime - 5;
      }
      else
      {
        exposureTime = 0;
      }
    }
    if (direction == 1)
    {
      exposureTime = exposureTime + 5;
    }
    position = newPosition;
  }
}

//////////////////////////////////////////////////////////////////////////////

void setup()
{
  pinMode(ENCODER_CLK, INPUT);
  pinMode(ENCODER_DT, INPUT);
  pinMode(ENCODER_BUTTON, INPUT_PULLUP);
  pinMode(START_BUTTON, INPUT_PULLUP);
  pinMode(STOP_BUTTON, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(UV_LED, OUTPUT);
  digitalWrite(UV_LED, LOW);

  initEncoder();
  initBlinkInterval();
  initDisplay();

  // Setup Serial Monitor
  Serial.begin(9600);
}

short int secretButton = 0;

void checkState()
{
  switch (state)
  {
  case SETUP:
    setState(INITIAL);
    break;
  case INITIAL:
    if (encoderButton.getSingleDebouncedPress())
    {
      setState(TIMER_EDITABLE);
    }
    if (stopButton.getSingleDebouncedPress())
    {
      secretButton = secretButton + 1;
      if (secretButton == 5)
      {
        secretButton = 0;
        setState(SECRET);
      }
    }
    break;
  case TIMER_EDITABLE:
    if (encoderButton.getSingleDebouncedPress())
    {
      if (exposureTime == 0)
      {
        setState(INITIAL);
      }
      else
      {
        setState(TIMER_SET);
      }
    }
    if (stopButton.getSingleDebouncedPress())
    {
      setState(INITIAL);
    }
    readEncoder();
    break;
  case TIMER_SET:
    if (encoderButton.getSingleDebouncedPress())
    {
      setState(TIMER_EDITABLE);
    }
    if (startButton.getSingleDebouncedPress())
    {
      setState(TIMER_RUNNING);
    }
    if (stopButton.getSingleDebouncedPress())
    {
      setState(INITIAL);
    }
    break;
  case TIMER_RUNNING:
    if (stopButton.getSingleDebouncedPress())
    {
      setState(TIMER_PAUSED);
    }
    break;
  case TIMER_PAUSED:
    if (encoderButton.getSingleDebouncedPress())
    {
      setState(TIMER_EDITABLE);
    }
    if (startButton.getSingleDebouncedPress())
    {
      setState(TIMER_RUNNING);
    }
    if (stopButton.getSingleDebouncedPress())
    {
      setState(INITIAL);
    }
    break;
  case SECRET:
    handleSecret();
    break;
  }
}

void loop()
{
  interval.update();
  blinkInterval.update();
  checkState();
}