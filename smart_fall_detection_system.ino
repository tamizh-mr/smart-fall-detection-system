#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// ---------- Wi-Fi credentials ----------
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ---------- Telegram Bot credentials ----------
// Create a bot via @BotFather on Telegram to get BOT_TOKEN.
// Send a message to your bot, then visit:
// https://api.telegram.org/bot<BOT_TOKEN>/getUpdates
// to find your CHAT_ID.
const char* BOT_TOKEN = "YOUR_BOT_TOKEN";
const char* CHAT_ID   = "YOUR_CHAT_ID";

// ---------- Fall detection thresholds ----------
const float FALL_THRESHOLD_G  = 3.0;   // sudden impact spike
const float STILL_THRESHOLD_G = 1.2;   // lying still after impact
const unsigned long STILL_CHECK_DELAY = 5000; // 5 seconds

Adafruit_MPU6050 mpu;

bool fallFlag = false;
unsigned long fallTimestamp = 0;

void setup()
{
  Serial.begin(115200);

  // ---- Connect to Wi-Fi ----
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  // ---- Initialize MPU6050 ----
  if (!mpu.begin())
  {
    Serial.println("MPU6050 not found! Check wiring.");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("MPU6050 initialized.");

  sendTelegramMessage("Fall Detection System is now online and monitoring.");
}

void loop()
{
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Convert acceleration (m/s^2) to g-force units
  float ax = a.acceleration.x / 9.81;
  float ay = a.acceleration.y / 9.81;
  float az = a.acceleration.z / 9.81;

  float magnitude = sqrt(ax * ax + ay * ay + az * az);

  Serial.print("Accel magnitude: ");
  Serial.print(magnitude, 2);
  Serial.println(" g");

  if (!fallFlag)
  {
    // Step 1: watch for a sudden impact spike
    if (magnitude > FALL_THRESHOLD_G)
    {
      fallFlag = true;
      fallTimestamp = millis();
      Serial.println(">> Possible fall detected. Confirming in 5 seconds...");
    }
  }
  else
  {
    // Step 2: after the impact, wait and confirm stillness
    if (millis() - fallTimestamp >= STILL_CHECK_DELAY)
    {
      if (magnitude < STILL_THRESHOLD_G)
      {
        // Confirmed: sharp impact followed by stillness = real fall
        Serial.println(">> FALL CONFIRMED. Sending alert...");
        sendTelegramMessage("Elderly person has fallen! Immediate attention required.");
      }
      else
      {
        // Person is still moving normally -> false alarm, ignore
        Serial.println(">> False alarm, person still active. Resetting.");
      }
      fallFlag = false; // reset and resume monitoring
    }
  }

  delay(200);
}

// ---------- Send a Telegram message via Bot API ----------
void sendTelegramMessage(const String &message)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("WiFi not connected, cannot send alert.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // skip certificate validation (fine for hobby projects)

  HTTPClient https;
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) +
               "/sendMessage?chat_id=" + String(CHAT_ID) +
               "&text=" + urlEncode(message);

  if (https.begin(client, url))
  {
    int httpCode = https.GET();
    if (httpCode > 0)
    {
      Serial.println("Telegram alert sent. HTTP code: " + String(httpCode));
    }
    else
    {
      Serial.println("Telegram send failed: " + https.errorToString(httpCode));
    }
    https.end();
  }
  else
  {
    Serial.println("Unable to connect to Telegram API.");
  }
}

// ---------- Simple URL encoder for message text ----------
String urlEncode(const String &str)
{
  String encoded = "";
  char c;
  char code0, code1;
  for (int i = 0; i < str.length(); i++)
  {
    c = str.charAt(i);
    if (isalnum(c))
    {
      encoded += c;
    }
    else if (c == ' ')
    {
      encoded += "%20";
    }
    else
    {
      code1 = (c & 0xf) + '0';
      if ((c & 0xf) > 9) code1 = (c & 0xf) - 10 + 'A';
      c = (c >> 4) & 0xf;
      code0 = c + '0';
      if (c > 9) code0 = c - 10 + 'A';
      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }
  return encoded;
}
