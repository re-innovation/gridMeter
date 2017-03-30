/*
 * gridMeter,ino
 *
 * Author: James Fowkes (jamesfowkes@gmail.com)
 * for The Curious Electric Company (https://www.curiouselectric.co.uk)
 *
 * March 2017
 *
 * Target microcontroller is an ATTINY(2/4/8)4A.
 * This program accurately determines the frequency of the "mains" (national grid)
 * and displays it.
 * 
 * Frequency determination is done by counting ticks of a reference timer over
 * a fixed number of mains cycles. This number is then used to determine the frequency
 * of the mains input.
 *
 * For example, if a 250kHz reference is used, and the mains is counted over 50 cycles:
 *  - The expected number of counts if the frequency is exactly 50.000Hz is 250000.
 *  - If the frequency were only 49.960Hz, the count would be 250000 * (50/49.960) = 250200
 *
 * The reference clock must be very stable and accurate for the final result to
 * be meaningful. Therefore a TCXO or other such source should be used.
 *
 */

/*
 * AVR/Arduino Library Includes
 */

#include <SoftwareSerial.h>

/*
 * Application Includes
 */

#include "indicator.h"
#include "stepper.h"

/*
 * Defines and Typedefs
 */

#define MAINS_FREQ_INPUT_PIN (3) // Arduino digital pin 3 = physical pin 10, PA3)
#define TX_PIN (5) // Arduino digital pin 5 = physical pin 8, PA5)
#define RX_PIN (6) // Arduino digital pin 6 = physical pin 7, PA6)

// Timer1 counts pulses from TCXO (which is FCPU)
// Select FCPU/64 as Timer1 clock, for a 250kHz reference
#define TMR1_CLK_FREQ_BITS ((1 << CS11) | (1 << CS10))
#define TMR1_CLK_FREQ (F_CPU/64)

/*
 * Module private objects and variables
 */

// This variable stores the number of reference timer ticks
static volatile uint32_t s_tmr_counts;

static const uint8_t IDEAL_SECONDS = 1;  // How seconds we should be counting for
static const uint8_t IDEAL_F_MAINS = 50;  // Ideal mains frequency
static const uint8_t IDEAL_CYCLES = IDEAL_F_MAINS * IDEAL_SECONDS;  // The number of mains cycles to count

// All maths is fixed point, so multiply final result by 1000 (e.g. 50.124Hz is stored as 50124)
static const uint32_t FIXED_POINT_MULTIPLIER = 1000;

static const uint32_t IDEAL_F_CLK = TMR1_CLK_FREQ; // Frequency of the incoming clock
static const uint32_t IDEAL_F_CLK_COUNTS = IDEAL_SECONDS * IDEAL_F_CLK;  // If mains is exactly 50Hz, should count exactly this many clocks in time period

// Software serial is used for debugging - outputs frequency every second
SoftwareSerial s_serial(RX_PIN, TX_PIN); // RX, TX (physical pins 11, 12)

/*
 * Module private functions
 */

/* output_to_serial
 * Print "output" wth the given printf format, with or without newline
 */
static void output_to_serial(uint32_t output, char const * const fmt, bool newline = false)
{
	char buf[16];
	sprintf(buf, fmt, output);

	if (newline)
	{
		s_serial.println(buf);
	}
	else
	{
		s_serial.print(buf);
	}
}

/* output_freq_to_serial
 * Print frequency after removing fixed point multiplier, with or without newline
 */
static void output_freq_to_serial(uint32_t freq, bool newline = false)
{
	uint32_t units = freq / FIXED_POINT_MULTIPLIER;

	output_to_serial(units, "%lu", false);
	s_serial.print(".");
	output_to_serial(freq - (units  * FIXED_POINT_MULTIPLIER), "%03d",false);

	if (newline)
	{
  	s_serial.println("Hz");
  }
  else
  {
  	s_serial.print("Hz");	
  }
}

/* calculate_frequency
 * Convert counts to frequency (fixed point maths)
 */
static uint32_t calculate_frequency(uint32_t counts)
{
  // Adding counts/2 ensures correct rounding after dvision
  uint64_t freq = ((uint64_t)IDEAL_F_CLK_COUNTS * FIXED_POINT_MULTIPLIER * IDEAL_F_MAINS) + (counts/2);
  return freq / counts;
}

/* Called every loop: process the timer count and output frequency */
static void process_count(uint32_t count)
{
	output_to_serial(count, "%lu", false);
	s_serial.print(" = ");
  uint32_t freq = calculate_frequency(count);
  output_freq_to_serial(freq, true);
  indicator_moveto_freq(freq, TCNT1);
}

/* setup_timer
 * The 16-bit timer is used in free-running mode, no special features except the overflow interrupt
 */
static void setup_timer()
{
  // Set prescaler at div.1 (ensures system clock is 16MHz)
  CLKPR = (1 << CLKPCE);
  CLKPR = (0 << CLKPCE);

  // Disable all 16-bit timer functions
  TCCR1A = 0;
  TCCR1C = 0;
  
  // Set 16-bit timer source
  TCCR1B = TMR1_CLK_FREQ_BITS;

  // Enable 16-bit timer overflow interrupt
  TIMSK1 = (1 << TOIE1);

}

static void disable_watchdog()
{
  MCUSR = 0x00;
  WDTCSR |= (1<<WDCE) | (1<<WDE);
  WDTCSR = 0x00;
}

static void setup_io()
{
  pinMode(MAINS_FREQ_INPUT_PIN, INPUT);
}

/* sync_with_mains_falling_edge
 * wait_for_mains_input_to_be_high
 * wait_for_mains_input_to_be_low
 * wait_for_mains_periods
 * In order to accurately count frequency, counting code must syncronise with the mains input.
 * These functions perform that task.
 */
static void sync_with_mains_falling_edge()
{
  wait_for_mains_input_to_be_high();
  wait_for_mains_input_to_be_low();
}

static void wait_for_mains_input_to_be_high()
{
  while(digitalRead(MAINS_FREQ_INPUT_PIN) == LOW) {
    indicator_tick(TCNT1); // Keep the motor moving
  }
  // The digital read change occurs 1ms before the rising edge
  _delay_ms(1);
}

static void wait_for_mains_input_to_be_low()
{
  while(digitalRead(MAINS_FREQ_INPUT_PIN) == HIGH) {
    indicator_tick(TCNT1);  // Keep the motor moving
  }
  // The digital read change occurs 1ms before the falling edge
  _delay_ms(1);
}

static void wait_for_mains_periods(uint16_t counts)
{
  while(counts)
  {
    sync_with_mains_falling_edge();
    counts--;   
  }
}

/* get_count
 * Called every loop. Waits for mains cycles, and counts reference clocks. */

static uint32_t get_count()
{
  sync_with_mains_falling_edge();

  TCNT1 = 0;
  s_tmr_counts = 0;

  wait_for_mains_periods(IDEAL_CYCLES);

  return s_tmr_counts + TCNT1;
}

/*
 * Public functions
 */

void setup() {

  // Ensure the watchdog will not reset the microcontroller
  disable_watchdog();

  // Enable the serial debugging output
  s_serial.begin(9600);

  // Setup the microcontroller peripherals
  setup_io();
  setup_timer();

  // Ask the indicator module to set itself up and home if needed
  indicator_setup();
  indicator_home();

  // "Self-test" the indicator by moving to its limit then back to centre
  indicator_moveto_freq_blocking(MAX_FREQ_LIMIT);
  indicator_moveto_freq_blocking(MIN_FREQ_LIMIT);
  indicator_moveto_freq_blocking(50000);
}

void loop() {
  // Very simple loop: get the count and display it!
  uint32_t count = get_count();
  process_count(count);
}

ISR(TIM1_OVF_vect)
{
  // The reference clock is counted on the 16-bit timer.
  // Each time it overflows, save another 16-bits of count,
  // which is 65536.
  s_tmr_counts += 65536;
}
