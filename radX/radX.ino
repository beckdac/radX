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

void setup() {
#ifdef DEBUG
	delay(5000);
	Serial.begin(115200);
	Serial.println("setup...");
#endif

	i2c_init();
	// setup the OLED i2c display
	oled_init();

#ifdef DEBUG
	Serial.println("...done");
#endif
}

void loop() {
#ifdef DEBUG
	Serial.print("loop\n\r");
	delay(100);
#endif
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
