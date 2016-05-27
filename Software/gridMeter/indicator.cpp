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

#define MOTOR_PIN_1 (7) // Arduino digital pin 7 = physical pin 6, PA7)
#define MOTOR_PIN_2 (2) // Arduino digital pin 2 = physical pin 11, PA2)
#define MOTOR_PIN_3 (8) // Arduino digital pin 8 = physical pin 5, PB2)
#define MOTOR_PIN_4 (5) // Arduino digital pin 5 = physical pin 5, PA5)

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

static void save_pin_states()
{
	//s_pinStates[0] = bitRead(PORTA, 7) ? HIGH : LOW;
	//s_pinStates[1] = bitRead(PORTA, 2) ? HIGH : LOW;
	//s_pinStates[2] = bitRead(PORTB, 2) ? HIGH : LOW;
	//s_pinStates[3] = bitRead(PORTA, 5) ? HIGH : LOW;
}

static void restore_pin_states()
{
	//digitalWrite(MOTOR_PIN_1, s_pinStates[0]);
	//digitalWrite(MOTOR_PIN_2, s_pinStates[1]);
	//digitalWrite(MOTOR_PIN_3, s_pinStates[2]);
	//digitalWrite(MOTOR_PIN_4, s_pinStates[3]);
}

static void motor_off()
{
	//digitalWrite(MOTOR_PIN_1, LOW);
	//digitalWrite(MOTOR_PIN_2, LOW);
	//digitalWrite(MOTOR_PIN_3, LOW);
	//digitalWrite(MOTOR_PIN_4, LOW);
}

static void move_one_step_towards_target()
{
	restore_pin_states();
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
	save_pin_states();
	motor_off();
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
		s_us_per_step = 1000000UL / abs(s_target_position - s_current_position);
		s_last_step_time = timer;
	}
	
	return s_us_per_step;
}
