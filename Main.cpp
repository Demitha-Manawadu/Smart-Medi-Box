#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>
#include <DHT.h>
// OLED display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Wi-Fi credentials
#define WIFI_SSID       "Wokwi-GUEST"
#define WIFI_PASSWORD   ""

// Buzzer pin
#define BUZZER 5

// Push button pins
#define PB_CANCEL 33
#define PB_OK     34
#define PB_UP     35
#define PB_DOWN   32


//Environmental sensor pins
#define DHTPIN 12
#define DHTTYPE DHT22
#define RED_LED 15
#define BLUE_LED 4

DHT dht(DHTPIN, DHTTYPE);

// Time variables
int crnt_hour = 0;
int crnt_minute = 0;
int timezone_offset = 19800; // IST by default = 5.5 * 3600

// Alarm variables
bool alarm_enabled = true;
const int n_alarms = 2;
int alarm_hours[n_alarms] = {10, 10};
int alarm_minutes[n_alarms] = {30, 31};
bool alarm_triggered[n_alarms] = {false, false};
unsigned long snooze_until[n_alarms] = {0, 0};

// Notes for buzzer melody
int notes[] = {262, 294, 330, 349, 392, 440, 494};

// Menu system
int current_mode = 0;
const int max_modes = 5;
String modes[] = {
  "1. Set Time Zone", 
  "2. Set Alarm 1", 
  "3. Set Alarm 2", 
  "4. View Alarms",
  "5. Delete Alarm"
};

// Time zone selection arrays
const float time_zones[] = {
  -12, -11, -10, -9.5, -9, -8, -7, -6, -5.5, -5, -4.5, -4, -3.5, -3, -2, -1,
   0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 5.5, 5.75, 6, 6.5, 7, 8, 8.75,
   9, 9.5, 10, 10.5, 11, 11.5, 12, 13, 14
};
const char* time_zone_names[] = {
  "AoE", "NUT", "HST", "MART", "AKST", "PST", "MST", "CST", "EST", "AST",
  "VET", "AMT", "NST", "ART", "GST", "CET", "UTC", "IST", "CET", "IST",
  "EET", "IRST", "MSK", "AFT", "GET", "IRST", "PKT", "IST", "NPT", "BST",
  "MMT", "WIB", "SGT", "ACST", "JST", "ACDT", "AEST", "LHDT", "AEDT", "NFT",
  "NZDT", "PHOT"
};
const int time_zones_len = sizeof(time_zones) / sizeof(time_zones[0]);
int crnt_time_zone = 27; // Default to IST (5.5)

const char* format_utc_offset(int index) {
  static char buffer[10];
  float offset = time_zones[index];
  int hours = int(offset);
  int minutes = abs(int((offset - hours) * 60));
  sprintf(buffer, "%+d:%02d", hours, minutes);
  return buffer;
}

void print_line(String text, int text_size, int row, int column) {
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}

int wait_for_button_press() {
  while (true) {
    if (digitalRead(PB_UP) == LOW) {
      delay(200);
      return PB_UP;
    } else if (digitalRead(PB_DOWN) == LOW) {
      delay(200);
      return PB_DOWN;
    } else if (digitalRead(PB_OK) == LOW) {
      delay(200);
      return PB_OK;
    } else if (digitalRead(PB_CANCEL) == LOW) {
      delay(200);
      return PB_CANCEL;
    }
  }
}
void display_environment() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  display.setCursor(0, 20);
  display.setTextSize(1);
  display.print("Temp: " + String(temp) + "C");
  if (temp < 24) display.print(" Low");
  else if (temp > 32) display.print(" High");

  display.setCursor(0, 30);
  display.print("Hum : " + String(hum) + "%");
  if (hum < 65) display.print(" Low");
  else if (hum > 80) display.print(" High");

  digitalWrite(RED_LED, (temp < 24 || temp > 32) ? HIGH : LOW);
  digitalWrite(BLUE_LED, (hum < 65 || hum > 80) ? HIGH : LOW);

  display.display();
}

void updateTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to get time");
    return;
  }

  char timeHour[3];
  strftime(timeHour, sizeof(timeHour), "%H", &timeinfo);
  crnt_hour = atoi(timeHour);

  char timeMinute[3];
  strftime(timeMinute, sizeof(timeMinute), "%M", &timeinfo);
  crnt_minute = atoi(timeMinute);
}

void print_time_now() {
  display.clearDisplay();
  print_line(String(crnt_hour), 2, 0, 10);
  print_line(":", 2, 0, 40);
  print_line((crnt_minute < 10 ? "0" : "") + String(crnt_minute), 2, 0, 50);
  delay(1000);
}

void ring_alarm(int index) {
  display.clearDisplay();
  print_line("MEDICINE TIME!", 1, 20, 10);

  bool break_happened = false;

  while (!break_happened) {
    for (int i = 0; i < 7; i++) {
      if (digitalRead(PB_CANCEL) == LOW) {
        snooze_until[index] = 0;
        break_happened = true;
        break;
      }

      if (digitalRead(PB_OK) == LOW) {
        unsigned long snooze_duration = 5 * 60 * 1000;
        snooze_until[index] = millis() + snooze_duration;

        unsigned long start = millis();
        while (millis() - start < snooze_duration) {
          display.clearDisplay();
          unsigned long remaining = (snooze_until[index] - millis()) / 1000;
          int min = remaining / 60;
          int sec = remaining % 60;
          String countdown = "Snooze: " + String(min) + ":" + (sec < 10 ? "0" : "") + String(sec);
          print_line(countdown, 1, 20, 10);

          if (digitalRead(PB_CANCEL) == LOW) {
            snooze_until[index] = 0;
            return;
          }
          delay(500);
        }
        return;
      }

      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(100);
    }
  }

  noTone(BUZZER);
  display.clearDisplay();
}

void update_time_with_check_alarm() {
  updateTime();
  print_time_now();

  if (!alarm_enabled) return;

  for (int i = 0; i < n_alarms; i++) {
    if (alarm_hours[i] == crnt_hour && alarm_minutes[i] == crnt_minute) {
      if (!alarm_triggered[i] && (snooze_until[i] == 0 || millis() > snooze_until[i])) {
        alarm_triggered[i] = true;
        ring_alarm(i);
      }
    } else {
      alarm_triggered[i] = false;
    }
  }
}

void set_timezone() {
  static int timeZone = crnt_time_zone;

  while (true) {
    display.clearDisplay();
    print_line("Zone: UTC " + String(format_utc_offset(timeZone)), 1, 0, 0);
    print_line("Name: " + String(time_zone_names[timeZone]), 1, 10, 0);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      timeZone = (timeZone + 1) % time_zones_len;
    } else if (pressed == PB_DOWN) {
      timeZone = (timeZone - 1 + time_zones_len) % time_zones_len;
    } else if (pressed == PB_OK) {
      crnt_time_zone = timeZone;
      timezone_offset = time_zones[crnt_time_zone] * 3600;
      configTime(timezone_offset, 0, "pool.ntp.org");
      display.clearDisplay();
      print_line("Time zone set to", 1, 0, 0);
      print_line(time_zone_names[crnt_time_zone], 2, 20, 0);
      delay(1500);
      break;
    } else if (pressed == PB_CANCEL) {
      break;
    }
  }

  display.clearDisplay();
}

void set_alarm(int alarm) {
  int temp_hour = alarm_hours[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 1, 0, 2);
    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      temp_hour = (temp_hour + 1) % 24;
    } else if (pressed == PB_DOWN) {
      temp_hour = (temp_hour - 1 + 24) % 24;
    } else if (pressed == PB_OK) {
      alarm_hours[alarm] = temp_hour;
      break;
    } else if (pressed == PB_CANCEL) {
      return;
    }
  }

  int temp_minute = alarm_minutes[alarm];
  while (true) {
    display.clearDisplay();
    print_line("Enter min: " + String(temp_minute), 1, 0, 2);
    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      temp_minute = (temp_minute + 1) % 60;
    } else if (pressed == PB_DOWN) {
      temp_minute = (temp_minute - 1 + 60) % 60;
    } else if (pressed == PB_OK) {
      alarm_minutes[alarm] = temp_minute;
      break;
    } else if (pressed == PB_CANCEL) {
      return;
    }
  }

  display.clearDisplay();
  print_line("Alarm is set", 0, 0, 2);
  delay(1000);
  display.clearDisplay();
}

void view_alarms() {
  display.clearDisplay();
  bool any_active = false;
  for (int i = 0, row = 0; i < n_alarms; i++) {
    if (alarm_hours[i] >= 0 && alarm_minutes[i] >= 0) {
      String line = "Alarm " + String(i + 1) + ": " + String(alarm_hours[i]) + ":" + (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]);
      print_line(line, 1, row * 10, 0);
      row++;
      any_active = true;
    }
  }
  if (!any_active) {
    print_line("No alarms active", 1, 10, 0);
  }
  delay(2000);
  display.clearDisplay();
}

void delete_alarms() {
  int selected = 0;
  while (true) {
    display.clearDisplay();
    bool has_any = false;
    for (int i = 0, row = 0; i < n_alarms; i++) {
      if (alarm_hours[i] >= 0 && alarm_minutes[i] >= 0) {
        String prefix = (i == selected ? "> " : "  ");
        String line = prefix + "Alarm " + String(i + 1) + ": " + String(alarm_hours[i]) + ":" + (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]);
        print_line(line, 1, row * 10, 0);
        row++;
        has_any = true;
      }
    }
    if (!has_any) {
      print_line("No alarms to delete", 1, 0, 0);
      delay(2000);
      break;
    }

    int pressed = wait_for_button_press();
    if (pressed == PB_UP) {
      selected = (selected + 1) % n_alarms;
    } else if (pressed == PB_DOWN) {
      selected = (selected - 1 + n_alarms) % n_alarms;
    } else if (pressed == PB_OK) {
      alarm_hours[selected] = -1;
      alarm_minutes[selected] = -1;
      alarm_triggered[selected] = false;
      display.clearDisplay();
      print_line("Deleted Alarm " + String(selected + 1), 1, 10, 0);
      delay(1000);
      break;
    } else if (pressed == PB_CANCEL) {
      break;
    }
  }
  display.clearDisplay();
}

void run_mode(int mode) {
  if (mode == 0) {
    set_timezone();
  } else if (mode == 1 || mode == 2) {
    set_alarm(mode - 1);
  } else if (mode == 3) {
    view_alarms();
  } else if (mode == 4) {
    delete_alarms();
  }
}

void go_to_menu() {
  while (digitalRead(PB_CANCEL) == HIGH) {
    display.clearDisplay();
    print_line(modes[current_mode], 1, 20, 0);

    int pressed = wait_for_button_press();

    if (pressed == PB_UP) {
      current_mode = (current_mode + 1) % max_modes;
    } else if (pressed == PB_DOWN) {
      current_mode = (current_mode - 1 + max_modes) % max_modes;
    } else if (pressed == PB_OK) {
      run_mode(current_mode);
      break;
    } else if (pressed == PB_CANCEL) {
      break;
    }
  }
  display.clearDisplay();
}

void setup() {
  Serial.begin(9600);
  pinMode(BUZZER, OUTPUT);
  pinMode(PB_CANCEL, INPUT_PULLUP);
  pinMode(PB_OK, INPUT_PULLUP);
  pinMode(PB_UP, INPUT_PULLUP);
  pinMode(PB_DOWN, INPUT_PULLUP);
  pinMode(RED_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }

  display.clearDisplay();
  print_line("Connecting WiFi", 1, 10, 0);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  display.clearDisplay();
  print_line("WiFi Connected!", 1, 10, 0);
  delay(1000);

  configTime(timezone_offset, 0, "pool.ntp.org");
  display.clearDisplay();
}

void loop() {
  if (digitalRead(PB_OK) == LOW) {
    while (digitalRead(PB_OK) == LOW);
    delay(200);
    go_to_menu();
  }
  else{update_time_with_check_alarm();
    display_environment();
    delay(1000);
    }
  
}
