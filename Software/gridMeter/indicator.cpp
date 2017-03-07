/*
 * Arduino Includes
 */

#include <Arduino.h>
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


/*
 * Module scope objects and variables
 */

static Stepper s_stepper(MOTOR_PIN_1, MOTOR_PIN_3, MOTOR_PIN_2, MOTOR_PIN_4);

static const float STEPS_PER_DEGREE = (float)STEPS_PER_REV / 360.0;

static const int16_t STEPS_AT_MIN_FREQ_LIMIT = (int16_t)(-45.0 * STEPS_PER_DEGREE);
static const int16_t STEPS_AT_MAX_FREQ_LIMIT = (int16_t)(45.0 * STEPS_PER_DEGREE);

static int16_t s_current_position = 0;
static int16_t s_target_position = 0;
static uint16_t s_us_per_step = 0;
static uint16_t s_last_step_time = 0;

static uint8_t s_pinStates[4] = {LOW, LOW, LOW, LOW};

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

void indicator_setup()
{
	pinMode(MOTOR_PIN_1, OUTPUT);
	pinMode(MOTOR_PIN_2, OUTPUT);
	pinMode(MOTOR_PIN_3, OUTPUT);
	pinMode(MOTOR_PIN_4, OUTPUT);
}

void indicator_tick(uint16_t timer)
{
	if(move_is_required() && step_time_elapsed(timer))
	{
		move_one_step_towards_target();
		s_last_step_time = timer;
	}
}

int indicator_moveto_freq(uint16_t freq, uint16_t timer)
{
	freq = constrain(freq, MIN_FREQ_LIMIT, MAX_FREQ_LIMIT);
	s_target_position = map(freq, MIN_FREQ_LIMIT, MAX_FREQ_LIMIT, STEPS_AT_MIN_FREQ_LIMIT, STEPS_AT_MAX_FREQ_LIMIT);
	
	if(s_target_position != s_current_position)
	{
		// Make speed of movement is relative to distance to travel
		// More distance -> more speed
		// This means that time-to-move should be equal for all distance
		// this looks best as it avoids the needle quickly jumping between positions
		s_us_per_step = 1000000UL / abs(s_target_position - s_current_position);
		s_last_step_time = timer;
	}
	
	return s_us_per_step;
}

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

	pinMode(HOME_OUT_PIN, OUTPUT);
	digitalWrite(HOME_OUT_PIN, HIGH);

	// Delay allows for response time of phototransistor
	// Otherwise the correct start state isn't found
	_delay_ms(10);

	// Get the initial state of phototransistor
	// Then move motor (forwards or backwards, depending)
	// until the state changes

	int start_state = digitalRead(HOME_IN_PIN);
	while (digitalRead(HOME_IN_PIN) == start_state)
	{
		s_stepper.step(start_state ? -1 : 1);
		_delay_ms(4);
	}

	digitalWrite(HOME_OUT_PIN, LOW);
	pinMode(HOME_OUT_PIN, INPUT);

	s_current_position = 0;
}