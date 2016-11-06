#include <avr/sleep.h>
// This library contains functions to set various low-power 
// states for the ATmega328

// TODO:
// - Test battery's life.
// - Open/close watter with servo or relay
// - Activate a led while watering
// - Show remaining time to start watering (using a coded signal through a led)

const int TRIGGER_PIN = 2;
const int PULSE_PIN = 3;
const int LED_PIN = 13;

const int MINUTES = 60;
const int HOURS = 60 * MINUTES;

const int WAITING_TIME = 0.5 * MINUTES;   // Waiting time between waking and doing tasks.
const int WAIT_CYCLES = WAITING_TIME/8; // Number of waiting cycles needed before the time defined
                                        // above elapses. Note that this does integer math.
const int WATERING_TIME = 16;
const int WATER_CYCLES = WATERING_TIME/8;

// States
const int WAITING = 0;
const int WATERING = 1;

// These variables are made volatile because they are changed inside an interrupt function
volatile int waitingCycles = 0;
volatile int wateringCycles = 0;

int currentHours = 0;
int state = WAITING;

void setup(void) {
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(9600);
  
  watchdogOn(); // Turn on the watch dog timer.
  
  // The following saves some extra power by disabling some peripherals I am not using.
  disableExtraPeripherals();
}

void loop(void) {
  sleepUntilInterruption();
  checkStates();
}

void watchdogOn() {
  // Clear the reset flag, the WDRF bit (bit 3) of MCUSR.
  MCUSR = MCUSR & B11110111;
    
  // Set the WDCE bit (bit 4) and the WDE bit (bit 3) of WDTCSR. The WDCE bit must be set in order
  // to change WDE or the watchdog prescalers. Setting the WDCE bit will allow updtaes to the 
  // prescalers and WDE for 4 clock cycles then it will be reset by hardware.
  WDTCSR = WDTCSR | B00011000; 
  
  // Set the watchdog timeout prescaler value to 1024 K 
  // which will yeild a time-out interval of about 8.0 s.
  WDTCSR = B00100001;
  
  // Enable the watchdog timer interupt.
  WDTCSR = WDTCSR | B01000000;
  MCUSR = MCUSR & B11110111;
}

void disableExtraPeripherals() {
  // Disable the ADC by setting the ADEN bit (bit 7) to zero.
  ADCSRA = ADCSRA & B01111111;
  
  // Disable the analog comparator by setting the ACD bit (bit 7) to one.
  ACSR = B10000000;
  
  // Disable digital input buffers on all analog input pins by setting bits 0-5 to one.
  DIDR0 = DIDR0 | B00111111;
}

void sleepUntilInterruption()   {
  // The ATmega328 has five different sleep states.
  // See the ATmega 328 datasheet for more information.
  // SLEEP_MODE_IDLE -the least power savings 
  // SLEEP_MODE_ADC
  // SLEEP_MODE_PWR_SAVE
  // SLEEP_MODE_STANDBY
  // SLEEP_MODE_PWR_DOWN -the most power savings
  // I am using the deepest sleep mode from which a watchdog timer interrupt can wake the ATMega328
  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();   // Enable sleep mode.
  sleep_mode();     // Enter sleep mode.
  // After waking from watchdog interrupt the code continues to execute from this point.
  
  sleep_disable();  // Disable sleep mode after waking.
}

void checkStates() {
  if (state == WAITING && waitingCycles == WAIT_CYCLES) {
    startWatering();
  } else if (state == WATERING && wateringCycles == WATER_CYCLES) {
    stopWatering();
  }
}

void startWatering() {
    Serial.println("Start watering!");
    Serial.println(waitingCycles);
    waitingCycles = 0;
    state = WATERING;
    delay(1000);
}

void stopWatering() {
    Serial.println("Stop watering!");
    Serial.println(wateringCycles);
    wateringCycles = 0;
    state = WAITING;
    delay(1000);
}

ISR(WDT_vect) {
  if (state == WAITING) waitingCycles++;
  else wateringCycles++;
}