#include <stdbool.h>
#include <stdint.h>
#include <algorithm>
#include <queue>
#include <string>

// TivaC specific includes
extern "C"
{
  #include <driverlib/interrupt.h>
  #include <driverlib/sysctl.h>
  #include <driverlib/gpio.h>
  #include <driverlib/adc.h>
  #include "inc/hw_ints.h"
  #include "driverlib/ssi.h"
  #include "driverlib/pin_map.h"
  #include "driverlib/timer.h"

    #include "inc/hw_types.h"
  #include "inc/hw_gpio.h"
  #include "inc/hw_memmap.h"
}

#include "include/spi_AMIS_30543_stepper.h"

// ROS includes
#include <ros.h>

//message includes
#include "adc_joystick_msg/ADC_Joystick.h"
#include "stepper_msg/Stepper_Target.h"
#include "stepper_msg/Stepper_Status.h"

// ROS nodehandle
ros::NodeHandle nh;

//Pin definitions
#define GPIO_JOYSTICK_CLICK_PORT        GPIO_PORTA_BASE
#define GPIO_JOYSTICK_CLICK_PIN         GPIO_PIN_5

#define GPIO_JOYSTICK_X_AXIS_PORT       GPIO_PORTE_BASE
#define GPIO_JOYSTICK_X_AXIS_PIN        GPIO_PIN_2
#define ADC_CH_JOYSTICK_X_AXIS          ADC_CTL_CH1

#define GPIO_JOYSTICK_Y_AXIS_PORT       GPIO_PORTE_BASE
#define GPIO_JOYSTICK_Y_AXIS_PIN        GPIO_PIN_1
#define ADC_CH_JOYSTICK_Y_AXIS          ADC_CTL_CH2


#define GPIO_STEPPER_1_CS_PORT          GPIO_PORTD_BASE
#define GPIO_STEPPER_1_CS_PIN           GPIO_PIN_7
#define GPIO_STEPPER_1_STEP_PORT        GPIO_PORTD_BASE
#define GPIO_STEPPER_1_STEP_PIN         GPIO_PIN_6
#define GPIO_STEPPER_1_LIMIT_SW_PORT    GPIO_PORTA_BASE
#define GPIO_STEPPER_1_LIMIT_SW_PIN     GPIO_PIN_7

#define GPIO_STEPPER_2_CS_PORT          GPIO_PORTC_BASE
#define GPIO_STEPPER_2_CS_PIN           GPIO_PIN_7
#define GPIO_STEPPER_2_STEP_PORT        GPIO_PORTC_BASE
#define GPIO_STEPPER_2_STEP_PIN         GPIO_PIN_6
#define GPIO_STEPPER_2_LIMIT_SW_PORT    GPIO_PORTA_BASE
#define GPIO_STEPPER_2_LIMIT_SW_PIN     GPIO_PIN_6

#define GPIO_STEPPER_3_CS_PORT          GPIO_PORTA_BASE
#define GPIO_STEPPER_3_CS_PIN           GPIO_PIN_3
#define GPIO_STEPPER_3_STEP_PORT        GPIO_PORTA_BASE
#define GPIO_STEPPER_3_STEP_PIN         GPIO_PIN_4
#define GPIO_STEPPER_3_LIMIT_SW_PORT    GPIO_PORTA_BASE
#define GPIO_STEPPER_3_LIMIT_SW_PIN     GPIO_PIN_2

#define GPIO_STEPPER_4_CS_PORT          GPIO_PORTC_BASE
#define GPIO_STEPPER_4_CS_PIN           GPIO_PIN_5
#define GPIO_STEPPER_4_STEP_PORT        GPIO_PORTC_BASE
#define GPIO_STEPPER_4_STEP_PIN         GPIO_PIN_4
#define GPIO_STEPPER_4_LIMIT_SW_PORT    GPIO_PORTB_BASE
#define GPIO_STEPPER_4_LIMIT_SW_PIN     GPIO_PIN_3

#define GPIO_STEPPER_ALL_CLR_PORT       GPIO_PORTB_BASE
#define GPIO_STEPPER_ALL_CLR_PIN        GPIO_PIN_2

#define GPIO_STEPPER_ALL_ERR_PORT       GPIO_PORTE_BASE
#define GPIO_STEPPER_ALL_ERR_PIN        GPIO_PIN_0

#define DEFAULT_STEPPER_MAX_SPEED       2400
#define DEFAULT_STEPPER_ACCEL           100
#define DEFAULT_STEPPER_TRAVEL_LIMIT    400
#define DEFAULT_STEPPER_STEPMODE        2
#define DEFAULT_STEPPER_PH_CURRENT      800

//
// type declarations
//

struct Stepper {
  int max_speed_steps_per_second;
  int travel_limit_steps;
  int acceleration;
  int microstep_mode;
  int phase_current_ma;
  stepper_msg::Stepper_Status status;
  int target_speed;

  uint32_t TIMER_BASE;
  uint32_t CS_PORT;
  uint8_t  CS_PIN;
  uint32_t STEP_PORT;
  uint8_t  STEP_PIN;
};

//
// function declarations
//

void StepperEnable(Stepper &stepper);
void StepperDisable(Stepper &stepper);
void JoystickClicked(void);
void JoystickReleased(void);
void ReadADC(void);

void enableSysPeripherals(void);
void setupSharedStepperPins(void);
void setupJoystick(void);
void GetParamsFromROS(void);


bool SameSign(int x, int y)
{
    return (x >= 0) ^ (y < 0);
}



//globals

Stepper Tendon1Stepper;
ros::Publisher stepper_status("stepper_status", &Tendon1Stepper.status);


adc_joystick_msg::ADC_Joystick js_msg;
ros::Publisher adc_joystick("adc_joystick", &js_msg);




void JoystickClicked(void) {
    if (GPIOIntStatus(GPIO_JOYSTICK_CLICK_PORT, false) & GPIO_JOYSTICK_CLICK_PIN) {
        // PA5 was interrupt cause
        js_msg.select = true;
        adc_joystick.publish(&js_msg);
        GPIOIntRegister(GPIO_JOYSTICK_CLICK_PORT, JoystickReleased);   // Register our handler function for port A
        GPIOIntTypeSet(GPIO_JOYSTICK_CLICK_PORT, GPIO_JOYSTICK_CLICK_PIN,
            GPIO_RISING_EDGE);          // Configure PA5 for rising edge trigger
        GPIOIntClear(GPIO_JOYSTICK_CLICK_PORT, GPIO_JOYSTICK_CLICK_PIN);  // Clear interrupt flag

        if(Tendon1Stepper.status.enabled)
            StepperDisable(Tendon1Stepper);
        else
            StepperEnable(Tendon1Stepper);
    }
}

void JoystickReleased(void) {
    if (GPIOIntStatus(GPIO_JOYSTICK_CLICK_PORT, false) & GPIO_JOYSTICK_CLICK_PIN) {
        // PA5 was interrupt cause
        js_msg.select = false;
        adc_joystick.publish(&js_msg);
        GPIOIntRegister(GPIO_JOYSTICK_CLICK_PORT, JoystickClicked); // Register our handler function for port F
        GPIOIntTypeSet(GPIO_JOYSTICK_CLICK_PORT, GPIO_JOYSTICK_CLICK_PIN,
            GPIO_FALLING_EDGE);         // Configure PF4 for falling edge trigger
        GPIOIntClear(GPIO_JOYSTICK_CLICK_PORT, GPIO_JOYSTICK_CLICK_PIN);  // Clear interrupt flag
    }
}


void SW1_SW2_pressed(void){
    if (GPIOIntStatus(GPIO_PORTF_BASE, false) & GPIO_PIN_0) {

        GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_0);  // Clear interrupt flag
    }
    else if(GPIOIntStatus(GPIO_PORTF_BASE, false) & GPIO_PIN_4) {

        GPIOIntClear(GPIO_PORTF_BASE, GPIO_PIN_4);  // Clear interrupt flag

        GetParamsFromROS();


        // ADCProcessorTrigger(ADC0_BASE, 1);

        // while(ADCBusy(ADC0_BASE));
         
        // uint32_t adc_values[8] = {0,0,0,0,0,0,0,0};

        // ADCSequenceDataGet(ADC0_BASE, 1, adc_values);

        // js_msg.x_axis_zero = adc_values[0];
        // js_msg.y_axis_zero = adc_values[2];

    }
}


//
// Stepper control functions
//

void StepperEnable(Stepper &stepper){
    SPIStepperEnable(stepper.CS_PORT, stepper.CS_PIN);

    stepper.status.enabled = true;
}
void StepperDisable(Stepper &stepper){
    SPIStepperDisable(stepper.CS_PORT, stepper.CS_PIN);

    stepper.status.enabled = false;
    stepper.status.speed_steps_per_second = 0;
}

void ScheduleStepPinReset(){

    TimerEnable(TIMER1_BASE, TIMER_A); //Enable reset
}

void Stepper1StepPinSet(void)
{
    // Clear the timer interrupt
    TimerIntClear(TIMER2_BASE, TIMER_TIMA_TIMEOUT);

    if(Tendon1Stepper.status.direction_forward)
    {
      if(Tendon1Stepper.status.position_steps < Tendon1Stepper.travel_limit_steps)
      {
        Tendon1Stepper.status.position_steps++;
      }
      else
      {
        Tendon1Stepper.status.speed_steps_per_second = 0;
        return;
      }
    }
    else
    {
      if(Tendon1Stepper.status.position_steps > 0)
      {
        Tendon1Stepper.status.position_steps--;
      }
      else
      {
        Tendon1Stepper.status.speed_steps_per_second = 0;
        return;
      }
    }

    GPIOPinWrite(GPIO_STEPPER_1_STEP_PORT, GPIO_STEPPER_1_STEP_PIN, 255);


    ScheduleStepPinReset();
}

void StepPinReset(void)
{
    TimerIntClear(TIMER1_BASE, TIMER_TIMA_TIMEOUT);
    GPIOPinWrite(GPIO_STEPPER_1_STEP_PORT, GPIO_STEPPER_1_STEP_PIN, 0);
}

void StepperError(void){
    if (GPIOIntStatus(GPIO_STEPPER_ALL_ERR_PORT, false) & GPIO_STEPPER_ALL_ERR_PIN) {

        GPIOIntClear(GPIO_STEPPER_ALL_ERR_PORT, GPIO_STEPPER_ALL_ERR_PIN);  // Clear interrupt flag
        
        //StepperDisable();

        // uint32_t sr0_stat = SPIReadByte(SR0);
        // uint32_t sr1_stat = SPIReadByte(SR1);
        // uint32_t sr2_stat = SPIReadByte(SR2);

        // std::string errormsg;

        // if(sr0_stat & 0b01000000)
        //     errormsg.append("Temp Warning");
        // if(sr2_stat & 0b00000100)
        //     errormsg.append("Temp Shutdown");
        // if(sr0_stat & 0b00010000)
        //     errormsg.append("Watchdog ");
        // if(sr0_stat & 0b00001100)
        //     errormsg.append("Open coil ");
        // if((sr1_stat & 0b01111000 ) || (sr2_stat & 0b01111000 ))
        //     errormsg.append("Overcurrent ");

        // Tendon1Stepper.status.errors = errormsg.c_str();

        // stepper_status.publish(&Tendon1Stepper.status);
    }
}



void ReadADC(void){

  ADCIntClear(ADC0_BASE,1);

  uint32_t adc_values[8] = {0,0,0,0,0,0,0,0};

  ADCSequenceDataGet(ADC0_BASE, 1, adc_values);

  if( abs(adc_values[0] - js_msg.x_axis_raw) > 100 || abs(adc_values[2] - js_msg.y_axis_raw) > 100)
  {
      js_msg.x_axis_raw = adc_values[0];
      js_msg.y_axis_raw = adc_values[2];

    //   if(abs((js_msg.x_axis_raw-js_msg.x_axis_zero)<100))
    //     js_msg.x_axis_raw = js_msg.x_axis_zero;

    // if(abs((js_msg.y_axis_raw-js_msg.y_axis_zero)<100))
    //     js_msg.y_axis_raw = js_msg.y_axis_zero;

      adc_joystick.publish(&js_msg);



      Tendon1Stepper.target_speed = (Tendon1Stepper.max_speed_steps_per_second*(js_msg.x_axis_raw-js_msg.x_axis_zero))/2048;
  }
}

#define PERIODIC_UPDATE_RATE_HZ 32

void PeriodicUpdate(void){

    TimerIntClear(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    if(Tendon1Stepper.status.enabled){

      if(Tendon1Stepper.status.speed_steps_per_second != Tendon1Stepper.target_speed)
      {
          int original_speed = Tendon1Stepper.status.speed_steps_per_second;

          if(Tendon1Stepper.status.speed_steps_per_second < Tendon1Stepper.target_speed)
          {
            if(Tendon1Stepper.target_speed - Tendon1Stepper.status.speed_steps_per_second  < Tendon1Stepper.acceleration)
              Tendon1Stepper.status.speed_steps_per_second = Tendon1Stepper.target_speed;
            else
              Tendon1Stepper.status.speed_steps_per_second += Tendon1Stepper.acceleration;

          }
          else{
            if(Tendon1Stepper.status.speed_steps_per_second - Tendon1Stepper.target_speed < Tendon1Stepper.acceleration)
              Tendon1Stepper.status.speed_steps_per_second = Tendon1Stepper.target_speed;
            else
              Tendon1Stepper.status.speed_steps_per_second -= Tendon1Stepper.acceleration;
          }

          if(!SameSign(original_speed, Tendon1Stepper.status.speed_steps_per_second) || original_speed == 0){
              if(Tendon1Stepper.status.speed_steps_per_second>=0){
                  SetStepperDirection(GPIO_STEPPER_1_CS_PORT, GPIO_STEPPER_1_CS_PIN, true);
                  Tendon1Stepper.status.direction_forward = true;
              }else{
                  SetStepperDirection(GPIO_STEPPER_1_CS_PORT, GPIO_STEPPER_1_CS_PIN, false);
                  Tendon1Stepper.status.direction_forward = false;
              }
          }

          TimerLoadSet(TIMER2_BASE, TIMER_A, std::min((SysCtlClockGet() / abs(Tendon1Stepper.status.speed_steps_per_second)) -1, SysCtlClockGet()/PERIODIC_UPDATE_RATE_HZ));
      }

      stepper_status.publish(&Tendon1Stepper.status);
    }


    nh.spinOnce();

    ADCProcessorTrigger(ADC0_BASE, 1);
}

int main(void)
{
    // TivaC application specific code
    MAP_FPUEnable();
    MAP_FPULazyStackingEnable();
    // TivaC system clock configuration. Set to 80MHz.
    MAP_SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);

    IntMasterDisable();

    enableSysPeripherals();

    setupJoystick();
    setupSharedStepperPins();


    Tendon1Stepper.CS_PIN = GPIO_STEPPER_1_CS_PIN;
    Tendon1Stepper.CS_PORT = GPIO_STEPPER_1_CS_PORT;
    Tendon1Stepper.TIMER_BASE = TIMER2_BASE;

    GPIOPinTypeGPIOOutput(GPIO_STEPPER_1_STEP_PORT, GPIO_STEPPER_1_STEP_PIN);
    GPIOPadConfigSet(GPIO_STEPPER_1_STEP_PORT, GPIO_STEPPER_1_STEP_PIN, GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD);

    GPIOPinTypeGPIOOutput(GPIO_STEPPER_1_CS_PORT, GPIO_STEPPER_1_CS_PIN);
    GPIOPadConfigSet(GPIO_STEPPER_1_CS_PORT, GPIO_STEPPER_1_CS_PIN,GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD);

    

    GPIOPinWrite(GPIO_STEPPER_1_CS_PORT, GPIO_STEPPER_1_CS_PIN, 255); //Pull CS HIGH

    GPIOPinWrite(GPIO_STEPPER_ALL_CLR_PORT, GPIO_STEPPER_ALL_CLR_PIN, 255); //Pull CLR HIGH
    SysCtlDelay(1000);
    GPIOPinWrite(GPIO_STEPPER_ALL_CLR_PORT, GPIO_STEPPER_ALL_CLR_PIN, 0); //Pull CLR LOW



    nh.initNode();
     
    nh.advertise(adc_joystick);
    nh.advertise(stepper_status);

    while(!nh.connected()) {nh.spinOnce();}

    GetParamsFromROS();

    ClearStepperRegisters(GPIO_STEPPER_1_CS_PORT, GPIO_STEPPER_1_CS_PIN);
    SetStepperCurrent(GPIO_STEPPER_1_CS_PORT, GPIO_STEPPER_1_CS_PIN, Tendon1Stepper.phase_current_ma);
    SetStepperStepMode(GPIO_STEPPER_1_CS_PORT, GPIO_STEPPER_1_CS_PIN, Tendon1Stepper.microstep_mode);
    SetStepperDirection(GPIO_STEPPER_1_CS_PORT, GPIO_STEPPER_1_CS_PIN, true);


    Tendon1Stepper.status.position_steps = 0;
    Tendon1Stepper.status.speed_steps_per_second = 1;
    Tendon1Stepper.status.enabled = false;
    Tendon1Stepper.target_speed   = 2000;




    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER1);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER2);

    TimerDisable(TIMER0_BASE, TIMER_A);
    TimerDisable(TIMER1_BASE, TIMER_A);
    TimerDisable(TIMER2_BASE, TIMER_A);

    TimerConfigure(TIMER0_BASE, TIMER_CFG_PERIODIC); //Periodic tasks
    TimerConfigure(TIMER1_BASE, TIMER_CFG_ONE_SHOT); //stepper pin reset
    TimerConfigure(TIMER2_BASE, TIMER_CFG_PERIODIC); //stepper1 set

    TimerLoadSet(TIMER0_BASE, TIMER_A, (SysCtlClockGet() / PERIODIC_UPDATE_RATE_HZ)-1);

    uint32_t StepPinResetDelay_us = 2; //microseconds
    TimerLoadSet(TIMER1_BASE, TIMER_A, (StepPinResetDelay_us*(SysCtlClockGet()/1000000))-1);

    //Tendon1Stepper.status.speed_steps_per_second = 3200;
    TimerLoadSet(TIMER2_BASE, TIMER_A, (SysCtlClockGet() / Tendon1Stepper.status.speed_steps_per_second) -1);
    TimerUpdateMode(TIMER2_BASE, TIMER_A, TIMER_UP_LOAD_TIMEOUT);

    TimerIntRegister(TIMER0_BASE, TIMER_A, PeriodicUpdate);
    TimerIntRegister(TIMER1_BASE, TIMER_A, StepPinReset);
    TimerIntRegister(TIMER2_BASE, TIMER_A, Stepper1StepPinSet);
    
    
    IntEnable(INT_TIMER0A);
    TimerIntEnable(TIMER0_BASE, TIMER_TIMA_TIMEOUT);

    IntEnable(INT_TIMER1A);
    TimerIntEnable(TIMER1_BASE, TIMER_TIMA_TIMEOUT);

    IntEnable(INT_TIMER2A);
    TimerIntEnable(TIMER2_BASE, TIMER_TIMA_TIMEOUT);

    IntPrioritySet(INT_TIMER1A, 0b00100000); //Motor reset
    IntPrioritySet(INT_TIMER0A, 0b01000000); //Motor update
    IntPrioritySet(INT_TIMER2A, 0b00000000); //Motor set

    TimerEnable(TIMER0_BASE, TIMER_A);
    TimerEnable(TIMER1_BASE, TIMER_A);
    TimerEnable(TIMER2_BASE, TIMER_A);

    IntMasterEnable();

    while(1)
    {
    }
}

//
// Initialization functions
//

void enableSysPeripherals(void)
{
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOC);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_SSI2);
  SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
  
  while(!SysCtlPeripheralReady(SYSCTL_PERIPH_ADC0))
  {
  }

  
  HWREG(GPIO_PORTD_BASE+GPIO_O_LOCK) = GPIO_LOCK_KEY; //Unlock pin D7 for use
  HWREG(GPIO_PORTD_BASE+GPIO_O_CR) |= GPIO_PIN_7;
}

void setupJoystick(void)
{
  //Setup Joystick click
  GPIOPinTypeGPIOInput(GPIO_JOYSTICK_CLICK_PORT, GPIO_JOYSTICK_CLICK_PIN);
  GPIOPadConfigSet(GPIO_JOYSTICK_CLICK_PORT, GPIO_JOYSTICK_CLICK_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);  // Enable weak pullup resistor

  //Joystick click Interrupt setup
  GPIOIntDisable(GPIO_JOYSTICK_CLICK_PORT, GPIO_JOYSTICK_CLICK_PIN);        // Disable interrupt for PA5 (in case it was enabled)
  GPIOIntClear(GPIO_JOYSTICK_CLICK_PORT, GPIO_JOYSTICK_CLICK_PIN);      // Clear pending interrupts for PA5
  GPIOIntRegister(GPIO_JOYSTICK_CLICK_PORT, JoystickClicked);     // Register our handler function for port A
  GPIOIntTypeSet(GPIO_JOYSTICK_CLICK_PORT, GPIO_JOYSTICK_CLICK_PIN, GPIO_FALLING_EDGE);             // Configure PF4 for falling edge trigger
  GPIOIntEnable(GPIO_JOYSTICK_CLICK_PORT, GPIO_JOYSTICK_CLICK_PIN);     // Enable interrupt for PF4


  //Setup joystic axis pins
  GPIOPinTypeADC(GPIO_JOYSTICK_X_AXIS_PORT, GPIO_JOYSTICK_X_AXIS_PIN);
  GPIOPinTypeADC(GPIO_JOYSTICK_Y_AXIS_PORT, GPIO_JOYSTICK_Y_AXIS_PIN);  

  ADCSequenceDisable( ADC0_BASE, 0 );
  ADCSequenceDisable( ADC0_BASE, 1 );
  ADCSequenceDisable( ADC0_BASE, 2 );
  ADCSequenceDisable( ADC0_BASE, 3 );

  ADCSequenceConfigure(ADC0_BASE, 1, ADC_TRIGGER_PROCESSOR, 0);
  ADCSequenceStepConfigure(ADC0_BASE, 1, 0, ADC_CH_JOYSTICK_X_AXIS);
  ADCSequenceStepConfigure(ADC0_BASE, 1, 1, ADC_CH_JOYSTICK_X_AXIS);
  ADCSequenceStepConfigure(ADC0_BASE, 1, 2, ADC_CH_JOYSTICK_Y_AXIS);
  ADCSequenceStepConfigure(ADC0_BASE,1,3, ADC_CTL_END | ADC_CH_JOYSTICK_Y_AXIS | ADC_CTL_IE);
  ADCSequenceEnable(ADC0_BASE, 1);

  ADCIntRegister(ADC0_BASE, 1, ReadADC);
  ADCIntEnable(ADC0_BASE, 1);


  js_msg.x_axis_zero = 2070;
  js_msg.y_axis_zero = 2066;
}

void setupSharedStepperPins(void)
{

    GPIOPinTypeGPIOInput(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_4);
    GPIOPadConfigSet(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_4, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);  // Enable weak pullup resistor
    GPIOIntRegister(GPIO_PORTF_BASE, SW1_SW2_pressed);     // Register our handler function for port A
    GPIOIntTypeSet(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_4, GPIO_RISING_EDGE);         // Configure PF4 for falling edge trigger
    GPIOIntEnable(GPIO_PORTF_BASE, GPIO_PIN_0 | GPIO_PIN_4);     // Enable interrupt for PF4

    GPIOPinTypeGPIOInput(GPIO_STEPPER_ALL_ERR_PORT, GPIO_STEPPER_ALL_ERR_PIN);
    GPIOPadConfigSet(GPIO_STEPPER_ALL_ERR_PORT, GPIO_STEPPER_ALL_ERR_PIN, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD_WPU);  // Enable weak pullup resistor
    GPIOIntRegister(GPIO_STEPPER_ALL_ERR_PORT, StepperError);     // Register our handler function for port A
    GPIOIntTypeSet(GPIO_STEPPER_ALL_ERR_PORT, GPIO_STEPPER_ALL_ERR_PIN, GPIO_FALLING_EDGE);         // Configure PF4 for falling edge trigger
    GPIOIntEnable(GPIO_STEPPER_ALL_ERR_PORT, GPIO_STEPPER_ALL_ERR_PIN);     // Enable interrupt for PF4

    GPIOPinTypeGPIOOutput(GPIO_STEPPER_ALL_CLR_PORT, GPIO_STEPPER_ALL_CLR_PIN);
    GPIOPadConfigSet(GPIO_STEPPER_ALL_CLR_PORT, GPIO_STEPPER_ALL_CLR_PIN,GPIO_STRENGTH_2MA,GPIO_PIN_TYPE_STD);

    SetupSPIStepper();
}

void GetParamsFromROS(void){
  if(!nh.getParam("/TUhand/Tendon1Stepper/max_speed_steps_per_second", &Tendon1Stepper.max_speed_steps_per_second, 1))
    Tendon1Stepper.max_speed_steps_per_second = DEFAULT_STEPPER_MAX_SPEED;
  if(!nh.getParam("/TUhand/Tendon1Stepper/travel_limit_steps", &Tendon1Stepper.travel_limit_steps, 1))
    Tendon1Stepper.travel_limit_steps = DEFAULT_STEPPER_TRAVEL_LIMIT;
  if(!nh.getParam("/TUhand/Tendon1Stepper/acceleration", &Tendon1Stepper.acceleration, 1))
    Tendon1Stepper.acceleration = DEFAULT_STEPPER_ACCEL;
  if(!nh.getParam("/TUhand/Tendon1Stepper/microstep_mode", &Tendon1Stepper.microstep_mode, 1))
    Tendon1Stepper.microstep_mode = DEFAULT_STEPPER_STEPMODE;
  if(!nh.getParam("/TUhand/Tendon1Stepper/phase_current_ma", &Tendon1Stepper.phase_current_ma, 1))
    Tendon1Stepper.phase_current_ma = DEFAULT_STEPPER_PH_CURRENT;
}