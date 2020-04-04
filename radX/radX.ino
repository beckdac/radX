#include <Wire.h>
#include <Adafruit_MCP23017.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <si5351.h>

// Comment from previous version - this has not been tested with current sketch
// NOTE: dacb has edited the Adafruit library to include a call to 
// Wire.setClock after Wire.begin to increase the i2c clock to 1.7M
// which is an allowed rate in the MCP23017 datasheet

#undef DEBUG
#define DEBUG

// MCP23017 setup
Adafruit_MCP23017 mcp0; //, mcp1;

typedef struct encoder {
	Adafruit_MCP23017 *mcpX;	// which mcp chip is this encoder on
	uint8_t pinA, pinB, pinSW;	// which pins are A, B, and SW
	boolean A, B, sw;			// last value of A and B and SW
	int16_t value;				// value of encoder
	int16_t min_value;			// min value encoder can have
	int16_t max_value;			// max value encoder can have
	uint8_t control_number;		// control number
} encoder_t;

#define ENCODERS 2
// setup the encoder data structures
encoder_t encoders[ENCODERS] = {
	{ &mcp0, 0, 1, 2,    true, true, false, 63, 0, 127, 70 },
	{ &mcp0, 3, 4, 5,    true, true, false, 63, 0, 127, 71 },
/*
	{ &mcp0, 8, 9, 10,   true, true, false, 63, 0, 127, 72 },
	{ &mcp0, 11, 12, 13, true, true, false, 63, 0, 127, 73 },
	{ &mcp1, 0, 1, 2,    true, true, false, 63, 0, 127, 74 },
	{ &mcp1, 3, 4, 5,    true, true, false, 63, 0, 127, 75 },
	{ &mcp1, 8, 9, 10,   true, true, false, 63, 0, 127, 76 },
	{ &mcp1, 11, 12, 13, true, true, false, 63, 0, 127, 77 }
*/
};

typedef struct button {
	Adafruit_MCP23017 *mcpX;	// which mcp is this button on, NULL if teensy pin
	uint8_t pin;			// pin #
	boolean sw;			// state of switch
} button_t;

#define BUTTONS 1
button_t buttons[BUTTONS] = {
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

// OLED setup
// this pin is not used but needs to be provided to the driver, use the LED pin
#define OLED_RESET 13
Adafruit_SSD1306 oled(OLED_RESET);

void setup() {
#ifdef DEBUG
	Serial.begin(115200);
	Serial.print("setup...");
#endif

	// intialize the i2c mcp23017 devices
	mcp0.begin(0);
	//mcp1.begin(1);

	// encoders
	encoders_init();

	// buttons
	buttons_init();

#ifdef DEBUG
	Serial.println("done");
#endif

	// setup the OLED i2c display
	oled_init();
}

uint8_t gpioAB[2];
boolean A, B, sw;
unsigned int value;
int i;

void loop() {
	// read all the ports on the mcps
	gpioAB[0] = mcp0.readGPIOAB();
	//gpioAB[1] = mcp1.readGPIOAB();

	// process the encoders
	encoders_process();

	// process the buttons
	buttons_process();
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
#ifdef DEBUG
				Serial.print("encoder ");
				Serial.print(i, DEC);
				Serial.print(" value change (+1): ");
				Serial.println(encoders[i].value, DEC);
#endif
#ifdef MIDI
				usbMIDI.sendControlChange(encoders[i].control_number, encoders[i].value, channel);
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
#ifdef DEBUG
				Serial.print("encoder ");
				Serial.print(i, DEC);
				Serial.print(" value change (-1): ");
				Serial.println(encoders[i].value, DEC);
#endif
#ifdef MIDI
				usbMIDI.sendControlChange(encoders[i].control_number, encoders[i].value, channel);
#endif
			}
		}
	}
}

/******************************************************************************
 * Buttons
 *****************************************************************************/
void buttons_init(void) {
	// setup input pins and enable pullups
	for (i = 0; i < BUTTONS; ++i) {
		if (buttons[i].mcpX) {
			buttons[i].mcpX->pinMode(buttons[i].pin, INPUT);
			buttons[i].mcpX->pullUp(buttons[i].pin, HIGH);
			buttons[i].sw = buttons[i].mcpX->digitalRead(buttons[i].pin);
		} else {
			pinMode(buttons[i].pin, INPUT_PULLUP);
			buttons[i].sw = digitalRead(buttons[i].pin);
		}
	}
}

void buttons_process(void) {
	for (i = 0; i < BUTTONS; ++i) {
		if (buttons[i].mcpX == &mcp0) {
			sw = bitRead(gpioAB[0], buttons[i].pin);
		//} else if (buttons[i].mcpX == &mcp1) {
		//	sw = bitRead(gpioAB[1], buttons[i].pin);
		} else {
			sw = digitalRead(buttons[i].pin);
		}
		// this needs to actually do something here
		// like report the state change over midi
		if (sw != buttons[i].sw) {
			buttons[i].sw = sw;
#ifdef DEBUG
			Serial.print("button ");
			Serial.print(i, DEC);
			Serial.print(" state change: ");
			Serial.println(sw, DEC);
#endif
		}
	}
}

/******************************************************************************
 * OLED
 *****************************************************************************/
void oled_init(void) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(WHITE);
    oled.setCursor(0, 0);
    oled.println("Welcome!");
    oled.display();
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
    oled.fillRect(x0, y0, width, fill_height, 1);
}

