/*
 * AVR/Arduino Library Includes
 */

#include <avr/wdt.h> 
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
#define HEARTBEAT_PIN (0) // Arduino digital pin 0 = physical pin 13, PA0)
#define TX_PIN (1) // Arduino digital pin 1 = physical pin 12, PA1)
#define RX_PIN (2) // Arduino digital pin 2 = physical pin 11, PA2)

#define TMR1_CLK_FREQ_BITS ((1 << CS11) | (1 << CS10))

/*
 * Module private objects and variables
 */

static SoftwareSerial s_serial(RX_PIN, TX_PIN); // RX, TX (physical pins 11, 12)
static volatile uint32_t s_tmr_counts;

static const uint8_t IDEAL_SECONDS = 1;  // How seconds we should be counting for
static const uint8_t IDEAL_F_MAINS = 50;  // Ideal mains frequency
static const uint8_t IDEAL_CYCLES = IDEAL_F_MAINS * IDEAL_SECONDS;  // The number of mains cycles to count
static const uint32_t FIXED_POINT_MULTIPLIER = 1000;

static const uint32_t IDEAL_F_CLK = 250000; // Frequency of the incoming clock
static const uint32_t IDEAL_F_CLK_COUNTS = IDEAL_SECONDS * IDEAL_F_CLK;  // If mains is exactly 50Hz, should count exactly this many clocks in time period

/*
 * Module private functions
 */

static uint32_t calculate_frequency(uint32_t counts)
{
  uint64_t freq = ((uint64_t)IDEAL_F_CLK_COUNTS * FIXED_POINT_MULTIPLIER * IDEAL_F_MAINS) + (counts/2);
  return freq / counts;
}

static void process_count(uint32_t count)
{
  //output_to_serial(count);
  s_serial.println(count);
  uint32_t freq = calculate_frequency(count);
  s_serial.println(freq);
  //output_to_serial(freq);
  int step = indicator_moveto_freq(freq, TCNT1);
  s_serial.println(step);
  //output_to_serial(step);
}

//
//static void update_display(uint16_t frequency)
//{
//  (void)frequency;
//}
//
//static void reset_counts(void)
//{
//  s_kHzCount = 0;
//  s_cycleCount = 0;
//}
//

static void output_to_serial(int output)
{
  char buf[16];
  sprintf(buf, "%d", output);
  s_serial.println(buf);
}

static void output_to_serial(uint32_t output)
{
  char buf[16];
  sprintf(buf, "%lu", output);
  s_serial.println(buf);
}

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
  pinMode(HEARTBEAT_PIN, OUTPUT);
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(HEARTBEAT_PIN, LOW);
}

static void sync_with_mains_falling_edge()
{
  wait_for_mains_input_to_be_high();
  wait_for_mains_input_to_be_low();
}

static void wait_for_mains_input_to_be_high()
{
  while(digitalRead(MAINS_FREQ_INPUT_PIN) == LOW) {
    indicator_tick(TCNT1);
  }
  _delay_ms(1);
}

static void wait_for_mains_input_to_be_low()
{
  while(digitalRead(MAINS_FREQ_INPUT_PIN) == HIGH) {
    indicator_tick(TCNT1);
  }
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

static uint32_t get_count()
{
  sync_with_mains_falling_edge();

  TCNT1 = 0;
  s_tmr_counts = 0;

  wait_for_mains_periods(IDEAL_CYCLES);

  return s_tmr_counts + TCNT1;
}

static void toggle_heartbeat()
{
  static bool hb_led = false;
  digitalWrite(HEARTBEAT_PIN, hb_led ? LOW : HIGH);
  hb_led = !hb_led;
}

/*
 * Public functions
 */

void setup() {

  disable_watchdog();

  s_serial.begin(9600);
  setup_io();
  setup_timer();

  indicator_setup();
}

void loop() {

  uint32_t count = get_count();
  toggle_heartbeat();
  process_count(count);
}

ISR(TIM1_OVF_vect)
{
  s_tmr_counts += 65536;
}