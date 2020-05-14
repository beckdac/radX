#include <Adafruit_MCP23017.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <si5351.h>
#include <Wire.h>

// if enabled clk1 is disabled for tuning and will
// output quadrature of clk0
#define QUAD_CLK1
#undef QUAD_CLK1

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
//	Serial.println("# interrupt");
}

// si3531
Si5351 si5351;

#define PLL_MIN 60000000000ULL
#define PLL_MAX 90000000000ULL

typedef struct si_plls {
	si5351_pll pll;
	uint64_t pll_freq;
	uint64_t freq;
	int n;
	int last_n;
} si_plls_t;

#define SI_PLLS 2
si_plls_t si_plls[SI_PLLS] = {
	{ SI5351_PLLA, PLL_MIN, 0, -1 },
	{ SI5351_PLLB, PLL_MIN, 0, -1 }
};

typedef struct si_clock {
	enum si5351_clock clock;
	uint64_t freq;
	enum si5351_pll pll;
	enum si5351_drive drive_strength;
} si_clock_t;

#define SI_CLOCKS 3
// setup the clock data structures
si_clock_t si_clocks[SI_CLOCKS] = {
	{ SI5351_CLK0, 700000000ULL, SI5351_PLLA, SI5351_DRIVE_2MA },	// clk0
	{ SI5351_CLK1, 800000000ULL, SI5351_PLLA, SI5351_DRIVE_2MA },	// clk1
	{ SI5351_CLK2, 3000000000ULL, SI5351_PLLB, SI5351_DRIVE_2MA }	// clk2
};

// encoders and switches
si5351_clock current_clk = SI5351_CLK0;
uint8_t freq_digit = 0;		// what digit is being manipulated by the tuning encoder
bool freq_hold = false;		// is the tuning encoder locked out of frequency changes
#ifdef QUAD_CLK1
bool freq_hold_last = false;	// for remembering the freq_hold state as you cycle through clk1
#endif

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
	uint64_t delta = pow(10, freq_digit) * SI5351_FREQ_MULT;
	if (dir > 0) {	// +
		si_clocks[current_clk].freq += delta;
	} else {	// -
		si_clocks[current_clk].freq -= delta;
	}
#ifdef DEBUG
	Serial.print("# delta ");
	Serial.print((uint32_t) delta, DEC);
	Serial.print("clock ");
	Serial.print(current_clk, DEC);
	Serial.print(" frequency: ");
	Serial.println((uint32_t)(si_clocks[current_clk].freq / SI5351_FREQ_MULT), DEC);
#endif
	si5351_set_freq(si_clocks[current_clk].clock, si_clocks[current_clk].freq);
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
	Serial.print("# freq_digit: ");
	Serial.println(freq_digit, DEC);
#endif
	oled_update_display();
}

bool encoder_freq_sw_callback(struct encoder *encoder) {
	if (encoder->sw) {
		switch (current_clk) {
			case SI5351_CLK0:
				current_clk = SI5351_CLK1;
#ifdef QUAD_CLK1
				freq_hold_last = freq_hold;
				freq_hold = true;
#endif
				break;
			case SI5351_CLK1:
#ifdef QUAD_CLK1
				freq_hold = freq_hold_last;
#endif
				current_clk = SI5351_CLK2;
				break;
			case SI5351_CLK2:
				current_clk = SI5351_CLK0;
				break;
			default:
				current_clk = SI5351_CLK0;
		};
#ifdef DEBUG
		Serial.print("# current clock changed to ");
		Serial.println(current_clk, DEC);
#endif
		oled_update_display();
	}
}

bool encoder_select_sw_callback(struct encoder *encoder) {
	if (encoder->sw) {
#ifdef QUAD_CLK1
		// clock 1 is quad locked to clock 0
		if (current_clk != SI5351_CLK1) {
			freq_hold = !freq_hold;
			oled_update_display();
		}
#else
		oled_update_display();
#endif
#ifdef DEBUG
		Serial.print("# freq_hold = ");
		Serial.println(freq_hold, DEC);
#endif
	}
}

#define ENCODERS 2
// setup the encoder data structures
encoder_t encoders[ENCODERS] = {
	{ &mcp0, 0, 1, 2,	true, true, false, 63, 0, 127, &encoder_freq_callback, &encoder_freq_sw_callback },
	{ &mcp0, 3, 4, 5,	true, true, false, 63, 0, 127, &encoder_select_callback, &encoder_select_sw_callback },
/*
	{ &mcp0, 8, 9, 10,   true, true, false, 63, 0, 127, 72 },
	{ &mcp0, 11, 12, 13, true, true, false, 63, 0, 127, 73 },
	{ &mcp1, 0, 1, 2,	true, true, false, 63, 0, 127, 74 },
	{ &mcp1, 3, 4, 5,	true, true, false, 63, 0, 127, 75 },
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

// Command line interface
const char MSG_OK[] = "OK\r\n";
const char MSG_ERROR[] = "ERROR";
const char MSG_LF[] = "\r\n";
const char MSG_CSEP[] = ": ";
const char MSG_INVCMD[] = "invalid command";
const char MSG_TOOLONG[] = "command line too long";
const char MSG_UNKPC[] = "unhandled exception in process_command";
const char MSG_READY[] = "READY\r\n";
const char MSG_INV_FREQ[] = "invalid frequency";
const char MSG_INV_CLOCK[] = "invalid clock";
const char MSG_NO_SI5351[] = "si5351 not found on I2C bus";
const char MSG_SI5351_INT_ERROR[] = "reached prohibited state in si5351_set_freq";

#define SEND_ERROR(msg)	Serial.print(MSG_ERROR); Serial.print(MSG_CSEP); Serial.print(msg); Serial.print(MSG_LF);
#define SEND_ERROR_DEC(msg, num)	Serial.print(MSG_ERROR); Serial.print(MSG_CSEP); Serial.print(msg); Serial.print(MSG_CSEP); Serial.print(num, DEC); Serial.print(MSG_LF);
#define SEND_ERROR_CHAR(msg, ch)	Serial.print(MSG_ERROR); Serial.print(MSG_CSEP); Serial.print(msg); Serial.print(MSG_CSEP); Serial.write(ch); Serial.print(MSG_LF);
#define SEND_ERROR_CHAR_DEC(msg, ch, num)	Serial.print(MSG_ERROR); Serial.print(MSG_CSEP); Serial.print(msg); Serial.print(MSG_CSEP); Serial.write(ch); Serial.print(MSG_CSEP); Serial.print(num, DEC); Serial.print(MSG_LF);

bool cmd_reset(char *cmd) {
	NVIC_SystemReset();      // processor software reset
}

bool cmd_clock(char *cmd) {
	char *endptr = cmd, *token;
	uint8_t clock;
	uint64_t new_freq;
#ifdef DEBUG
	Serial.print("# changing clock on serial command: ");
	Serial.println(cmd);
#endif
	token = strtok_r(endptr, " ", &endptr);
	if (token) {
		clock = atoi(token);
		if (clock < 0 || clock >= SI_CLOCKS) {
			SEND_ERROR(MSG_INV_CLOCK);
			return false;
		}
		token = strtok_r(endptr, " ", &endptr);
		if (token) {
			new_freq = atoi(token);
			if (new_freq < 0) {
				SEND_ERROR(MSG_INV_FREQ);
				return false;
			}
			si_clocks[clock].freq = (uint64_t)SI5351_FREQ_MULT * new_freq;
			si5351_set_freq(si_clocks[clock].clock, si_clocks[clock].freq);
			oled_update_display();
			return true;
		}
	}
	return false;
}

struct cmd_table_str {
	char *name;
	bool (*func)(char *);
} cmd_table[] = {
	{ "clk", &cmd_clock },
	{ "rst", &cmd_reset },
};
const uint8_t cmd_table_entries = 2;

bool process_command(char *cmd) {
	char *endptr = cmd, *token;
	uint8_t i, len = strlen(cmd);
	String msg;

	if (len == 0) return false;

	// process first word in command, handled commands return true
	token = strtok_r(endptr, " ", &endptr);
	if (token) {
		for (i = 0; i < cmd_table_entries; ++i) {
			if (strcmp(token, cmd_table[i].name) == 0) {
				return cmd_table[i].func(endptr);
			}
		}
	}
	
	// no command handler found
	SEND_ERROR(MSG_INVCMD);
	Serial.print('#'); Serial.print(cmd); Serial.print(MSG_LF);
	return false;
}

//////////////////////////////////////////////////////

void setup() {
//#ifdef DEBUG
	delay(3000);
	Serial.begin(115200);
	Serial.print("# setup...");
//#endif

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
	attachInterrupt(MCP0_INTERRUPT_PIN, &mcp0_interrupt_callback, FALLING);

	// si5351
	si5351_init();

	oled_update_display();
//#ifdef DEBUG
	Serial.println("... done");
//#endif
	Serial.println(MSG_READY);
}

#define MAX_CMD_LEN 64
char c, cmd_buf[MAX_CMD_LEN + 1];
#define CMD_COMMENT_CHAR '#'
bool suspend_store = false;
uint8_t c_idx;
uint8_t gpioAB[2];
boolean A, B, sw;
unsigned int value;
int i;

void loop() {
#ifdef DEBUG
	//Serial.print("loop\n\r");
#endif

	// read to newline
	// ignore any line that has over MAX_CMD_LEN characters
	if (Serial.available() > 0) {
		c = Serial.read();
		if (c == '\n' || c == '\r') {
			if (suspend_store == false) {
				cmd_buf[c_idx] = '\0';
#ifdef DEBUG
				Serial.print(MSG_LF);
#endif
				process_command(cmd_buf);
			} else {
				suspend_store = false;
			}
			c_idx = 0;
		} else {
			if (c_idx < MAX_CMD_LEN) {
				// if the first character is not printable or CMD_COMMENT_CHAR
				// then suspend_store and ignore the line
				if (c_idx == 0 && (!isprint(c) || c == CMD_COMMENT_CHAR)) {
					suspend_store = true;
				}
				// if suspend_store is false, then update the command buffer
				if (!suspend_store) {
#ifdef DEBUG
					//Serial.print(c);
#endif
					cmd_buf[c_idx] = c;
					++c_idx;
				}
			} else {
				SEND_ERROR(MSG_TOOLONG);
				suspend_store = true;
			}
		}
	}

	if (mcp0_interrupt) {
#ifdef DEBUG
		Serial.print("interrupt\n\r");
#endif
		detachInterrupt(MCP0_INTERRUPT_PIN);
		gpioAB[0] = mcp0.readGPIOAB();
		encoders_process();
		switches_process();
		mcp0_interrupt = false;
		attachInterrupt(MCP0_INTERRUPT_PIN, &mcp0_interrupt_callback, FALLING);
	}
}

/******************************************************************************
 * I2C
 *****************************************************************************/
void i2c_init(void) {
	Wire.begin();
	Wire.setClock(I2C_FREQ);
#ifdef DEBUG
	Serial.print("i2c ");
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
	Serial.print("oled ");
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
	snprintf(buf, 12, "%10lu", (unsigned long)(si_clocks[current_clk].freq / 100ULL));
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
	oled->print((current_clk == SI5351_CLK0 ? "clock 0" : (current_clk == SI5351_CLK1 ? "clock 1" : "clock 2")));
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
	Serial.print("encoders ");
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
			Serial.print("# encoder ");
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
				Serial.print("# encoder ");
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
				Serial.print("# encoder ");
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
	Serial.print("switches ");
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
			Serial.print("# switch ");
			Serial.print(i, DEC);
			Serial.print(" state change: ");
			Serial.println(sw, DEC);
#endif
		}
	}
}

#ifdef QUAD_CLK1
void si5351_pll_freq_calculate(uint8_t pll) {
	double pll_freq_dbl;

	si_plls[pll].last_n = si_plls[pll].n;
	for (uint8_t i = 10; i <= 200; i = i + 2) {
		pll_freq_dbl = si_plls[pll].freq * i;
		if (pll_freq_dbl >= PLL_MIN) {
			if (pll_freq_dbl <= PLL_MAX) {
				if (pll_freq_dbl == floor(pll_freq_dbl)) {
					si_plls[pll].pll_freq = pll_freq_dbl;
					si_plls[pll].n = si_plls[pll].pll_freq / si_plls[pll].freq;
					break;
				}
			}
		}
	} 
}
#endif

void si5351_init() {
	bool i2c_found;

	i2c_found = si5351.init(SI5351_CRYSTAL_LOAD_8PF, 0, 0);
	if(!i2c_found) {
#ifdef DEBUG
		SEND_ERROR(MSG_NO_SI5351);
		return;
#endif
	}
	// Set clocks to default
	for (i = 0; i < SI_CLOCKS; ++i) {
		si5351_set_freq(i, si_clocks[i].freq);
	}
#ifdef DEBUG
	Serial.print("si5351 ");
#endif
}	

void si5351_set_freq(uint8_t clock, uint64_t freq) {
	uint8_t pll;
	// don't change the frequency if the clock is on hold
#ifdef QUAD_CLK1
	// or this is clock 1 which is locked as quad of clk0
	if (freq_hold || si_clocks[clock].clock == SI5351_CLK1)
#else
	if (freq_hold)
#endif
		return;
#ifdef QUAD_CLK1
	for (pll = 0; pll < SI_PLLS; ++pll) {
		if (si_plls[pll].pll == si_clocks[clock].pll)
			break;
	}
#ifdef DEBUG
	if (pll == SI_PLLS) {	// this shouldn't happen
		SEND_ERROR(MSG_SI5351_INT_ERROR);
		return;
	}
#endif
	// get the pll setup for the new frequency
	si_plls[pll].freq = freq;
	si5351_pll_freq_calculate(pll);
	si5351.set_pll(si_plls[pll].pll_freq, si_plls[pll].pll);

	si_clocks[clock].freq = freq;
	si5351.set_freq_manual(si_clocks[clock].freq, si_plls[pll].pll_freq, si_clocks[clock].clock);
	if (si_clocks[clock].clock == SI5351_CLK0) {
		si5351.set_freq_manual(si_clocks[clock].freq, si_plls[pll].pll_freq, SI5351_CLK1);
		si5351.set_phase(SI5351_CLK0, 0);
		si5351.set_phase(SI5351_CLK1, si_plls[pll].n);
	}
	if (si_plls[pll].n != si_plls[pll].last_n)
		si5351.pll_reset(si_plls[pll].pll);
#else
	si_clocks[clock].freq = freq;
	si5351.set_freq(si_clocks[clock].freq, si_clocks[clock].clock);
#endif
}

void si5351_status() {
	si5351.update_status();
	Serial.print("# SYS_INIT: ");
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


/******************************************************************************
 * Serial command interpreter
 *****************************************************************************/
// # in either direction is info to be ignored

