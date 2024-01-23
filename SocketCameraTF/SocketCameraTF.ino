#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include "esp_camera.h"
#include <stdio.h>
#include <stdlib.h>

#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "model.h"


namespace {
  tflite::ErrorReporter* error_reporter = nullptr;
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr;
  TfLiteTensor* model_input = nullptr;
  TfLiteTensor* model_output = nullptr;
  int inference_count = 0;

  constexpr int kTensorArenaSize = 3465 * 1024;
  static uint8_t *tensor_arena;//uint8_t tensor_arena[kTensorArenaSize];
}


// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    4
#define SIOD_GPIO_NUM    18
#define SIOC_GPIO_NUM    23

#define Y9_GPIO_NUM      36
#define Y8_GPIO_NUM      37
#define Y7_GPIO_NUM      38
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM      35
#define Y4_GPIO_NUM      14
#define Y3_GPIO_NUM      13
#define Y2_GPIO_NUM      34
#define VSYNC_GPIO_NUM   5
#define HREF_GPIO_NUM    27
#define PCLK_GPIO_NUM    25

#define LED_GPIO_NUM     22

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP  10

//const char* ssid = "DIGIFIBRA-hh5K";
//const char* password = "9DK6sKF5GN";
const char* ssid = "3e8m/s";
const char* password = "Iwannarock123";
String serverName = "192.168.229.240";
const int serverPort = 9879;

WiFiClient client;

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  if (tensor_arena == NULL) {
    tensor_arena = (uint8_t *) heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); //MALLOC_CAP_INTERNAL
  }
  if (tensor_arena == NULL) {
    Serial.printf("Couldn't allocate memory of %d bytes\n", kTensorArenaSize);
    return;
  }

  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  model = tflite::GetModel(depth_model);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    TF_LITE_REPORT_ERROR(error_reporter,
                         "Model provided is schema version %d not equal "
                         "to supported version %d.",
                         model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  static tflite::AllOpsResolver resolver;

  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  TfLiteStatus allocate_status = interpreter->AllocateTensors();
  if (allocate_status != kTfLiteOk) {
    TF_LITE_REPORT_ERROR(error_reporter, "AllocateTensors() failed");
    return;
  }

  model_input = interpreter->input(0);
  model_output = interpreter->output(0);

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
  config.jpeg_quality = 4;
  config.fb_count = 2;
  config.frame_size = FRAMESIZE_240X240;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  WiFi.mode(WIFI_STA);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  while (Serial.available() > 0) {
    String teststr = Serial.readString();
    teststr.trim();
    Serial.print("\nInput string = ");
    Serial.println(teststr);
    if(teststr == "shot") {
      capturePhoto();
    }
  }
}

void capturePhoto() {

  bool data_sent;
  camera_fb_t * fb = NULL;

  fb = esp_camera_fb_get();
  if(!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  Serial.println("Camera capture succesfully");

  if (!psramFound()) {
    Serial.println("-Error no psram found-\n");
    return;
  }
  void *ptrVal = NULL;
  uint32_t ARRAY_LENGTH = fb->width * fb->height * 3;
  if (heap_caps_get_free_size( MALLOC_CAP_SPIRAM) <  ARRAY_LENGTH) {
    Serial.println("-Error not enough free psram to store rgb data-\n");
    return;
  }
  ptrVal = heap_caps_malloc(ARRAY_LENGTH, MALLOC_CAP_SPIRAM);
  uint8_t *rgb = (uint8_t *)ptrVal;
  bool jpeg_converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb);
  if (!jpeg_converted) {
    Serial.println("-Error converting image to RGB-\n");
    return;
  }

  applyInference(rgb, fb->width*fb->height*3);

  data_sent = sendData(rgb, fb->width, fb->width*fb->height*3, "rgb");
  if (data_sent == true) {
    Serial.println("Camera frame transsmission done");
  }
  else {
    Serial.println("Camera frame transsmission failed");
  }

  heap_caps_free(ptrVal);
  esp_camera_fb_return(fb);
}

void applyInference(uint8_t * image, int length) {

  bool data_sent;

  for (uint32_t i = 0; i < length; i+=3) {
    model_input->data.uint8[i] =   (uint8_t) image[i];
    model_input->data.uint8[i+1] = (uint8_t) image[i+1];
    model_input->data.uint8[i+2] = (uint8_t) image[i+2];
  }

  Serial.println("Running inference on depth model...");
  if(interpreter->Invoke() != kTfLiteOk) {
    Serial.println("There was an error invoking the TF interpreter!");
    return;
  }
  else {
    Serial.println("Inference done");
  }

  void *outVal = NULL;
  uint32_t ARRAY_LENGTH = 48*48;
  if (heap_caps_get_free_size( MALLOC_CAP_SPIRAM) <  ARRAY_LENGTH) {
    Serial.println("-Error not enough free psram to store rgb data-\n");
    return;
  }
  outVal = heap_caps_malloc(ARRAY_LENGTH, MALLOC_CAP_SPIRAM);
  uint8_t *depth = (uint8_t *)outVal;

  for (uint32_t i = 0; i < 48*48; i++) {
    depth[i] = (uint8_t) model_output->data.uint8[i];
  }

  data_sent = sendData(depth, 48, 48*48, "gray");
  if (data_sent == true) {
    Serial.println("Depth map transsmission done");
  }
  else {
    Serial.println("Depth map transsmission failed");
  }
  heap_caps_free(outVal);

}

String PrintHex8(uint8_t *data, uint8_t length) {

  String block;
  char tmp[length*2+1];
  byte first ;
  int j=0;
  for (uint8_t i=0; i<length; i++) 
  {
    first = (data[i] >> 4) | 48;
    if (first > 57) tmp[j] = first + (byte)39;
    else tmp[j] = first ;
    j++;

    first = (data[i] & 0x0F) | 48;
    if (first > 57) tmp[j] = first + (byte)39; 
    else tmp[j] = first;
    j++;
  }
  tmp[length*2] = 0;
  block = String(tmp);
  return block;
}

bool sendData(uint8_t * image, int size, int length, String format) {

  bool completed = false;
  Serial.println("Connecting to server: " + serverName);
  if (client.connect(serverName.c_str(), serverPort)) {

    Serial.println("Connection successful!");
    String msg = "Image-Format=" + format + "/Image-Size=" + String(size) + "/Image-Data=";
    client.print(msg);

    uint8_t subimg[64] = {0};
    for (int i=0; i<length; i+=64) {
      for (uint8_t j=0; j<64; j++) {
        subimg[j] = (uint8_t) image[i+j];
      }
      String data = PrintHex8(subimg, 64);
      client.print(data);
    }
    client.stop();
    completed = true;
  }
  else {
    Serial.println("Connection to " + serverName +  " failed.");
  }
  return completed;
}