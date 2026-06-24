#include <Sign_language_translator_inferencing.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <Wire.h>
#include <U8g2lib.h>

// Wi-Fi credentials
const char* ssid = "KRC";
const char* password = "12345678";

// ESP32-CAM pin mappings
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define EI_WIDTH      96
#define EI_HEIGHT     96
#define EI_CHANNELS   3

// Use PSRAM
float *input_buffer = NULL;

// Create U8g2 object
// GPIO15 = SCL, GPIO14 = SDA
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ 15, /* data=*/ 14);

String translateGesture(String gesture) {
  gesture.trim();
  if (gesture == "HELLO!!SIR!!") return "Hello sir";
  if (gesture == "This is") return "This is";
  if (gesture == "Our project") return "Our project";
  if (gesture == "Demo on") return "Demo on";
  if (gesture == "Sign Language Translator") return "Sign Language";
  if (gesture == "by models!!") return "by models";
  if (gesture == "Trained on") return "Trained on";
  if (gesture == "AI") return "AI";
  if (gesture == "edge impulse") return "edge impulse";
  if (gesture == "we worked") return "we worked";
  if (gesture == "on this project") return "on project";
  if (gesture == "for 2 weeks") return "for 2 weeks";
  if (gesture == "a prototype") return "a prototype";
  if (gesture == "model...") return "model";
  return "Gesture unknown!";
}

bool decode_and_preprocess(camera_fb_t *fb) {
  if (fb->format != PIXFORMAT_RGB565) {
    Serial.println("Frame format not RGB565!");
    return false;
  }

  uint16_t *src = (uint16_t *)fb->buf;

  for (int y = 0; y < EI_HEIGHT; y++) {
    for (int x = 0; x < EI_WIDTH; x++) {
      int idx = y * EI_WIDTH + x;
      uint16_t pixel = src[idx];

      uint8_t r = ((pixel >> 11) & 0x1F) << 3;
      uint8_t g = ((pixel >> 5) & 0x3F) << 2;
      uint8_t b = (pixel & 0x1F) << 3;

      int pixel_idx = idx * 3;
      input_buffer[pixel_idx + 0] = (float)r;
      input_buffer[pixel_idx + 1] = (float)g;
      input_buffer[pixel_idx + 2] = (float)b;
    }
  }
  return true;
}

int get_signal_data(size_t offset, size_t length, float *out_ptr) {
  memcpy(out_ptr, input_buffer + offset, length * sizeof(float));
  return 0;
}

void startCamera() {
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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_96X96;
  config.jpeg_quality = 12;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed 0x%x\n", err);
    while (true);
  }
}

void setup() {
  Serial.begin(57600);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  if(psramFound()) {
    Serial.println("PSRAM found!");
  } else {
    Serial.println("PSRAM NOT found! May crash on large buffers.");
  }

  input_buffer = (float*) ps_malloc(EI_WIDTH * EI_HEIGHT * EI_CHANNELS * sizeof(float));
  if (!input_buffer) {
    Serial.println("Failed to allocate input buffer!");
    while(true);
  }

  startCamera();

  // Initialize I2C on custom pins
  Wire.begin(14, 15);

  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 15, "System Ready...");
  u8g2.sendBuffer();

  WiFi.begin(ssid, password);
  Serial.print("WiFi Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
  Serial.println("IP: " + WiFi.localIP().toString());
}

void loop() {
  camera_fb_t *fb = NULL;

  for (int i = 0; i < 3; i++) {
    fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
    delay(30);
  }

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture failed");
    return;
  }

  if (!decode_and_preprocess(fb)) {
    Serial.println("Preprocess failed");
    esp_camera_fb_return(fb);
    return;
  }

  signal_t signal;
  signal.total_length = EI_WIDTH * EI_HEIGHT * EI_CHANNELS;
  signal.get_data = &get_signal_data;

  ei_impulse_result_t result = { 0 };
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

  String displayText;

  if (res == EI_IMPULSE_OK) {
    float max_val = 0.0;
    int best_idx = -1;

    Serial.println("Label scores:");
    for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
      float val = result.classification[ix].value;
      const char* label = result.classification[ix].label;
      Serial.printf("  %s: %.2f\n", label, val);
      if (val > max_val) {
        max_val = val;
        best_idx = ix;
      }
    }

    if (best_idx >= 0 && max_val >= 0.4f) {
      String gesture = String(result.classification[best_idx].label);
      gesture.trim();
      String meaning = translateGesture(gesture);
      Serial.printf("Detected: %s (%.2f)\n", gesture.c_str(), max_val);
      Serial.println("Meaning: " + meaning);
      displayText = meaning;
    } else {
      Serial.println("Gesture unknown (low confidence)");
      displayText = "Gesture unknown!";
    }
  } else {
    Serial.println("Inference error");
    displayText = "Inference error!";
  }

  // Display on OLED
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(2, 20, "Gesture:");
  u8g2.setCursor(2, 37);
  u8g2.print(displayText);
  u8g2.sendBuffer();

  esp_camera_fb_return(fb);
  delay(1000);
}
