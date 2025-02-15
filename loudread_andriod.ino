#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <FS.h>
#include <Crypto.h>
#include <SHA256.h>
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceBuffer.h>
#include <AudioFileSourcePROGMEM.h>
#include <AudioOutputI2SNoDAC.h>
#include <AudioFileSourceSPIFFS.h>

AudioGeneratorMP3 * mp3;
AudioOutputI2SNoDAC * out;
AudioFileSourceSPIFFS * file;

// Constants
const char* ENDPOINT_URL = "https://dev.microsofttranslator.com/apps/endpoint?api-version=1.0";
const char* FORMAT = "audio-24khz-48kbitrate-mono-mp3";
const int SAMPLE_RATE = 24000;
SHA256 sha256;
// HMAC key (base64 decoded)
const uint8_t HMAC_KEY[] = {
  0xa2,  0x29,  0x3a,  0x3d,  0xd0,  0xdd,  0x32,  0x73,  0x97,  0x7a,
  0x64,  0xdb,  0xc2,  0xf3,  0x27,  0xf5,  0xd7,  0xbf,  0x87,  0xd9,
  0x45,  0x9d,  0xf0,  0x5a,  0x09,  0x66,  0xc6,  0x30,  0xc6,  0x6a,
  0xaa,  0x84,  0x9a,  0x41,  0xaa,  0x94,  0x3a,  0xa8,  0xd5,  0x1a,
  0x6e,  0x4d,  0xaa,  0xc9,  0xa3,  0x70,  0x12,  0x35,  0xc7,  0xeb,
  0x12,  0xf6,  0xe8,  0x23,  0x07,  0x9e,  0x47,  0x10,  0x95,  0x91,
  0x88,  0x55,  0xd8,  0x17
};

// TTS configuration structure
struct TTSConfig {
  String styleDegree = "1.0";
  String style = "general";
  String role = "default";
  String languageSkill = "";
};

class MicrosoftTTS {
  private:
    TTSConfig config;
    String endpoint;
    String token;

    // Generate UUID (simplified version)
    String generateUUID() {
      String uuid = "";
      for (int i = 0; i < 32; i++) {
        if (i == 8 || i == 12 || i == 16 || i == 20)
          uuid += "-";
        uuid += "0123456789abcdef"[random(16)];
      }
      return uuid;
    }

    // URL encode function
    String urlEncode(String str) {
      String encodedString = "";
      char c;
      char code0;
      char code1;
      for (int i = 0; i < str.length(); i++) {
        c = str.charAt(i);
        if (c == ' ') {
          encodedString += '+';
        } else if (isalnum(c)) {
          encodedString += c;
        } else {
          code1 = (c & 0xf) + '0';
          if ((c & 0xf) > 9) {
            code1 = (c & 0xf) - 10 + 'A';
          }
          c = (c >> 4) & 0xf;
          code0 = c + '0';
          if (c > 9) {
            code0 = c - 10 + 'A';
          }
          encodedString += '%';
          encodedString += code0;
          encodedString += code1;
        }
      }
      return encodedString;
    }

    // Get formatted date for signature
    String getFormattedDate() {
      time_t now;
      time(&now);
      char buf[80];
      strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));
      String date(buf);
      date.toLowerCase();
      return date;
    }

    // Calculate HMAC-SHA256
    String calculateHMAC(const char* stringToSign) {
      uint8_t kIpad[64];
      uint8_t kOpad[64];

      for (int i = 0; i < 64; i++) {
        kIpad[i] = HMAC_KEY[i] ^ 0x36;
        kOpad[i] = HMAC_KEY[i] ^ 0x5c;
      }

      sha256.reset();
      sha256.update(kIpad, 64);
      sha256.update((uint8_t*)stringToSign, strlen(stringToSign));
      uint8_t innerHash[32];
      sha256.finalize(innerHash, 32);

      sha256.reset();
      sha256.update(kOpad, 64);
      sha256.update(innerHash, 32);
      uint8_t signature[32];
      sha256.finalize(signature, 32);
      /*Serial.println();
      // 打印签名结果
      for (int i = 0; i < 32; i++) {
        if (signature[i] < 0x10) Serial.print("0");
        Serial.print(signature[i], HEX);
      }
      Serial.println();
      String signature1 =  base64Encode(signature, 32);
      Serial.println(signature1);
      for (size_t i = 0; i < 32; i++) {
        uint8_t byte = signature[i];
        Serial.print((byte >> 4) & 0x0F, HEX); // 高4位
        Serial.print(byte & 0x0F, HEX); // 低4位
        Serial.print(" ");
      }
      Serial.println();
*/
      // Convert to base64
      return base64Encode(signature, 32);
    }

    // Base64 encoding
    String base64Encode(uint8_t* data, size_t length) {
      static const char* ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      String encoded;
      int i = 0;
      int j = 0;
      uint8_t char_array_3[3];
      uint8_t char_array_4[4];

      while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
          char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
          char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
          char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
          char_array_4[3] = char_array_3[2] & 0x3f;

          for (i = 0; i < 4; i++)
            encoded += ALPHABET[char_array_4[i]];
          i = 0;
        }
      }

      if (i) {
        for (j = i; j < 3; j++)
          char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
          encoded += ALPHABET[char_array_4[j]];

        while ((i++ < 3))
          encoded += '=';
      }
      return encoded;
    }

    // Generate signature
    String getSignature() {
      String url = String(ENDPOINT_URL);
      url = url.substring(url.indexOf("://") + 3);
      //String encodedUrl = urlEncode(url);
      String encodedUrl = "dev.microsofttranslator.com%2fapps%2fendpoint%3fapi-version%3d1.0";
      String uuid = generateUUID();
      uuid.replace("-", "");
      String date = getFormattedDate();
      String stringToSign = "MSTranslatorAndroidApp" + encodedUrl + date + uuid;
      stringToSign.toLowerCase();
      //Serial.println(stringToSign);
      String signature = calculateHMAC(stringToSign.c_str());
      //Serial.println(signature);
      return "MSTranslatorAndroidApp::" + signature + "::" + date + "::" + uuid;
    }

  public:
    MicrosoftTTS() {}

    bool init() {
      // Configure time for proper date formatting
      configTime(8 * 3600, 0, "ntp2.aliyun.com");

      // Wait for time sync
      time_t now = time(nullptr);
      while (now < 8 * 3600 * 2) {
        delay(500);
        now = time(nullptr);
      }

      return true;
    }

    String getEndpoint() {
      String signature = getSignature();
      //Serial.println(signature);
      WiFiClientSecure client;
      client.setInsecure(); // Warning: This skips SSL certificate verification
      HTTPClient http;
      http.begin(client, ENDPOINT_URL);

      http.addHeader("Accept-Language", "zh-Hans");
      http.addHeader("X-ClientVersion", "4.0.530a 5fe1dc6c");
      http.addHeader("X-UserId", "0f04d16a175c411e");
      http.addHeader("X-HomeGeographicRegion", "zh-Hans-CN");
      http.addHeader("X-ClientTraceId", "aab069b9-70a7-4844-a734-96cd78d94be9");
      http.addHeader("X-MT-Signature", signature);
      http.addHeader("User-Agent", "okhttp/4.5.0");
      http.addHeader("Content-Type", "application/json; charset=utf-8");
      http.addHeader("Accept-Encoding", "gzip");

      int httpCode = http.POST("");

      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        // Serial.println(payload);
        // Parse JSON response
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(payload);

        if (root.success()) {
          endpoint = root["r"].as<String>();
          token = root["t"].as<String>();
          return payload;
        }
      }

      http.end();
      return "";
    }

    bool getAudio(String text, String locale, String voice, int rate, int volume, int pitch)
    {
      if (endpoint.length() == 0) {
        if (getEndpoint().length() == 0) {
          return false;
        }
      }

      rate = (rate * 2) - 100;
      pitch = pitch - 50;

      String textSsml = text;
      textSsml.replace("&", "&amp;");
      textSsml.replace("<", "&lt;");
      textSsml.replace(">", "&gt;");
      textSsml.replace("\"", "&quot;");
      textSsml.replace("'", "&apos;");
      textSsml.replace("/", "");
      textSsml.replace("\\", "");

      String ssml = "<speak xmlns=\"http://www.w3.org/2001/10/synthesis\" "
                    "xmlns:mstts=\"http://www.w3.org/2001/mstts\" "
                    "xmlns:emo=\"http://www.w3.org/2009/10/emotionml\" "
                    "version=\"1.0\" xml:lang=\"zh-CN\">"
                    "<voice name=\"" + voice + "\">"
                    "<mstts:express-as style=\"" + config.style + "\" "
                    "styledegree=\"" + config.styleDegree + "\" "
                    "role=\"" + config.role + "\">"
                    "<prosody rate=\"" + String(rate) + "%\" "
                    "pitch=\"" + String(pitch) + "%\" "
                    "volume=\"" + String(volume) + "\">" +
                    textSsml +
                    "</prosody></mstts:express-as></voice></speak>";

      String url = "https://" + endpoint + ".tts.speech.microsoft.com/cognitiveservices/v1";

      WiFiClientSecure client;
      client.setInsecure();
      HTTPClient http;
      http.begin(client, url);

      http.addHeader("Authorization", token);
      http.addHeader("Content-Type", "application/ssml+xml");
      http.addHeader("X-Microsoft-OutputFormat", FORMAT);
      int httpCode = http.POST(ssml);
      if (httpCode == HTTP_CODE_OK) {
        if (SPIFFS.exists("/tts.mp3"))
          SPIFFS.remove("/tts.mp3");

        File file = SPIFFS.open("/tts.mp3", "w");
        if (!file) {
          Serial.println("Failed to open file for writing");
          http.end();
          return false;
        }

        WiFiClient* stream = http.getStreamPtr();
        size_t totalSize = http.getSize();
        size_t downloaded = 0;


        while (http.connected() && (stream->available() || http.getSize() > 0)) {
          size_t size = stream->available();
          uint8_t buff[size];
          stream->readBytes(buff, size);
          file.write(buff, size);
          downloaded += size;
          Serial.printf("Downloaded: %zu bytes\r\n", downloaded);
        }

        file.close();
        http.end();
//        Serial.print("Get File len:%d\r\n");
//        Serial.println(totalSize);
//        Serial.println("\nDownload complete");
        // Here you would handle the audio data
        // For example, save to SD card or stream to audio output
        Serial.println("Received audio data successfully");

        return true;
      }

      http.end();
      return false;
    }
};

// WiFi credentials
const char* ssid = "";
const char* password = "";

MicrosoftTTS tts;

void setup()
{
  Serial.begin(115200);
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // Initialize TTS
  if (tts.init()) {
    Serial.println("TTS initialized successfully");
  } else {
    Serial.println("TTS initialization failed");
  }
  if (WiFi.status() == WL_CONNECTED) {
    // Example TTS request
    bool success = tts.getAudio(
                     "台湾收回后，台湾海峡属于内海，外舰是不是只能绕着走了？",
                     "zh-CN",
                     "Microsoft Server Speech Text to Speech Voice (zh-CN, YunxiNeural)",
                     50,  // rate
                     100,  // volume
                     50    // pitch
                   );

    if (success) {
      Serial.println("Audio retrieved successfully");
      file = new AudioFileSourceSPIFFS("/tts.mp3");
      out = new AudioOutputI2SNoDAC();
      mp3 = new AudioGeneratorMP3();
      mp3->begin(file, out);
    } else {
      Serial.println("Failed to get audio");
    }
  }
}

void loop() {

  if (mp3->isRunning()) {
    if (!mp3->loop()) mp3->stop();
  }

}
