/***************************************************************************
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
 ***************************************************************************/

#include "hardware.h"
#include "uppmainwindow_callback.h"

using namespace std;

#define USB_DEBUG 10
/*The class Hardware connects to usbpicprog using libusb. The void* CB points 
 to the parent UppMainWindowCallBack which is used for updating the progress
 bar. If initiated with no argument, progress is not updated*/
Hardware::Hardware(void* CB, HardwareType SetHardware)
{
	struct usb_bus *bus=NULL;
	struct usb_device *dev=NULL;
	int hwtype = SetHardware;
	
	_handle=NULL;
	
	usb_init();
	ptCallBack=CB;
	usb_find_busses();
	usb_find_devices();
#ifdef USB_DEBUG
	cout<<"USB debug enabled, remove #define USB_DEBUG 10 in hardware.cpp to disable it"<<endl; 
	usb_set_debug(USB_DEBUG);
#endif
	
	while (hwtype > -1)
	{
		for (bus=usb_get_busses();bus;bus=bus->next)
		{
			for (dev=bus->devices;dev;dev=dev->next)
			{
				
				_handle = usb_open(dev);
				if (!_handle)continue; //failed to open this device, choose the next one
				if (hwtype == HW_UPP)
				{
					if ((dev->descriptor.idVendor == UPP_VENDOR) && (dev->descriptor.idProduct == UPP_PRODUCT) )
						break; //found usbpicprog, exit the for loop
				}
				else
				{
					if ((dev->descriptor.idVendor == BOOTLOADER_VENDOR) && (dev->descriptor.idProduct == BOOTLOADER_PRODUCT) )
						break; //found usbpicprog, exit the for loop
				}
				usb_close(_handle);
				_handle=NULL;
			}
			if(_handle!=NULL)	//successfully initialized? don't try any other buses
				break;
		}

		if(_handle==NULL)
		{
			hwtype++;
			if (hwtype >= HARDWARETYPE_NUM)
			{
				hwtype = 0;
			}
			
			if (hwtype == SetHardware)
			{
				CurrentHardware = HW_NONE;
				hwtype = -1;
			}
		}
		else
		{
			CurrentHardware = (HardwareType)hwtype;
			hwtype = -1;
		}
	}
	if(_handle!=NULL)
	{
		tryToDetachDriver();
        usb_config_descriptor &config = dev->config[0]; 
		if (usb_set_configuration(_handle, config.bConfigurationValue) < 0) 
			cerr<<"Couldn't set configuration"<<endl;
	
		if (usb_claim_interface(_handle, config.interface[0].altsetting[0].bInterfaceNumber) < 0)
			cerr<<"Couldn't claim interface"<<endl;

	}
}

Hardware::~Hardware()
{
	statusCallBack (0);
	
	if(_handle)
	{
		usb_release_interface(_handle, bInterfaceNumber);
		usb_close(_handle);
		_handle=NULL;
	}
}

HardwareType Hardware::getHardwareType(void)
{
	return CurrentHardware;
}

void Hardware::tryToDetachDriver()
{
  // try to detach an already existing driver... (linux only)
#if defined(LIBUSB_HAS_GET_DRIVER_NP) && LIBUSB_HAS_GET_DRIVER_NP
//  log(Log::DebugLevel::Extra, "find if there is already an installed driver");
  char dname[256] = "";
  if ( usb_get_driver_np(_handle, bInterfaceNumber, dname, 255)<0 ) return;
//  log(Log::DebugLevel::Normal, QString("  a driver \"%1\" is already installed...").arg(dname));
#  if defined(LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP) && LIBUSB_HAS_DETACH_KERNEL_DRIVER_NP
  usb_detach_kernel_driver_np(_handle, bInterfaceNumber);
 // log(Log::DebugLevel::Normal, "  try to detach it...");
  if ( usb_get_driver_np(_handle, bInterfaceNumber, dname, 255)<0 ) return;
 // log(Log::DebugLevel::Normal, "  failed to detach it");
#  endif
#endif
}

/*give the hardware the command to switch to a certain pic algorithm*/
int Hardware::setPicType(PicType* picType)
{
	char msg[64];
	
	if (CurrentHardware == HW_BOOTLOADER) return 0;
	
	statusCallBack (0);
	msg[0]=CMD_SET_PICTYPE;
	msg[1]=picType->getCurrentPic().picFamily;
	
	int nBytes=-1;
	if (_handle !=NULL)
	{
		if(writeString(msg,2)<0)
		{
			return 0;
		}
		
		nBytes = readString(msg,1);

		if (nBytes < 0 )
		{
			cerr<<"Usb Error"<<endl;
			return nBytes;
		}
		else
		{
			//cout<<"Id: "<<hex<<((int)msg[0]&0xFF)<<" "<<hex<<((int)msg[1]&0xFF)<<", "<<nBytes<<" bytes"<<endl;
			return (int)msg[0];
			statusCallBack (100);

		}
	}
	return nBytes;
}

/*Erase all the contents (code, data and config) of the pic*/
int Hardware::bulkErase(void)
{
	char msg[64];

	msg[0]=CMD_ERASE;
	int nBytes=-1;
	statusCallBack (0);
	if (_handle !=NULL)
	{
		if(writeString(msg,1)<0)
		{
			return 0;
		}
		nBytes = readString(msg,1);

		if (nBytes < 0 )
		{
			cerr<<"Usb Error"<<endl;
			return nBytes;
		}
		else
		{
			return (int)msg[0];
			statusCallBack (100);
		}
	}
	return nBytes;
}

/*Read the code memory from the pic (starting from address 0 into *hexData*/
int Hardware::readCode(ReadHexFile *hexData,PicType *picType)
{
	int nBytes;
	nBytes=-1;
	vector<int> mem;
	mem.resize(picType->getCurrentPic().CodeSize, 0xFF);
	char dataBlock[BLOCKSIZE_CODE];
	int blocktype;
	statusCallBack (0);
	if (_handle !=NULL)
	{
		nBytes=0;
		for(int blockcounter=0;blockcounter<picType->getCurrentPic().CodeSize;blockcounter+=BLOCKSIZE_CODE)
		{
			statusCallBack ((blockcounter*100)/((signed)picType->getCurrentPic().CodeSize));
			blocktype=BLOCKTYPE_MIDDLE;
			if(blockcounter==0)blocktype|=BLOCKTYPE_FIRST;
			if((picType->getCurrentPic().CodeSize-BLOCKSIZE_CODE)<=blockcounter)blocktype|=BLOCKTYPE_LAST;
			nBytes+=readCodeBlock(dataBlock,blockcounter,BLOCKSIZE_CODE,blocktype);
			for(int i=0;i<BLOCKSIZE_CODE;i++)
			{
				if(picType->getCurrentPic().CodeSize>(blockcounter+i))
				{
					/*if (blockcounter+i >= 0x800 && blockcounter+i <= 0xA00)
					{
						cerr<<(int)(unsigned char)dataBlock[i]<<endl;
					}*/
					mem[blockcounter+i]=((unsigned char)(dataBlock[i]&0xFF));
				}
				else
				{
					cerr<<"Trying to read memory outside Code area"<<endl;
					//return -1;
				}
			}
				
			/*if (dataBlock[BLOCKSIZE_CODE-1] == 0)
			{
				blockcounter-=BLOCKSIZE_CODE-1;
			}*/
		}
		hexData->putCodeMemory(mem);
	}
	return nBytes;
}

/* Write the code memory area of the pic with the data in *hexData */
int Hardware::writeCode(ReadHexFile *hexData,PicType *picType)
{
	int nBytes;
	nBytes=-1;
	unsigned char dataBlock[BLOCKSIZE_CODE];
	int blocktype;
	if (_handle !=NULL)
	{
		nBytes=0;
		for(int blockcounter=0;blockcounter<(signed)hexData->getCodeMemory().size();blockcounter+=BLOCKSIZE_CODE)
		{
			statusCallBack ((blockcounter*100)/((signed)hexData->getCodeMemory().size()));
			for(int i=0;i<BLOCKSIZE_CODE;i++)
			{
				if((signed)hexData->getCodeMemory().size()>(blockcounter+i))
				{
					dataBlock[i]=hexData->getCodeMemory()[blockcounter+i];
				}
				else
				{
					dataBlock[i]=0;
				}
			}
				
			blocktype=BLOCKTYPE_MIDDLE;
			if(blockcounter==0)blocktype|=BLOCKTYPE_FIRST;
			if(((signed)hexData->getCodeMemory().size()-BLOCKSIZE_CODE)<=blockcounter)blocktype|=BLOCKTYPE_LAST;
			nBytes=writeCodeBlock(dataBlock,blockcounter,BLOCKSIZE_CODE,blocktype);
		}
	}
	return nBytes;
}

/* read the Eeprom Data area of the pic into *hexData->dataMemory */
int Hardware::readData(ReadHexFile *hexData,PicType *picType)
{
	int nBytes;
	nBytes=-1;
	vector<int> mem;
	mem.resize(picType->getCurrentPic().DataSize);
	char dataBlock[BLOCKSIZE_DATA];
	int blocktype;
	statusCallBack (0);
	if (_handle !=NULL)
	{
		for(int blockcounter=0;blockcounter<picType->getCurrentPic().DataSize;blockcounter+=BLOCKSIZE_DATA)
		{
			statusCallBack ((blockcounter*100)/((signed)picType->getCurrentPic().DataSize));
			blocktype=BLOCKTYPE_MIDDLE;
			if(blockcounter==0)blocktype|=BLOCKTYPE_FIRST;
			if((picType->getCurrentPic().DataSize-BLOCKSIZE_DATA)<=blockcounter)blocktype|=BLOCKTYPE_LAST;
			nBytes+=readDataBlock(dataBlock,blockcounter,BLOCKSIZE_DATA,blocktype);
			for(int i=0;i<BLOCKSIZE_DATA;i++)
			{
				if(picType->getCurrentPic().DataSize>(blockcounter+i))
				{
					mem[blockcounter+i]=(unsigned char)(dataBlock[i]&0xFF);
				}
				else
				{
					cerr<<"Trying to read memory outside Data area"<<endl;
	//				return -1;
				}
			}
		}
		hexData->putDataMemory(mem);
	}
	return nBytes;

}


/*write the Eeprom data from *hexData->dataMemory into the pic*/
int Hardware::writeData(ReadHexFile *hexData,PicType *picType)
{
	int nBytes;
	nBytes=-1;
	unsigned char dataBlock[BLOCKSIZE_DATA];
	int blocktype;
	statusCallBack (0);
	if (_handle !=NULL)
	{
		nBytes=0;
		for(int blockcounter=0;blockcounter<(signed)hexData->getDataMemory().size();blockcounter+=BLOCKSIZE_DATA)
		{
			statusCallBack ((blockcounter*100)/((signed)hexData->getDataMemory().size()));
			for(int i=0;i<BLOCKSIZE_DATA;i++)
			{
				if((signed)hexData->getDataMemory().size()>(blockcounter+i))
				{
					dataBlock[i]=hexData->getDataMemory()[blockcounter+i];
				}
				else
				{
					dataBlock[i]=0;
				}
			}
				
			blocktype=BLOCKTYPE_MIDDLE;
			if(blockcounter==0)blocktype|=BLOCKTYPE_FIRST;
			if(((signed)hexData->getDataMemory().size()-BLOCKSIZE_DATA)<=blockcounter)blocktype|=BLOCKTYPE_LAST;
			nBytes+=writeDataBlock(dataBlock,blockcounter,BLOCKSIZE_DATA,blocktype);
		}
	}
	return nBytes;
}

/* Read the configuration words (and user ID's for PIC16 dev's) */
int Hardware::readConfig(ReadHexFile *hexData,PicType *picType)
{
	int nBytes;
	nBytes=-1;
	
	vector<int> mem;
	mem.resize(picType->getCurrentPic().ConfigSize);
	char dataBlock[BLOCKSIZE_CONFIG];
	int blocktype;
	statusCallBack (0);
	if (_handle !=NULL)
	{
		for(int blockcounter=0;blockcounter<picType->getCurrentPic().ConfigSize;blockcounter+=BLOCKSIZE_CONFIG)
		{
			statusCallBack ((blockcounter*100)/((signed)picType->getCurrentPic().ConfigSize));
			blocktype=BLOCKTYPE_MIDDLE;
			if(blockcounter==0)blocktype|=BLOCKTYPE_FIRST;
			if((picType->getCurrentPic().ConfigSize-BLOCKSIZE_CONFIG)<=blockcounter)blocktype|=BLOCKTYPE_LAST;
	
			nBytes+=readConfigBlock(dataBlock,blockcounter+picType->getCurrentPic().ConfigAddress,BLOCKSIZE_CONFIG,blocktype);		
			for(int i=0;i<BLOCKSIZE_CONFIG;i++)
			{
				if(picType->getCurrentPic().ConfigSize>(blockcounter+i))
				{
					mem[blockcounter+i]=(unsigned char)(dataBlock[i]&0xFF);
//					cerr<<hex<<(int)dataBlock[i]<<" "<<dec;
				}
				else
				{
					cerr<<"Trying to read memory outside Config area: bc+i="<<blockcounter+i<<"Configsize="<<picType->getCurrentPic().ConfigSize<<endl;
//					return -1;
				}
			}
		}
		hexData->putConfigMemory(mem);
	}
	cerr<<endl;
	return nBytes;
}

/*Writes the configuration words (and user ID's for PIC16 dev's)*/
int Hardware::writeConfig(ReadHexFile *hexData,PicType *picType)
{
	int nBytes;
	nBytes=-1;
	unsigned char dataBlock[BLOCKSIZE_CONFIG];
	int blocktype;
	statusCallBack (0);
	if (_handle !=NULL)
	{
		nBytes=0;
		for(int blockcounter=0;blockcounter<(signed)hexData->getConfigMemory().size();blockcounter+=BLOCKSIZE_CONFIG)
		{
			statusCallBack ((blockcounter*100)/((signed)hexData->getConfigMemory().size()));
			for(int i=0;i<BLOCKSIZE_CONFIG;i++)
			{
				if((signed)hexData->getConfigMemory().size()>(blockcounter+i))
				{
					dataBlock[i]=hexData->getConfigMemory()[blockcounter+i];
				}
				else
				{
					dataBlock[i]=0;
				}
			}
				
			blocktype=BLOCKTYPE_MIDDLE;
			if(blockcounter==0)blocktype|=BLOCKTYPE_FIRST;
			if(((signed)hexData->getConfigMemory().size()-BLOCKSIZE_CONFIG)<=blockcounter)blocktype|=BLOCKTYPE_LAST;
			nBytes+=writeConfigBlock(dataBlock,blockcounter+picType->getCurrentPic().ConfigAddress,BLOCKSIZE_CONFIG,blocktype);
		}
	}
	return nBytes;
}

/*Reads the whole PIC and checks if the data matches hexData*/
VerifyResult Hardware::verify(ReadHexFile *hexData, PicType *picType, bool doCode, bool doConfig, bool doData)
{
    VerifyResult res;
    ReadHexFile *verifyHexFile = new ReadHexFile;
    if(doCode)
    {
        if(readCode(verifyHexFile,picType)<0)
    	{
            res.Result=VERIFY_USB_ERROR;
            res.DataType=TYPE_CODE;
            return res;
        }
    }
    if(doData)
    {
    	if(readData(verifyHexFile,picType)<0)
    	{
            res.Result=VERIFY_USB_ERROR;
            res.DataType=TYPE_DATA;
            return res;
        }
    }
    if(doConfig)
    {
    	if(readConfig(verifyHexFile,picType)<0)
    	{
            res.Result=VERIFY_USB_ERROR;
            res.DataType=TYPE_CONFIG;
            return res;
        }
    }
    if ((hexData->getCodeMemory().size()+
        hexData->getDataMemory().size())>0) //there should be at least some data in the file
    {
        if(doCode)
        {
            for(int i=0;i<(signed)hexData->getCodeMemory().size();i++)
            {
                if((signed)verifyHexFile->getCodeMemory().size()<(i+1))
                {
                    res.Result=VERIFY_OTHER_ERROR;
                    return res;
                }
                if(verifyHexFile->getCodeMemory()[i]!=hexData->getCodeMemory()[i])
                {
                    res.Result=VERIFY_MISMATCH;
                    res.DataType=TYPE_CODE;
                    res.Address=i;
                    res.Read=verifyHexFile->getCodeMemory()[i];
                    res.Expected=hexData->getCodeMemory()[i];
                    return res;
                }
    
            }
        }
        if(doData)
        {
            for(int i=0;i<(signed)hexData->getDataMemory().size();i++)
            {
                if((signed)verifyHexFile->getDataMemory().size()<(i+1))
                {
                    res.Result=VERIFY_OTHER_ERROR;              
                    return res;
                }
                if(verifyHexFile->getDataMemory()[i]!=hexData->getDataMemory()[i])
                {
                    res.Result=VERIFY_MISMATCH;
                    res.DataType=TYPE_DATA;
                    res.Address=i;
                    res.Read=verifyHexFile->getDataMemory()[i];
                    res.Expected=hexData->getDataMemory()[i];
                    return res;
                }
    
            }
        }	
        if(doConfig)
        {
            for(int i=0;i<(signed)hexData->getConfigMemory().size();i++)
            {
                if((signed)verifyHexFile->getConfigMemory().size()<(i+1))
                {
                    res.Result=VERIFY_OTHER_ERROR;                
                    return res;
                }
                if(verifyHexFile->getConfigMemory()[i]!=hexData->getConfigMemory()[i])
                {
                    res.Result=VERIFY_MISMATCH;
                    res.DataType=TYPE_CONFIG;
                    res.Address=i;
                    res.Read=verifyHexFile->getConfigMemory()[i];
                    res.Expected=hexData->getConfigMemory()[i];
                    return res;
                }
    
            }
        }
        res.Result=VERIFY_SUCCESS;
    }
    else
    {
      res.Result=VERIFY_OTHER_ERROR;
    }
    return res;
}

/*Reads the whole PIC and checks if it is blank*/
VerifyResult Hardware::blankCheck(PicType *picType)
{
    ReadHexFile* blankHexFile=new ReadHexFile(picType);
    return verify(blankHexFile,picType);
}


/*This function does nothing but reading the devid from the PIC, call it the following way:
 
	 Hardware* hardware=new Hardware();
	 int devId=hardware->autoDetectDevice();
	 PicType* picType=new PicType(devId);
	 hardware->setPicType(picType);
	 
 */
int Hardware::autoDetectDevice(void)
{
	int devId=0;
	if (CurrentHardware == HW_BOOTLOADER) return 0;
	setPicType (new PicType("P18F2550"));	//need to set hardware to PIC18, no matter which one
	devId=readId();
	if((devId!=0xFFFF)&&(devId>1))return devId;
	setPicType(new PicType("P16F628A"));	//and now try PIC16*/
	return readId();
}

/*check if usbpicprog is successfully connected to the usb bus and initialized*/
bool Hardware::connected(void)
{
		if (_handle == NULL)
			return 0;
		else
			return 1;
}

/*Return a string containing the firmware version of usbpicprog*/
int Hardware::getFirmwareVersion(char* msg)
{
	if (CurrentHardware == HW_UPP)
	{
		msg[0]=CMD_FIRMWARE_VERSION;
		int nBytes=-1;
		statusCallBack (0);
		if (_handle !=NULL)
		{
			if(writeString(msg,1)<0)
			{
				return -1;
			}
			nBytes = readString(msg,64);
			
			if (nBytes < 0 )
			{
				return nBytes;
			}
			else
			{
				statusCallBack (100);
			}
		}
		return nBytes;
	}
	else
	{
		msg[0]=0;
		msg[1]=0;
		msg[2]=0;
		msg[3]=0;
		msg[4]=0;
		
		int nBytes=-1;
		statusCallBack (0);
		if (_handle !=NULL)
		{
			if(writeString(msg,5)<0)
			{
				return -1;
			}
			nBytes = readString(msg,64);
			
			if (nBytes < 0 )
			{
				return nBytes;
			}
			else
			{
				statusCallBack (100);
				sprintf((char*)msg, "Bootloader v%d.%d", msg[3], msg[2]);
						
				return nBytes;
			}
		}
	}
	return -1;
}

/*read a string of data from usbpicprog (through interrupt_read)*/
int Hardware::readString(char* msg,int size)
{
	if (_handle == NULL) return -1;
	int nBytes = usb_interrupt_read(_handle,READ_ENDPOINT,(char*)msg,size,5000);
		if (nBytes < 0 )
		{
			cerr<<"Usb Error while reading: "<<nBytes<<endl;
			cerr<<usb_strerror()<<endl;
			return -1;
		}
		return nBytes;

}

/*Send a string of data to usbpicprog (through interrupt write)*/
int Hardware::writeString(const char * msg,int size)
{
	int nBytes=0;
	if (_handle != NULL)
	{
		
		/*
		int todo = size;
		while(1)
		{
			int res = 0;
			//qDebug("write ep=%i todo=%i/%i", ep, todo, size);
			if ( mode==Interrupt ) res = usb_interrupt_write(_handle, ep, (char *)data + size - todo, todo, 1000);
			else res = usb_bulk_write(_handle, ep, (char *)data + size - todo, todo, timeout(todo));
			//qDebug("res: %i", res);
			if ( res==todo ) break;
			if ( uint(time.elapsed())>3000 ) 
			{ // 3 s
				if ( res<0 ) setSystemError(i18n("Error sending data (ep=%1 res=%2)").arg(toHexLabel(ep, 2)).arg(res));
				else log(Log::LineType::Error, i18n("Timeout: only some data sent (%1/%2 bytes).").arg(size-todo).arg(size));
				return false;
			}
			if ( res==0 ) log(Log::DebugLevel::Normal, i18n("Nothing sent: retrying..."));
			if ( res>0 ) todo -= res;
			msleep(100);
	  }

*/
		
		
		
		
		//for(int i=0;i<size;i++)printf("%2X ",msg[i]&0xFF);
		//cout<<endl;

		nBytes = usb_interrupt_write(_handle,WRITE_ENDPOINT,(char*)msg,size,5000);
		if (nBytes < size )
		{
			cerr<<"Usb Error while writing to device: "<<nBytes<<" bytes"<<endl;
			cerr<<usb_strerror()<<endl;
		}
	}
	else
	{
		cerr<<"Error: Not connected"<<endl;
		return -1;
	}
	return nBytes;
}

/*Private function called by autoDetectDevice */
int Hardware::readId(void)
{
	char msg[64];
	msg[0]=CMD_READ_ID;
	int nBytes=-1;
	statusCallBack (0);
	if (_handle !=NULL)
	{
		if(writeString(msg,1)<0)
		{
			return -1;
		}
		nBytes = readString(msg,2);
			
		if (nBytes < 0 )
		{
			return -1;
		}
		else
		{
   			statusCallBack (100);
			return ((((int)msg[1])&0xFF)<<8)|(((int)msg[0])&0xFF);
		}
	}
	return nBytes;
}

int Hardware::readConfigBlock(char * msg, int address, int size, int lastblock)
{
	int nBytes = -1;
	
	if (_handle !=NULL)
	{
		if (CurrentHardware == HW_UPP)
		{
			return readCodeBlock(msg, address, size, lastblock);
		}
		else
		{
			BootloaderPackage bootloaderPackage;
			
			bootloaderPackage.fields.cmd=0x06;
			bootloaderPackage.fields.size=size;
			bootloaderPackage.fields.addrU=(unsigned char)((address>>16)&0xFF);
			bootloaderPackage.fields.addrH=(unsigned char)((address>>8)&0xFF);
			bootloaderPackage.fields.addrL=(unsigned char)(address&0xFF);
			
			nBytes = writeString(bootloaderPackage.data,5);
			
			if (nBytes < 0 )
			{
				return nBytes;
			}
			
			char tmpmsg[size+5];
			
			nBytes = readString(tmpmsg,size+5) - 5;
			
			memcpy(msg,tmpmsg+5,nBytes);
		}
		
		
		if (nBytes < 0 )
			cerr<<"Usb Error"<<endl;
		return nBytes;
	}
	else return -1;
}


/*private function to read one block of code memory*/
int Hardware::readCodeBlock(char * msg,int address,int size,int lastblock)
{
	int nBytes = -1;
	if (_handle !=NULL)
	{
		if (CurrentHardware == HW_UPP)
		{
			UppPackage uppPackage;
			
			uppPackage.fields.cmd=CMD_READ_CODE;
			uppPackage.fields.size=size;
			uppPackage.fields.addrU=(unsigned char)((address>>16)&0xFF);
			uppPackage.fields.addrH=(unsigned char)((address>>8)&0xFF);
			uppPackage.fields.addrL=(unsigned char)(address&0xFF);
			uppPackage.fields.blocktype=(unsigned char)lastblock;
			nBytes = writeString(uppPackage.data,6);
			
			if (nBytes < 0 )
			{
				return nBytes;
			}
			
			nBytes = readString(msg,size);
		}
		else
		{
			BootloaderPackage bootloaderPackage;
			
			bootloaderPackage.fields.cmd=0x01;
			bootloaderPackage.fields.size=size;
			bootloaderPackage.fields.addrU=(unsigned char)((address>>16)&0xFF);
			bootloaderPackage.fields.addrH=(unsigned char)((address>>8)&0xFF);
			bootloaderPackage.fields.addrL=(unsigned char)(address&0xFF);
			
			nBytes = writeString(bootloaderPackage.data,5);
			
			if (nBytes < 0 )
			{
				return nBytes;
			}
			
			char tmpmsg[size+5];
			
			nBytes = readString(tmpmsg,size+5) - 5;
			
			memcpy(msg,tmpmsg+5,nBytes);
		}
		
		
		if (nBytes < 0 )
			cerr<<"Usb Error"<<endl;
		return nBytes;
	}
	else return -1;
}

/*private function to write one block of code memory*/
int Hardware::writeCodeBlock(unsigned char * msg,int address,int size,int lastblock)
{
	char resp_msg[64];
	UppPackage uppPackage;
	if (_handle !=NULL)
	{
		uppPackage.fields.cmd=CMD_WRITE_CODE;
		uppPackage.fields.size=size;
		uppPackage.fields.addrU=(unsigned char)((address>>16)&0xFF);
		uppPackage.fields.addrH=(unsigned char)((address>>8)&0xFF);
		uppPackage.fields.addrL=(unsigned char)(address&0xFF);
		uppPackage.fields.blocktype=(unsigned char)lastblock;
		memcpy(uppPackage.fields.dataField,msg,size);
		if(address==0)
		{
			for(int i=0;i<size+6;i++)
				cout<<hex<<(int)uppPackage.data[i]<<" "<<dec;
			cout<<endl;
		}
		int nBytes = writeString(uppPackage.data,size+6);
		if (nBytes < 0 )
		{
			return nBytes;
		}
//		nBytes = readString(resp_msg,size+6);
		nBytes = readString(resp_msg,1);
		if (nBytes < 0 )
			cerr<<"Usb Error"<<endl;
/*		if(address==0)
		{
			for(int i=0;i<size+6;i++)
				cout<<hex<<(int)resp_msg[i]<<" "<<dec;
			cout<<endl;
		}*/
//		return 0;
		return (int)resp_msg[0];
	}
	else return -1;
	
}

/*private function to write one block of config memory*/
int Hardware::writeConfigBlock(unsigned char * msg,int address,int size,int lastblock)
{
	char resp_msg[64];
	UppPackage uppPackage;
	if (_handle !=NULL)
	{
		uppPackage.fields.cmd=CMD_WRITE_CONFIG;
		uppPackage.fields.size=size;
		uppPackage.fields.addrU=(unsigned char)((address>>16)&0xFF);
		uppPackage.fields.addrH=(unsigned char)((address>>8)&0xFF);
		uppPackage.fields.addrL=(unsigned char)(address&0xFF);
		uppPackage.fields.blocktype=(unsigned char)lastblock;
		memcpy(uppPackage.fields.dataField,msg,size);
		int nBytes = writeString(uppPackage.data,size+6);
		if (nBytes < 0 )
		{
			return nBytes;
		}
			
		nBytes = readString(resp_msg,1);
		if (nBytes < 0 )
			cerr<<"Usb Error"<<endl;
		return (int)resp_msg[0];
	}
	else return -1;
	
}


/*private function to read one block of data memory*/
int Hardware::readDataBlock(char * msg,int address,int size,int lastblock)
{
	int nBytes = -1;
	
	if (_handle !=NULL)
	{
		if (CurrentHardware == HW_UPP)
		{
			UppPackage uppPackage;
			
			uppPackage.fields.cmd=CMD_READ_DATA;
			uppPackage.fields.size=size;
			uppPackage.fields.addrU=(unsigned char)((address>>16)&0xFF);
			uppPackage.fields.addrH=(unsigned char)((address>>8)&0xFF);
			uppPackage.fields.addrL=(unsigned char)(address&0xFF);
			uppPackage.fields.blocktype=(unsigned char)lastblock;
			nBytes = writeString(uppPackage.data,6);
			
			if (nBytes < 0 )
			{
				return nBytes;
			}
			
			nBytes = readString(msg,size);
		}
		else
		{
			BootloaderPackage bootloaderPackage;
			
			bootloaderPackage.fields.cmd=0x04;
			bootloaderPackage.fields.size=size;
			bootloaderPackage.fields.addrU=(unsigned char)((address>>16)&0xFF);
			bootloaderPackage.fields.addrH=(unsigned char)((address>>8)&0xFF);
			bootloaderPackage.fields.addrL=(unsigned char)(address&0xFF);
			
			nBytes = writeString(bootloaderPackage.data,5);
			
			if (nBytes < 0 )
			{
				return nBytes;
			}
			
			char tmpmsg[size+5];
			
			nBytes = readString(tmpmsg,size+5) - 5;
			
			memcpy(msg,tmpmsg+5,nBytes);
		}
		
		
		if (nBytes < 0 )
			cerr<<"Usb Error"<<endl;
		return nBytes;
	}
	else return -1;
}

/*private function to write one block of data memory*/
int Hardware::writeDataBlock(unsigned char * msg,int address,int size,int lastblock)
{
	char resp_msg[64];
	UppPackage uppPackage;
	if (_handle !=NULL)
	{
		uppPackage.fields.cmd=CMD_WRITE_DATA;
		uppPackage.fields.size=size;
		uppPackage.fields.addrU=0;
		uppPackage.fields.addrH=(unsigned char)((address>>8)&0xFF);
		uppPackage.fields.addrL=(unsigned char)(address&0xFF);
		uppPackage.fields.blocktype=(unsigned char)lastblock;
		memcpy(uppPackage.fields.dataField,msg,size);
		int nBytes = writeString(uppPackage.data,size+6);
		if (nBytes < 0 )
		{
			return nBytes;
		}
			
		nBytes = readString(resp_msg,1);
		if (nBytes < 0 )
			cerr<<"Usb Error"<<endl;
		return (int)resp_msg[0];
	}
	else return -1;	
}

/*When Hardware is constructed, ptCallBack is initiated by a pointer
 to UppMainWindowCallBack, this function calls the callback function
 to update the progress bar*/
void Hardware::statusCallBack(int value)
{
	if(ptCallBack!=NULL)
	{
		UppMainWindowCallBack* CB=(UppMainWindowCallBack*)ptCallBack;
		CB->updateProgress(value);
	}
}


