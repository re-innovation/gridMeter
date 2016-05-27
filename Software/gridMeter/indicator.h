#ifndef _INDICATOR_H_
#define _INDICATOR_H_

static const uint16_t MIN_FREQ_LIMIT = 49900;
static const uint16_t MAX_FREQ_LIMIT = 50100;

void indicator_setup();
int indicator_moveto_freq(uint16_t freq, uint16_t timer);
void indicator_tick(uint16_t timer);

#endif
