#include <WiFi.h>
#include <WiFiClient.h>
#include <ESP_I2S.h>
#include <base64.h>

#define I2S_WS 19   //需替换
#define I2S_SD 17   //需替换
#define I2S_SCK 18  //需替换

#define BUTTON_PIN 0

#define SAMPLE_RATE 16000
#define SAMPLE_BITS 16
#define DURATION_SECONDS 6 // 16000Hz采样率，16bit位码的条件下，如果使用片内内存，1s都会导致数据丢失。因此，此代码使用PSRAM
#define BUFFER_SIZE (SAMPLE_RATE * DURATION_SECONDS * (SAMPLE_BITS / 8))

#define SAMPLE_SIZE 1024
#define PACKET_SIZE (1024 * 4) // 每个UDP包的大小（字节）

#define BASE64_CHUNK 1200

#define DEFAULT_RET_TEXT "not support"

I2SClass I2S;

const char *ssid = "你的WiFi名称";
const char *password = "你的WiFi密码";
const char *serverIp = "你的服务器ip"; 
const uint16_t serverPort = 8085;

WiFiClient client;

int16_t *audio_buffer;
char *json_data;

bool b_send_over = true;

void setup()
{
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  init_wifi();

  init_inmp441();
}

void loop()
{
  if (b_send_over && digitalRead(BUTTON_PIN) == LOW )
  {
    delay(100); // 消抖延迟

    if (b_send_over && digitalRead(BUTTON_PIN) == LOW)
    {
      b_send_over = false;

      size_t audio_len = record_audio();

      String audio_text = sound2text(audio_len);

      b_send_over = true;
    }
  }
}

void print_mem_info()
{
  Serial.printf("free(%d) max_alloc(%d)\n", ESP.getFreeHeap(), ESP.getMaxAllocHeap());

  Serial.print("PSRAM: ");
  Serial.print(ESP.getPsramSize());
  Serial.print(" (");
  Serial.print(ESP.getFreePsram());
  Serial.println(" available).");
}

void init_wifi()
{
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
  }
  Serial.print("已连接到WiFi: ");
  Serial.println(WiFi.localIP());
}

void init_inmp441()
{
  I2S.setPins(I2S_SCK, I2S_WS, -1, I2S_SD, -1);

  if (!I2S.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO, I2S_STD_SLOT_LEFT))
  {
    Serial.println("初始化I2S总线失败!");
    return;
  }

  audio_buffer = (int16_t *)ps_malloc(BUFFER_SIZE);
  if (!audio_buffer)
  {
    Serial.println("无法为样本分配内存");
    return;
  }

  json_data = (char *)ps_malloc(BUFFER_SIZE * 4 / 3 + 500);
  if (!json_data)
  {
    Serial.println("无法为json分配内存");
    return;
  }
}

size_t record_audio()
{
  uint32_t time1, time2;

  size_t length;
  size_t total_bytes = 0;

  Serial.println("开始录音...");

  memset((void *)audio_buffer, 0, BUFFER_SIZE);

  time1 = micros();
  while (total_bytes < BUFFER_SIZE)
  {
    length = min(SAMPLE_SIZE * sizeof(int16_t), BUFFER_SIZE - total_bytes);

    length = I2S.readBytes((char *)audio_buffer + total_bytes, length);
    total_bytes += length;
  }
  time2 = micros() - time1;

  Serial.printf("录音时间: %d 微秒\n", time2);

  return total_bytes;
}

String sound2text(size_t total_bytes)
{
  size_t length;

  Serial.println("编码中...");

  memset(json_data, 0, BUFFER_SIZE * 4 / 3 + 500); // 清空数组

  int base64_len = strlen(json_data);

  for (size_t i = 0; i < total_bytes; i += BASE64_CHUNK)
  {
    length = (i + BASE64_CHUNK >= total_bytes) ? (total_bytes - i) : BASE64_CHUNK;
    strcat(json_data, base64::encode((uint8_t *)audio_buffer + i, length).c_str());
  }

  base64_len = strlen(json_data) - base64_len;

  //print_mem_info();

  Serial.printf("总字节数: %d -> %d\n", BUFFER_SIZE, base64_len);

  Serial.println("发送中...");

  int total_length = base64_len;
  int cur_offset = 0;
  uint8_t *ptr_audio = (uint8_t *)json_data;

  if (client.connect(serverIp, serverPort))
  {
    while (cur_offset < total_length)
    {
      int bytesToSend = min(PACKET_SIZE, total_length - cur_offset);
      bytesToSend = client.write(ptr_audio + cur_offset, bytesToSend);
      cur_offset += bytesToSend;

      // Serial.printf("Sent %d bytes, offset: %d\n", bytesToSend, cur_offset);

      // 通过定时器或延迟控制发送频率
      // delay(10); // 根据网络条件调整延迟时间
    }

    client.stop();
  }
  else
  {
    Serial.println("连接失败");
  }

  return String(DEFAULT_RET_TEXT);
}