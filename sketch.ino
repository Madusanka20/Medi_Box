#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <time.h>
#include <Arduino.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUZZER 18
#define LED_1 15
#define PB_CANCEL 34
#define PB_OK 32
#define PB_UP 35
#define PB_DOWN 33
#define DHTPIN 12

#define NTP_SERVER "pool.ntp.org"
#define DEBOUNCE_DELAY 200
#define GMT_OFFSET_SEC 19800  // +5.30 hours in seconds (5*3600 + 30*60)

// Display and sensor initialization
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

// Time variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

// Alarm variables
bool alarm_enabled = true;
int n_alarms = 2;
int alarm_hours[2] = {0, 1};
int alarm_minutes[2] = {1, 10};
bool alarm_triggered[2] = {false, false};
bool alarm_snoozing[2] = {false, false};
unsigned long snooze_end_time[2] = {0, 0};
const int SNOOZE_DURATION = 5 * 60; // 5 minutes in seconds

// Temperature and humidity thresholds
const float TEMP_MIN = 24.0;
const float TEMP_MAX = 32.0;
const float HUMID_MIN = 65.0;
const float HUMID_MAX = 80.0;

// Environment monitoring variables
bool temp_warning_active = false;
bool humid_warning_active = false;
unsigned long last_env_check = 0;
const unsigned long ENV_CHECK_INTERVAL = 5000; // Check every 5 seconds

// Buzzer notes
int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

// Menu system
int current_mode = 0;
int max_modes = 6;
String modes[] = {
  "1-Set timezone", 
  "2-Set alarm 1", 
  "3-Set alarm 2", 
  "4-View alarms", 
  "5-Delete alarm",
  "6-Toggle alarms"
};

// Function prototypes
void print_line(String text, int column, int row, int text_size);
void print_time_now(void);
void update_time(void);
void update_time_with_check_alarm(void);
int wait_for_button_press(void);
void go_to_menu(void);
void set_timezone(void);
void set_alarm(int alarm);
void view_alarms(void);
void delete_alarm(void);
void toggle_alarms(void);
void run_mode(int mode);
void ring_alarm(int alarm_index);
void check_temp_humidity(void);
void reset_alarms_for_new_day(void);
bool debounce(int pin);
void handle_environment_warnings(void);
void display_environment_status(void);

void setup() {
  // Initialize pins
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_1, OUTPUT);
  pinMode(PB_CANCEL, INPUT_PULLUP);
  pinMode(PB_OK, INPUT_PULLUP);
  pinMode(PB_UP, INPUT_PULLUP);
  pinMode(PB_DOWN, INPUT_PULLUP);

  // Initialize DHT sensor
  dhtSensor.setup(DHTPIN, DHTesp::DHT22);

  Serial.begin(9600);

  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.display();
  delay(500);

  // Connect to WiFi
  display.clearDisplay();
  print_line("Connecting to", 0, 0, 1);
  print_line("WiFi...", 0, 10, 1);
  display.display();
  
  WiFi.begin("Wokwi-GUEST", "", 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  display.clearDisplay();
  print_line("WiFi Connected!", 0, 0, 1);
  display.display();
  delay(1000);

  // Configure time with GMT+5:30 offset
  configTime(GMT_OFFSET_SEC, 0, NTP_SERVER);

  // Welcome message
  display.clearDisplay();
  print_line("Welcome to", 20, 10, 2);
  print_line("Medibox!", 20, 30, 2);
  display.display();
  delay(2000);
  display.clearDisplay();
}

void loop() {
  update_time_with_check_alarm();
  
  // Check for button press to enter menu
  if (debounce(PB_OK)){
    go_to_menu();
  }
  
  // Check temperature and humidity at regular intervals
  if (millis() - last_env_check >= ENV_CHECK_INTERVAL) {
    check_temp_humidity();
    last_env_check = millis();
  }
  
  // Handle environment warnings
  handle_environment_warnings();
  
  // Handle snooze functionality
  for (int i = 0; i < n_alarms; i++) {
    if (alarm_snoozing[i] && millis() >= snooze_end_time[i]) {
      alarm_snoozing[i] = false;
      ring_alarm(i);
    }
  }
  
  delay(100); // Small delay to prevent CPU overload
}

// Button debounce function
bool debounce(int pin) {
  static unsigned long lastDebounceTime = 0;
  
  if (digitalRead(pin) == LOW) {
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
      lastDebounceTime = millis();
      return true;
    }
  }
  return false;
}

// Print text on OLED display
void print_line(String text, int column, int row, int text_size) {
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text); 
  display.display();
}

// Display current time on OLED
void print_time_now() {
  display.clearDisplay();
  
  // Time in HH:MM:SS format
  String hourStr = (hours < 10) ? "0" + String(hours) : String(hours);
  String minStr = (minutes < 10) ? "0" + String(minutes) : String(minutes);
  String secStr = (seconds < 10) ? "0" + String(seconds) : String(seconds);
  
  print_line(hourStr + ":" + minStr + ":" + secStr, 10, 0, 3);
  
  // Show timezone information
  print_line("GMT+5:30", 45, 28, 1);
}

// Update time from NTP server
void update_time() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  hours = timeinfo.tm_hour;
  minutes = timeinfo.tm_min;
  seconds = timeinfo.tm_sec;
  days = timeinfo.tm_mday;
  
  // Reset alarm triggers for a new day
  static int last_day = -1;
  if (days != last_day) {
    reset_alarms_for_new_day();
    last_day = days;
  }
}

// Reset alarm triggered flags at the start of a new day
void reset_alarms_for_new_day() {
  for (int i = 0; i < n_alarms; i++) {
    alarm_triggered[i] = false;
    alarm_snoozing[i] = false;
  }
}

// Update time and check if any alarms should be triggered
void update_time_with_check_alarm() {
  update_time();
  print_time_now();

  if (alarm_enabled) {
    for (int i = 0; i < n_alarms; i++) {
      if (!alarm_triggered[i] && !alarm_snoozing[i] && 
          alarm_hours[i] == hours && alarm_minutes[i] == minutes && seconds == 0) {
        ring_alarm(i);
      }
    }
  }
}

// Wait for a button press and return the button pressed
int wait_for_button_press() {
  while(true) {
    if (debounce(PB_UP)) {
      return PB_UP;
    }
    else if (debounce(PB_DOWN)) {
      return PB_DOWN;
    }
    else if (debounce(PB_OK)) {
      return PB_OK;
    }
    else if (debounce(PB_CANCEL)) {
      return PB_CANCEL;
    }
    update_time();
    delay(50);
  }
}

// Menu navigation system
void go_to_menu() {
  display.clearDisplay();
  print_line("MENU", 40, 0, 2);
  display.display();
  delay(1000);
  
  while (true) {
    // Display current menu item
    display.clearDisplay();
    print_line(modes[current_mode], 0, 0, 2);
    print_line("UP/DOWN: Navigate", 0, 40, 1);
    print_line("OK: Select", 0, 50, 1);
    display.display();

    int pressed = wait_for_button_press();
    
    if (pressed == PB_UP) {
      current_mode = (current_mode - 1 + max_modes) % max_modes; // Move up (with wrap-around)
    }
    else if (pressed == PB_DOWN) {
      current_mode = (current_mode + 1) % max_modes; // Move down (with wrap-around)
    }
    else if (pressed == PB_OK) {
      run_mode(current_mode); // Execute selected mode
    }
    else if (pressed == PB_CANCEL) {
      break; // Exit menu
    }
  }
  
  // Reset menu position when exiting
  current_mode = 0;
  display.clearDisplay();
  display.display();
}

// Set timezone offset from UTC (removed timezone setting since we're fixed to GMT+5:30)
void set_timezone() {
  display.clearDisplay();
  print_line("Timezone fixed", 0, 0, 1);
  print_line("to GMT+5:30", 0, 15, 1);
  print_line("Press any key", 0, 50, 1);
  display.display();
  wait_for_button_press();
}

// Set an alarm time
void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];
  int temp_minute = alarm_minutes[alarm];
  
  display.clearDisplay();
  print_line("Set Alarm " + String(alarm + 1), 0, 0, 2);
  display.display();
  delay(1000);
  
  // Set hour
  while(true) {
    display.clearDisplay();
    String hourStr = (temp_hour < 10) ? "0" + String(temp_hour) : String(temp_hour);
    print_line("Hour: " + hourStr, 0, 0, 2);
    print_line("UP/DOWN: change", 0, 40, 1);
    print_line("OK: confirm", 0, 50, 1);
    display.display();

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      temp_hour = (temp_hour + 1) % 24;
    }
    else if (pressed == PB_DOWN) {
      temp_hour = (temp_hour - 1 + 24) % 24;
    }
    else if (pressed == PB_OK) {
      break;
    }
    else if (pressed == PB_CANCEL) {
      return;
    }
  }
  
  // Set minute
  while(true) {
    display.clearDisplay();
    String minStr = (temp_minute < 10) ? "0" + String(temp_minute) : String(temp_minute);
    print_line("Minute: " + minStr, 0, 0, 2);
    print_line("UP/DOWN: change", 0, 40, 1);
    print_line("OK: confirm", 0, 50, 1);
    display.display();

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      temp_minute = (temp_minute + 1) % 60;
    }
    else if (pressed == PB_DOWN) {
      temp_minute = (temp_minute - 1 + 60) % 60;
    }
    else if (pressed == PB_OK) {
      alarm_hours[alarm] = temp_hour;
      alarm_minutes[alarm] = temp_minute;
      alarm_triggered[alarm] = false;
      
      display.clearDisplay();
      String hourStr = (alarm_hours[alarm] < 10) ? "0" + String(alarm_hours[alarm]) : String(alarm_hours[alarm]);
      String minStr = (alarm_minutes[alarm] < 10) ? "0" + String(alarm_minutes[alarm]) : String(alarm_minutes[alarm]);
      print_line("Alarm " + String(alarm + 1) + " set:", 0, 0, 1);
      print_line(hourStr + ":" + minStr, 20, 20, 2);
      display.display();
      delay(2000);
      break;
    }
    else if (pressed == PB_CANCEL) {
      return;
    }
  }
}

// View all active alarms
void view_alarms() {
  display.clearDisplay();
  print_line("Active Alarms:", 0, 0, 2);
  
  for (int i = 0; i < n_alarms; i++) {
    String hourStr = (alarm_hours[i] < 10) ? "0" + String(alarm_hours[i]) : String(alarm_hours[i]);
    String minStr = (alarm_minutes[i] < 10) ? "0" + String(alarm_minutes[i]) : String(alarm_minutes[i]);
    String status = alarm_enabled ? "ON" : "OFF";
    print_line(String(i + 1) + ": " + hourStr + ":" + minStr + " " + status, 0, 20 + i * 15, 1);
  }
  
  print_line("Press any key", 0, 50, 1);
  display.display();
  wait_for_button_press();
}

// Delete a specific alarm
void delete_alarm() {
  int selected_alarm = 0;
  
  display.clearDisplay();
  print_line("Delete Alarm", 0, 0, 2);
  display.display();
  delay(1000);
  
  while(true) {
    display.clearDisplay();
    print_line("Select Alarm:", 0, 0, 2);
    String hourStr = (alarm_hours[selected_alarm] < 10) ? "0" + String(alarm_hours[selected_alarm]) : String(alarm_hours[selected_alarm]);
    String minStr = (alarm_minutes[selected_alarm] < 10) ? "0" + String(alarm_minutes[selected_alarm]) : String(alarm_minutes[selected_alarm]);
    print_line("Alarm " + String(selected_alarm + 1) + ": " + hourStr + ":" + minStr, 0, 20, 1);
    print_line("UP/DOWN: change", 0, 40, 1);
    print_line("OK: delete", 0, 50, 1);
    display.display();

    int pressed = wait_for_button_press();
    if (pressed == PB_UP || pressed == PB_DOWN) {
      selected_alarm = (selected_alarm + 1) % n_alarms;
    }
    else if (pressed == PB_OK) {
      // Reset alarm to midnight
      alarm_hours[selected_alarm] = 0;
      alarm_minutes[selected_alarm] = 0;
      alarm_triggered[selected_alarm] = false;
      alarm_snoozing[selected_alarm] = false;
      
      display.clearDisplay();
      print_line("Alarm " + String(selected_alarm + 1), 0, 0, 2);
      print_line("deleted!", 0, 20, 2);
      display.display();
      delay(2000);
      break;
    }
    else if (pressed == PB_CANCEL) {
      break;
    }
  }
}

// Toggle all alarms on/off
void toggle_alarms() {
  alarm_enabled = !alarm_enabled;
  
  display.clearDisplay();
  print_line("Alarms:", 0, 0, 2);
  print_line(alarm_enabled ? "ENABLED" : "DISABLED", 0, 25, 2);
  display.display();
  delay(2000);
}

// Run the selected menu mode
void run_mode(int mode) {
  switch (mode) {
    case 0:
      set_timezone();
      break;
    case 1:
      set_alarm(0);
      break;
    case 2:
      set_alarm(1);
      break;
    case 3:
      view_alarms();
      break;
    case 4:
      delete_alarm();
      break;
    case 5:
      toggle_alarms();
      break;
    default:
      break;
  }
}

// Ring the alarm and handle stop/snooze
void ring_alarm(int alarm_index) {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 0, 0, 2);
  print_line("Alarm " + String(alarm_index + 1), 0, 25, 2);
  print_line("OK: Snooze 5min", 0, 45, 1);
  print_line("CANCEL: Stop", 0, 55, 1);
  display.display();
  
  digitalWrite(LED_1, HIGH);
  
  bool alarm_handled = false;
  unsigned long alarm_start_time = millis();
  
  // Ring alarm until stopped or snoozed
  while (!alarm_handled) {
    // Check for button press while ringing
    if (debounce(PB_CANCEL)) {
      alarm_handled = true;
      alarm_triggered[alarm_index] = true;
    }
    else if (debounce(PB_OK)) {
      alarm_handled = true;
      alarm_triggered[alarm_index] = false;
      alarm_snoozing[alarm_index] = true;
      snooze_end_time[alarm_index] = millis() + SNOOZE_DURATION * 1000;
      
      display.clearDisplay();
      print_line("Alarm Snoozed", 0, 0, 2);
      print_line("for 5 minutes", 0, 20, 2);
      display.display();
      delay(2000);
    }
    
    // Play alarm sound
    for (int i = 0; i < n_notes && !alarm_handled; i++) {
      if (digitalRead(PB_CANCEL) == LOW || digitalRead(PB_OK) == LOW) {
        break;
      }
      tone(BUZZER, notes[i]);
      delay(200);
      noTone(BUZZER);
      delay(50);
    }
    
    // Auto-stop after 30 seconds to prevent excessive noise
    if (millis() - alarm_start_time > 30000) {
      alarm_handled = true;
      alarm_triggered[alarm_index] = true;
    }
  }
  
  digitalWrite(LED_1, LOW);
  noTone(BUZZER);
  display.clearDisplay();
  display.display();
}

// Check temperature and humidity against thresholds
void check_temp_humidity() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  if (isnan(data.temperature) || isnan(data.humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    temp_warning_active = false;
    humid_warning_active = false;
    return;
  }
  
  // Check if outside healthy ranges
  temp_warning_active = (data.temperature < TEMP_MIN || data.temperature > TEMP_MAX);
  humid_warning_active = (data.humidity < HUMID_MIN || data.humidity > HUMID_MAX);
  
  // Display warnings if needed
  if (temp_warning_active || humid_warning_active) {
    display_environment_status();
  }
}

// Handle environment warnings (LED and display)
void handle_environment_warnings() {
  static unsigned long last_warning_display = 0;
  const unsigned long WARNING_DISPLAY_INTERVAL = 10000; // 10 seconds
  
  // Control LED based on warnings
  digitalWrite(LED_1, temp_warning_active || humid_warning_active ? HIGH : LOW);
  
  // Periodically show warnings on display
  if (temp_warning_active || humid_warning_active) {
    if (millis() - last_warning_display >= WARNING_DISPLAY_INTERVAL) {
      display_environment_status();
      last_warning_display = millis();
    }
  }
}

// Display environment status on OLED
void display_environment_status() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  
  if (isnan(data.temperature) || isnan(data.humidity)) {
    return; // Don't display if data is invalid
  }
  
  display.clearDisplay();
  print_time_now(); // Redraw the time
  
  if (temp_warning_active) {
    String temp_msg;
    if (data.temperature < TEMP_MIN) {
      temp_msg = "TEMP LOW: " + String(data.temperature, 1) + "C";
    } else {
      temp_msg = "TEMP HIGH: " + String(data.temperature, 1) + "C";
    }
    print_line(temp_msg, 0, 40, 1);
  }
  
  if (humid_warning_active) {
    String humid_msg;
    if (data.humidity < HUMID_MIN) {
      humid_msg = "HUMID LOW: " + String(data.humidity, 1) + "%";
    } else {
      humid_msg = "HUMID HIGH: " + String(data.humidity, 1) + "%";
    }
    print_line(humid_msg, 0, 50, 1);
  }
  
  display.display();
}