#include <Adafruit_MCP23017.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <si5351.h>
#include <Wire.h>

// Comment from previous version - this has not been tested with current sketch
// NOTE: dacb has edited the Adafruit library to include a call to 
// Wire.setClock after Wire.begin to increase the i2c clock to 1.7M
// which is an allowed rate in the MCP23017 datasheet

#undef DEBUG
#define DEBUG

// OLED setup
#define OLED_WIDTH  128
#define OLED_HEIGHT  64
#define OLED_RESET -1
Adafruit_SSD1306 *oled;

// MCP23017 setup
Adafruit_MCP23017 mcp0; //, mcp1;

// si3531
Si5351 si5351;

// encoders and switches
typedef struct encoder {
	Adafruit_MCP23017 *mcpX;	// which mcp chip is this encoder on
	uint8_t pinA, pinB, pinSW;	// which pins are A, B, and SW
	boolean A, B, sw;		// last value of A and B and SW
	int16_t value;			// value of encoder
	int16_t min_value;		// min value encoder can have
	int16_t max_value;		// max value encoder can have
	bool (*encoder_callback)(struct encoder *encoder, int8_t dir);
} encoder_t;


unsigned long long clk0_freq = 1400000000ULL;
uint8_t freq_digit = 0;

bool encoder_freq_callback(encoder_t *encoder, int8_t dir) {
	if (dir > 0) {	// +
		clk0_freq += pow(10, freq_digit) * 100;
	} else if (dir == 0) { // switch
	} else {	// -
		unsigned long delta = pow(10, freq_digit) * 100;
		if ((signed long long)clk0_freq - delta < 0)
			clk0_freq = 0;
		else
			clk0_freq -= delta;
	}
#ifdef DEBUG
	Serial.print("frequency: ");
	Serial.println((unsigned long)(clk0_freq / 100), DEC);
#endif
	if (clk0_freq > 2000000000ULL)
		clk0_freq = 2000000000ULL;
	si5351.set_freq(clk0_freq, SI5351_CLK0);
#ifdef DEBUG
	//si5351_status();
#endif
	oled_update_display();
}

bool encoder_select_callback(encoder_t *encoder, int8_t dir) {
	if (dir > 0) {	// +
		if (freq_digit < 9)
			freq_digit++;
	} else if (dir == 0) { // switch
	} else {	// -
		if (freq_digit > 0)
			freq_digit--;
	}
#ifdef DEBUG
	Serial.print("freq_digit: ");
	Serial.println(freq_digit, DEC);
#endif
}

#define ENCODERS 2
// setup the encoder data structures
encoder_t encoders[ENCODERS] = {
	{ &mcp0, 0, 1, 2,    true, true, false, 63, 0, 127, &encoder_freq_callback },
	{ &mcp0, 3, 4, 5,    true, true, false, 63, 0, 127, &encoder_select_callback },
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
	//Serial.print("loop\n\r");
	//delay(100);
#endif
	gpioAB[0] = mcp0.readGPIOAB();
	encoders_process();
	switches_process();
}

/******************************************************************************
 * I2C
 *****************************************************************************/
void i2c_init(void) {
	Wire.begin();
	Wire.setClock(400000);
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
	oled->clearDisplay();
	oled->setTextSize(2);
	oled->setTextColor(WHITE);
	oled->setCursor(0, 0);
	oled->println((unsigned long)(clk0_freq / 100), DEC);
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
	// Set CLK0 to output 14 MHz
	si5351.set_freq(clk0_freq, SI5351_CLK0);
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
