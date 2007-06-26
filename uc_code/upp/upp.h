/*********************************************************************
 *
 *                UsbPicProg v0.1
 *                Frans Schreuder 16-06-2007
 ********************************************************************/

#ifndef UPP_H
#define UPP_H

/** P U B L I C  P R O T O T Y P E S *****************************************/
void UserInit(void);
void ProcessIO(void);

extern volatile near union {
  struct
  {
    unsigned Leds:3;
    unsigned unused:5;
  };
}PORTCbits;

#endif //UPP_H
