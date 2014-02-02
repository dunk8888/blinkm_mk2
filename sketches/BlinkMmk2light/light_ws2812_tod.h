/*
 * light weight WS2812 lib
 *
 * Created: 07.04.2013 15:58:05 - v0.1
 *			06.05.2013          - v0.3 - clean up
 *			27.05.2013			- v0.6 - clean up, removed RC variants, added mask
 *  Author: Tim (cpldcpu@gmail.com) 
 */ 

#include <avr/io.h>

#ifndef LIGHT_WS2812_H_
#define LIGHT_WS2812_H_

// Call with address to led color array (order is Green-Red-Blue)
// Numer of bytes to be transmitted is leds*3

void ws2812_sendarray(uint8_t *ledarray,uint16_t length);
void ws2812_sendarray_mask(uint8_t *ledarray,uint16_t length, uint8_t mask);

///////////////////////////////////////////////////////////////////////
// User defined area: Define I/O pin
///////////////////////////////////////////////////////////////////////

//#define ws2812_port PORTB						// Data port register
//#define ws2812_pin 7							// Number of the data out pin
#define ws2812_port PORTA						// Data port register
#define ws2812_pin 7							// Number of the data out pin

///////////////////////////////////////////////////////////////////////
// User defined area: Define CPU clock speed
// Uncomment one and only one #define matching your application
///////////////////////////////////////////////////////////////////////

// #define ws2812_4MHz
#define ws2812_8MHz		
// #define ws2812_9p6MHz
// #define ws2812_12MHz  // Warning: Untested in v0.6
// #define ws2812_16MHz

///////////////////////////////////////////////////////////////////////
// End user defined area
///////////////////////////////////////////////////////////////////////


/*
 * light weight WS2812 lib
 *
 * Created: 07.04.2013 15:57:49 - v0.1
 *			21.04.2013 15:57:49 - v0.2 - Added 12 Mhz code, cleanup
 *			07.05.2013          - v0.4 - size optimization, disable irq
 *			20.05.2013          - v0.5 - Fixed timing bug from size optimization
 *			27.05.2013			- v0.6 - Major update: Changed I/O Port access to byte writes
 *										 instead of bit manipulation. This removes this timing
 *										 discrepancy between standard AVR and reduced core so that
 *										 only one routine is required. This comes at the cost of
 *										 additional register usage.
 *			28.05.2013			- v0.7 - Optimized timing and size of 8 and 12 Mhz routines. 
 *										 All routines are within datasheet specs now, except of
 *										 9.6 Mhz which is marginally off.			
 *			03.06.2013			- v0.8 - 9.6 Mhz implementation now within specifications.
 *								-		 brvs->brcs. Loops terminate correctly
 *  Author: Tim (cpldcpu@gmail.com) 
 */ 

// Tested:
// Attiny 85	4 MHz, 8 MHz, 16 MHz, 16.5 MHz (Little-Wire)
// Attiny 13A	9.6 MHz
// Attiny 10	4 Mhz, 8 Mhz (Reduced core)
// Atmega 8		12 Mhz


//#include "light_ws2812.h"
#include <avr/interrupt.h>
#include <avr/io.h>

void ws2812_sendarray_mask(uint8_t *, uint16_t , uint8_t);

void ws2812_sendarray(uint8_t *data,uint16_t datlen)
{
	ws2812_sendarray_mask(data,datlen,_BV(ws2812_pin));
}

/*
	This routine writes an array of bytes with RGB values to the Dataout pin
	using the fast 800kHz clockless WS2811/2812 protocol.
	
	The description of the protocol in the datasheet is somewhat confusing and
	it appears that some timing values have been rounded. 
	
	The order of the color-data is GRB 8:8:8. Serial data transmission begins 
	with the most significant bit in each byte.
	
	The total length of each bit is 1.25Âµs (20 cycles @ 16Mhz)
	* At 0Âµs the dataline is pulled high.
	* To send a zero the dataline is pulled low after 0.375Âµs (6 cycles).
	* To send a one the dataline is pulled low after 0.625Âµs (10 cycles).
	
	After the entire bitstream has been written, the dataout pin has to remain low
	for at least 50Âµs (reset condition).
	
	Due to the loop overhead there is a slight timing error: The loop will execute
	in 21 cycles for the last bit write. This does not cause any issues though,
	as only the timing between the rising and the falling edge seems to be critical.
	Some quick experiments have shown that the bitstream has to be delayed by 
	more than 3Âµs until it cannot be continued (3Âµs=48 cyles).

*/

#if defined ws2812_16MHz

void ws2812_sendarray_mask(uint8_t *data,uint16_t datlen,uint8_t maskhi)
{
	uint8_t curbyte,ctr,masklo;
	masklo	=~maskhi&ws2812_port;
	maskhi |=ws2812_port;
	
	while (datlen--) {
		curbyte=*data++;
		
		asm volatile(
		
		"		ldi	%0,8		\n\t"		// 0
		"loop%=:out	%2,	%3		\n\t"		// 1
		"		lsl	%1			\n\t"		// 2
		"		dec	%0			\n\t"		// 3

		"		rjmp .+0		\n\t"		// 5
		
		"		brcs .+2		\n\t"		// 6l / 7h
		"		out	%2,	%4		\n\t"		// 7l / -

		"		rjmp .+0		\n\t"		// 9
		
		"		nop				\n\t"		// 10
		"		out	%2,	%4		\n\t"		// 11
		"		breq end%=		\n\t"		// 12      nt. 13 taken

		"		rjmp .+0		\n\t"		// 14
		"		rjmp .+0		\n\t"		// 16
		"		rjmp .+0		\n\t"		// 18
		"		rjmp loop%=		\n\t"		// 20
		"end%=:					\n\t"
		:	"=&d" (ctr)
		:	"r" (curbyte), "I" (_SFR_IO_ADDR(ws2812_port)), "r" (maskhi), "r" (masklo)
		);
	}
}

/*
	Timing optimized for 12Mhz AVR 

	The total length of each bit is 1.25Âµs (15 cycles @ 12Mhz)
	* At 0Âµs the dataline is pulled high.  (cycle 1+0)
	* To send a zero the dataline is pulled low after 0.333Âµs (1+4=5 cycles).
	* To send a one the dataline is pulled low after 0.666Âµs (1+8=9 cycles).

	Total loop timing is correct, but the timing for the falling edge can
	not be accurately reached as the correct 0.375Âµs (4.5 cyc.) and 0.675Âµs (7.5 cyc)
	timings fall in between cycles.
	
	Final timing: 
	* 15 cycles for bits 7-1
	* 16 cycles for bit 0	
	- The bit 0 timing exceeds the 1.25Âµs bit-timing by 66.7Âµs, which is still
	within datasheet tolerances (600Âµs)
*/

#elif defined ws2812_12MHz

void ws2812_sendarray_mask(uint8_t *data,uint16_t datlen,uint8_t maskhi)
{
	uint8_t curbyte,ctr,masklo;
	masklo=~maskhi;
	
	asm volatile(
	"		in		%0,%2		\n\t"
	"		or		%3,%0		\n\t"
	"		and		%4,%0		\n\t"
	"olop%=:subi	%A6,1		\n\t"		// 12
	"		sbci	%B6,0		\n\t"		// 13
	"		brcs	exit%=		\n\t"		// 14
	"		ld		%1,X+		\n\t"		// 15
	"		ldi		%0,8		\n\t"		// 16
	
	"loop%=:out		%2,	%3		\n\t"		// 1
	"		lsl		%1			\n\t"		// 2
	"		nop					\n\t"		// 3
	
	"		brcs	.+2			\n\t"		// 4nt / 5t
	"		out		%2,	%4		\n\t"		// 5
	"		dec		%0			\n\t"		// 6
	"		rjmp .+0			\n\t"		// 8	
	"		out		%2,	%4		\n\t"		// 9
	"		breq	olop%=		\n\t"		// 10nt  / 11t
	"		nop					\n\t"		// 11
	"		rjmp .+0			\n\t"		// 13
	"		rjmp	loop%=		\n\t"		// 15
	"exit%=:					\n\t"		//
	:	"=&d" (ctr)
	:	"r" (curbyte), "I" (_SFR_IO_ADDR(ws2812_port)), "r" (maskhi), "r" (masklo), "x" (data), "r" (datlen)
	);
}

/*
	Timing optimized for 8Mhz AVR (excl. XMEGA and reduced instruction set)

	The total length of each bit is 1.25Âµs (10 cycles @ 8Mhz)
	* At 0Âµs the dataline is pulled high.  (cycle 1+0=1)
	* To send a zero the dataline is pulled low after 0.375Âµs (1+3=4 cycles).
	* To send a one the dataline is pulled low after 0.625Âµs (1+5=6 cycles).

	Final timing: 
	* 10 cycles for bits 7-1
	* 14 cycles for bit 0		
	
	- The bit 0 timing exceeds the 1.25Âµs bit-timing by 500Âµs, which is still
	  within datasheet tolerances (600Âµs)
*/

#elif defined ws2812_8MHz

void ws2812_sendarray_mask(uint8_t *data,uint16_t datlen,uint8_t maskhi)
{
	uint8_t curbyte,ctr,masklo;
	masklo=~maskhi;
	
	asm volatile(
	"		in		%0,%2		\n\t"		
	"		or		%3,%0		\n\t"
	"		and		%4,%0		\n\t"	
	"olop%=:subi	%A6,1		\n\t"		// 9
	"		sbci	%B6,0		\n\t"		// 10
	"		brcs	exit%=		\n\t"		// 11
	"		ld		%1,X+		\n\t"		// 13
	"		ldi		%0,8		\n\t"		// 14
		
	"loop%=:out		%2,	%3		\n\t"		// 1
	"		lsl		%1			\n\t"		// 2
		
	"		brcs	.+2			\n\t"		// 3nt / 4t
	"		out		%2,	%4		\n\t"		// 4
	"		dec		%0			\n\t"		// 5
	"		out		%2,	%4		\n\t"		// 6
	"		breq	olop%=		\n\t"		// 7nt  / 8t
	"		nop					\n\t"		// 8
	"		rjmp	loop%=		\n\t"		// 10 
	"exit%=:					\n\t"		// 
	:	"=&d" (ctr)
	:	"r" (curbyte), "I" (_SFR_IO_ADDR(ws2812_port)), "r" (maskhi), "r" (masklo), "x" (data), "r" (datlen)
	);
}

/*
Timing optimized for 9.6Mhz AVR 

The total length of each bit is 1.25Âµs (12 cycles @ 9.6Mhz)
* At 0Âµs the dataline is pulled high.  (cycle 1)
* To send a zero the dataline is pulled low after 0.312Âµs (1+3=4 cycles) (error 0.06Âµs)
* To send a one the dataline is pulled low after 0.625Âµs (1+6=7 cycles) (no error).

12 cycles can not be reached for bit 0 write. However since the timing 
between the rising and falling edge is correct, it seems to be acceptable 
to slightly increase bit timing

Final timing:
* 12 cycles for bits 7-1
* 15 cycles for bit 0

	- The bit 0 timing exceeds the 1.25Âµs timing by 312Âµs, which is still within
	  datasheet tolerances (600Âµs).
*/

#elif defined ws2812_9p6MHz

void ws2812_sendarray_mask(uint8_t *data,uint16_t datlen,uint8_t maskhi)
{
	uint8_t curbyte,ctr,masklo;
	masklo=~maskhi;
	
	asm volatile(
	"		in		%0,%2		\n\t"
	"		or		%3,%0		\n\t"
	"		and		%4,%0		\n\t"
	"olop%=:subi	%A6,1		\n\t"		// 10
	"		sbci	%B6,0		\n\t"		// 11
	"		brcs	exit%=		\n\t"		// 12
	"		ld		%1,X+		\n\t"		// 14
	"		ldi		%0,8		\n\t"		// 15
	
	"loop%=:out		%2,	%3		\n\t"		// 1
	"		lsl		%1			\n\t"		// 2
	
	"		brcs	.+2			\n\t"		// 3nt / 4t
	"		out		%2,	%4		\n\t"		// 4
	"		dec		%0			\n\t"		// 5
	"		nop					\n\t"		// 6
	"		out		%2,	%4		\n\t"		// 7
	"		breq	olop%=		\n\t"		// 8nt  / 9t
	"		rjmp	.+0			\n\t"		// 10
	"		rjmp	loop%=		\n\t"		// 12
	"exit%=:					\n\t"		//
	:	"=&d" (ctr)
	:	"r" (curbyte), "I" (_SFR_IO_ADDR(ws2812_port)), "r" (maskhi), "r" (masklo), "x" (data), "r" (datlen)
	);
}


/*
	Timing optimized for 4Mhz AVR
	
	The total length of each bit is 1.25Âµs (5 cycles @ 4Mhz)
	* At 0Âµs the dataline is pulled high.  (cycle 0+1)
	* To send a zero the dataline is pulled low after 0.5Âµs (spec: 0.375Âµs)  (2+1=3 cycles).
	* To send a one the dataline is pulled low after 0.75Âµs  (spec: 0.625Âµs) (3+1=4 cycles).

	The timing of this implementation is slightly off, however it seems to 
	work empirically. 
		
	Final timing: 
	* 5 cycles for bits 7-1
	* 6 cycles for bit 0	
	
	- The bit 0 timing exceeds the 1.25Âµs timing by 250Âµs, which is still within
	the tolerances stated in the datasheet (600 Âµs).
		
*/

#elif defined ws2812_4MHz

void ws2812_sendarray_mask(uint8_t *data,uint16_t datlen,uint8_t maskhi)
{
	uint8_t curbyte,ctr,masklo;
	masklo	=~maskhi&ws2812_port;
	maskhi |=ws2812_port;
	
	if (!datlen) return;
	
	asm volatile(
	"		ld  %0,X		\n\t"
	
	"olop%=:out	%2,	%3		\n\t"		// 1
	"		sbrs %0,7		\n\t"		// 2
	"		out	%2,	%6		\n\t"		// 3
	"		out	%2,	%6		\n\t"		// 4
	"		subi r26,-1		\n\t"		// 5
	
	"		out	%2,	%3		\n\t"		// 1
	"		sbrs %0,6		\n\t"		// 2
	"		out	%2,	%6		\n\t"		// 3
	"		out	%2,	%6		\n\t"		// 4
	"		sbci r27,-1		\n\t"		// 5

	"		out	%2,	%3		\n\t"		// 1
	"		sbrs %0,5		\n\t"		// 2
	"		out	%2,	%6		\n\t"		// 3
	"		out	%2,	%6		\n\t"		// 4
	"		mov %1,%0		\n\t"		// 5

	"		out	%2,	%3		\n\t"		// 1
	"		sbrs %1,4		\n\t"		// 2
	"		out	%2,	%6		\n\t"		// 3
	"		out	%2,	%6		\n\t"		// 4
	"		nop				\n\t"		// 5

	"		out	%2,	%3		\n\t"		// 1
	"		sbrs %1,3		\n\t"		// 2
	"		out	%2,	%6		\n\t"		// 3
	"		out	%2,	%6		\n\t"		// 4
	"		nop				\n\t"		// 5

	"		out	%2,	%3		\n\t"		// 1
	"		sbrs %1,2		\n\t"		// 2
	"		out	%2,	%6		\n\t"		// 3
	"		out	%2,	%6		\n\t"		// 4
	"		ld  %0,X		\n\t"		// 5

	"		out	%2,	%3		\n\t"		// 1
	"		sbrs %1,1		\n\t"		// 2
	"		out	%2,	%6		\n\t"		// 3
	"		out	%2,	%6		\n\t"		// 4
	"		dec %5			\n\t"		// 5

	"		out	%2,	%3		\n\t"		// 1
	"		sbrs %1,0		\n\t"		// 2
	"		out	%2,	%6		\n\t"		// 3
	"		out	%2,	%6		\n\t"		// 4

	"		brne olop%=		\n\t"		// 6
	
	:	"=&d" (ctr), "=&d" (curbyte)
	:	 "I" (_SFR_IO_ADDR(ws2812_port)), "r" (maskhi), "x" (data), "r" (datlen), "r" (masklo)
	);

}

#else
	#error "Invalid processor selection for ws2812 library!"
#endif


#endif /* LIGHT_WS2812_H_ */

