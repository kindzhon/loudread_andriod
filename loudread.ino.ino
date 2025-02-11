#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <Base64.h>
#include <ArduinoJson.h>
#include <time.h>
#include <bearssl/bearssl_hmac.h>

// WiFi credentials
const char* ssid = "CMCC-good";
const char* password = "lcl=86237239+";

// Constants
const char* ENDPOINT_URL = "https://dev.microsofttranslator.com/apps/endpoint?api-version=1.0";
const char* FORMAT = "audio-24khz-48kbitrate-mono-mp3";
char HMAC_KEY[] = "oik6PdDdMnOXemTbwvMn9de/h9lFnfBaCWbGMMZqqoSaQaqUOqjVGm5NqsmjcBI1x+sS9ugjB55HEJWRiFXYFw==";

// Global variables for endpoint info
String endpointRegion;
String endpointToken;

// Function declarations
String getUUID();
String getCurrentTime();
String percentEncode(String value);
String calculateSignature(String url);
bool getEndpoint();
bool getTTSAudio(const char* text, const char* locale, const char* voice, int rate, int volume, int pitch);

void setup() {
  Serial.begin(115200);
  
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize time
  configTime(8*3600, 0, "ntp2.aliyun.com");
  while (time(nullptr) < 1000000000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTime synchronized");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    // Example TTS request
    getTTSAudio(
      "hi,xiao li",
      "zh-CN",
      "Microsoft Server Speech Text to Speech Voice (zh-CN, XiaoxiaoNeural)",
      100,
      100,
      75
    );
  }
  delay(30000); // Wait 30 seconds before next request
}

String getUUID() {
  String uuid = "";
  for (int i = 0; i < 32; i++) {
    uint8_t random_byte = random(16);
    uuid += String(random_byte, HEX);
  }
  return uuid;
}

String getCurrentTime() {
  time_t now = time(nullptr);
  struct tm* timeinfo = gmtime(&now);
  char buffer[50];
  strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);
  return String(buffer);
}

String percentEncode(String value) {
  String encoded = "";
  char c;
  for (unsigned int i = 0; i < value.length(); i++) {
    c = value.charAt(i);
    if (isAlphaNumeric(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      encoded += String("%") + String(c, HEX);
    }
  }
  return encoded;
}

String calculateSignature(String url) {
  String urlWithoutProtocol = url;
  urlWithoutProtocol.replace("https://", "");
  
  String encodedUrl = percentEncode(urlWithoutProtocol);
  String uuid = getUUID();
  Serial.println(uuid);
  String currentTime = getCurrentTime();
  Serial.println(currentTime);
  String stringToSign = "MSTranslatorAndroidApp" + encodedUrl + currentTime + uuid;
  stringToSign.toLowerCase();
Serial.println(stringToSign);
  // Decode base64 HMAC key
  int keyLength = Base64.decodedLength(HMAC_KEY, strlen(HMAC_KEY));
  uint8_t decodedKey[keyLength+1];
  char  decodedKey0[keyLength+1];
  Base64.decode(decodedKey0,HMAC_KEY,  strlen(HMAC_KEY));
  Serial.println(stringToSign);
memcpy(decodedKey, decodedKey0, keyLength);
  // Calculate HMAC-SHA256 using BearSSL
  uint8_t hmacResult[32];
  br_hmac_key_context kc;
  br_hmac_context ctx;
  
  // Initialize HMAC-SHA256 context
  br_hmac_key_init(&kc, &br_sha256_vtable, decodedKey, keyLength);
  br_hmac_init(&ctx, &kc, 0);
  
  // Update with data
  br_hmac_update(&ctx, stringToSign.c_str(), stringToSign.length());
  
  // Get the result
  br_hmac_out(&ctx, hmacResult);
//Serial.println(decodedKey);
      char* hexString = (char*)malloc((32 * 2) + 1);
    if (hexString == NULL) {
        return "err"; // 内存分配失败
    }

    for (size_t i = 0; i < 32; i++) {
        sprintf(&hexString[i * 2], "%02x", hmacResult[i]);
    }

  
  
  // Convert HMAC to base64
  int base64Length = Base64.encodedLength(32);
  char base64Result[base64Length];
  Base64.encode( base64Result,hexString, 32);
    Serial.println(base64Result);

    currentTime.toLowerCase();
  return "MSTranslatorAndroidApp::" + String(base64Result) + "::" + currentTime + "::" + uuid;
}

bool getEndpoint() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure(); // Warning: This skips SSL certificate verification
  
  HTTPClient http;
  
  String signature = calculateSignature(ENDPOINT_URL);
  //signature.toLowerCase();
  Serial.println(signature);
  http.begin(client, ENDPOINT_URL);
  http.addHeader("Accept-Language", "zh-Hans");
  http.addHeader("X-ClientVersion", "4.0.530a 5fe1dc6c");
  http.addHeader("X-UserId", "0f04d16a175c411e");
  http.addHeader("X-HomeGeographicRegion", "zh-Hans-CN");
  http.addHeader("X-ClientTraceId", "aab069b9-70a7-4844-a734-96cd78d94be9");//getUUID());
  http.addHeader("X-MT-Signature", signature);
  http.addHeader("User-Agent", "okhttp/4.5.0");
  http.addHeader("Content-Type", "application/json; charset=utf-8");
  http.addHeader("Accept-Encoding", "gzip");
  
  int httpCode = http.POST("");
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    
    StaticJsonBuffer<200> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);
    
    if (root.success()) {
      endpointRegion = root["r"].as<String>();
      endpointToken = root["t"].as<String>();
      return true;
    }
  }
  
  http.end();
  return false;
}

bool getTTSAudio(const char* text, const char* locale, const char* voice, int rate, int volume, int pitch) {
  if (!getEndpoint()) {
    Serial.println("Failed to get endpoint");
    return false;
  }
  
  rate = (rate * 2) - 100;
  pitch = pitch - 50;
  
  String ssml = String("") +
    "<speak xmlns=\"http://www.w3.org/2001/10/synthesis\" " +
    "xmlns:mstts=\"http://www.w3.org/2001/mstts\" " +
    "xmlns:emo=\"http://www.w3.org/2009/10/emotionml\" " +
    "version=\"1.0\" xml:lang=\"zh-CN\">" +
    "<voice name=\"" + voice + "\">" +
    "<mstts:express-as style=\"general\" styledegree=\"1.0\" role=\"default\">" +
    "<prosody rate=\"" + String(rate) + "%\" pitch=\"" + String(pitch) + "%\" volume=\"" + String(volume) + "\">" +
    text +
    "</prosody></mstts:express-as></voice></speak>";

  WiFiClientSecure client;
  client.setInsecure(); // Warning: This skips SSL certificate verification
  
  HTTPClient http;
  String url = "https://" + endpointRegion + ".tts.speech.microsoft.com/cognitiveservices/v1";
  
  http.begin(client, url);
  http.addHeader("Authorization", endpointToken);
  http.addHeader("Content-Type", "application/ssml+xml");
  http.addHeader("X-Microsoft-OutputFormat", FORMAT);
  
  int httpCode = http.POST(ssml);
  
  if (httpCode == HTTP_CODE_OK) {
    // Here you would handle the audio data
    // For example, save to SD card or stream to audio output
    Serial.println("Received audio data successfully");
    return true;
  }
  
  Serial.println("Failed to get audio data");
  http.end();
  return false;
}
