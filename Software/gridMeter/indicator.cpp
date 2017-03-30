/*
 * indicator.cpp
 *
 * Provides indicator functionality for the grid meter.
 * The current indicator is a BYJ48 geared stepper motor.
 * Homing is provided by an IR photoreflector.
 *
 */

 /*
 * Arduino Includes
 */

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "stepper.h"

/*
 * Application Includes
 */

#include "indicator.h"

/*
 * Defines and Typedefs
 */

#define MOTOR_PIN_1 (1) // Arduino digital pin 1 = physical pin 12, PA1)
#define MOTOR_PIN_2 (2) // Arduino digital pin 2 = physical pin 11, PA2)
#define MOTOR_PIN_3 (8) // Arduino digital pin 8 = physical pin 5, PB2)
#define MOTOR_PIN_4 (7) // Arduino digital pin 7 = physical pin 6, PA7)

#define HOME_OUT_PIN (9)  // Arduino digital pin 9 = physical pin 3, PB1)
#define HOME_IN_PIN (0)  // Arduino digital pin 0 = physical pin 3, PA0)

#define HOME_PHOTODETECT_THRESHOLD (150)

/*
 * Module scope objects and variables
 */

static Stepper s_stepper(MOTOR_PIN_1, MOTOR_PIN_3, MOTOR_PIN_2, MOTOR_PIN_4);

static const float STEPS_PER_DEGREE = (float)STEPS_PER_REV / 360.0;

// Set the movement range and deadzone here
static const int16_t STEPS_AT_MIN_FREQ_LIMIT = (int16_t)(-45.0 * STEPS_PER_DEGREE);
static const int16_t STEPS_AT_MAX_FREQ_LIMIT = (int16_t)(45.0 * STEPS_PER_DEGREE);
static const int16_t MOVEMENT_DEADZONE = (int16_t)(6.0 * STEPS_PER_DEGREE);

static int16_t s_current_position = 0;
static int16_t s_target_position = 0;
static uint16_t s_us_per_step = 0;
static uint16_t s_last_step_time = 0;

static uint8_t s_pinStates[4] = {LOW, LOW, LOW, LOW};

extern SoftwareSerial s_serial;

/*
 * Public Functions
 */

static void move_one_step_towards_target()
{
	if (s_target_position > s_current_position)
	{
		s_stepper.step(1);
		s_current_position++;
	}
	else
	{
		s_stepper.step(-1);
		s_current_position--;	
	}
}

static bool move_is_required()
{
	return s_target_position != s_current_position;
}

static bool step_time_elapsed(uint32_t timer)
{
	uint32_t us_since_last_step = timer - s_last_step_time;
	us_since_last_step *= 4;
	return (timer - s_last_step_time) >= s_us_per_step;
}

static void debug_home_pin_analog(int analog_value)
{
  static unsigned long last_time = 0;
  if ((millis() - last_time) > 100)
  {
    last_time = millis();
    s_serial.println(analog_value);
  }
}

void indicator_setup()
{
	pinMode(MOTOR_PIN_1, OUTPUT);
	pinMode(MOTOR_PIN_2, OUTPUT);
	pinMode(MOTOR_PIN_3, OUTPUT);
	pinMode(MOTOR_PIN_4, OUTPUT);

	s_serial.print("Deadzone: ");
	s_serial.print(MOVEMENT_DEADZONE);
	s_serial.println("steps");
}

void indicator_tick(uint16_t timer)
{
	if(s_stepper.enabled() && move_is_required() && (step_time_elapsed(timer)))
	{
		move_one_step_towards_target();
		s_last_step_time = timer;
	}
}

/* indicator_moveto_freq
 * Sets target needle location and movement speed. Does not block. */
int indicator_moveto_freq(uint16_t freq, uint16_t timer)
{
	// Ensure the needle does not move beyond limits by constraining the input
	freq = constrain(freq, MIN_FREQ_LIMIT, MAX_FREQ_LIMIT);
	
	// Convert the input frequency to new position
	int16_t new_target = map(freq, MIN_FREQ_LIMIT, MAX_FREQ_LIMIT, STEPS_AT_MIN_FREQ_LIMIT, STEPS_AT_MAX_FREQ_LIMIT);
	
	// Only move if the new target is outside the deadzone
	if(abs(new_target - s_current_position) >= MOVEMENT_DEADZONE)
	{
		s_target_position = new_target;
		s_serial.print("Target: ");
		s_serial.print(freq);
		s_serial.print("(");
		s_serial.print(new_target);
		s_serial.print(" steps, d=");
		s_serial.print(new_target - s_current_position);
		s_serial.println(")");
		// Make speed of movement is relative to distance to travel
		// More distance -> more speed
		// This means that time-to-move should be equal for all distance
		// this looks best as it avoids the needle quickly jumping between positions
		s_us_per_step = 1000000UL / abs(s_target_position - s_current_position);
		s_last_step_time = timer;

		if (!s_stepper.enabled())
		{
			s_serial.println("Motor enable");
			s_stepper.enable();
		}
	}
	else
	{
		if (s_stepper.enabled())
		{
			s_serial.println("Motor disable");
			s_stepper.disable();
		}
	}
	return s_us_per_step;
}

/* indicator_moveto_freq
 * Sets target needle location and moves until reached. */
void indicator_moveto_freq_blocking(uint16_t freq)
{
	freq = constrain(freq, MIN_FREQ_LIMIT, MAX_FREQ_LIMIT);
	s_target_position = map(freq, MIN_FREQ_LIMIT, MAX_FREQ_LIMIT, STEPS_AT_MIN_FREQ_LIMIT, STEPS_AT_MAX_FREQ_LIMIT);
	
	while(s_target_position != s_current_position)
	{
		move_one_step_towards_target();
		_delay_ms(3);
	}
}

void indicator_home()
{
	// Turn on the IR LED in the photoreflector
	pinMode(HOME_OUT_PIN, OUTPUT);
	digitalWrite(HOME_OUT_PIN, HIGH);

	// Delay allows for response time of phototransistor
	// Otherwise the correct start state isn't found
	_delay_ms(10);

	// Get the initial state of phototransistor
	// Then move motor (forwards or backwards, depending)
	// until the state changes

	int start_state = analogRead(HOME_IN_PIN);
  	int val;
  	bool homed = false;
  	int step_direction = start_state < HOME_PHOTODETECT_THRESHOLD ? 1 : -1;
  	s_serial.print("Moving ");
  	s_serial.println(step_direction > 0 ? "CW": "CCW");
  
  	while (!homed)
	{
		s_stepper.step(step_direction);
		_delay_ms(4);
		val = analogRead(HOME_IN_PIN);

		// Motor is homed if the value has passed the threshold (test both directions)
		homed = ((val >= HOME_PHOTODETECT_THRESHOLD) && (start_state < HOME_PHOTODETECT_THRESHOLD));
		homed = homed || ((val <= HOME_PHOTODETECT_THRESHOLD) && (start_state > HOME_PHOTODETECT_THRESHOLD));

		debug_home_pin_analog(val);
	}

	// Turn off the IR LED
	digitalWrite(HOME_OUT_PIN, LOW);
	pinMode(HOME_OUT_PIN, INPUT);

	// Now homed, so reset the current position
	s_current_position = 0;
}
