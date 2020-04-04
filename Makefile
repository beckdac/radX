DEV=/dev/cu.usbmodem141401 
DEV=/dev/ttyACM0

FQBN=stm32duino:STM32F1:genericSTM32F103C
FQBN=STM32:stm32:GenF1
FQBN=adafruit:samd:adafruit_trinket_m0

all: build install

build:
	arduino-cli compile -v --fqbn ${FQBN} radX

install:
	arduino-cli upload -p ${DEV} --fqbn ${FQBN} radX

monitor:
	minicom -D ${DEV} -b 115200
