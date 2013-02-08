// Daniel Gilbert
// loglow@gmail.com
// copyright 2013



#include "PITimer.h"
#include <mk20dx128.h>
#include <stdint.h>
#include <math.h>



// ------------------------------------------------------------
// these are the pre-defined timer objects corresponding
// to the 4 internal Periodic Interrupt Timers (PITs).
// PIT3 is disabled because it conflicts with tone()
// ------------------------------------------------------------
PITimer PITimer0(0);
PITimer PITimer1(1);
PITimer PITimer2(2);
//PITimer PITimer3(3);



// ------------------------------------------------------------
// these are the ISRs (Interrupt Service Routines)
// that get called by each timer when it fires.
// they're defined here a) so that they can auto-clear
// themselves and b) so the user can specify a custom
// ISR of their own, and even reassign it as needed
// ------------------------------------------------------------
void pit0_isr() { PITimer0.clear(); PITimer0.myISR(); }
void pit1_isr() { PITimer1.clear(); PITimer1.myISR(); }
void pit2_isr() { PITimer2.clear(); PITimer2.myISR(); }
//void pit3_isr() { PITimer3.clear(); PITimer3.myISR(); }



// ------------------------------------------------------------
// the actual period of a timer is stored as a quantity
// of bus clock cycles, and that's what "value" represents.
// the PIT_LDVALn register is used to store this value.
// all this function does is perform the register write.
// it needs to be called whenever myValue is changed
// ------------------------------------------------------------
void PITimer::writeValue() {
  if      (myID == 0) PIT_LDVAL0 = myValue;
  else if (myID == 1) PIT_LDVAL1 = myValue;
  else if (myID == 2) PIT_LDVAL2 = myValue;
  else if (myID == 3) PIT_LDVAL3 = myValue;
}



// ------------------------------------------------------------
// very simply rounds a float to its nearest integer value
// ------------------------------------------------------------
float PITimer::round(float value) {
  return floor(value + 0.5);
}



// ------------------------------------------------------------
// initializer for the PITimer class, mostly used to set defaults.
// F_BUS is equal to the frequency of the bus clock. we're also
// enabling the overall clock access to the PIT module, both via the
// SIM (System Integration Module) and the PIT's own MCR (Module
// Control Register). enabling these global controls for each timer
// isn't necessary, but it doesn't do any harm either.
// ------------------------------------------------------------
PITimer::PITimer(uint8_t timerID) : myID(timerID), isRunning(false) {
  SIM_SCGC6 |= SIM_SCGC6_PIT;
  PIT_MCR = 0;
  value(F_BUS);
}



// ------------------------------------------------------------
// this version of value() (with an argument) is used to set the
// timer value directly, either by the user, or by one of the other
// (more useful) set functions. only useful externally if the timer
// needs to perform some kind of bus-clock-specific function.
// includes some basic range validation. timer behavior seems to
// become unstable at very low values or at 2^32-1 (UINT32_MAX)
// ------------------------------------------------------------
void PITimer::value(uint32_t newValue) {
  if      (newValue == UINT32_MAX) newValue = UINT32_MAX - 1;
  else if (newValue < valueMin)    newValue = valueMin;
  myValue = newValue;
  writeValue();
}



// ------------------------------------------------------------
// this version of period() (with an argument) is used to set the
// period of the timer in terms of units of time (seconds).
// for 48 MHz bus, range is about 14 ns (0.000014) to 89 s (89.0)
// ------------------------------------------------------------
void PITimer::period(float newPeriod) {
  uint32_t newValue = round(F_BUS * newPeriod) - 1;
  value(newValue);
}



// ------------------------------------------------------------
// this version of frequency() (with an argument) is used to set the
// frequency of the timer in terms of hertz (1 cycle per second).
// for 48 MHz bus, range is about 12 mHz (0.012) to 75 kHz (75000)
// ------------------------------------------------------------
void PITimer::frequency(float newFrequency) {
  uint32_t newValue = round(F_BUS / newFrequency) - 1;
  value(newValue);
}



// ------------------------------------------------------------
// get the current value of the timer (in bus clock cycles)
// ------------------------------------------------------------
uint32_t PITimer::value() {
  return myValue;
}



// ------------------------------------------------------------
// get the current period of the timer (in seconds)
// ------------------------------------------------------------
float PITimer::period() {
  return (value() + 1) / float(F_BUS);
}



// ------------------------------------------------------------
// get the current frequency of the timer (in hertz)
// ------------------------------------------------------------
float PITimer::frequency() {
  return float(F_BUS) / (value() + 1);
}



// ------------------------------------------------------------
// this function initializes and starts the timer, using the specified
// function as a callback. must be passed the name of a function taking
// no arguments and returning void. make sure this function can
// complete within the time allowed (aka less than its period)
// ------------------------------------------------------------
void PITimer::start(void (*newISR)()) {
  myISR = newISR;
  isRunning = true;
  if      (myID == 0) { PIT_TCTRL0 = 3; NVIC_ENABLE_IRQ(IRQ_PIT_CH0); }
  else if (myID == 1) { PIT_TCTRL1 = 3; NVIC_ENABLE_IRQ(IRQ_PIT_CH1); }
  else if (myID == 2) { PIT_TCTRL2 = 3; NVIC_ENABLE_IRQ(IRQ_PIT_CH2); }
  else if (myID == 3) { PIT_TCTRL3 = 3; NVIC_ENABLE_IRQ(IRQ_PIT_CH3); }
}



// ------------------------------------------------------------
// clears the timer flag, allowing further interrupts to occur.
// this is handled automatically by the PIT ISR wrappers above.
// ------------------------------------------------------------
void PITimer::clear() {
  if      (myID == 0) PIT_TFLG0 = 1;
  else if (myID == 1) PIT_TFLG1 = 1;
  else if (myID == 2) PIT_TFLG2 = 1;
  else if (myID == 3) PIT_TFLG3 = 1;
}



// ------------------------------------------------------------
// calling this function causes the current countdown cycle of the
// timer to reset, essentially delaying the firing of the callback
// until another full period of the timer's cycle has elapsed.
// ------------------------------------------------------------
void PITimer::reset() {
  if      (myID == 0) { PIT_TCTRL0 = 1; PIT_TCTRL0 = 3; }
  else if (myID == 1) { PIT_TCTRL1 = 1; PIT_TCTRL1 = 3; }
  else if (myID == 2) { PIT_TCTRL2 = 1; PIT_TCTRL2 = 3; }
  else if (myID == 3) { PIT_TCTRL3 = 1; PIT_TCTRL3 = 3; }
}



// ------------------------------------------------------------
// stops the timer and disables its interrupts
// ------------------------------------------------------------
void PITimer::stop() {
  isRunning = false;
  if      (myID == 0) { NVIC_DISABLE_IRQ(IRQ_PIT_CH0); PIT_TCTRL0 = 0; }
  else if (myID == 1) { NVIC_DISABLE_IRQ(IRQ_PIT_CH1); PIT_TCTRL1 = 0; }
  else if (myID == 2) { NVIC_DISABLE_IRQ(IRQ_PIT_CH2); PIT_TCTRL2 = 0; }
  else if (myID == 3) { NVIC_DISABLE_IRQ(IRQ_PIT_CH3); PIT_TCTRL3 = 0; }
}



// ------------------------------------------------------------
// check to see if the timer is currently active
// ------------------------------------------------------------
bool PITimer::running() {
  return isRunning;
}



// ------------------------------------------------------------
// returns the number of bus clock cycles
// remaining until the timer will fire next.
// this number decreases until it reaches 0
// ------------------------------------------------------------
uint32_t PITimer::current() {
  uint32_t currentValue = 0;
  if      (myID == 0) currentValue = PIT_CVAL0;
  else if (myID == 1) currentValue = PIT_CVAL1;
  else if (myID == 2) currentValue = PIT_CVAL2;
  else if (myID == 3) currentValue = PIT_CVAL3;
  return currentValue;
}



// ------------------------------------------------------------
// returns the amount of time (in seconds)
// until the timer will fire next
// ------------------------------------------------------------
float PITimer::remains() {
  return current() / float(F_BUS);
}



// EOF