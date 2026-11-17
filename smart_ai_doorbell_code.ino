
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";

// AI Thinker ESP32-CAM pin configuration
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

// Component pins
#define BUTTON_PIN 14
#define BUZZER_PIN 13

WebServer server(80);

bool doorbellPressed = false;
bool buzzerActive = false;

unsigned long buzzerStart = 0;
unsigned long lastButtonTime = 0;

const unsigned long debounceDelay = 50;

// Ring doorbell
void ringDoorbell() {
  digitalWrite(BUZZER_PIN, HIGH);

  buzzerActive = true;
  buzzerStart = millis();
  doorbellPressed = true;

  Serial.println("DOORBELL RINGING!");
}

// Initialize camera
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
  config.pixel_format = PIXFORMAT_JPEG;

  config.frame_size = FRAMESIZE_VGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  }

  esp_err_t result = esp_camera_init(&config);

  if (result != ESP_OK) {
    Serial.printf("Camera failed: 0x%x\n", result);

    while (true) {
      delay(1000);
    }
  }

  Serial.println("Camera initialized!");
}

// Capture camera image
void handleCapture() {
  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  server.sendHeader("Cache-Control", "no-store");

  server.send_P(
    200,
    "image/jpeg",
    (const char*)fb->buf,
    fb->len
  );

  esp_camera_fb_return(fb);
}

// Website
void handleHome() {
  String page = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>Smart AI Doorbell</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">

    <style>
      body {
        background: #101820;
        color: white;
        text-align: center;
        font-family: Arial;
      }

      h1 {
        color: #00d4ff;
      }

      img {
        width: 90%;
        max-width: 500px;
        border-radius: 10px;
      }

      button {
        background: #00bcd4;
        color: white;
        padding: 15px 30px;
        border: none;
        border-radius: 8px;
        font-size: 18px;
        margin: 10px;
      }
    </style>
  </head>

  <body>
    <h1>SMART AI DOORBELL</h1>

    <h3>Camera Snapshot</h3>

    <img id="camera" src="/capture">

    <br><br>

    <button onclick="takePicture()">
      Refresh Camera
    </button>

    <button onclick="ring()">
      Ring Doorbell
    </button>

    <h3 id="status">Doorbell Ready</h3>

    <script>
      function takePicture() {
        document.getElementById("camera").src =
          "/capture?t=" + Date.now();
      }

      function ring() {
        fetch("/ring");
      }

      async function updateStatus() {
        try {
          const response = await fetch("/status");
          const data = await response.json();

          document.getElementById("status").innerText =
            data.ringing ? "DOORBELL RINGING!" : "Doorbell Ready";
        }
        catch(error) {
          document.getElementById("status").innerText =
            "Connection Error";
        }
      }

      setInterval(updateStatus, 1000);
    </script>
  </body>
  </html>
  )rawliteral";

  server.send(200, "text/html", page);
}

// Remote doorbell
void handleRing() {
  ringDoorbell();
  server.send(200, "text/plain", "Doorbell activated!");
}

// Doorbell status
void handleStatus() {
  String json = "{\"ringing\":";

  json += doorbellPressed ? "true" : "false";
  json += "}";

  server.send(200, "application/json", json);
}

// Setup
void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("SMART AI DOORBELL STARTING...");

  startCamera();

  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleHome);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/ring", HTTP_GET, handleRing);
  server.on("/status", HTTP_GET, handleStatus);

  server.begin();

  Serial.println("Web server started!");
}

// Main loop
void loop() {
  server.handleClient();

  if (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - lastButtonTime > debounceDelay) {
      lastButtonTime = millis();
      ringDoorbell();
    }
  }

  if (buzzerActive &&
      millis() - buzzerStart >= 300) {

    digitalWrite(BUZZER_PIN, LOW);

    buzzerActive = false;
    doorbellPressed = false;
  }
}
