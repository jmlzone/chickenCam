#include "esp_camera.h"
#include <WiFi.h>
#include <Time.h>
#include <TimeLib.h>
#include <ESP32Servo.h>
#include "esp_http_server.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "ptl.h"


//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE
#define CAMERA_MODEL_AI_THINKER
#define TEMPSENSOR
#include "camera_pins.h"
#include "mycam.h"

/* Don't hardwire the IP address or we won't get the benefits of the pool.
 *  Lookup the IP address for the host name instead */
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "1.north-america.pool.ntp.org";
// GPIO where the DS18B20 is connected to
const int oneWireBus = 15;     
// Setup a oneWire instance to communicate with any OneWire devices
#ifdef TEMPSENSOR
OneWire oneWire(oneWireBus);
// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
#endif

time_t seconds;
time_t ltime;
time_t diff_time;
struct tm  timeinfo;
struct tm * gmtt;
extern httpd_handle_t camera_httpd;

void startCameraServer();
Servo servoV;
Servo servoH;
bool streamConnected =  false;
//float temperatureC = 0.0;
float temperatureF = 0.0;
int fl = 0;
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  setenv("TZ", MYTZ, 1);
  tzset();
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }


#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  //esp_err_t err = esp_camera_init(&config);
  //if (err != ESP_OK) {
  //  Serial.printf("Camera init failed with error 0x%x", err);
  //  return;
 // }
 esp_err_t err;
  while ((err = esp_camera_init(&config)) != ESP_OK) {
    delay(500);
    Serial.print("c");
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_SVGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif
  WiFi.begin(ssid, pass);
  WiFi.setHostname(myHostname);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  seconds = now();
  gmtt = gmtime(&seconds);
  ltime = mktime(gmtt);
  diff_time = seconds - ltime;
  Serial.print("GMT time offset is seconds-ltime(gmt): ");
  Serial.print(diff_time);
  Serial.println(" Seconds");
  pinMode(WHITE_LED, OUTPUT);
  pinMode(RED_LED,OUTPUT);
  #ifdef TEMPSENSOR
  sensors.begin();
  #endif
  streamDisconnect();
  Serial.println("Setup Done.");
}
void loop() {
  if(streamConnected) {
      String ptr = "time is:";
      if(!getLocalTime(&timeinfo)){
	Serial.println("Failed to obtain time");
      } else {
	ptr +=asctime(&timeinfo);
	Serial.println(ptr);
      }
    } else {
      #ifdef TEMPSENSOR
      sensors.requestTemperatures(); 
      //temperatureC = sensors.getTempCByIndex(0);
      temperatureF = sensors.getTempFByIndex(0);
      //Serial.print(temperatureC);
      //Serial.println("ºC");
      Serial.print(temperatureF);
      Serial.println("ºF");
      #endif
    }
  delay(10*60*60);
}

void streamDisconnect() {
  if(streamConnected) {
    servoV.detach();
    servoH.detach();
    flashLight(0);
  }
  streamConnected = false;
  pinMode(WHITE_LED, OUTPUT);
  digitalWrite(WHITE_LED,0);
  digitalWrite(RED_LED,1);
  Serial.println("Stream Disconnected");
}
void streamConnect() {
  servoV.attach(VERT_SERVO);
  servoH.attach(HORIZ_SERVO);
  servoV.write(90);
  servoH.write(90);
  streamConnected = true;
  digitalWrite(RED_LED,0); // tally light on
  Serial.println("Stream Connected");
}



 void servoMove(int axis, int dir){
   int p;
   if(axis==VERT) {
     p = servoV.read();
   } else {
     p = servoH.read();
   }
   if(dir==1) {
     p = p+10;
     if (p>180) { p=180;}
   } else {
     p = p-10;
     if (p<0) { p=0;}
   }
   if(axis==VERT) {
     servoV.write(p);
   } else {
     servoH.write(p);
   }
}

void flashLight(int val){
  if (val==0) {
      fl=0;
  } else {
    fl=fl+val;
    if(fl > 255) {
      fl=0;
    }
  }
  analogWrite(WHITE_LED,fl);
}
