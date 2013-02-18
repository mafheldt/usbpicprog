/******************************************************************************
 * PIC USB
 * interrupt.c
 * to use the interrupt table with the bootloader in SDCC, the linker must
 * be called with the option --lvt-loc=0x800
 ******************************************************************************/

/** I N C L U D E S **********************************************************/
#ifdef SDCC
#include <pic18f2550.h>
#else
#include <p18cxxx.h>
#endif
#include "typedefs.h"
#include "interrupt.h"
#include "io_cfg.h"
/** V A R I A B L E S ********************************************************/
long timerCnt;
unsigned char timerRunning;
/** I N T E R R U P T  V E C T O R S *****************************************/
#ifndef SDCC
#pragma code high_vector=0x08
void interrupt_at_high_vector( void )
{
	_asm goto high_isr _endasm
}
#pragma code

#pragma code low_vector=0x18
void interrupt_at_low_vector( void )
{
	_asm goto low_isr _endasm
}
#pragma code
#endif
/** D E C L A R A T I O N S **************************************************/
/******************************************************************************
 * Function:        void high_isr(void)
 * PreCondition:    None
 * Input:
 * Output:
 * Side Effects:
 * Overview:
 *****************************************************************************/
unsigned int VppVolt;
extern byte History[];
unsigned char Histcnt = 0;
unsigned char VppPWMon = 0;
int VppTarget;

#ifndef SDCC
#pragma interrupt high_isr
void high_isr( void )
#else
void high_isr(void) interrupt 2
#endif
{
	if( PIR1bits.TMR1IF )
	{
		if( timerRunning && --timerCnt <= 0 )
			timerRunning = 0;
		ADCON0bits.GO = 1;
		TMR1H = TMR1H_PRESET;		//reset timer1
		TMR1L = TMR1L_PRESET;
		PIR1bits.TMR1IF = 0;
	}
	else if( PIR1bits.ADIF )
	{
		static int on;

		PIR1bits.ADIF = 0;
		VppVolt = ADRES;

		if( Histcnt < 119 )
		{
			History[Histcnt++] = ADRESL;
//			History[Histcnt++] = VppPWMon;
		}
		else if( Histcnt == 119 )
			History[Histcnt++] = VppPWMon;

#if 0
		{
#else
						// 13.0V 831	208
						// 11.0V 700	175
						//  8.5V 556	139	15
						//  5.0V 325	 81	 8
						//  3.3V 216	 54	 8
#define TARGET VppTarget // 320

		if( VppVolt > TARGET + 20 ) {
			if( VppPWMon > 10 )
				VppPWMon -= 10;
			else
				VppPWMon = 0;
		}
		else if( VppVolt > TARGET + 2 )
		{
			if( VppPWMon )
				--VppPWMon;
		}
		else
		{
			if( VppVolt > TARGET )
			{
				if( VppPWMon )
					--VppPWMon;
			}
			else if( VppVolt < TARGET )
				if( ++VppPWMon == 0 )
					VppPWMon = 255;

#endif
			if( VppPWMon >= History[0] )
				History[0] = VppPWMon+1;
#if 1
			if( VppPWMon <= 100 )
			{
				unsigned char i;
					
				i = (VppPWMon>>1) + 1;
				Pump2 = 1;
				Pump1 = 0;
				while( --i );
				Pump2 = 0;
				Pump1 = 1;
			}
			else
			{					// (101-84) * 8 + interrupt latency approx 100/2 * 3
				TMR0L = 84 - VppPWMon;		// timer0 VppPWMon-82 counts
				INTCONbits.TMR0IF = 0;
				T0CON = 0xC2; //On and prescaler div by 1:8
				Pump2 = 1;
				Pump1 = 0;
			}
#endif
		}

	}
	else if( INTCONbits.TMR0IF )
	{
		Pump2 = 0;
		Pump1 = 1;
		T0CON = 0;
		INTCONbits.TMR0IF = 0;
	}

}

/******************************************************************************
 * Function:        void low_isr(void)
 * PreCondition:    None
 * Input:
 * Output:
 * Side Effects:
 * Overview:
 *****************************************************************************/
#ifndef SDCC
#pragma interruptlow low_isr
void low_isr( void )
#else
void low_isr(void) interrupt 1
#endif
{
}
#pragma code

/******************************************************************************
 * This function set up timerCnt
 * at the moment timer1 is a 1 ms timer, if it is changes cnt needs to be adjusted
 */
void startTimerMs( unsigned int cnt )
{
	timerRunning = 0; // no surprises from timer interrupt
	if( cnt ) {
		timerCnt = cnt*MSCOUNT + 1;
		timerRunning = 1;
	}
}

void DelayMs( unsigned cnt )
{
	startTimerMs( cnt );
	while( timerRunning )
		continue;
}
/** EOF interrupt.c **********************************************************/
