/*
 * stepper.cpp
 *
 * Provides low level stepper functionality.
 * Basic functionality copied from standard stepper 
 * library to save program space and allow enable/disable
 *
 */

#include <Arduino.h>

#include "stepper.h"

#define SPEED_IN_RPM 1

#define STEP_DELAY (60L * 1000L * 1000L / STEPS_PER_REV / SPEED_IN_RPM)

Stepper::Stepper(int motor_pin_1, int motor_pin_3, int motor_pin_2, int motor_pin_4)
{
  this->step_number = 0;    // which step the motor is on
  this->direction = 0;      // motor direction
  this->last_step_time = 0; // time stamp in us of the last step taken

  // Arduino pins for the motor control connection:
  this->motor_pin_1 = motor_pin_1;
  this->motor_pin_2 = motor_pin_2;
  this->motor_pin_3 = motor_pin_3;
  this->motor_pin_4 = motor_pin_4;

  // setup the pins on the microcontroller:
  pinMode(this->motor_pin_1, OUTPUT);
  pinMode(this->motor_pin_2, OUTPUT);
  pinMode(this->motor_pin_3, OUTPUT);
  pinMode(this->motor_pin_4, OUTPUT);
}

/*
 * Moves the motor steps_to_move steps.  If the number is negative,
 * the motor moves in the reverse direction.
 */
void Stepper::step(int steps_to_move)
{
  int steps_left = abs(steps_to_move);  // how many steps to take

  // determine direction based on whether steps_to_mode is + or -:
  if (steps_to_move > 0) { this->direction = 1; }
  if (steps_to_move < 0) { this->direction = 0; }


  // decrement the number of steps, moving one step each time:
  while (steps_left > 0)
  {
    // increment or decrement the step number,
    // depending on direction:
    if (this->direction == 1)
    {
      this->step_number++;
      if (this->step_number == STEPS_PER_REV) {
        this->step_number = 0;
      }
    }
    else
    {
      if (this->step_number == 0) {
        this->step_number = STEPS_PER_REV;
      }
      this->step_number--;
    }
    
    // decrement the steps left:
    steps_left--;
    stepMotor(this->step_number % STEP_LIMIT);
    if (steps_left) { _delay_us(STEP_DELAY); }

  }
}

void Stepper::enable()
{
  if (!this->enabled)
  {
    this->enabled = true;
    stepMotor(this->step_number % STEP_LIMIT);
  }
}

void Stepper::disable()
{
  if (this->enabled)
  {
    this->enabled = false;
    digitalWrite(motor_pin_1, LOW);
    digitalWrite(motor_pin_2, LOW);
    digitalWrite(motor_pin_3, LOW);
    digitalWrite(motor_pin_4, LOW);
  }
}

/*
 * Moves the motor forward or backwards (quarter stepping)
 */

void Stepper::stepMotor(int thisStep)
{
  switch (thisStep) {
    case 0:
      digitalWrite(motor_pin_1, HIGH);
      digitalWrite(motor_pin_2, LOW);
      digitalWrite(motor_pin_3, LOW);
      digitalWrite(motor_pin_4, LOW);
    break;
    case 1:
      digitalWrite(motor_pin_1, HIGH);
      digitalWrite(motor_pin_2, HIGH);
      digitalWrite(motor_pin_3, LOW);
      digitalWrite(motor_pin_4, LOW);
    break;
    case 2:
      digitalWrite(motor_pin_1, LOW);
      digitalWrite(motor_pin_2, HIGH);
      digitalWrite(motor_pin_3, LOW);
      digitalWrite(motor_pin_4, LOW);
    break;
    case 3:
      digitalWrite(motor_pin_1, LOW);
      digitalWrite(motor_pin_2, HIGH);
      digitalWrite(motor_pin_3, HIGH);
      digitalWrite(motor_pin_4, LOW);
    break;
    case 4:
      digitalWrite(motor_pin_1, LOW);
      digitalWrite(motor_pin_2, LOW);
      digitalWrite(motor_pin_3, HIGH);
      digitalWrite(motor_pin_4, LOW);
    break;
    case 5:
      digitalWrite(motor_pin_1, LOW);
      digitalWrite(motor_pin_2, LOW);
      digitalWrite(motor_pin_3, HIGH);
      digitalWrite(motor_pin_4, HIGH);
    break;
    case 6:
      digitalWrite(motor_pin_1, LOW);
      digitalWrite(motor_pin_2, LOW);
      digitalWrite(motor_pin_3, LOW);
      digitalWrite(motor_pin_4, HIGH);
    break;
    case 7:
      digitalWrite(motor_pin_1, HIGH);
      digitalWrite(motor_pin_2, LOW);
      digitalWrite(motor_pin_3, LOW);
      digitalWrite(motor_pin_4, HIGH);
    break;
  }
}
