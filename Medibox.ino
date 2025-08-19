#include <iostream>
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <time.h>
#include <DHTesp.h>
#include <esp32servo.h>
#include <PubSubClient.h>

#define NTP_SERVER "pool.ntp.org"

#define UTC_OFFSET_DST 0

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDERSS 0x3c

#define BUZZER 18
#define LED_1 15
#define LED_2 2
#define CANCEL 26
#define UP 35
#define DOWN 32
#define OK 33
#define DHT 12
#define servoPin 25
#define ldr 34

Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;
Servo servoMotor;

int UTC_OFFSET[]={5,30};

int n_notes=8;
int c=262;
int d=294;
int e=330;
int f=349;
int g=392;
int a=440;
int b=494;
int c_h =523;
int notes[]={c,d,e,f,g,a,b,c_h};
float ServoAngle=0.0;
float SamplingInterval=5.0;
float SendingInterval=120.0;
float temp=0.0;
float light_intensity=0.0;
unsigned long sec_sample=0;
unsigned long sec_send=0;
int n=0;

int days=0;
int hours =0;
int minutes =0;
int seconds =0;
bool alarm_enabled = false;
int n_alarms =2;
int alarm_hours[] = {9,0};
int alarm_minutes[] = {39,10};
bool alarm_triggered[] = {false,false};
bool alarm_active[] = {false,false};

int current_mode =0;
int max_modes = 4; 
String options[]={"1 - Set Time", "2 - Set Alarm", "3 - View Active Alarm", "4-Remove Alarm"};

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void print_line(String text, int text_size, int row, int column){
  display.setTextSize(text_size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column,row);
  display.println(text);

  display.display();
}

void update_time(void){
  struct tm timeinfor;
  getLocalTime(&timeinfor);

  char day_str[8];
  char hour_str[8];
  char min_str[8];
  char sec_str[8];
  strftime(day_str,8, "%d",&timeinfor);
  strftime(sec_str,8, "%S",&timeinfor);
  strftime(hour_str,8, "%H",&timeinfor);
  strftime(min_str,8, "%M",&timeinfor);
  hours = atoi(hour_str);
  minutes = atoi(min_str);
  days = atoi(day_str);  
  seconds = atoi(sec_str);
}

void print_time_now(void){
  print_line(String(days),2,0,0);
  print_line(":",2,0,30);
  print_line(String(hours),2,0,30);
  print_line(":",2,0,30);
  print_line(String(minutes),2,0,60);
  print_line(":",2,0,30);
  print_line(String(seconds),2,0,90);
}

void ring_alarm(){
  display.clearDisplay();
  print_line("Medicine Time",2,0,0);
  
  digitalWrite(LED_1, HIGH);

  while (digitalRead(CANCEL)==HIGH){
    for (int i=0; i< n_notes; i++){
      if (digitalRead(CANCEL)==LOW){
        display.clearDisplay();
        print_line("Alarm stopped",1,32,0);
        delay(200);
        digitalWrite(LED_1, LOW);
        break;
      }
      else if (digitalRead(OK)==LOW){
        display.clearDisplay();
        print_line("Alarm snoozed for 5 mins",1,32,0);
        delay(5*60*1000);
      }
      tone(BUZZER, notes[i]);
      delay(500);
      noTone(BUZZER);
      delay(200);
    }
  }
  int UTC_Offset = (UTC_OFFSET[0]*60*60) + (UTC_OFFSET[1]*60);
  configTime(UTC_Offset, UTC_OFFSET_DST, NTP_SERVER);
}

void update_time_with_check_alarm () {
  display.clearDisplay();
  update_time();
  print_time_now();
  bool alarm_enabled = false;

  if (alarm_active[0]==true){
    alarm_enabled = true;
  }
  else if (alarm_active[1]==true){
    alarm_enabled = true;
  }
  if (alarm_enabled ==  true){
    delay(500);
  }
  if (alarm_enabled ==  true){
    for (int i=0; i<n_alarms; i++){
      if (alarm_triggered[i] ==false & alarm_active[i] ==true & hours == alarm_hours[i] & minutes==alarm_minutes[i]){
        ring_alarm();
        alarm_triggered[i] = true;
        alarm_active[i] = false;
      }
    }
  }
}

const char* wait_for_button_press(){
  while (true){
    if (digitalRead(UP) == 0){
      delay(200);
      delay(200);
      return "UP";
    }
    else if (digitalRead(DOWN) == 0){
      delay(200);
      delay(200);
      return "DOWN";
    }
    else if (digitalRead(CANCEL) == 0){
      delay(200);
      delay(200);
      return "CANCEL";
    }
    else if (digitalRead(OK) == 0){
      delay(200);
      delay(200);
      return "OK";
    }
    update_time();
  }
}

void set_time(){
  while (true){
    display.clearDisplay();
    print_line("Enter hour from UTC:" + String(UTC_OFFSET[0]),0,0,2);

    String pressed = wait_for_button_press();
    if (pressed == "UP"){
      delay(200);
      UTC_OFFSET[0] +=1;
      UTC_OFFSET[0]= UTC_OFFSET[0] % 24;
    }
    else if (pressed == "DOWN"){
        delay(200);
        UTC_OFFSET[0] -= 1;
        if (UTC_OFFSET[0]<0){
          UTC_OFFSET[0]=23;
        }
    }
    else if (pressed == "OK"){
        delay(200);
        break;
    }
    else if (pressed == "CANCEL"){
        delay(200);
        break;
    }
  }
  while (true){
    display.clearDisplay();
    print_line("Enter minutes from UTC:" + String(UTC_OFFSET[1]), 0, 0,2);

    String pressed = wait_for_button_press();
    if (pressed == "UP"){
      delay(200);
      UTC_OFFSET[1] +=1;
      UTC_OFFSET[1] = UTC_OFFSET[1] % 60;
    }
    else if (pressed == "DOWN"){
      delay(200);
      UTC_OFFSET[1] -= 1;
      if (UTC_OFFSET[1]<0){
        UTC_OFFSET[1]=59;
      }
    }
    else if (pressed == "OK"){
      delay(200);
      UTC_OFFSET[1] = (UTC_OFFSET[1]);
      break;
    }

    else if (pressed == "CANCEL"){
      delay(200);
      break;
    }
  }
  int UTC_Offset = (UTC_OFFSET[0]*60*60) + (UTC_OFFSET[1]*60);
  display.clearDisplay();
  print_line("Enter the direction from UTC", 0, 0, 2);
  print_line("+ up", 0, 16, 2);
  print_line("- Down", 0, 24, 2);
  String pressed = wait_for_button_press();
  if (pressed == "DOWN") {
    UTC_Offset *= -1;
  }
  
  configTime(UTC_Offset, UTC_OFFSET_DST, NTP_SERVER);
  delay(2000);
  display.clearDisplay();
  print_line("Time is set", 0, 0, 2);
  delay(1000);
}

void active_alarm(){
  display.clearDisplay();
  int x=0;
  for (int i=0; i<n_alarms;i++){
    if (alarm_active[i]==true){
      
      print_line("Alarm " + String(i+1) , 1, x, 2);
      x +=9;
      print_line( String(i+1) +":"+ String(alarm_hours[i]) +":"+ String(alarm_minutes[i]), 1, x, 0);
      x +=9;
    }
  }
  delay(1000);
}

void set_alarm(int alarm){
  alarm -=1;
  alarm_active[alarm] = true;
  alarm_triggered[alarm] = false;
  alarm_enabled=true;
  int temp_hour = alarm_hours[alarm];
  while (true){
    display.clearDisplay();
    print_line("Enter hour: " + String(temp_hour), 1, 0, 2);

    String pressed = wait_for_button_press();
    if (pressed == "UP") {
      delay(200);
      temp_hour +=1;
      temp_hour = temp_hour % 24;
    }
    else if (pressed == "DOWN"){
      delay(200);
      temp_hour -=1;
      if (temp_hour < 0) {
        temp_hour =23;
      }
    }
    else if (pressed == "OK"){
      delay(200);
      alarm_hours[alarm] = temp_hour;
      break;
    }
    else if (pressed == "CANCEL"){
      delay(200);
      break;
    }
  }
  int temp_minute = alarm_minutes[alarm];
  while (true){
    display.clearDisplay();
    print_line("Enter minute: " + String(temp_minute),1,0,2);
    String pressed = wait_for_button_press();
    if (pressed == "UP"){
      delay(200);
      temp_minute +=1;
      temp_minute = temp_minute % 60;
    }
    else if (pressed == "DOWN"){
      delay(200);
      temp_minute -= 1;
      if (temp_minute<0){
           temp_minute=59;
      }
    }
    else if (pressed == "OK"){
      delay(200);
      alarm_minutes[alarm] = temp_minute;
      break;
    }
    else if (pressed == "CANCEL"){
      delay(200);
      break;
    }
  }
  display.clearDisplay();
  print_line("Alarm is set", 1, 0, 2);
  print_line("Alarm " + String(alarm+1) +"  "+ String(alarm_hours[alarm]) +":"+ String(alarm_minutes[alarm]), 1, 8, 0);
  delay(2000);
  int UTC_Offset = (UTC_OFFSET[0]*60*60) + (UTC_OFFSET[1]*60);
  configTime(UTC_Offset, UTC_OFFSET_DST, NTP_SERVER);
}

void remove_alarm(int alarm){
  alarm_active[alarm-1]=false;
  
  display.clearDisplay();
  print_line(String(alarm) +" is removed", 2, 4, 0);
  
}

void run_mode(int mode){
  if (mode ==0){
    display.clearDisplay();
    print_line("Enter alarm you want to remove",1,0,0);
    print_line("UP-1",1,16,0);
    print_line("DOWN-2",1,24,0);
    String pressed2= wait_for_button_press();
    if (pressed2=="UP"){
      remove_alarm(1);
    }
    else if (pressed2=="DOWN"){
      remove_alarm(2);
    }  
  }

  else if (mode == 1){
    set_time();
  }
  else if (mode ==2){
    display.clearDisplay();
    print_line("Enter alarm (1-2)",1,0,0);
    print_line("UP-1",1,8,0);
    print_line("DOWN-2",1,16,0);
    String pressed1= wait_for_button_press();
    if (pressed1=="UP"){
      set_alarm(1);
      alarm_enabled = true;
    }
    else if (pressed1=="DOWN"){
      set_alarm(2);
      alarm_enabled = true;
    }
    
  }
  else if (mode == 3){
    if (alarm_active[0] == true){
       active_alarm();
    }
    else if (alarm_active[1] == true){
      active_alarm();
    }
    else{
      display.clearDisplay();
      print_line("No alarms are active",1,0,0);
    }
  }
}

void go_to_menu(){
  while (digitalRead(CANCEL)==0){
    display.clearDisplay();
    for (int i=0; i<5;i++){
      print_line("1-set time",1,0,0);
      print_line("2-set alarm",1,8,0);
      print_line("3-View Active Alarm",1,16,0);
      print_line("4-remove alarm",1,24,0);
      delay(200);
    }
    if (alarm_enabled == true){
      delay(500);
    }
    String pressed= wait_for_button_press();
    display.clearDisplay();
    while(pressed != "OK" & (pressed == "UP" | pressed == "DOWN")){
      if (pressed=="UP"){
        current_mode +=1;
        current_mode %= max_modes;
      }
      else if (pressed == "DOWN"){
        current_mode -= 1;
        if (current_mode < 0){
          current_mode = max_modes -1;
        }
        
      }
      if ( current_mode == 0){
        display.clearDisplay();
        print_line(String(String(options[3])) ,1,0,0);
        delay(200);
      }
      else{
        display.clearDisplay();
        print_line(String(options[current_mode-1]) ,1,0,0);
        delay(200);
      }
      display.clearDisplay();
      pressed = wait_for_button_press();
      delay(200);
    }
    Serial.println(current_mode);
    delay(200);
    run_mode(current_mode);
    
  }
}

void check_temp(void){
  TempAndHumidity data = dhtSensor.getTempAndHumidity();
  bool all_good = true;
  temp= data.temperature;
  if (data.temperature > 32){
    all_good = false;
    digitalWrite(LED_2, HIGH);
    print_line("TEMP HIGH", 1, 40, 0);
  }
  else if(data.temperature < 24){
    all_good = false;
    digitalWrite(LED_2,HIGH);
    print_line("TEMP LOW", 1, 40, 0);
  }
  if(data.humidity > 80){
    all_good = false;
    digitalWrite(LED_2,HIGH);
    print_line("HUMD HIGH", 1, 50, 0);
  }
  else if(data.humidity < 65){
    all_good = false;
    digitalWrite(LED_2,HIGH);
    print_line("HUMP LOW", 1, 50, 0);
  }
  if (all_good){
    digitalWrite(LED_2, LOW);
  }
}

void connectToBroker(){
  while(!mqttClient.connected()){
    Serial.println("Attempting mqtt");
    display.clearDisplay();
    print_line("Attempting mqtt",2,0,0);
    delay(3000);
    if(mqttClient.connect("ESP32-463517108394")){
      Serial.println("mqtt connected");
      print_line("Connected to mqtt",2,0,0);
      delay(3000);
      display.clearDisplay();
      print_line("Connecting to subscribe",2,0,0);
      mqttClient.subscribe("sampling_interval");
      mqttClient.subscribe("sending_interval");
      mqttClient.subscribe("MINIMUM-SERVO-ANGLE");
    }
    else{
      display.clearDisplay();
      print_line("failed mqtt",2,0,0);
      Serial.print("Failed");
      Serial.print(mqttClient.state());
      delay(5000);
    }
  }
}

void receiveCallback(char* topic, byte* payload, unsigned int length){
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char payloadCharAr[length];
  for(int i=0 ; i <length; i++){
    Serial.println((char)payload[i]);
    payloadCharAr[i]=(char)payload[i];
    Serial.print("Message arrived [");
  }
  Serial.println();
  if(strcmp(topic,"sampling_interval")==0){
     SamplingInterval = atof(payloadCharAr);
  }
  else if(strcmp(topic,"sending_interval")==0){
    SendingInterval = atof(payloadCharAr);
  }  
  else if (strcmp(topic, "MINIMUM-SERVO-ANGLE") == 0) {
    ServoAngle = atof(payloadCharAr);
  }
}

void lightIntensity(){
  float I = analogRead(ldr)*1.00;
  light_intensity += 1-(I/4096.00);
  n+=1;

}
void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(LED_1,OUTPUT);
  pinMode(BUZZER,OUTPUT);
  pinMode(LED_2,OUTPUT);
  pinMode(CANCEL ,INPUT);
  pinMode(UP ,INPUT);
  pinMode(DOWN ,INPUT);
  pinMode(OK ,INPUT);
  std::cout << "connected";
  dhtSensor.setup(DHT, DHTesp::DHT22);
  servoMotor.setPeriodHertz(50);       // Standard servo PWM frequency
  servoMotor.attach(servoPin, 500, 2400);  // Pin, min/max pulse width in µs
  servoMotor.write(0); 
  std::cout << "connected";
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDERSS)){
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.display();
  delay(2000);

  display.clearDisplay();
  print_line("Welcome to Medibox", 2, 4, 0);
  delay(3000);

  WiFi.begin("Wokwi-GUEST");
  while (WiFi.status() != WL_CONNECTED){
    delay(250);
    display.clearDisplay();
    print_line("Connecting to WiFiiii",2,0,0);
    std::cout<< "connected";
  }

  display.clearDisplay();
  print_line("Connected to wifiiiii",2 ,0, 0);
  
  mqttClient.setServer("test.mosquitto.org", 1883);

  mqttClient.setCallback(receiveCallback);

  connectToBroker();
  
  int UTC_Offset=UTC_OFFSET[0];
  configTime(UTC_Offset, UTC_OFFSET_DST, NTP_SERVER);
  lightIntensity();
  
}

void loop() {
  Serial.println("loop");
  if (!mqttClient.connected()){
    Serial.println("Reconnecting to MQTT");
    connectToBroker();
  }
  check_temp();
  
  mqttClient.loop();
  servoMotor.write(ServoAngle);
  if (SamplingInterval<=(millis()/1000)-sec_sample){
      sec_sample = millis()/1000;
      lightIntensity();
      
  }
  if (SendingInterval<=(millis()/1000)-sec_send){
      sec_send = millis()/1000;
      light_intensity /= n;
      n=0;
      Serial.println("ltintensity");
      Serial.println(light_intensity);
      mqttClient.publish("TEMP", String(temp).c_str());
      mqttClient.publish("Light intensity", String(light_intensity).c_str());

  }

  update_time_with_check_alarm();
  if (digitalRead(CANCEL) == LOW){
    delay(1000);
    Serial.println("Menu");
    go_to_menu();
  }
}
