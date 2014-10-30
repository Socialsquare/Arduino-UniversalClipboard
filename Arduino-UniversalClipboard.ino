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

// ========== Includes ========== //

#include <hidboot.h>

// ========== Constants ========== //

// The  Keyboard proxy functionality is activated by lowering a pin to GND, the
// pin number is determined by the following constant.
const int ACTIVATION_PIN = 12;
// The red error LED is connected to the following pin.
const int ERROR_LED_PIN = 7;
// The yellow working LED is connected to the following pin.
const int WORKING_LED_PIN = 11;
// The green ok LED is connected to the following pin.
const int OK_LED_PIN = 13;

// ========== Controlling the different LEDs ========== //

// Error!
void error() {
  digitalWrite(ERROR_LED_PIN, HIGH);
}
void reset_error() {
  digitalWrite(ERROR_LED_PIN, LOW);
}

// Working
void working() {
  digitalWrite(WORKING_LED_PIN, HIGH);
}
void reset_working() {
  digitalWrite(WORKING_LED_PIN, LOW);
}
void working_blink() {
  working();
  delay(10);
  reset_working();
}

// OK
void ok() {
  digitalWrite(OK_LED_PIN, HIGH);
}
void reset_ok() {
  digitalWrite(OK_LED_PIN, LOW);
}

// ========== Communicating outward to the computer ========== //

class KeyboardOut : public Keyboard_
{
  private:
    KeyReport _keyReport;
  public:
    virtual size_t press(uint8_t k);
    virtual size_t release(uint8_t k);
    virtual uint8_t get_modifiers();
    virtual void set_modifiers(uint8_t modifiers);
    virtual void send_report();
};

// Registering a global singleton KeyboardOut, using this
// instead of the Keyboard singleton, will skip the translation
// from ASCII characters.
KeyboardOut keyboard_out;

// Sending out a key press to the computer
size_t KeyboardOut::press(uint8_t k) {
  uint8_t i;
  // Add k to the key report only if it's not already present
  // and if there is an empty slot.
  if (_keyReport.keys[0] != k && _keyReport.keys[1] != k && 
    _keyReport.keys[2] != k && _keyReport.keys[3] != k &&
    _keyReport.keys[4] != k && _keyReport.keys[5] != k) {

    for (i=0; i<6; i++) {
      if (_keyReport.keys[i] == 0x00) {
        _keyReport.keys[i] = k;
        break;
      }
    }
    if (i == 6) {
      setWriteError();
      return 0;
    }	
  }
  send_report();
  return 1;
}

// Sending out a key release to the computer
size_t KeyboardOut::release(uint8_t k) {
  uint8_t i;
  // Test the key report to see if k is present.  Clear it if it exists.
  // Check all positions in case the key is present more than once (which it shouldn't be)
  for (i=0; i<6; i++) {
    if (0 != k && _keyReport.keys[i] == k) {
      _keyReport.keys[i] = 0x00;
    }
  }
  send_report();
  return 1;
}

// Change of modifiers - remember to send the report.
void KeyboardOut::set_modifiers(uint8_t modifiers)
{
  _keyReport.modifiers = modifiers;
}

// Change of modifiers - remember to send the report.
uint8_t KeyboardOut::get_modifiers()
{
  return _keyReport.modifiers;
}

// Communicates the current state of the Arduino's keyboard to the computer.
void KeyboardOut::send_report()
{
  HID_SendReport(2, &_keyReport, sizeof(KeyReport));
}

USB UsbHost;
// Uncomment, if connected though a USB hub.
//USBHub     Hub(&UsbHost);
HIDBoot<HID_PROTOCOL_KEYBOARD> HidKeyboard(&UsbHost);

class ProxyKeyboardParser : public KeyboardReportParser
{
    void UnknownKey(uint8_t key);
    void SendModifierPressRelease(uint8_t before, uint8_t after, uint8_t key);
  protected:
    virtual void OnControlKeysChanged (uint8_t before, uint8_t after);
    virtual void OnKeyDown	      (uint8_t mod, uint8_t key);
    virtual void OnKeyUp	      (uint8_t mod, uint8_t key);
};

// A global keyboard parser, that will read keyboard strokes.
ProxyKeyboardParser keyboard_in;

void ProxyKeyboardParser::OnControlKeysChanged(uint8_t before, uint8_t after) {
  keyboard_out.set_modifiers(after);
  keyboard_out.send_report();
  working_blink();
}

void ProxyKeyboardParser::OnKeyDown(uint8_t mod, uint8_t key)
{
  bool controlled = intercept_recording_command(mod, key);
  if(!controlled) {
    intercept_recording_key(mod, key);
    keyboard_out.press(key);
  }
  working_blink();
}


void ProxyKeyboardParser::OnKeyUp(uint8_t mod, uint8_t key)
{
    keyboard_out.release(key);
    working_blink();
}

// ========== Main control flow ========= //

// A variable for reading the activation push button's status
uint8_t activationState;
// A state variable, determining if the proxy is activated or not.
bool proxy_activated = false;
// The channel onto which we are currently recording.
// This variable is 0, if we are not recording.
uint8_t active_recording_channel = 0;

const uint8_t CHANNEL_COUNT = 9;
const uint8_t CHANNEL_LENGTH = 128;
uint8_t channels[CHANNEL_COUNT][CHANNEL_LENGTH];
uint8_t active_channel_index;

enum KeyboardModifiers {
  LeftCtrl = 1,
  LeftShift = 2,
  Alt = 4,
  LeftCmd = 8,
  RightCtrl = 16,
  RightShift = 32,
  AltGr = 64,
  RightCmd = 128
};

const uint8_t NUMPAD_OFFSET = 89;
const uint8_t KEY_ENTER = 40;
const uint8_t KEY_TABULAR = 43;
const uint8_t KEY_NUMPAD_ENTER = 0x58;

void start_recording(uint8_t channel) {
  Serial.print("start_recording(");
  Serial.print(channel);
  Serial.println(")");
  active_recording_channel = channel;
  active_channel_index = 0;
  clear_channel(active_recording_channel);
}

bool is_recording() {
  return active_recording_channel > 0;
}

void stop_recording() {
  active_recording_channel = 0;
}

bool intercept_recording_command(uint8_t mod, uint8_t key) {
  bool recording = is_recording();
  bool control = (mod & LeftCtrl) || (mod & RightCtrl);
  bool shift = (mod & LeftShift) || (mod & RightShift);
  if(!recording && control && shift && key >= NUMPAD_OFFSET && key < NUMPAD_OFFSET+CHANNEL_COUNT) {
    // Switch recording channel
    start_recording(key - NUMPAD_OFFSET + 1);
    return true;
  } else if(!recording && control && key >= NUMPAD_OFFSET && key < NUMPAD_OFFSET+CHANNEL_COUNT) {
    // Switch recording channel
    replay_channel(key - NUMPAD_OFFSET + 1);
    return true;
  } else if(recording && (key == KEY_ENTER || key == KEY_TABULAR || key == KEY_NUMPAD_ENTER)) {
    print_channel(active_recording_channel);
    // Stop recording.
    stop_recording();
    return true;
  }
  return false;
}

void intercept_recording_key(uint8_t mod, uint8_t key) {
  if(active_recording_channel > 0) {
    // We are recording
    channels[active_recording_channel-1][active_channel_index++] = mod;
    channels[active_recording_channel-1][active_channel_index++] = key;
  }
}

void clear_channel(uint8_t c) {
  if(c > 0 && c <= CHANNEL_COUNT) {
    for(uint8_t i = 0; i < CHANNEL_LENGTH; i++) {
      // Null every byte in the channel
      channels[c-1][i] = 0;
    }
  }
}

void print_channel(uint8_t c) {
  if(c > 0 && c <= CHANNEL_COUNT) {
    Serial.print("Printing channel #");
    Serial.print(c);
    Serial.print(": ");
    for(uint8_t i = 0; i < CHANNEL_LENGTH; i++) {
      // Null every byte in the channel
      Serial.print(channels[c-1][i], HEX);
      if(i != CHANNEL_LENGTH-1) { // Fencepost
        Serial.print(" ");
      }
    }
    Serial.println(".");
  }
}

void replay_channel(uint8_t c) {
  if(c > 0 && c <= CHANNEL_COUNT) {
    uint8_t existing_mods = keyboard_out.get_modifiers();
    Serial.print("Replaying channel #");
    Serial.print(c);
    Serial.println(".");
    for(uint8_t i = 0; i < CHANNEL_LENGTH; i+=2) {
      uint8_t mods = channels[c-1][i];
      uint8_t key = channels[c-1][i+1];
      if(key > 0) {
        keyboard_out.set_modifiers(mods);
        keyboard_out.press(key);
        keyboard_out.release(key);
      } else {
        break; // We got a null key - stop replaying.
      }
    }
    // Restore modifiers
    keyboard_out.set_modifiers(existing_mods);
    // Tell the computer that the modifiers have changed back.
    keyboard_out.send_report();
  }
}

// The setup function runs once when you press reset or power the board
void setup() {
  pinMode(ERROR_LED_PIN, OUTPUT);
  pinMode(WORKING_LED_PIN, OUTPUT);
  pinMode(OK_LED_PIN, OUTPUT);

  pinMode(ACTIVATION_PIN, INPUT_PULLUP);
  
  // Initialize by clearing all channels
  for(uint8_t c = 0; c < CHANNEL_COUNT; c++) {
    clear_channel(c);
  }
  
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
  //*/
}

// Setup the keyboard proxy.
void setup_proxy() {
  if (UsbHost.Init() == -1) {
    error();
  } else {
    // Set the parser of the ingoing keyboard reports.
    HidKeyboard.SetReportParser(0, (HIDReportParser*) &keyboard_in);
    
    // Start acting as a keyboard to the computer
    keyboard_out.begin();
    
    // Show the world that we're okay!
    ok();
  }
}

// The loop function runs over and over again forever
void loop() {
  if (proxy_activated == false) {
    activationState = digitalRead(ACTIVATION_PIN);
    // If we're activated and the activation pin is pulled down.
    if (activationState == LOW) {
      proxy_activated = true;
      // First activation
      setup_proxy();
    }
  }

  if (proxy_activated) {
    UsbHost.Task();
    if(is_recording()) {
      // We are recording - blink on/off onces every second.
      bool working_on_off = (millis()/500) % 2;
      if(working_on_off) {
        working();
      } else {
        reset_working();
      }
    }
  }
}

