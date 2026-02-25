#include <linux/module.h>
#include "utility.h"

#define CHECK_PIN(pin) (pin < 0 || pin > 57 ? -1 : 0)       
#define CHOOSE_X_GPxxxX(pin) (pin < 32 ? 0 : 1)         // GPXXXx: GPSETx, GPCLRx


void SetGPIOFunction (GPIORegister* s_GPIOregister, const int GPIO_PinNum, const FSELx functionCode){
    if (!s_GPIOregister || CHECK_PIN(GPIO_PinNum) != 0)
        return;

    int GPFSELindex = (GPIO_PinNum/10);
    int startBit = (GPIO_PinNum%10)*3;
    int mask = 0b111 << startBit;
    uint32_t previousValue = s_GPIOregister->GPFSEL[GPFSELindex];

    s_GPIOregister->GPFSEL[GPFSELindex] =   (previousValue & ~mask) |       // Clear position value
                                            (functionCode << startBit);    // Insert new value
}

void SetGPIOState (GPIORegister* s_GPIOregister, const int GPIO_PinNum, const bool outputValue){
    if (!s_GPIOregister || CHECK_PIN(GPIO_PinNum) != 0)
        return; 

    uint8_t x = CHOOSE_X_GPxxxX(GPIO_PinNum);
    if (outputValue)
        s_GPIOregister->GPSET[x] = 1 << (GPIO_PinNum % 32);
    else
        s_GPIOregister->GPCLR[x] = 1 << (GPIO_PinNum % 32);  
}
