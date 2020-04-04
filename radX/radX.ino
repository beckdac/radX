
void setup() {
	pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  while (!Serial);
}

void loop() {
  Serial.println("loop");
	digitalWrite(LED_BUILTIN, HIGH);
	delay(1000);
	digitalWrite(LED_BUILTIN, LOW);
	delay(1000);
}
