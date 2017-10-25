#include <avr/sleep.h>
#include <avr/power.h>
#include <TM1637Display.h>


// TODO:
// - show time in the 4-digit display
// - Test battery's life.
//    - Use an Arduino Pro instead?
//    - http://arduino.stackexchange.com/questions/6/what-are-or-how-do-i-use-the-power-saving-options-of-the-arduino-to-extend-bat
//    - http://www.gammon.com.au/forum/?id=11497
//    - http://oregonembedded.com/batterycalc.htm
// - buy pipes, cube with valve, splitters, battery, pump/solenoid valve, case
// - Remove Serial references

// Hardware interrupt ports of arduino: digital pin 2 and 3
#define INTERRUPT_PIN 2
#define VALVE_PIN 4
#define TM1637_CLK_PIN 5
#define TM1637_DIO_PIN 6

#define CYCLE_TIME_SECONDS 8

const long MINUTES = 60L;
const long HOURS = 60L * MINUTES;

const long WATERING_TIME = 6L * MINUTES;
const long SLEEPING_TIME = 24L * HOURS - WATERING_TIME;

const int SLEEP_CYCLES = SLEEPING_TIME / CYCLE_TIME_SECONDS;  // Number of sleeping cycles needed before the time defined
                                                            // above elapses. Note that this does integer math.
const int WATER_CYCLES = WATERING_TIME / CYCLE_TIME_SECONDS;

// States
const int SLEEPING = 0;
const int WATERING = 1;

// DISPLAY
const uint8_t FULL = SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G;
const uint8_t HALF = SEG_C | SEG_D | SEG_E | SEG_G;
const uint8_t NONE = 0;
const uint8_t OFF[] = { NONE, NONE, NONE, NONE };
const uint8_t BATTERY_100[] = { FULL, FULL, FULL, FULL };
const uint8_t BATTERY_87[] =  { HALF, FULL, FULL, FULL };
const uint8_t BATTERY_75[] =  { NONE, FULL, FULL, FULL };
const uint8_t BATTERY_62[] =  { NONE, HALF, FULL, FULL };
const uint8_t BATTERY_50[] =  { NONE, NONE, FULL, FULL };
const uint8_t BATTERY_37[] =  { NONE, NONE, HALF, FULL };
const uint8_t BATTERY_25[] =  { NONE, NONE, NONE, FULL };
const uint8_t BATTERY_12[] =  { NONE, NONE, NONE, HALF };

// These variables are made volatile because they are changed inside an interrupt function
volatile int sleepingCycles = 0;
volatile int wateringCycles = 0;

volatile boolean externalInterruption = false;
 
int state = SLEEPING;

TM1637Display display(TM1637_CLK_PIN, TM1637_DIO_PIN);

void setup(void) {
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  pinMode(VALVE_PIN, OUTPUT);
  Serial.begin(9600);
  display.setBrightness(7);
  
  enableWatchdog();
  
  // The following saves some extra power by disabling some peripherals I am not using.
  disableExtraPeripherals();
}

void loop(void) {
  if (externalInterruption) {
    showRemainingTime();
    showBatteryStatus();
    externalInterruption = false;
  }
  attachInterrupt(getInterruptPin(), interrupt_isr, FALLING);
  sleepUntilInterruption();
  checkStates();
}

int getInterruptPin() {
  return digitalPinToInterrupt(INTERRUPT_PIN);
}

void enableWatchdog() {
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
  // If we disable the ADC we cannot check the battery life
  // ADCSRA = ADCSRA & B01111111;
  // power_adc_disable();
  
  // Disable the analog comparator by setting the ACD bit (bit 7) to one.
  ACSR = B10000000;
  //power_aca_disable();
  
  // Disable digital input buffers on all analog input pins by setting bits 0-5 to one.
  DIDR0 = DIDR0 | B00111111;

  // Disable the Serial Peripheral Interface module.
  power_spi_disable();

  // Disable the Two Wire Interface module.
  power_twi_disable();
}

void sleepUntilInterruption()   {
  // The ATmega328 has five different sleep states.
  // See the ATmega 328 datasheet for more information.
  // SLEEP_MODE_IDLE -the least power savings 
  // SLEEP_MODE_ADC
  // SLEEP_MODE_PWR_SAVE
  // SLEEP_MODE_STANDBY
  // SLEEP_MODE_PWR_DOWN -the most power savings
  
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_bod_disable();
  sleep_mode();           // Enter sleep mode.
  // After waking from watchdog interrupt the code continues to execute from this point.
}

void checkStates() {
  if (state == SLEEPING && sleepingCycles == SLEEP_CYCLES) {
    startWatering();
  } else if (state == WATERING && wateringCycles == WATER_CYCLES) {
    stopWatering();
  }
}

void startWatering() {
  digitalWrite(VALVE_PIN, HIGH);
  sleepingCycles = 0;
  state = WATERING;
}

void stopWatering() {
  digitalWrite(VALVE_PIN, LOW);
  wateringCycles = 0;
  state = SLEEPING;
}

void interrupt_isr() {
  detachInterrupt(getInterruptPin());
  externalInterruption = true;
}

ISR(WDT_vect) {
  if (state == SLEEPING) sleepingCycles++;
  else wateringCycles++;
}

const long InternalReferenceVoltage = 1099;  // Adjust this value to your board's specific internal BG voltage
 
// Code courtesy of "Coding Badly" and "Retrolefty" from the Arduino forum
// results are Vcc * 100
// So for example, 5V would be 500.
int getBandgap() {
  // REFS0 : Selects AVcc external reference
  // MUX3 MUX2 MUX1 : Selects 1.1V (VBG)
  ADMUX = bit (REFS0) | bit (MUX3) | bit (MUX2) | bit (MUX1);
  ADCSRA |= bit( ADSC );  // start conversion
  
  while (ADCSRA & bit (ADSC)) { }  // wait for conversion to complete
  
  int results = (((InternalReferenceVoltage * 1024) / ADC) + 5) / 10;
  return results;
}

void showRemainingTime() {
  long remainingCycles = SLEEP_CYCLES - sleepingCycles;
  long remainingSeconds = remainingCycles * CYCLE_TIME_SECONDS;

  int hours = remainingSeconds / HOURS;
  int minutes = (remainingSeconds / MINUTES) % MINUTES;

  int num = hours * 100 + minutes;
  
  display.showNumberDecEx(num, (0x80 >> 1));
  delay(1200);
  display.setSegments(OFF);
}

void showBatteryStatus() {
  uint8_t batteryStatus[4];

  getBatteryStatus(batteryStatus);
  
  for (int i=0; i<3; i++) {
    display.setSegments(batteryStatus);
    delay(200);
    display.setSegments(OFF);
    delay(100);
  }
}

void getBatteryStatus(uint8_t batteryStatus[]) {
  int bandgap = getBandgap();

  // Below 1.8V - cannot operate
  int normalizedMargin = ((bandgap - 180) * 100) / 320;

  if (normalizedMargin > 87)      getDisplayUint(batteryStatus, FULL, FULL, FULL, FULL);
  else if (normalizedMargin > 75) getDisplayUint(batteryStatus, HALF, FULL, FULL, FULL);
  else if (normalizedMargin > 62) getDisplayUint(batteryStatus, NONE, FULL, FULL, FULL);
  else if (normalizedMargin > 50) getDisplayUint(batteryStatus, NONE, HALF, FULL, FULL);
  else if (normalizedMargin > 37) getDisplayUint(batteryStatus, NONE, NONE, FULL, FULL);
  else if (normalizedMargin > 25) getDisplayUint(batteryStatus, NONE, NONE, HALF, FULL);
  else if (normalizedMargin > 12) getDisplayUint(batteryStatus, NONE, NONE, NONE, FULL);
  else                            getDisplayUint(batteryStatus, NONE, NONE, NONE, HALF);
}

void getDisplayUint(uint8_t displayUint[], uint8_t display0, uint8_t display1, uint8_t display2, uint8_t display3) {
  displayUint[0] = display0;
  displayUint[1] = display1;
  displayUint[2] = display2;
  displayUint[3] = display3;
}

