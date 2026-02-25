#ifndef __LED_DD__
#define __LED_DD__

#include <linux/types.h>

#define NUM_SELECT_REG   6
#define NUM_SET_CLR_REG  2
#define GPIO_BASE_ADDR   0XFE200000

//Datasheet typedef
typedef enum {
    INPUT,      
    OUTPUT,     
    ALT5,       
    ALT4,       
    ALT0,       
    ALT1,       
    ALT2,       
    ALT3,         
} FSELx;


typedef struct{
    uint32_t GPFSEL[NUM_SELECT_REG];
    uint32_t Reserved0;
    uint32_t GPSET[NUM_SET_CLR_REG];
    uint32_t Reserved1;
    uint32_t GPCLR[NUM_SET_CLR_REG];
} GPIORegister; 

void SetGPIOFunction    (GPIORegister* s_GPIOregister, const int GPIO_PinNum, const FSELx functionCode);

void SetGPIOState   (GPIORegister* s_GPIOregister, const int GPIO_PinNum, const bool outputValue);


#endif
