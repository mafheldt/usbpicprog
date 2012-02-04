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
#include "interrupt.h"
#include "prog.h"
#include "upp.h" 
#include "io_cfg.h"             // I/O pin mapping
#include "prog_lolvl.h"

#ifdef TEST
#undef I2C_delay
#undef set_vdd_vpp
#endif

void I2C_delay()
{
	char i;
	for(i=0;i<10;i++)continue;
}

void set_vdd_vpp( PICTYPE pictype, PICFAMILY picfamily, char level )
{
	unsigned int i;
	if(level==1)
	{
		enablePGD();    //PGD output
		enablePGC();    //PGC output
		if(picfamily==dsP30F_LV)
		{
			enablePGD_LOW();
			enablePGC_LOW();
			PGD_LOWon();
			PGC_LOWon();
		}
		if((picfamily==PIC18J)||(picfamily==PIC18K)||(picfamily==PIC24))
		{
			VPP_RUNoff(); //MCLR low 
			VDDon();
			DelayMs(10);
			PGD_LOWon();	//PGD and PGC to 3.3V mode (output)
			if(picfamily!=PIC18K)
			{
				enablePGD_LOW();
				enablePGC_LOW();
			}
			PGC_LOWon();
			VPP_RUNon();	//VPP to 4.5V
			for(i=0;i<300;i++)continue; //aprox 0.5ms 
			if(picfamily==PIC18J)
			{
				VPP_RUNoff();	//and immediately back to 0...
				VPP_RSTon();
				DelayMs(4);
				DelayMs(6);
			}
			//clock_delay();	//P19 = 40ns min
			//write 0x4D43, high to low, other than the rest of the commands which are low to high...
			//0x3D43 => 0100 1101 0100 0011
			//from low to high => 1100 0010 1011 0010
			//0xC2B2	
			pic_send_word(0xC2B2);
			//write 0x4850 => 0100 1000 0101 0000 => 0000 1010 0001 0010 => 0x0A12
			pic_send_word(0x0A12);	
			VPP_RSToff(); //release from reset
			VPP_RUNon();
			DelayMs(1);
			if(picfamily==PIC24)
			{
				DelayMs(1);
				pic_send_n_bits(5,0);
				dspic_send_24_bits(0); //send a nop instruction with 5 additional databits
				DelayMs(1);
			}
			return;
		}
		if((pictype==I2C_EE_1)||(pictype==I2C_EE_2))
		{
			PGDhigh();
			PGChigh();
		}
		else
		{
			PGDlow();        // initial value for programming mode
			PGClow();        // initial value for programming mode
		}
		clock_delay();    // dummy tempo
		switch(pictype)
		{	
			case I2C_EE_1:
			case I2C_EE_2:
				VDDon(); //no VPP needed	
				break;
			case dsP30F:
				break;
			case P16F62X:	//VPP first
			case P16F62XA:
			case P12F629:
			case P12F6XX:
		   case P16F87:
				VPPon();
				break;
			default:
				VDDon(); //high, (inverted)
				break;
		}
		DelayMs(100);
		switch(pictype)
		{
			case I2C_EE_1:
			case I2C_EE_2:
				break;
			case P16F62X:
			case P16F62XA:				
			case P12F629:
			case P12F6XX:
		   case P16F87:			
				VDDon();
				break;
			case dsP30F:
				VDDon();
				clock_delay();
				VPPon();
				DelayMs(26);
				dspic_send_24_bits(0);
				dspic_send_24_bits(0);
				dspic_send_24_bits(0);
				dspic_send_24_bits(0);
				VPPoff();
				VPP_RSTon();
				VPP_RSToff();
				for(i=0;i<1;i++)continue;
				VPPon();
				break;
			default:
				VPPon(); //high, (inverted)
				break;
		}
		DelayMs(100);
	}
	else
	{
		VPPoff(); //low, (inverted)
		VPP_RUNoff();
		VPP_RSTon(); //hard reset, low (inverted)
		DelayMs(40);
		VPP_RSToff(); //hard reset, high (inverted)
		VDDoff(); //low, (inverted)
		trisPGD_LOW(); //input
		trisPGC_LOW(); //input
		trisPGD();    //PGD input
		trisPGC();    //PGC input
		DelayMs(20);
	}
}

void set_address(PICFAMILY picfamily, unsigned long address)
{
	unsigned long i;
	switch(picfamily)
	{
		case PIC18:
		case PIC18J:
		case PIC18K:
			pic_send(4,0x00,(unsigned int)(0x0E00|((address>>16)&0xFF))); //MOVLW Addr [23:16]
			pic_send(4,0x00,0x6EF8); //MOVWF TBLPTRU
			pic_send(4,0x00,(unsigned int)(0x0E00|((address>>8)&0xFF))); //MOVLW Addr [15:8]
			pic_send(4,0x00,0x6EF7); //MOVWF TBLPTRU
			pic_send(4,0x00,(unsigned int)(0x0E00|((address)&0xFF))); //MOVLW Addr [7:0]
			pic_send(4,0x00,0x6EF6); //MOVWF TBLPTRU
			break;
		case PIC10:
		case PIC16:
			for(i=0;i<address;i++)
				pic_send_n_bits(6,0x06);	//increment address
		default:
			break;
	}
}

#ifndef TEST
/**
Writes a n-bit command
**/
void pic_send_n_bits(char cmd_size, char command)
{
	char i;
	enablePGD();
	enablePGC();
	PGClow();
	PGDlow();
	for(i=0;i<cmd_size;i++)
	{
		if(command&1)PGDhigh();
		else PGDlow();
		PGChigh();		
		command>>=1;
		clock_delay();
		PGClow();
		clock_delay();
	}
	for(i=0;i<10;i++)continue;	//wait at least 1 us <<-- this could be tweaked to get the thing faster
}

void pic_send_word(unsigned int payload)
{
	char i;
	for(i=0;i<16;i++)
	{
		if(payload&1)PGDhigh();
		else PGDlow();
		PGChigh();
		payload>>=1;
		clock_delay();
		PGClow();
		clock_delay();
	}
	clock_delay();
}

void pic_send_word_14_bits(unsigned int payload)
{
	char i;
	PGDlow();
	clock_delay();
	PGChigh();
	clock_delay();
	PGClow();
	clock_delay();
	for(i=0;i<14;i++)
	{
		if(payload&1)PGDhigh();
		else PGDlow();
		PGChigh();		
		payload>>=1;
		clock_delay();
		PGClow();
		clock_delay();

	}
	PGDlow();
	clock_delay();
	PGChigh();
	clock_delay();
	PGClow();
	clock_delay();
	clock_delay();
}

/**
Writes a n-bit command + 16 bit payload to a pic18 device
**/
void pic_send(char cmd_size, char command, unsigned int payload)
{
	pic_send_n_bits(cmd_size,command);
	pic_send_word(payload);
	PGDlow();      //  <=== Must be low at the end, at least when VPP and VDD go low.
	
}

/**
Writes a n-bit command + 14 bit payload to a pic16 device
**/
void pic_send_14_bits(char cmd_size,char command, unsigned int payload)
{
	pic_send_n_bits(cmd_size,command);
	pic_send_word_14_bits(payload);
	PGDlow();      //  <=== Must be low at the end, at least when VPP and VDD go low.
}

unsigned int pic_read_14_bits(char cmd_size, char command)
{
	char i;
	unsigned int result;
	pic_send_n_bits(cmd_size,command);
	//for(i=0;i<80;i++)continue;	//wait at least 1us 
					///PIC10 only...
	trisPGD(); //PGD = input
	for(i=0;i<10;i++)continue;
	result=0;
	PGChigh();
	clock_delay();
	PGClow();
	clock_delay();
	for(i=0;i<14;i++)
	{

		PGChigh();
		clock_delay();
		result|=((unsigned int)PGD_READ)<<i;
		clock_delay();
		PGClow();
		clock_delay();
	}
	PGChigh();
	clock_delay();
	PGClow();
	clock_delay();
	enablePGD(); //PGD = output
	PGDlow();
	clock_delay();
	return result;
}


/**
reads 8 bits from a pic device with a given cmd_size bits command
**/
char pic_read_byte2(char cmd_size, char command)
{
	char i;
	char result;
	pic_send_n_bits(cmd_size,command);
//	for(i=0;i<80;i++)continue;	//wait at least 1us
	for(i=0;i<8;i++)
	{
		PGDlow();
		clock_delay();
		PGChigh();
		clock_delay();
		PGClow();
		clock_delay();
	}
	trisPGD(); //PGD = input
	trisPGD_LOW();
	for(i=0;i<10;i++)continue;
	result=0;
	for(i=0;i<8;i++)
	{

		PGChigh();
		clock_delay();
		result|=((char)PGD_READ)<<i;
		clock_delay();
		PGClow();
		clock_delay();
	}
	enablePGD(); //PGD = output
	PGDlow();
	clock_delay();
	return result;
}

/// read a 16 bit "word" from a dsPIC
unsigned int dspic_read_16_bits(unsigned char isLV)
{
	char i;
	unsigned int result;
	PGDlow();
	PGDhigh();	//send 1
	PGChigh();	//clock pulse
	PGClow();
	PGDlow();	//send 3 zeroes
	for(i=0;i<3;i++)
	{
		PGChigh();
		PGClow();
	}
	//pic_send_n_bits(4,1);
	result=0;
	for(i=0;i<8;i++)
	{
		PGChigh();
		PGClow();
	}
	//pic_send_n_bits(8,0);
	if(isLV) trisPGD_LOW();
	trisPGD(); //PGD = input
	clock_delay();
	for(i=0;i<16;i++)
	{
		PGChigh();
		clock_delay();
		result|=((unsigned int)PGD_READ)<<i;
		PGClow();
	}
	if(isLV) enablePGD_LOW();
	enablePGD(); //PGD = output
	PGDlow();
	return result;
}


void dspic_send_24_bits(unsigned long payload)
{
	unsigned char i;
	PGDlow();
	for(i=0;i<4;i++)
	{
		PGChigh();
		PGClow();
	}
	for(i=0;i<24;i++)
	{
		
		if(payload&1)PGDhigh();
		else PGDlow();
		payload>>=1;
		clock_delay();
		PGChigh();
		clock_delay();
		PGClow();
	}
}



void I2C_start(void)
{
	//initial condition
	PGDhigh();
	PGChigh();
	I2C_delay();
	PGDlow();
	I2C_delay();
	PGClow();
	I2C_delay();
}

void I2C_stop(void)
{
	PGChigh();
	I2C_delay();
	PGDhigh();
	I2C_delay();
}

unsigned char I2C_write(unsigned char d)
{
	unsigned char i,j;
	j=d;
	for(i=0;i<8;i++)
	{
		if((j&0x80)==0x80)PGDhigh();
		else PGDlow();
		j<<=1;
		I2C_delay();
		PGChigh();
		I2C_delay();
		PGClow();
		I2C_delay();
	}
	trisPGD();
	PGChigh();
	I2C_delay();
	i=(unsigned char)PGD_READ;
	PGClow();
	I2C_delay();
	enablePGD();
	return i;
}

unsigned char I2C_read(unsigned char ack)
{
	unsigned char i,d;
	trisPGD();
	d=0;
	for(i=0;i<8;i++)
	{
		PGChigh();
		I2C_delay();
		d<<=1;
		if(PGD_READ)d|=0x01;
		PGClow();
		I2C_delay();
	}
	enablePGD();
	I2C_delay();
	if(ack==1)PGDhigh();
	else PGDlow();
	PGChigh();
	I2C_delay();
	PGClow();
	I2C_delay();
	return d;
}

/*#define pulseclock() PGChigh();PGClow()

unsigned char jtag2w4p(unsigned char TDI, unsigned char TMS, unsigned char nbits)
{
	unsigned char i;
	unsigned char res=0;
	unsigned char orval=1<<(nbits-1);
	for(i=0; i<nbits; i++)
	{
		res>>=1;
		if(TDI&1)PGDhigh(); else PGDlow();
		pulseclock();
		if(TMS&1)PGDhigh(); else PGDlow();
		pulseclock();
		trisPGD();
		pulseclock();
		PGChigh();
		if(PGD_READ)res|=orval;
		PGClow();
		enablePGD();
		TDI>>=1;
		TMS>>=1;
		
	}
	return res;
}

XferFastData()
{
//TMS header 100
	unsigned char Ack;
	Ack=jtag2w5p(0,0b001, 3);
	///TODO: check Ack
	
	
}
*/

#endif
