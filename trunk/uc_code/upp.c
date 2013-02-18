/**************************************************************************
 *   Copyright (C) 2008 by Frans Schreuder                                 *
 *   usbpicprog.sourceforge.net                                            *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/

#ifdef SDCC
#include <pic18f2550.h>
#else
#include <p18cxxx.h>
#endif

#include "typedefs.h"
#include "usb.h"

#include "io_cfg.h"             // I/O pin mapping
#include "upp.h"
#include <string.h>
#include "prog.h"
#include "prog_lolvl.h"
#include "svn_revision.h"
#include "interrupt.h"
#include "main.h"

/** V A R I A B L E S ********************************************************/
#ifndef SDCC
#pragma udata
#endif
byte old_sw2, old_sw3;
struct {
	int Target;
	int Startup;
	unsigned char InitPWMon;
} Vpp[5] = { 	{ 202, 0, 255 },
		{ 313, 0, 255 },
		{ 549, 0, 255 },
		{ 720, 0, 255 },
		{ 1023, 1000, 255 }
	};


// FIXME: why 0x100?
//#pragma udata BUFFERS=0x100

byte input_buffer[USBGEN_EP_SIZE];
byte output_buffer[USBGEN_EP_SIZE];
#pragma udata BUFFERS=0x100
byte History[128];
#pragma udata

rom char upp_version[] = {
		SVN_REVISION };

/** P R I V A T E  P R O T O T Y P E S ***************************************/
void BlinkUSBStatus( void );
BOOL Switch2IsPressed( void );

void setLeds( char n );

/** D E C L A R A T I O N S **************************************************/
#ifndef SDCC
#pragma code
#endif

void InitAdc( void )
{
	ADCON0 = 0x09; //Enable AD converter to Channel AN2 (measure VPP);
	ADCON2 = 0xBE; //0xA6; //0x88		//Right justified, 2TAD, Fosc/2
	ADCON1 = 0x0C; //An0-An2 => Analog
	TRISAbits.TRISA2 = 1; //Input;
	PIE1bits.ADIE = 1;//enable timer1 interrupt
	PIR1bits.ADIF = 0;//clear interrupt flag
	IPR1bits.ADIP = 1;//high priority interrupt
}

void ReadAdc( unsigned char* data )
{
#if 1
	extern unsigned char VppPWMon;

	while( ADCON0bits.GO )
		continue;
#if 1
	data[0] = ADRESL;
	data[1] = ADRESH;
#else
	data[0] = ADRES >> 6;
	data[1] = ADRESH >> 6;
#endif
	data[2] = VppPWMon;
#else
	char i;
	i = 0;
	ADCON1 = 0x0C;	//channel AN2 .. AN0 to analog
	ADCON0bits.GO = 1;
	while( ADCON0bits.GO )
		continue;//||(i++<100))continue;
	data[0] = ADRESL;
	data[1] = ADRESH;
	ADCON1 = 0x0C;	//back to digital...
#endif
}

void UserInit( void )
{
	mInitAllLEDs();
	mInitAllSwitches();
	InitAdc();
	old_sw2 = sw2;
	Pump1tris = 0;
	Pump2tris = 0;

	TRISVPP = 0;	//output
	TRISVPP_RST = 0;//output
	TRISPGD = 0;
	TRISPGC = 0;
	TRISVDD = 0;
	TRISVPP_RUN = 0;
	VPP_RUN = 0;	//run = off
	PGD_LOW = 0;	// always set to 0 -except for I2C EEproms
	TRISPGD_LOW = 1;//LV devices disabled, high impedance / input
	PGC_LOW = 0;	// always set to 0
	TRISPGC_LOW = 1;//LV devices disabled, high impedance / input
	VPP = 1;	//VPP is low (inverted)
	VPP_RST = 0;	//No hard reset (inverted
	PGD = 0;
	PGC = 0;
	INTCON2bits.RBPU = 1; //disable Portb pullup resistors
	timer1Init();
	timer0Init();
	{
#define OFFSET 14
		int i, test_vpp(int);

		mLED_3_Toggle();
		i = 10;
		// 3.1 V
		while( (i += 10) && test_vpp(i) < Vpp[VPP_3_3V].Target+OFFSET )
			;
		Vpp[VPP_3_3V].Startup = i-10;
		// 4.8 V
		while( (i += 10) && test_vpp(i) < Vpp[VPP_5V].Target+OFFSET )
			;
		Vpp[VPP_5V].Startup = i-10;
		// 8.4V
		while( (i += 10) && test_vpp(i) < Vpp[VPP_8_5V].Target+OFFSET )
			;
		Vpp[VPP_8_5V].Startup = i-10;
		mLED_3_Toggle();
		// 11.0 V
		while( (i += 10) && test_vpp(i) < Vpp[VPP_11V].Target+OFFSET )
			;
		Vpp[VPP_11V].Startup = i-10;
		mLED_3_Toggle();

		// 3.1 V
		i = Vpp[VPP_3_3V].Startup;
		while( ++i && test_vpp(i) < Vpp[VPP_3_3V].Target+OFFSET )
			;
		Vpp[VPP_3_3V].Startup = i-1;
		mLED_3_Toggle();

		// 4.8 V
		i = Vpp[VPP_5V].Startup;
		while( ++i && test_vpp(i) < Vpp[VPP_5V].Target+OFFSET )
			;
		Vpp[VPP_5V].Startup = i-1;
		mLED_3_Toggle();

		// 8.4V
		i = Vpp[VPP_8_5V].Startup;
		while( ++i && test_vpp(i) < Vpp[VPP_8_5V].Target+OFFSET )
			;
		Vpp[VPP_8_5V].Startup = i-1;
		mLED_3_Toggle();

		// 11.0 V
		i = Vpp[VPP_11V].Startup;
		while( ++i && test_vpp(i) < Vpp[VPP_11V].Target+OFFSET )
			;
		Vpp[VPP_11V].Startup = i-1;
	}
}//end UserInit

void timer1Init( void )
{
	INTCON |= 0xC0; //global and peripheral interrupts enabled
	PIE1bits.TMR1IE = 1;//enable timer1 interrupt
	PIR1bits.TMR1IF = 0;//clear interrupt flag
	IPR1bits.TMR1IP = 1;//high priority interrupt
	TMR1H = TMR1H_PRESET;
	TMR1L = TMR1L_PRESET;
	T1CON = 0x01; //timer1 on, prescaler 1;1, internal clock
}

void timer0Init( void )
{
	INTCON |= 0xE0; //global, peripheral and t0ie bits enabled
//	INTCONbits.TMR0IF = 0; //clear interrupt flag
//	TMR0L = 68;//TMR0L_PRESET;
	INTCON2bits.TMR0IP = 1;//high priority interrupt
	T0CON = 0xC8; // no prescaler //prescaler div by 1:16
}

void setLeds( char n )
{
	mLED_1 = ~(n & 1);
	mLED_2 = ~((n & 2) >> 1);
	mLED_3 = ~((n & 4) >> 2);
}

/******************************************************************************
 * Function:        void ProcessIO(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        This function is a place holder for other user routines.
 *                  It is a mixture of both USB and non-USB tasks.
 *
 * Note:            None
 *****************************************************************************/

//unsigned int input_buffer_offset=0;

void ProcessIO( void )
{
	char oldPGDtris;
	char PIN;
	static byte counter = 0;
	int nBytes;
	unsigned long address;

	// When the device is plugged in, the leds give the numbers 1, 2, 3, 4, 5. 
	//After configured state, the leds are controlled by the next lines in this function
	if( (usb_device_state < CONFIGURED_STATE) || (UCONbits.SUSPND == 1) )
	{
		BlinkUSBStatus();
		return;
	}

	nBytes = USBGenRead( (byte*) input_buffer, 64 );
	if( nBytes > 0 )
	{
		switch( input_buffer[0] ) {
		case CMD_GET_PROTOCOL_VERSION:
			output_buffer[0] = 1;		// version 1
			counter = 1;
			break;
		case CMD_EXIT_TO_BOOTLOADER:
			exitToBootloader( 1 );		// if this returns -> bad bootloader version, return error
			for( counter = 0; counter < 10; counter++ )
			{
				setLeds(7);
				DelayMs( 200 );
				setLeds( 0 );
				DelayMs( 200 );
			}
			output_buffer[0] = 3;
			counter = 1;
			break;
		case CMD_ERASE:
			setLeds( LEDS_ON | LEDS_WR );
			output_buffer[0] = bulk_erase( input_buffer[1] );
			counter = 1;
			setLeds( LEDS_ON );
			break;
		case CMD_READ_ID:
			setLeds( LEDS_ON | LEDS_RD );
			switch( picfamily ) {
			case PIC24:
			case dsPIC30:
				read_code( 0xFF0000, (unsigned char*) output_buffer, 2, BLOCKTYPE_FIRST|BLOCKTYPE_LAST|BLOCKTYPE_CONFIG );
				break;
			case PIC18:
			case PIC18J:
			case PIC18K:
				//devid is at location 0x3ffffe   for PIC18 devices
				read_code( 0x3FFFFE, (unsigned char*) output_buffer, 2, BLOCKTYPE_FIRST|BLOCKTYPE_LAST|BLOCKTYPE_CONFIG );
				break;
			case PIC16:
				exit_ISCP();
				//devid is at location 0x2006  for PIC16 devices
				read_code( 0x2006, (unsigned char*) output_buffer, 2, BLOCKTYPE_FIRST|BLOCKTYPE_LAST|BLOCKTYPE_CONFIG );
				break;
			}
			counter = 2;
			setLeds( LEDS_ON );
			break;
		case CMD_WRITE_CODE:
			setLeds( LEDS_ON | LEDS_WR );
			address = ((unsigned long) input_buffer[2]) << 16 | ((unsigned long) input_buffer[3]) << 8
					| ((unsigned long) input_buffer[4]);
			output_buffer[0] = write_code( address,
					(unsigned char*) (input_buffer + 6), input_buffer[1], input_buffer[5] );
			counter = 1;
			setLeds( LEDS_ON );
			break;
		case CMD_READ_CONFIG:
			input_buffer[5] |= BLOCKTYPE_CONFIG;
		case CMD_READ_CODE_OLD:
			if( input_buffer[1] <= 8 )		// this is correct for all but PIC18F4321(?) where it doesn't matter
				input_buffer[5] |= BLOCKTYPE_CONFIG;
		case CMD_READ_CODE:
			setLeds( LEDS_ON | LEDS_RD );

			address = ((unsigned long) input_buffer[2]) << 16 | ((unsigned long) input_buffer[3]) << 8
					| ((unsigned long) input_buffer[4]);
			PIN = read_code( address, (unsigned char*) output_buffer, input_buffer[1],
					input_buffer[5] );
			if( PIN == 3 )
				output_buffer[0] = 0x3;
			counter = input_buffer[1];
			setLeds( LEDS_ON );
			break;
		case CMD_WRITE_DATA:
			setLeds( LEDS_ON | LEDS_WR );
			address = ((unsigned long) input_buffer[2]) << 16 | ((unsigned long) input_buffer[3]) << 8
					| ((unsigned long) input_buffer[4]);
			output_buffer[0] = write_data( address,
					(unsigned char*) (input_buffer + 6), input_buffer[1], input_buffer[5] );
			counter = 1;
			setLeds( LEDS_ON );
			break;
		case CMD_READ_DATA:
			setLeds( LEDS_ON | LEDS_RD );
			address = ((unsigned long) input_buffer[2]) << 16 | ((unsigned long) input_buffer[3]) << 8
					| ((unsigned long) input_buffer[4]);
			read_data( address, (unsigned char*) output_buffer, input_buffer[1],
					input_buffer[5] );
			counter = input_buffer[1];
			setLeds( LEDS_ON );
			break;
		case CMD_WRITE_CONFIG:
			setLeds( LEDS_ON | LEDS_WR );
			address = ((unsigned long) input_buffer[2]) << 16 | ((unsigned long) input_buffer[3]) << 8
					| ((unsigned long) input_buffer[4]);
			output_buffer[0] = write_config_bits( address,
					(unsigned char*) (input_buffer + 6), input_buffer[1], input_buffer[5] );
			counter = 1;
			setLeds( LEDS_ON );
			break;
		case CMD_SET_PICTYPE:
		{
			int i;

			output_buffer[0] = set_pictype( input_buffer[1] );
			counter = 1;
			setLeds( LEDS_ON );
			break;
		}
		case CMD_FIRMWARE_VERSION:
			strcpypgm2ram((char*)output_buffer,(const far rom char*)upp_version);
			counter = 18;
			setLeds( LEDS_ON );

			break;
		case CMD_DEBUG:
			setLeds( LEDS_ON | LEDS_WR | LEDS_RD );
			switch( input_buffer[1] ) {
			case 0:
				set_pictype( dsP30F );
				enter_ISCP();
				output_buffer[0] = 1;
				counter = 1;
				break;
			case 1:
				exit_ISCP();
				output_buffer[0] = 1;
				counter = 1;
				break;
			case 2:
				dspic_send_24_bits( ((unsigned long) input_buffer[2])
						| ((unsigned long) input_buffer[3]) << 8
						| ((unsigned long) input_buffer[4]) << 16 );
				output_buffer[0] = 1;
				counter = 1;
				break;
			case 3:
				nBytes = dspic_read_16_bits( 1 );
				output_buffer[0] = (unsigned char) nBytes;
				output_buffer[1] = (unsigned char) (nBytes >> 8);
				counter = 2;
				break;

			}
			break;
		case CMD_GET_PIN_STATUS:
			switch( input_buffer[1] ) {
			case SUBCMD_PIN_PGC:

				if( PGC == 0 )
					output_buffer[0] = PIN_STATE_0V;
				else if( (!TRISPGC_LOW) ) //3.3V levels
					output_buffer[0] = PIN_STATE_3_3V;
				else
					output_buffer[0] = PIN_STATE_5V;
				counter = 1;
				break;
			case SUBCMD_PIN_PGD:
				if( PGD == 0 )
					output_buffer[0] = PIN_STATE_0V;
				else if( (!TRISPGD_LOW) ) //3.3V levels
					output_buffer[0] = PIN_STATE_3_3V;
				else
					output_buffer[0] = PIN_STATE_5V;
				counter = 1;
				break;
			case SUBCMD_PIN_VDD:
				if( VDD )
					output_buffer[0] = PIN_STATE_FLOAT;
				else
					output_buffer[0] = PIN_STATE_5V;
				counter = 1;
				break;
			case SUBCMD_PIN_VPP:
				counter = 1;
				if( !VPP )
				{
					output_buffer[0] = PIN_STATE_12V;
					break;
				}
				if( VPP_RST )
				{
					output_buffer[0] = PIN_STATE_0V;
					break;
				}
				if( VPP_RUN )
				{
					output_buffer[0] = PIN_STATE_5V;
					break;
				}
				output_buffer[0] = PIN_STATE_FLOAT;
				break;
			case SUBCMD_PIN_VPP_VOLTAGE:
			{
#if 1
				extern unsigned char VppPWMon;
				static unsigned char half = 0;
				half = !half;
				ReadAdc( output_buffer );
				output_buffer[3] = half? 0: 1;
				output_buffer[4] = History[0];
				memcpy( output_buffer+5, half? &History[1]: &History[60], 59 );
				History[0] = 0;
				counter = 64;
#else
				extern int test_vpp( int i );
				static int i = 0;

				i = 0;
				((int *) output_buffer)[i++] = test_vpp( Vpp[0] );
				((int *) output_buffer)[i++] = Vpp[0];
				((int *) output_buffer)[i++] = Vpp[1];
				((int *) output_buffer)[i++] = Vpp[2];
				((int *) output_buffer)[i++] = Vpp[3];
				i = 8;
#if 1
				((int *) output_buffer)[i++] = test_vpp( Vpp[0] );
				((int *) output_buffer)[i++] = test_vpp( Vpp[1] );
				((int *) output_buffer)[i++] = test_vpp( Vpp[2] );
				((int *) output_buffer)[i++] = test_vpp( Vpp[3] );
				((int *) output_buffer)[i++] = test_vpp( Vpp[0]+1 );
				((int *) output_buffer)[i++] = test_vpp( Vpp[1]+1 );
				((int *) output_buffer)[i++] = test_vpp( Vpp[2]+1 );
				((int *) output_buffer)[i++] = test_vpp( Vpp[3]+1 );
#endif

				counter = 64;
#endif
			}
				break;
			default:
				output_buffer[0] = 3;
				counter = 1;
				break;
			}
			break;
		case CMD_SET_PIN_STATUS:
			switch( input_buffer[1] ) {
			case SUBCMD_PIN_PGC:
				switch( input_buffer[2] ) {
				case PIN_STATE_0V:
					TRISPGC = 0;
					PGC = 0;
					TRISPGC_LOW = 1;
					PGC_LOW = 0;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_3_3V:
					TRISPGC = 0;
					PGC = 1;
					TRISPGC_LOW = 0;
					PGC_LOW = 0;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_5V:
					TRISPGC = 0;
					PGC = 1;
					TRISPGC_LOW = 1;
					PGC_LOW = 0;
					output_buffer[0] = 1;//ok
					break;
				default:
					output_buffer[0] = 3;
					break;
				}
				break;
			case SUBCMD_PIN_PGD:
				switch( input_buffer[2] ) {
				case PIN_STATE_0V:
					TRISPGD = 0;
					PGD = 0;
					TRISPGD_LOW = 1;
					PGD_LOW = 0;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_3_3V:
					TRISPGD = 0;
					PGD = 1;
					TRISPGD_LOW = 0;
					PGD_LOW = 0;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_5V:
					TRISPGD = 0;
					PGD = 1;
					TRISPGD_LOW = 1;
					PGD_LOW = 0;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_INPUT:
					TRISPGD_LOW = 1;
					TRISPGD = 1;
					output_buffer[0] = 1;//ok
					break;
				default:
					output_buffer[0] = 3;
					break;
				}
				break;
			case SUBCMD_PIN_VDD:
				switch( input_buffer[2] ) {
				case PIN_STATE_5V:
					VDD = 0;
					output_buffer[0] = 1;
					break;
				case PIN_STATE_FLOAT:
					VDD = 1;
					output_buffer[0] = 1;
					break;
				default:
					output_buffer[0] = 3;
					break;
				}
				break;
			case SUBCMD_PIN_VPP:
			{
				extern unsigned char Histcnt;
				extern unsigned char VppPWMon;
				extern int VppTarget;
				static int volt = 0;

				Histcnt = 0;
				switch( input_buffer[2] ) {
				case PIN_STATE_0V:
					VPP = 1;
					VPP_RST = 1;
					VPP_RUN = 0;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_5V:
					VPP = 1;
					VPP_RST = 0;
					VPP_RUN = 1;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_12V:
					Histcnt = 250;
					History[1] = test_vpp( Vpp[volt].Startup );
					Histcnt = 2;
					VppTarget = Vpp[volt].Target;
					VppPWMon = Vpp[volt].InitPWMon;
					History[0] = 0;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_FLOAT:
					Histcnt = 0;
					VppTarget = 0;
					volt = ++volt % 4;
					output_buffer[0] = 1;//ok
					break;
#if 0
				case PIN_STATE_0V:
					VPP = 1;
					VPP_RST = 1;
					VPP_RUN = 0;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_5V:
					VPP = 1;
					VPP_RST = 0;
					VPP_RUN = 1;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_12V:
					VPP = 0;
					VPP_RST = 0;
					VPP_RUN = 0;
					output_buffer[0] = 1;//ok
					break;
				case PIN_STATE_FLOAT:
					VPP = 1;
					VPP_RST = 0;
					VPP_RUN = 0;
					output_buffer[0] = 1;//ok
					break;
#endif
				default:
					output_buffer[0] = 3;
					break;
				}
				break;
			}
			default:
				output_buffer[0] = 3;
			}
			counter = 1;
			break;
		default:
			output_buffer[0] = 3;			// unrecognized command
			counter = 1;
		}
	}
	if( counter != 0 )
	{
		if( !mUSBGenTxIsBusy() )
			USBGenWrite( (byte*) &output_buffer, counter );
		counter = 0;
	}
}//end ProcessIO

unsigned char set_pictype( unsigned char pt )
{
	unsigned char i;

	pictype = pt;
	for( i = 0; i < UPP_INVALID_PICTYPE; i++ )
	{
		if( devices[i].flags.type == pt )
		{
			currDevice = devices[i];
			break;
		}
	}

	if( i < UPP_INVALID_PICTYPE && currDevice.flags.family != UPP_INVALID_PICFAMILY )
	{
		picfamily = currDevice.flags.family;
		return (1);
	}
	pictype = P18F2XXX;
	for( i = 0; i < UPP_INVALID_PICTYPE; i++ )
	{
		if( devices[i].flags.type == pictype )
		{
			currDevice = devices[i];
			break;
		}
	}
	picfamily = currDevice.flags.family;
	return( 3 );
}

	/******************************************************************************
 * Function:        void BlinkUSBStatus(void)
 *
 * PreCondition:    None
 *
 * Input:           None
 *
 * Output:          None
 *
 * Side Effects:    None
 *
 * Overview:        BlinkUSBStatus turns on and off LEDs corresponding to
 *                  the USB device state.
 *
 * Note:            mLED macros can be found in io_cfg.h
 *                  usb_device_state is declared in usbmmap.c and is modified
 *                  in usbdrv.c, usbctrltrf.c, and usb9.c
 *****************************************************************************/
void BlinkUSBStatus( void )
{
	static word led_count = 0;
	static char startup_state = 0xFF;

	if( led_count == 0 )
		led_count = 10000U;
	led_count--;

	if( UCONbits.SUSPND == 1 )
	{
		//USBWakeFromSuspend();
		if( led_count == 0 )
		{
			mLED_1_Off();
			mLED_2_Off();
			mLED_3_Toggle();
		}//end if
	}
	else
	{
		if( usb_device_state == DETACHED_STATE )
		{
			setLeds( 1 );
		}
		else if( usb_device_state == ATTACHED_STATE )
		{
			setLeds( 2 );
		}
		else if( usb_device_state == POWERED_STATE )
		{
			setLeds( 4 );
		}
		else if( usb_device_state == DEFAULT_STATE )
		{
			setLeds( 2 );
		}
		else if( usb_device_state == ADDRESS_STATE )
		{
			setLeds( LEDS_ON );
		}
		else if( usb_device_state == CONFIGURED_STATE )
		{
			startup_state = 0;
		}
	}
}

BOOL Switch2IsPressed( void )
{
	if( sw2 != old_sw2 )
	{
		old_sw2 = sw2; // Save new value
		if( sw2 == 0 ) // If pressed
			return TRUE; // Was pressed
	}
	return FALSE; // Was not pressed
}//end Switch2IsPressed


/** EOF upp.c ***************************************************************/
