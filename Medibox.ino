#include <Arduino.h>
#include <DHTesp.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <sys/time.h>
#include <iostream>
#include <ESP32Servo.h>
#include <PubSubClient.h>

#define screen_width 128
#define screen_height 64
#define OLED_reset -1
#define screen_address 0x3C
Servo servoMotor;

const char* ssid = "Wokwi-GUEST";
const char* password = "";
int wifi_channel = 6;

const tm timeInfo = { 12, 0, 0 };
const char* NTP_SERVER = "pool.ntp.org";
String UTC_OFFSET = "IST-5:30";
String utc_offsets[] = {"UTC-12:00","UTC-11:00","UTC-10:00","UTC-09:30","UTC-09:00","UTC-08:00","UTC-07:00", "UTC-06:00","UTC-05:30", "UTC-04:30", "UTC-04:00", "UTC-03:30", "UTC-03:00","UTC-02:00","UTC-01:00", "UTC 00:00","UTC+01:00","UTC+02:00", "UTC+03:00", "UTC+03:30", "UTC+04:00", "UTC+04:30","UTC+05:00","UTC+05:30", "UTC+05:45","UTC+06:00","UTC+06:30","UTC+07:00","UTC+08:00","UTC+08:45", "UTC+09:00","UTC+09:30","UTC+10:00","UTC+10:30","UTC+11:00","UTC+12:00","UTC+12:45","UTC+13:00","UTC+14:00"};
int num_utc_offsets = sizeof(utc_offsets)/sizeof(utc_offsets[0]);

int seconds;
int minutes;
int hours;

bool alarm1_enabled = false;
bool alarm2_enabled = false;
int num_alarms = 2;
int alarm_hours[] = {0,0};
int alarm_minutes[] = {0,0};
bool alarm_triggered[] = {false,false};
float ServoAngle=0.0;
float SamplingInterval=5.0;
float SendingInterval=120.0;
float temp=0.0;
float light_intensity=0.0;
unsigned long sec_sample=0;
unsigned long sec_send=0;
int n=0;

bool is_countdown_active[2] = {false, false};
int countdown_hours[2] = {0, 0};
int countdown_minutes[2] = {0, 0};
int countdown_seconds[2] = {0, 0};

#define TEMPURATURE_HIGH 32
#define TEMPURATURE_LOW 24
#define HUMIDITY_HIGH 80
#define HUMIDITY_LOW 65

int notes[] = {262, 294, 330, 349, 392, 440, 494, 523};
int n_notes = sizeof(notes) / sizeof(notes[0]);

#define DHT_PIN 14
#define LED_RED_PIN 2
#define BUZZER 0
#define PB_BACK 26
#define PB_OK 35
#define PB_UP 32
#define PB_DOWN 33
#define PB_SNOOZE 25
#define servoPin 16
#define ldr 34

Adafruit_SSD1306 display(screen_width, screen_height, &Wire, OLED_reset);
DHTesp dhtSensor;

int current_mode = 0;
String modes[] = {"1 - Set Time","2 - Set Alarm 1","3 - Set Alarm 2","4 - Disable Alarms"};
int max_mode = 4;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void print_line(String text, String Clear_Screen="n", int text_size=1, int column=0, int row=0) {
  if (Clear_Screen == "y") {
    display.clearDisplay();
  }
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(text);
  display.display();
}

void print_lines(String lines[], int num_lines, int text_size=1, int columns[] = nullptr, int rows[] = nullptr, bool clear_screen=true) {
  if (clear_screen) {
    display.clearDisplay();
  }
  display.setTextColor(SSD1306_WHITE);
  for (int i = 0; i < num_lines; i++) {
    display.setCursor(columns[i], rows[i]);
    display.setTextSize(text_size);
    display.println(lines[i]);
  }
  display.display();
}

void ring_alarm() {
  print_line("MEDICINE", "y", 2, 16, 24);
  print_line("TIME", "n", 2, 40, 40);
  digitalWrite(LED_RED_PIN, HIGH);
  delay(5000);
  bool break_happened = false;
  while (break_happened == false && digitalRead(PB_BACK) == HIGH) {
    for(int i = 0; i < n_notes; i++) {
      if (digitalRead(PB_BACK) == LOW) {
        break_happened = true;
        delay(200);
        break;
      }
      tone(BUZZER, notes[i]);
      delay(560);
      noTone(BUZZER);
      delay(2);
    }
  }
  digitalWrite(LED_RED_PIN, LOW);
  noTone(BUZZER);
  display.clearDisplay();
}

void update_countdown(int alarm_index) {
  if (is_countdown_active[alarm_index]) {
    if (countdown_seconds[alarm_index] == 0) {
      if (countdown_minutes[alarm_index] == 0) {
        if (countdown_hours[alarm_index] == 0) {
          is_countdown_active[alarm_index] = false;
          ring_alarm();
        } else {
          countdown_hours[alarm_index]--;
          countdown_minutes[alarm_index] = 59;
          countdown_seconds[alarm_index] = 59;
        }
      } else {
        countdown_minutes[alarm_index]--;
        countdown_seconds[alarm_index] = 59;
      }
    } else {
      countdown_seconds[alarm_index]--;
    }
  }
}

void update_time() {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo)) {
    print_line("Failed to obtain time", "y");
    return;
  }
  seconds = timeInfo.tm_sec;
  minutes = timeInfo.tm_min;
  hours = timeInfo.tm_hour;
}

void print_time_now() {
  update_time();
}

void show_warning(float value, float threshold_low, float threshold_high, String message, String &warning_message) {
  if (value < threshold_low) {
    warning_message += message + "LOW";
  }
  if (value > threshold_high) {
    warning_message += message + "HIGH";
  }
}

void check_temp_and_humidity() {
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  String warning_message_temp = "";
  String warning_message_hum = "";
  temp=data.temperature;
  show_warning(data.temperature, TEMPURATURE_LOW, TEMPURATURE_HIGH, "TEMPERATURE ", warning_message_temp);
  show_warning(data.humidity, HUMIDITY_LOW, HUMIDITY_HIGH, "HUMIDITY ", warning_message_hum);
  if (warning_message_temp.length() > 0 || warning_message_hum.length() > 0) {
    String lines[2];
    int columns[2];
    int rows[2] = {20, 40};
    if (warning_message_temp.length() > 0) {
      lines[0] = warning_message_temp;
      columns[0] = (128 - warning_message_temp.length() * 6) / 2;
    } else {
      lines[0] = "";
      columns[0] = 0;
    }
    if (warning_message_hum.length() > 0) {
      lines[1] = warning_message_hum;
      columns[1] = (128 - warning_message_hum.length() * 6) / 2;
    } else {
      lines[1] = "";
      columns[1] = 0;
    }
    print_lines(lines, 2, 1, columns, rows, true);
    digitalWrite(LED_RED_PIN, HIGH);
    //tone(BUZZER, notes[0]);
    //delay(5000);
    //noTone(BUZZER);
    digitalWrite(LED_RED_PIN, LOW);
  }
}

void update_time_with_check_alarm_and_check_warning() {
  print_time_now();
  if (alarm1_enabled || alarm2_enabled) {
    for (int i = 0; i < num_alarms; i++) {
      if (!alarm_triggered[i] && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
        //ring_alarm();
        alarm_triggered[i] = true;
      }
    }
  }
  check_temp_and_humidity();
}

int wait_for_button_press() {
  int buttons[] = {PB_UP, PB_DOWN, PB_OK, PB_BACK};
  int numButtons = sizeof(buttons) / sizeof(buttons[0]);
  while (true) {
    for (int i = 0; i < numButtons; i++) {
      if (digitalRead(buttons[i]) == LOW) {
        delay(200);
        return buttons[i];
      }
    }
  }
}

void set_time_unit(int &unit, int max_value, String message) {
  int temp_unit = unit % max_value;
  while (true) {
    print_line(message + String(temp_unit), "y", 1, 5, 30);
    int pressed = wait_for_button_press();
    delay(200);
    switch (pressed) {
      case PB_UP:
        temp_unit = (temp_unit + 1) % max_value;
        break;
      case PB_DOWN:
        temp_unit = (temp_unit - 1 + max_value) % max_value;
        break;
      case PB_OK:
        unit = temp_unit;
        return;
      case PB_BACK:
        return;
    }
  }
}

void set_alarm(int alarm) {
  int temp_alarm_hours = 0;
  int temp_alarm_minutes = 0;
  set_time_unit(temp_alarm_hours, 24, "Enter hour: ");
  set_time_unit(temp_alarm_minutes, 60, "Enter minute: ");
  alarm_hours[alarm] = temp_alarm_hours;
  alarm_minutes[alarm] = temp_alarm_minutes;
  countdown_hours[alarm] = temp_alarm_hours;
  countdown_minutes[alarm] = temp_alarm_minutes;
  countdown_seconds[alarm] = 0;
  is_countdown_active[alarm] = true;
  print_line("Alarm is set", "y", 1, 5, 30);
  delay(1000);
}

void set_alarm1() {
  set_alarm(0);
  alarm1_enabled = true;
}

void set_alarm2() {
  set_alarm(1);
  alarm2_enabled = true;
}

void snooze_alarm(int alarm_index) {
  if (alarm_index < 0 || alarm_index >= num_alarms) return;
  countdown_minutes[alarm_index] += 5;
  if (countdown_minutes[alarm_index] >= 60) {
    countdown_hours[alarm_index]++;
    countdown_minutes[alarm_index] -= 60;
  }
  display.clearDisplay();
  print_line("Alarm " + String(alarm_index + 1), "n", 1, 40, 20);
  print_line("Snoozed +5m", "n", 1, 30, 40);
  display.display();
  delay(1000);
}

void handle_snooze() {
  if (alarm1_enabled && alarm2_enabled) {
    bool selection_made = false;
    unsigned long selection_start = millis();
    const unsigned long timeout = 5000;
    while (!selection_made && (millis() - selection_start < timeout)) {
      String lines[] = {"Snooze Alarm 1", "Snooze Alarm 2"};
      int columns[] = {5, 5};
      int rows[] = {20, 40};
      bool highlight[] = {digitalRead(PB_UP) == LOW, digitalRead(PB_DOWN) == LOW};
      display.clearDisplay();
      for (int i = 0; i < 2; i++) {
        display.setCursor(columns[i], rows[i]);
        if (highlight[i]) {
          display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
          display.println(">" + lines[i]);
        } else {
          display.setTextColor(SSD1306_WHITE);
          display.println(" " + lines[i]);
        }
      }
      display.display();
      if (digitalRead(PB_UP) == LOW) {
        snooze_alarm(0);
        selection_made = true;
        delay(200);
      } else if (digitalRead(PB_DOWN) == LOW) {
        snooze_alarm(1);
        selection_made = true;
        delay(200);
      } else if (digitalRead(PB_BACK) == LOW) {
        selection_made = true;
        delay(200);
      }
    }
    display.clearDisplay();
  } else {
    if (alarm1_enabled) snooze_alarm(0);
    if (alarm2_enabled) snooze_alarm(1);
  }
}

void disable_alarm(int mode) {
  display.clearDisplay();
  if (mode == 1 && alarm1_enabled) {
    print_line("Disable alarm 1 ?", "n", 1, 5, 30);
    int pressed = wait_for_button_press();
    if (pressed == PB_OK) {
      alarm1_enabled = false;
      is_countdown_active[0] = false;
      print_line("Alarm 1 disabled", "y", 1, 5, 30);
      delay(1000);
      return;
    }
  }
  if (mode == 2 && alarm2_enabled) {
    print_line("Disable alarm 2 ?", "n", 1, 5, 30);
    int pressed = wait_for_button_press();
    if (pressed == PB_OK) {
      alarm2_enabled = false;
      is_countdown_active[1] = false;
      print_line("Alarm 2 disabled", "y", 1, 5, 30);
      delay(1000);
      return;
    }
  }
}

void set_time_zone() {
  int index = 8;
  int index_max = num_utc_offsets;
  while (true) {
    print_line("Enter UTC offset", "y", 1, 13, 30);
    print_line(utc_offsets[index], "n", 1, 40, 45);
    int pressed = wait_for_button_press();
    delay(200);
    if (pressed == PB_UP)
      index = (index + 1) % index_max;
    else if (pressed == PB_DOWN)
      index = (index - 1 + index_max) % index_max;
    else if (pressed == PB_OK) {
      UTC_OFFSET = utc_offsets[index];
      configTzTime(UTC_OFFSET.c_str(), NTP_SERVER);
      print_line("Time zone is set", "y", 1, 10, 30);
      delay(1000);
      break;
    } else if (pressed == PB_BACK)
      break;
  }
}

void run_mode(int mode) {
  switch(mode) {
    case 0:
      set_time_zone();
      break;
    case 1:
      set_alarm1();
      break;
    case 2:
      set_alarm2();
      break;
    case 3:
      disable_alarm(1);
      disable_alarm(2);
      break;
  }
}

void go_to_menu() {
  while(digitalRead(PB_BACK) == HIGH) {
    print_line(modes[current_mode], "y", 1, 5, 30);
    int pressed = wait_for_button_press();
    delay(200);
    switch(pressed) {
      case PB_UP:
        current_mode = (current_mode + 1) % max_mode;
        break;
      case PB_DOWN:
        current_mode = (current_mode - 1 + max_mode) % max_mode;
        break;
      case PB_OK:
        run_mode(current_mode);
        break;
      case PB_BACK:
        return;
    }
  }
}

void receiveCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadBuffer[length + 1]; 
  for (int i = 0; i < length; i++) {
    payloadBuffer[i] = (char)payload[i];
    Serial.print((char)payload[i]);
  }
  payloadBuffer[length] = '\0';
  Serial.println();

  if (strcmp(topic, "SAMPLING-INTERVAL") == 0) {
    SamplingInterval = atof(payloadBuffer);
  }
  else if (strcmp(topic, "SENDING-INTERVAL") == 0) {
    SendingInterval = atof(payloadBuffer);
  }
  else if (strcmp(topic, "SERVO-ANGLE") == 0) {
    ServoAngle = atof(payloadBuffer);
  }
}

void connectToBroker() {
  while (!mqttClient.connected()) {
    Serial.println("Attempting to connect to MQTT broker");
    display.clearDisplay();
    print_line("Attempting MQTT","n",2,0,0);
    delay(3000);

    if (mqttClient.connect("ESP32-463517108394")) {
      Serial.println("MQTT connected.");
      print_line("Connected to MQTT","n",2,0,0);
      delay(3000);

      display.clearDisplay();
      print_line("Subscribing to topics...","n", 2, 0, 0);

      mqttClient.subscribe("SAMPLING-INTERVAL");
      mqttClient.subscribe("SENDING-INTERVAL");
      mqttClient.subscribe("SERVO-ANGLE");
    } 
    else {
      display.clearDisplay();
      print_line("MQTT connection failed","n", 2, 0, 0);
      Serial.print("Connection failed, state: ");
      Serial.println(mqttClient.state());
      delay(5000);
    }
  }
}

void lightIntensity(){
  float I = analogRead(ldr)*1.00;
  Serial.println(I);
  light_intensity += 1-(I/4096.00);
  Serial.print("lt true value ");
  Serial.println(light_intensity);
  n+=1;
}
void setup() {
  pinMode(DHT_PIN, INPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(PB_BACK, INPUT);
  pinMode(PB_OK, INPUT);
  pinMode(PB_UP, INPUT);
  pinMode(PB_DOWN, INPUT);
  pinMode(PB_SNOOZE, INPUT);
  std::cout << "connected";

  dhtSensor.setup(DHT_PIN, DHTesp::DHT22);
  servoMotor.setPeriodHertz(50);       // Standard servo PWM frequency
  servoMotor.attach(servoPin, 500, 2400);  // Pin, min/max pulse width in µs
  servoMotor.write(0); 
  Serial.begin(9600);
  if (!display.begin(SSD1306_SWITCHCAPVCC, screen_address)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  
  display.display();
  delay(1000);
  display.clearDisplay();

  WiFi.begin(ssid, password, wifi_channel);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    print_line("Connecting to WIFI", "y", 1, 0, 5);
  }
  
  print_line("Connected to WIFI", "y", 1, 0, 20);
  delay(2000);
  print_line("Updating Time...", "n", 1, 0, 35);
  configTzTime("IST-5:30", "pool.ntp.org");
  
  while (time(nullptr) < 1510644967) {
    delay(500);
  }
  
  print_line("Time config updated", "n", 1, 0, 50);
  delay(1000);
  display.clearDisplay();
  delay(2000);

  mqttClient.setServer("test.mosquitto.org", 1883);

  mqttClient.setCallback(receiveCallback);

  connectToBroker();
  
  print_line("Welcome to Medibox!", "y", 1, 0, 30);
  delay(2000);

  
}

void loop() {
  if (!mqttClient.connected()){
    Serial.println("Reconnecting  MQTT");
    connectToBroker();
  }
  check_temp_and_humidity();
  
  mqttClient.loop();
  mqttClient.publish("TEMPERATURE", String(temp).c_str());
  Serial.println("sending interval");
  Serial.println(SendingInterval);
  Serial.println("no of times");
  Serial.println(n);

  Serial.println(temp);
  Serial.println(ServoAngle);
  Serial.println(light_intensity);
  servoMotor.write(ServoAngle);
  if (SamplingInterval<=(millis()/1000)-sec_sample){
      sec_sample = millis()/1000;
      lightIntensity();
      
  }
  if (SendingInterval<=(millis()/1000)-sec_send){
      sec_send = millis()/1000;
      light_intensity /= n;
      
      Serial.println("ltintensity");
      Serial.println(light_intensity);
      
      mqttClient.publish("LIGHTit", String(light_intensity).c_str());
      n=0;
  }
  

  //update_time_with_check_alarm_and_check_warning();
  update_countdown(0);
  update_countdown(1);
  
  int columns[] = {0, 0, 0};
  int rows[] = {10, 20, 30};
  String lines[] = {
    "Time: " + String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds),
    "Alarm1: " + String(countdown_hours[0]) + ":" + (countdown_minutes[0] < 10 ? "0" : "") + String(countdown_minutes[0]) + ":" + (countdown_seconds[0] < 10 ? "0" : "") + String(countdown_seconds[0]),
    "Alarm2: " + String(countdown_hours[1]) + ":" + (countdown_minutes[1] < 10 ? "0" : "") + String(countdown_minutes[1]) + ":" + (countdown_seconds[1] < 10 ? "0" : "") + String(countdown_seconds[1])
  };
  print_lines(lines, 3, 1, columns, rows, true);
  
  if (digitalRead(PB_OK) == LOW) {
    //delay(200);
    go_to_menu();
  }
  else if (digitalRead(PB_SNOOZE) == LOW) {
    //delay(200);
    handle_snooze();
  }
  
  delay(100);
}
