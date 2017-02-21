#include "TriacDimmer.h"
#include <Arduino.h>


volatile uint16_t TriacDimmer::detail::ch_A_up;
volatile uint16_t TriacDimmer::detail::ch_A_dn;
volatile uint16_t TriacDimmer::detail::ch_B_up;
volatile uint16_t TriacDimmer::detail::ch_B_dn;

void TriacDimmer::begin(){
	TCCR1A = 0;
	TCCR1B = _BV(ICNC1) | _BV(ICES1) | _BV(CS11);
	TIMSK1 = _BV(ICIE1);
}

void TriacDimmer::setBrightness(uint8_t pin, float x){
	if(pin == 9){
		float y = TriacDimmer::detail::interpolate(x,
			TriacDimmer::detail::brightness_lut,
			TriacDimmer::detail::phase_lut,
			TriacDimmer::detail::lut_length);

		TriacDimmer::detail::ch_A_up = (1-y) * ICR1;
		TriacDimmer::detail::ch_A_dn = TriacDimmer::detail::ch_A_up + TriacDimmer::detail::pulse_length;
	} else if(pin == 10){
		float y = TriacDimmer::detail::interpolate(x,
			TriacDimmer::detail::brightness_lut,
			TriacDimmer::detail::phase_lut,
			TriacDimmer::detail::lut_length);

		TriacDimmer::detail::ch_B_up = (1-y) * ICR1;
		TriacDimmer::detail::ch_B_dn = TriacDimmer::detail::ch_B_up + TriacDimmer::detail::pulse_length;
	}
}

float TriacDimmer::getCurrentBrightness(uint8_t pin){
	if(pin == 9){
		float y = 1 - (float) TriacDimmer::detail::ch_A_up / ICR1;
		return TriacDimmer::detail::interpolate(y,
			TriacDimmer::detail::phase_lut,
			TriacDimmer::detail::brightness_lut,
			TriacDimmer::detail::lut_length);
	} else if(pin == 10){
		float y = 1 - (float) TriacDimmer::detail::ch_B_up / ICR1;
		return TriacDimmer::detail::interpolate(y,
			TriacDimmer::detail::phase_lut,
			TriacDimmer::detail::brightness_lut,
			TriacDimmer::detail::lut_length);
	}
	return 0;
}

void TriacDimmer::end(){
	TIMSK1 = 0; //disable the interrupts first!
	TCCR1A = 0; //clear to reset state
	TCCR1B = 0;
}

const float TriacDimmer::detail::interpolate(const float x, const float x_table[], const float y_table[], uint8_t size){
	if (x <= x_table[0]) return y_table[0];
	if (x >= x_table[size-1]) return y_table[size-1];
	uint8_t pos = 1;
	while(x > x_table[pos]) pos++;
	if (x == x_table[pos]) return y_table[pos];
	return (x - x_table[pos-1]) * (y_table[pos] - y_table[pos-1]) / (x_table[pos] - x_table[pos-1]) + y_table[pos-1];
}

ISR(TIMER1_CAPT_vect){
	OCR1A = TriacDimmer::detail::ch_A_up;
	OCR1B = TriacDimmer::detail::ch_B_up;
	TCCR1A = _BV(COM1A0) | _BV(COM1A1) | _BV(COM1B0) | _BV(COM1B1); //set OC1x on compare match
	TIMSK1 = _BV(ICIE1) | _BV(OCIE1A) | _BV(OCIE1B); //enable input capture and compare match interrupts

	// note, this is equivalent to `TCNT1 = TCNT1 - ICR1 + 1;`, 
	// but written in assembly to make sure the timing is corrent
	// the "ld r24,Z" and "st Z,r24" must be exactly 8 instructions apart
	#if defined(__AVR_ATmega328P__)
	register uint16_t tmpA;
	register uint16_t tmpB;
	asm volatile (
		"lds %A[tmpA],134	; load ICR1L\n\t"\
		"lds %B[tmpA],134+1	; load ICR1H\n\t"\
		"lds %A[tmpB],132	; load TCNT1L\n\t"\
		"lds %B[tmpB],132+1	; load TCNT1H\n\t"\
		"adiw %A[tmpB],1	; add 1\n\t"\
		"sub %A[tmpB],%A[tmpA]	; subtract ICR1L\n\t"\
		"sbc %B[tmpB],%B[tmpA]	; subtract ICR1H\n\t"\
		"nop		; pad for timing\n\t"\
		"sts 132+1,%B[tmpB]	; store TCNT1H\n\t"\
		"sts 132,%A[tmpB]	; store TCNT1L\n\t"\
	: [tmpA] "=&r" (tmpA), [tmpB] "=&r" (tmpB) );
	#else
		TCNT1 = TCNT1 - ICR1 + 1; //fallback in case using some other platform, although not as good
		//timing may be slightly off, but should still work for 90% of what you'd want
	#endif
}
ISR(TIMER1_COMPA_vect){
	TCCR1A &=~ _BV(COM1A1); //clear OC1x on compare match
	TIMSK1 &=~ _BV(OCIE1A); //disable compare match interrupt
	OCR1A = TriacDimmer::detail::ch_A_dn;
	if(TCNT1 - OCR1A >= 0){
		TCCR1C = _BV(FOC1A); //interrupt was run late, trigger match manually
	}
}
ISR(TIMER1_COMPB_vect){
	TCCR1B &=~ _BV(COM1B1);
	TIMSK1 &=~ _BV(OCIE1B);
	OCR1B = TriacDimmer::detail::ch_B_dn;
	if(TCNT1 - OCR1B >= 0){
		TCCR1C = _BV(FOC1B);
	}
}

