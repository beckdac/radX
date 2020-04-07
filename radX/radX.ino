#include <Adafruit_MCP23017.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <si5351.h>
#include <Wire.h>

#undef DEBUG
#define DEBUG

// i2c parameters
#define I2C_FREQ 400000

// OLED
#define OLED_WIDTH  128
#define OLED_HEIGHT  64
#define OLED_RESET -1
Adafruit_SSD1306 *oled;

// MCP23017
Adafruit_MCP23017 mcp0; //, mcp1;
#define MCP0_INTERRUPT_PIN 1
volatile bool mcp0_interrupt = false;
void mcp0_interrupt_callback(void) {
	mcp0_interrupt = true;
	Serial.println("interrupt");
}

// si3531
Si5351 si5351;

typedef struct si_clock {
	uint32_t freq;
	uint32_t freq_max;
} si_clock_t;

#define SI_CLOCKS 3
#define FREQ_MAX 200000000
#define FREQ_DEFAULT 14000000
// setup the clock data structures
si_clock_t si_clocks[SI_CLOCKS] = {
	{ FREQ_DEFAULT, FREQ_MAX },	// clk0
	{ 0, FREQ_MAX },	// clk1
	{ 0, FREQ_MAX }		// clk2
};
enum Si_clock { clk0 = 0, clk1 = 1, clk2 = 2 };

// encoders and switches
Si_clock current_clk = clk0;
uint8_t freq_digit = 0;		// what digit is being manipulated by the tuning encoder
bool freq_hold = false;		// is the tuning encoder locked out of frequency changes

typedef struct encoder {
	Adafruit_MCP23017 *mcpX;	// which mcp chip is this encoder on
	uint8_t pinA, pinB, pinSW;	// which pins are A, B, and SW
	boolean A, B, sw;		// last value of A and B and SW
	int16_t value;			// value of encoder
	int16_t min_value;		// min value encoder can have
	int16_t max_value;		// max value encoder can have
	bool (*encoder_callback)(struct encoder *encoder, int8_t dir);
	bool (*encoder_sw_callback)(struct encoder *encoder);
} encoder_t;

// called when the frequency tuning encoder changes value
bool encoder_freq_callback(struct encoder *encoder, int8_t dir) {
	if (freq_hold)
		return true;
	uint32_t delta = pow(10, freq_digit);
	if (dir > 0) {	// +
		if (si_clocks[current_clk].freq + delta > si_clocks[current_clk].freq_max)
			si_clocks[current_clk].freq = si_clocks[current_clk].freq_max;
		else
			si_clocks[current_clk].freq += pow(10, freq_digit);
	} else {	// -
		if (si_clocks[current_clk].freq - delta < 0)
			si_clocks[current_clk].freq = 0;
		else
			si_clocks[current_clk].freq -= delta;
	}
	if (current_clk == clk0)
		si5351.set_freq(si_clocks[current_clk].freq * SI5351_FREQ_MULT, SI5351_CLK0);
	// si5351.set_freq(si_clocks[current_clk].freq * SI5351_FREQ_MULT, (current_clk == clk0 ? SI5351_CLK0 : (current_clk == clk1 ? SI5351_CLK1 : SI5351_CLK2)));
#ifdef DEBUG
	Serial.print("clock ");
	Serial.print(current_clk, DEC);
	Serial.print(" frequency: ");
	Serial.println(si_clocks[current_clk].freq, DEC);
#endif
	oled_update_display();
	return true;
}

bool encoder_select_callback(struct encoder *encoder, int8_t dir) {
	if (dir > 0) {	// +
		if (freq_digit < 9)
			freq_digit++;
	} else {	// -
		if (freq_digit > 0)
			freq_digit--;
	}
#ifdef DEBUG
	Serial.print("freq_digit: ");
	Serial.println(freq_digit, DEC);
#endif
	oled_update_display();
}

bool encoder_freq_sw_callback(struct encoder *encoder) {
	if (encoder->sw) {
		switch (current_clk) {
			case clk0:
				current_clk = clk1;
				break;
			case clk1:
				current_clk = clk2;
				break;
			case clk2:
				current_clk = clk0;
				break;
			default:
				current_clk = clk0;
		};
#ifdef DEBUG
		Serial.print("current clock changed to ");
		Serial.println(current_clk, DEC);
#endif
		oled_update_display();
	}
}

bool encoder_select_sw_callback(struct encoder *encoder) {
	if (encoder->sw) {
		freq_hold = !freq_hold;
		oled_update_display();
#ifdef DEBUG
		Serial.print("freq_hold = ");
		Serial.println(freq_hold, DEC);
#endif
	}
}

#define ENCODERS 2
// setup the encoder data structures
encoder_t encoders[ENCODERS] = {
	{ &mcp0, 0, 1, 2,    true, true, false, 63, 0, 127, &encoder_freq_callback, &encoder_freq_sw_callback },
	{ &mcp0, 3, 4, 5,    true, true, false, 63, 0, 127, &encoder_select_callback, &encoder_select_sw_callback },
/*
	{ &mcp0, 8, 9, 10,   true, true, false, 63, 0, 127, 72 },
	{ &mcp0, 11, 12, 13, true, true, false, 63, 0, 127, 73 },
	{ &mcp1, 0, 1, 2,    true, true, false, 63, 0, 127, 74 },
	{ &mcp1, 3, 4, 5,    true, true, false, 63, 0, 127, 75 },
	{ &mcp1, 8, 9, 10,   true, true, false, 63, 0, 127, 76 },
	{ &mcp1, 11, 12, 13, true, true, false, 63, 0, 127, 77 }
*/
};

typedef struct switch_ {
	Adafruit_MCP23017 *mcpX;	// which mcp is this switch on, NULL if teensy pin
	uint8_t pin;			// pin #
	boolean sw;			// state of switch
} switch_t;

#define SWITCHES 1
switch_t switches[SWITCHES] = {
	{ &mcp0, 6,  false },	// latch
/*
	{ &mcp0, 7,  false },
	{ &mcp0, 14, false },
	{ &mcp0, 15, false },
	{ &mcp1, 6,  false },
	{ &mcp1, 7,  false },
	{ &mcp1, 14, false },
	{ &mcp1, 15, false },
	{ NULL, 2,  false },	// momentary
	{ NULL, 3,  false },
	{ NULL, 4,  false },
	{ NULL, 5,  false },
	{ NULL, 6,  false },
	{ NULL, 7,  false },
	{ NULL, 8,  false },
	{ NULL, 9,  false }
*/
};


void setup() {
#ifdef DEBUG
	delay(1000);
	Serial.begin(115200);
	Serial.println("setup...");
#endif

	i2c_init();
	// setup the OLED i2c display
	oled_init();
	// initialize the i2c io expander
	mcp0.begin(0);

	// encoders
	encoders_init();

	// switches
	switches_init();

	// tie together the encoders/switches with mcp interrupts
	pinMode(MCP0_INTERRUPT_PIN, INPUT);
	mcp0.setupInterrupts(true, false, LOW);
	for (int i = 0; i < ENCODERS; ++i) {
		encoders[i].mcpX->setupInterruptPin(encoders[i].pinA, CHANGE);
		encoders[i].mcpX->setupInterruptPin(encoders[i].pinB, CHANGE);
		encoders[i].mcpX->setupInterruptPin(encoders[i].pinSW, CHANGE);
	}
	for (int i = 0; i < SWITCHES; ++i) {
		switches[i].mcpX->setupInterruptPin(switches[i].pin, CHANGE);
	}

	// si5351
	si5351_init();

#ifdef DEBUG
	Serial.println("...done");
#endif
	oled_update_display();
}

uint8_t gpioAB[2];
boolean A, B, sw;
unsigned int value;
int i;

void loop() {
#ifdef DEBUG
	Serial.print("loop\n\r");
#endif
	attachInterrupt(MCP0_INTERRUPT_PIN, &mcp0_interrupt_callback, FALLING);

	while (!mcp0_interrupt);
#ifdef DEBUG
	Serial.print("interrupt\n\r");
#endif

	detachInterrupt(MCP0_INTERRUPT_PIN);

	if (mcp0_interrupt) {
		gpioAB[0] = mcp0.readGPIOAB();
		encoders_process();
		switches_process();
		mcp0_interrupt = false;
	}
}

/******************************************************************************
 * I2C
 *****************************************************************************/
void i2c_init(void) {
	Wire.begin();
	Wire.setClock(I2C_FREQ);
#ifdef DEBUG
	Serial.println("i2c");
#endif
}

/******************************************************************************
 * OLED
 *****************************************************************************/
void oled_init(void) {
	oled = new Adafruit_SSD1306(OLED_WIDTH, OLED_HEIGHT);
	oled->begin(SSD1306_SWITCHCAPVCC, 0x3C);
	oled->clearDisplay();
	oled->setTextSize(1);
	oled->setTextColor(WHITE);
	oled->setCursor(0, 0);
	oled->println("Welcome!");
	oled->display();
#ifdef DEBUG
	Serial.println("oled");
#endif
}

void oled_update_display(void) {
	char buf[12];
	oled->clearDisplay();
	oled->setTextSize(2);
	// current frequency
	oled->setTextColor(SSD1306_WHITE);
	oled->setCursor(0, 0);
	if (freq_hold) {
		Serial.print(freq_hold, DEC);
		oled->setTextColor(SSD1306_BLACK, SSD1306_WHITE);
	}
	snprintf(buf, 12, "%10lu", (unsigned long)si_clocks[current_clk].freq);
	oled->print(buf);
	if (freq_hold)
		oled->setTextColor(SSD1306_WHITE);
	// current digit cursor
	oled->setTextColor(SSD1306_WHITE);
	oled->setCursor(12*9 - freq_digit * 12, 20);
	oled->print((char)222);
	// current selected clock
	oled->setTextColor(SSD1306_WHITE);
	oled->setCursor(0, 30);
	oled->setTextSize(1);
	oled->print((current_clk == clk0 ? "clock 0" : (current_clk == clk1 ? "clock 1" : "clock 2")));
	oled->display();
}

/******************************************************************************
 * Graphics visuals
 *****************************************************************************/
void gfx_bar_graph(uint8_t base_x, uint8_t base_y, uint8_t width, 
                    uint8_t height, uint8_t value) {
    uint8_t x0, y0, fill_height;
    if (value > height)
        value = height;
    x0 = base_x;
    y0 = base_y - height;
    fill_height = value;
    oled->fillRect(x0, y0, width, fill_height, 1);
}

/******************************************************************************
 * Encoders
 *****************************************************************************/
void encoders_init(void) {
	// setup input pins for encoders, note the encoder
	// pins have their own pullups
	for (i = 0; i < ENCODERS; ++i) {
		encoders[i].mcpX->pinMode(encoders[i].pinA, INPUT);
		encoders[i].mcpX->pinMode(encoders[i].pinB, INPUT);
		encoders[i].mcpX->pinMode(encoders[i].pinSW, INPUT);
		encoders[i].A = encoders[i].mcpX->digitalRead(encoders[i].pinA);
		encoders[i].B = encoders[i].mcpX->digitalRead(encoders[i].pinB);
		encoders[i].sw = encoders[i].mcpX->digitalRead(encoders[i].pinSW);
	}
#ifdef DEBUG
	Serial.println("encoders");
#endif
}

void encoders_process(void) {
	// process the encoders
	for (i = 0; i < ENCODERS; ++i) {
		if (encoders[i].mcpX == &mcp0) {
			A = bitRead(gpioAB[0], encoders[i].pinA);
			B = bitRead(gpioAB[0], encoders[i].pinB);
			sw = bitRead(gpioAB[0], encoders[i].pinSW);
		} else {
			A = bitRead(gpioAB[1], encoders[i].pinA);
			B = bitRead(gpioAB[1], encoders[i].pinB);
			sw = bitRead(gpioAB[1], encoders[i].pinSW);
		}
		// this needs to actually do something here
		// like report the state change over midi
		if (sw != encoders[i].sw) {
			encoders[i].sw = sw;
#ifdef DEBUG
			Serial.print("encoder ");
			Serial.print(i, DEC);
			Serial.print(" switch state change: ");
			Serial.println(sw, DEC);
			encoders[i].encoder_sw_callback(&encoders[i]);
#endif
		}
		// the encoder has changed state
		// increase?
		if (A != encoders[i].A) {
			encoders[i].A = !encoders[i].A;
			if (encoders[i].A && !encoders[i].B) {
				encoders[i].value += 1;
				if (encoders[i].value > encoders[i].max_value)
					encoders[i].value = encoders[i].max_value;
					encoders[i].encoder_callback(&encoders[i], +1);

#ifdef DEBUG
				Serial.print("encoder ");
				Serial.print(i, DEC);
				Serial.print(" value change (+1): ");
				Serial.println(encoders[i].value, DEC);
#endif
			}
		}
		// decrease?
		if (B != encoders[i].B) {
			encoders[i].B = !encoders[i].B;
			if (encoders[i].B && !encoders[i].A) {
				encoders[i].value -= 1;
				if (encoders[i].value < encoders[i].min_value)
					encoders[i].value = encoders[i].min_value;
					encoders[i].encoder_callback(&encoders[i], -1);
#ifdef DEBUG
				Serial.print("encoder ");
				Serial.print(i, DEC);
				Serial.print(" value change (-1): ");
				Serial.println(encoders[i].value, DEC);
#endif
			}
		}
	}
}

/******************************************************************************
 * Buttons
 *****************************************************************************/
void switches_init(void) {
	// setup input pins and enable pullups
	for (i = 0; i < SWITCHES; ++i) {
		if (switches[i].mcpX) {
			switches[i].mcpX->pinMode(switches[i].pin, INPUT);
			switches[i].mcpX->pullUp(switches[i].pin, HIGH);
			switches[i].sw = switches[i].mcpX->digitalRead(switches[i].pin);
		} else {
			pinMode(switches[i].pin, INPUT_PULLUP);
			switches[i].sw = digitalRead(switches[i].pin);
		}
	}
#ifdef DEBUG
	Serial.println("switches");
#endif
}

void switches_process(void) {
	for (i = 0; i < SWITCHES; ++i) {
		if (switches[i].mcpX == &mcp0) {
			sw = bitRead(gpioAB[0], switches[i].pin);
		//} else if (switches[i].mcpX == &mcp1) {
		//	sw = bitRead(gpioAB[1], switches[i].pin);
		} else {
			sw = digitalRead(switches[i].pin);
		}
		// this needs to actually do something here
		// like report the state change over midi
		if (sw != switches[i].sw) {
			switches[i].sw = sw;
#ifdef DEBUG
			Serial.print("switch ");
			Serial.print(i, DEC);
			Serial.print(" state change: ");
			Serial.println(sw, DEC);
#endif
		}
	}
}

void si5351_init() {
	bool i2c_found;

	i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
	if(!i2c_found) {
#ifdef DEBUG
		Serial.println("si5351 not found on I2C bus!");
		return;
#endif
    	}
	// Set clocks to default
	si5351.set_freq(si_clocks[0].freq * 100ULL, SI5351_CLK0);
	//si5351.set_freq(si_clocks[clk1].freq, SI5351_CLK1);
	//si5351.set_freq(si_clocks[clk2].freq, SI5351_CLK2);
#ifdef DEBUG
	Serial.println("si5351");
#endif
}	

void si5351_status() {
	si5351.update_status();
	Serial.print("SYS_INIT: ");
	Serial.print(si5351.dev_status.SYS_INIT);
	Serial.print("  LOL_A: ");
	Serial.print(si5351.dev_status.LOL_A);
	Serial.print("  LOL_B: ");
	Serial.print(si5351.dev_status.LOL_B);
	Serial.print("  LOS: ");
	Serial.print(si5351.dev_status.LOS);
	Serial.print("  REVID: ");
	Serial.println(si5351.dev_status.REVID);
}
