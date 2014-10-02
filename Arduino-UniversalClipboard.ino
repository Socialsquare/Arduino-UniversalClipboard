/*
  Arduino-UniversalClipboard
  An Arduino sketch for an Arduino Leonardo combined with an Arduino USB Host Shield
  that turns it into a universal software independent clipboard.

  Hardware setup:
    - An Arduino Leonardo with
      - A USB Host shield attached.
      - A switch between GND and pin 12: Used to activate the USB proxying.
      - A green (OK) LED with its anode (the small part inside it) connected to pin 13
      - A yellow (working) LED with its anode (the small part inside it) connected to pin 11
      - A red (error) LED with its anode (the small part inside it) connected to pin 7
      - All of the LEDs cathodes are connected to GND
*/

/*
#define DEBUG_USB_HOST

void NotifyFailGetDevDescr() {
  Serial.println("Failed getting the device descriptor.");
}

void NotifyFailSetConfDescr() {
  Serial.println("Failed setting the device descriptor.");
}

void NotifyFail(unsigned char c) {
  Serial.print("Fail: ");
  Serial.println(c);
}
*/

#include <hidboot.h>

const int activationPin =  12; // the number of the pin to activate the USB keyboard interaction.
int activationState = LOW;       // variable for reading the pushbutton status
bool activated = false;

const int errorLedPin = 7;
const int workingLedPin = 11;
const int okLedPin = 13;

// const uint8_t key_mapping[128];

void error() {
  digitalWrite(errorLedPin, HIGH);
}

void working() {
  digitalWrite(workingLedPin, HIGH);
}

void working_blink() {
  working();
  delay(10);
  reset_working();
}

void ok() {
  digitalWrite(okLedPin, HIGH);
}

void reset_error() {
  digitalWrite(errorLedPin, LOW);
}

void reset_working() {
  digitalWrite(workingLedPin, LOW);
}

void reset_ok() {
  digitalWrite(okLedPin, LOW);
}

uint8_t translateKey(uint8_t key) {
  
  return 0;
}

USB UsbHost;
//USBHub     Hub(&UsbHost);
HIDBoot<HID_PROTOCOL_KEYBOARD> HidKeyboard(&UsbHost);

class ProxyKeyboardParser : public KeyboardReportParser
{
    void UnknownKey(uint8_t key);
  protected:
    virtual void OnControlKeysChanged(uint8_t before, uint8_t after);
    virtual void OnKeyDown	(uint8_t mod, uint8_t key);
    virtual void OnKeyUp	(uint8_t mod, uint8_t key);
};

char message[64];

void ProxyKeyboardParser::UnknownKey(uint8_t key) {
  snprintf(message, 64, "_%d", key);
  Keyboard.print(message);
}

void ProxyKeyboardParser::OnControlKeysChanged(uint8_t before, uint8_t after) {
  /*
  working_blink();

  MODIFIERKEYS beforeMod;
  *((uint8_t*)&beforeMod) = before;

  MODIFIERKEYS afterMod;
  *((uint8_t*)&afterMod) = after;
  */
}

void ProxyKeyboardParser::OnKeyDown(uint8_t mod, uint8_t key)
{
  working_blink();
  uint8_t translated_key = translateKey(key);
  //uint8_t c = OemToAscii(mod, key);
  if(translated_key > 0) {
    Keyboard.press(translated_key);
  } else {
    UnknownKey(key);
  }
}


void ProxyKeyboardParser::OnKeyUp(uint8_t mod, uint8_t key)
{
  working_blink();
  Keyboard.release(key);
  uint8_t translated_key = translateKey(key);
  if(translated_key > 0) {
    Keyboard.press(translated_key);
  }
  /*
  uint8_t c = OemToAscii(mod, key);
  Keyboard.release(c);
  */
}

ProxyKeyboardParser parser;

// the setup function runs once when you press reset or power the board
void setup() {
  pinMode(errorLedPin, OUTPUT);
  pinMode(workingLedPin, OUTPUT);
  pinMode(okLedPin, OUTPUT);

  pinMode(activationPin, INPUT_PULLUP);
}

void printDebugSequence() {
  int key = 0;
  for (int key = 0; key <= 127; key++) {
    snprintf(message, 64, "%d is \\", key);
    Keyboard.print(message);
    if(key != 96) {
      Keyboard.press(key);
    }
    Keyboard.releaseAll();
    Keyboard.println("\\");
  }
}

void setup_proxy() {
  if (UsbHost.Init() == -1) {
    error();
  } else {
    ok();
    HidKeyboard.SetReportParser(0, (HIDReportParser*)&parser);
    
    USB_HID_DESCRIPTOR descriptor;
    
    // Serial.println("Starting ...");
    
    USB *pUsb = (USB *) HidKeyboard.GetUsb();
    /*
    AddressPool &addrPool = pUsb->GetAddressPool();
    UsbDevice *p = addrPool.GetUsbDevicePtr(0);
    EpInfo *epinfo = p->epinfo;
    */
    /*
    USB_DEVICE_DESCRIPTOR *device_descriptor;
    pUsb->getDevDescr(0, 0, sizeof(device_descriptor), (uint8_t*) device_descriptor);
    Serial.println(device_descriptor->bDeviceClass);
    Serial.println(device_descriptor->bDeviceSubClass);
    */
    
    Serial.println("Done ...");
    
    // Start acting as a keyboard to the computer
    Keyboard.begin();
    delay(500);
    // printDebugSequence();
  }
}

// the loop function runs over and over again forever
void loop() {
  if (activated == false) {
    activationState = digitalRead(activationPin);
    if (activationState == LOW) {
      activated = true;
      // First activation
      setup_proxy();
    }
  }

  if (activated) {
    UsbHost.Task();
  }
}
