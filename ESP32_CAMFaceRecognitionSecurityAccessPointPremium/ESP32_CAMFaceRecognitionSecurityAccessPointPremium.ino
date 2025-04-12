#include "esp_camera.h"
#include "esp_timer.h"
#include "Arduino.h"
#include "fd_forward.h"
#include "fr_forward.h"
#include "fr_flash.h"

#include <Preferences.h>

#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include "esp_http_server.h"
#include "camera_index.h"

#define ENROLL_CONFIRM_TIMES 5
#define FACE_ID_SAVE_NUMBER 100

#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"

using namespace websockets;

const char* default_ssid = "esp32";
const char* default_password = "12345678";
char ssid[32];
char password[32];

#define relay_pin 2

int currentMode = 0;

Preferences preferences;

typedef struct
{
  uint8_t *image;
  box_array_t *net_boxes;
  dl_matrix3d_t *face_id;
} http_img_process_result;

typedef enum
{
  START_STREAM,
  START_DETECT,
  SHOW_FACES,
  START_RECOGNITION,
  START_ENROLL,
  ENROLL_COMPLETE,
  DELETE_ALL,
  WIFI_SETTINGS,
} en_fsm_state;

typedef struct
{
  char enroll_name[ENROLL_NAME_LEN];
} httpd_resp_value;

esp_err_t index_handler(httpd_req_t *req);

class ActiveMode {
public:
  void setup();
  void loop();
private:
  camera_fb_t * fb = NULL;
  unsigned long door_opened_millis = 0;
  long interval = 5000;
  mtmn_config_t mtmn_config;
  face_id_name_list st_face_list;
  dl_matrix3du_t * aligned_face = NULL;

  void app_facenet_main();
  mtmn_config_t app_mtmn_config();
  void open_door();
};

class AdminMode {
public:
  void setup();
  void loop();
private:
  camera_fb_t * fb = NULL;
  WebsocketsServer socket_server;
  long current_millis;
  long last_detected_millis = 0;
  unsigned long door_opened_millis = 0;
  long interval = 5000;
  bool face_recognised = false;

  mtmn_config_t mtmn_config;
  face_id_name_list st_face_list;
  dl_matrix3du_t *aligned_face = NULL;
  httpd_handle_t camera_httpd = NULL;

  en_fsm_state g_state = START_RECOGNITION;
  httpd_resp_value st_name;

  void app_facenet_main();
  void app_httpserver_init();
  static mtmn_config_t app_mtmn_config();
  static esp_err_t index_handler(httpd_req_t *req);
  void handle_message(WebsocketsClient &client, WebsocketsMessage msg);
  void open_door(WebsocketsClient &client);
  esp_err_t send_face_list(WebsocketsClient &client);
  esp_err_t delete_all_faces(WebsocketsClient &client);
  esp_err_t update_wifi_settings(WebsocketsClient &client, const char* new_ssid, const char* new_password);
};

ActiveMode activeMode;
AdminMode adminMode;

void setup() {
  Serial.begin(115200);
  Serial.println();

  preferences.begin("settings", false);
  currentMode = preferences.getInt("mode", 0);
  
  if (preferences.isKey("ssid")) {
    preferences.getString("ssid", ssid, sizeof(ssid));
  } else {
    strcpy(ssid, default_ssid);
  }
  
  if (preferences.isKey("password")) {
    preferences.getString("password", password, sizeof(password));
  } else {
    strcpy(password, default_password);
  }

  pinMode(relay_pin, OUTPUT);
  digitalWrite(relay_pin, LOW);

  if (currentMode == 0) {
    activeMode.setup();
  } else {
    adminMode.setup();
  }

  int nextMode = 1 - currentMode;
  preferences.putInt("mode", nextMode);

  preferences.end();
}

void loop() {
  if (currentMode == 0) {
    activeMode.loop();
  } else {
    adminMode.loop();
  }
}

void ActiveMode::setup() {
  Serial.println("Starting Active Mode");

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

  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

  mtmn_config = app_mtmn_config();
  app_facenet_main();
}

void ActiveMode::app_facenet_main() {
  face_id_name_init(&st_face_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);
  aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
  read_face_id_from_flash_with_name(&st_face_list);
}

mtmn_config_t ActiveMode::app_mtmn_config() {
  mtmn_config_t mtmn_config = {0};
  mtmn_config.type = FAST;
  mtmn_config.min_face = 80;
  mtmn_config.pyramid = 0.707;
  mtmn_config.pyramid_times = 4;
  mtmn_config.p_threshold.score = 0.6;
  mtmn_config.p_threshold.nms = 0.7;
  mtmn_config.p_threshold.candidate_number = 20;
  mtmn_config.r_threshold.score = 0.7;
  mtmn_config.r_threshold.nms = 0.7;
  mtmn_config.r_threshold.candidate_number = 10;
  mtmn_config.o_threshold.score = 0.7;
  mtmn_config.o_threshold.nms = 0.7;
  mtmn_config.o_threshold.candidate_number = 1;
  return mtmn_config;
}

void ActiveMode::open_door() {
  if (digitalRead(relay_pin) == LOW) {
    digitalWrite(relay_pin, HIGH);
    Serial.println("Door Unlocked");
    door_opened_millis = millis();
  }
}

void ActiveMode::loop() {
  dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, 320, 240, 3);
  http_img_process_result out_res = {0};
  out_res.image = image_matrix->item;

  fb = esp_camera_fb_get();

  if (fb) {
    fmt2rgb888(fb->buf, fb->len, fb->format, out_res.image);

    out_res.net_boxes = face_detect(image_matrix, &mtmn_config);

    if (out_res.net_boxes) {
      if (align_face(out_res.net_boxes, image_matrix, aligned_face) == ESP_OK) {
        out_res.face_id = get_face_id(aligned_face);

        if (st_face_list.count > 0) {
          face_id_node *f = recognize_face_with_name(&st_face_list, out_res.face_id);
          if (f) {
            Serial.printf("Face recognized: %s\n", f->id_name);
            open_door();
          } else {
            Serial.println("Face not recognized");
          }
        }
        dl_matrix3d_free(out_res.face_id);
      }
    }
    esp_camera_fb_return(fb);
    fb = NULL;
  }

  if (millis() - door_opened_millis > interval) {
    digitalWrite(relay_pin, LOW);
  }

  dl_matrix3du_free(image_matrix);
}

void AdminMode::setup() {
  Serial.println("Starting Admin Mode");

  digitalWrite(relay_pin, LOW);
  pinMode(relay_pin, OUTPUT);

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
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);

  WiFi.softAP(ssid, password);
  Serial.println("Access Point started");
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  app_httpserver_init();
  app_facenet_main();
  mtmn_config = app_mtmn_config();
  socket_server.listen(82);

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.softAPIP());
  Serial.println("' to connect");
}

void AdminMode::app_httpserver_init() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  if (httpd_start(&camera_httpd, &config) == ESP_OK)
  {
    Serial.println("HTTP Server started");
    httpd_uri_t index_uri = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = index_handler,
      .user_ctx  = NULL
    };
    httpd_register_uri_handler(camera_httpd, &index_uri);
  }
}

esp_err_t AdminMode::index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
  return httpd_resp_send(req, (const char *)index_ov2640_html_gz, index_ov2640_html_gz_len);
}

void AdminMode::app_facenet_main() {
  face_id_name_init(&st_face_list, FACE_ID_SAVE_NUMBER, ENROLL_CONFIRM_TIMES);
  aligned_face = dl_matrix3du_alloc(1, FACE_WIDTH, FACE_HEIGHT, 3);
  read_face_id_from_flash_with_name(&st_face_list);
}

mtmn_config_t AdminMode::app_mtmn_config() {
  mtmn_config_t mtmn_config = {0};
  mtmn_config.type = FAST;
  mtmn_config.min_face = 80;
  mtmn_config.pyramid = 0.707;
  mtmn_config.pyramid_times = 4;
  mtmn_config.p_threshold.score = 0.6;
  mtmn_config.p_threshold.nms = 0.7;
  mtmn_config.p_threshold.candidate_number = 20;
  mtmn_config.r_threshold.score = 0.7;
  mtmn_config.r_threshold.nms = 0.7;
  mtmn_config.r_threshold.candidate_number = 10;
  mtmn_config.o_threshold.score = 0.7;
  mtmn_config.o_threshold.nms = 0.7;
  mtmn_config.o_threshold.candidate_number = 1;
  return mtmn_config;
}

esp_err_t AdminMode::update_wifi_settings(WebsocketsClient &client, const char* new_ssid, const char* new_password) {
  preferences.begin("settings", false);
  preferences.putString("ssid", new_ssid);
  preferences.putString("password", new_password);
  preferences.end();
  
  strcpy(ssid, new_ssid);
  strcpy(password, new_password);
  
  client.send("WIFI_UPDATED");
  
  return ESP_OK;
}

void AdminMode::handle_message(WebsocketsClient &client, WebsocketsMessage msg) {
  String data = msg.data();
  Serial.println("Received message: " + data);

  if (data == "stream") {
    g_state = START_STREAM;
    client.send("STREAMING");
  }
  else if (data == "detect") {
    g_state = START_DETECT;
    client.send("DETECTING");
  }
  else if (data.startsWith("capture:")) {
    g_state = START_ENROLL;
    String person = data.substring(8);
    person.toCharArray(st_name.enroll_name, sizeof(st_name.enroll_name));
    client.send("CAPTURING");
  }
  else if (data == "recognise") {
    g_state = START_RECOGNITION;
    client.send("RECOGNISING");
  }
  else if (data.startsWith("remove:")) {
    String person = data.substring(7);
    char person_name[ENROLL_NAME_LEN * FACE_ID_SAVE_NUMBER];
    person.toCharArray(person_name, sizeof(person_name));
    delete_face_id_in_flash_with_name(&st_face_list, person_name);
    send_face_list(client);
  }
  else if (data == "delete_all") {
    delete_all_faces(client);
  }
  else if (data == "wifi_settings") {
    g_state = WIFI_SETTINGS;
    char wifi_info[100];
    sprintf(wifi_info, "WIFI_INFO:%s:%s", ssid, password);
    client.send(wifi_info);
  }
  else if (data.startsWith("update_wifi:")) {
    String params = data.substring(12);
    int separator = params.indexOf(":");
    if (separator > 0) {
      String new_ssid = params.substring(0, separator);
      String new_password = params.substring(separator + 1);
      update_wifi_settings(client, new_ssid.c_str(), new_password.c_str());
    }
  }
}

esp_err_t AdminMode::send_face_list(WebsocketsClient &client) {
  client.send("delete_faces");
  face_id_node *head = st_face_list.head;
  char add_face[64];
  for (int i = 0; i < st_face_list.count; i++) {
    sprintf(add_face, "listface:%s", head->id_name);
    client.send(add_face);
    head = head->next;
  }
  return ESP_OK;
}

esp_err_t AdminMode::delete_all_faces(WebsocketsClient &client) {
  delete_face_all_in_flash_with_name(&st_face_list);
  client.send("delete_faces");
  return ESP_OK;
}

void AdminMode::open_door(WebsocketsClient &client) {
  if (digitalRead(relay_pin) == LOW) {
    digitalWrite(relay_pin, HIGH);
    Serial.println("Door Unlocked");
    client.send("door_open");
    door_opened_millis = millis();
  }
}

void AdminMode::loop() {
  if (socket_server.poll()) {
    auto client = socket_server.accept();
    if (client.available()) {
      Serial.println("Client connected");

      client.onMessage([this, &client](WebsocketsMessage msg) {
        handle_message(client, msg);
      });

      dl_matrix3du_t *image_matrix = dl_matrix3du_alloc(1, 320, 240, 3);
      http_img_process_result out_res = {0};
      out_res.image = image_matrix->item;

      send_face_list(client);
      client.send("STREAMING");

      while (client.available()) {
        client.poll();

        if (millis() - door_opened_millis > interval) {
          digitalWrite(relay_pin, LOW);
        }

        fb = esp_camera_fb_get();
        if (!fb) {
          Serial.println("Camera capture failed");
          continue;
        }

        if (g_state == START_DETECT || g_state == START_ENROLL || g_state == START_RECOGNITION) {
          out_res.net_boxes = NULL;
          out_res.face_id = NULL;

          fmt2rgb888(fb->buf, fb->len, fb->format, out_res.image);

          out_res.net_boxes = face_detect(image_matrix, &mtmn_config);

          if (out_res.net_boxes) {
            if (align_face(out_res.net_boxes, image_matrix, aligned_face) == ESP_OK) {
              out_res.face_id = get_face_id(aligned_face);
              last_detected_millis = millis();
              if (g_state == START_DETECT) {
                client.send("FACE DETECTED");
              }

              if (g_state == START_ENROLL) {
                int left_sample_face = enroll_face_id_to_flash_with_name(&st_face_list, out_res.face_id, st_name.enroll_name);
                char enrolling_message[64];
                sprintf(enrolling_message, "SAMPLE NUMBER %d FOR %s", ENROLL_CONFIRM_TIMES - left_sample_face, st_name.enroll_name);
                client.send(enrolling_message);
                if (left_sample_face == 0) {
                  Serial.printf("Enrolled Face ID: %s\n", st_face_list.tail->id_name);
                  g_state = START_STREAM;
                  char captured_message[64];
                  sprintf(captured_message, "FACE CAPTURED FOR %s", st_face_list.tail->id_name);
                  client.send(captured_message);
                  send_face_list(client);
                }
              }

              if (g_state == START_RECOGNITION  && (st_face_list.count > 0)) {
                face_id_node *f = recognize_face_with_name(&st_face_list, out_res.face_id);
                if (f) {
                  char recognised_message[64];
                  sprintf(recognised_message, "DOOR OPEN FOR %s", f->id_name);
                  open_door(client);
                  client.send(recognised_message);
                } else {
                  client.send("FACE NOT RECOGNISED");
                }
              }
              dl_matrix3d_free(out_res.face_id);
            }
          } else {
            if (g_state != START_DETECT) {
              client.send("NO FACE DETECTED");
            }
          }

          if (g_state == START_DETECT && millis() - last_detected_millis > 500) {
            client.send("DETECTING");
          }
        }

        client.sendBinary((const char *)fb->buf, fb->len);

        esp_camera_fb_return(fb);
        fb = NULL;
      }
      Serial.println("Client disconnected");

      dl_matrix3du_free(image_matrix);
      client.close();
    } else {
      delay(100);
    }
  } else {
    delay(100);
  }
}