#define TINY_GSM_MODEM_SIM800

#define TINY_GSM_RX_BUFFER 256

#include <TinyGPS++.h> //https://github.com/mikalhart/TinyGPSPlus
#include <TinyGsmClient.h> //https://github.com/vshymanskyy/TinyGSM
#include <ArduinoHttpClient.h> //https://github.com/arduino-libraries/ArduinoHttpClient

const char FIREBASE_HOST[]  = "myfirebasetest-2ee2";
const String FIREBASE_AUTH  = "-----";
const String FIREBASE_PATH  = "/";
const int SSL_PORT          = 443;

// Your GPRS credentials
// Leave empty, if missing user or pass
char apn[]  = "internet";
char user[] = "";
char pass[] = "";

//GSM Module RX pin to ESP32 2
//GSM Module TX pin to ESP32 4
#define rxPin 9
#define txPin 10
HardwareSerial sim800(1);
TinyGsm modem(sim800);
//NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN

// GPS Module RX pin to ESP32 17
// GPS Module TX pin to ESP32 16
#define RXD2 20
#define TXD2 21
HardwareSerial neogps(0);
TinyGPSPlus gps;
//NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN

//NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
TinyGsmClientSecure gsm_client_secure_modem(modem, 0);
HttpClient http_client = HttpClient(gsm_client_secure_modem, FIREBASE_HOST, SSL_PORT);
//NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN

unsigned long previousMillis = 0;
long interval = 10000;
String response = "";

//**************************************************************************************************
void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  Serial.println("ESP32 serial initialized");

  // Initialize GSM Module
  sim800.begin(9600, SERIAL_8N1, rxPin, txPin);
  Serial.println("SIM800L serial initialized");

  // Initialize GPS Module
  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.println("NeoGPS serial initialized");
  delay(2000);

  // Ensure GSM Module is Responsive
  while (!checkGSMModule()) {
    Serial.println("Waiting for GSM module to respond...");
    delay(2000);  // Wait before checking again
  }

  Serial.println("GSM module is responsive. Proceeding with setup...");

  // Initialize Modem
  Serial.println("Initializing modem...");
  modem.restart();
  String modemInfo = modem.getModemInfo();
  Serial.print("Modem: ");
  Serial.println(modemInfo);
  // Configure HTTP Client
  http_client.setHttpResponseTimeout(90 * 1000);  // Set HTTP client timeout
  Serial.println("Setup completed successfully.");
}


void loop() {
   
  Serial.print(F("Connecting to "));
  Serial.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println(" fail");
    delay(1000);
    return;
  }
  Serial.println(" OK");
  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
  
  http_client.connect(FIREBASE_HOST, SSL_PORT);
  
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
  while (true) {
    if (!http_client.connected()) {
      Serial.println();
      http_client.stop();// Shutdown
      Serial.println("HTTP  not connect");
      break;
    }
    else{
     
      gps_loop();
      sendATCommand("AT+CBC");
      delay(5000);
    }
  }
  //MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM
}

void PostToFirebase(const char* method, const String & path , const String & data, HttpClient* http) {
  String response;
  int statusCode = 0;
  http->connectionKeepAlive(); // Currently, this is needed for HTTPS
  
  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
  String url;
  if (path[0] != '/') {
    url = "/";
  }
  url += path + ".json";
  url += "?auth=" + FIREBASE_AUTH;
  Serial.print("POST:");
  Serial.println(url);
  Serial.print("Data:");
  Serial.println(data);
  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
  
  String contentType = "application/json";
  http->put(url, contentType, data);
  
  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
  // read the status code and body of the response
  //statusCode-200 (OK) | statusCode -3 (TimeOut)
  statusCode = http->responseStatusCode();
  Serial.print("Status code: ");
  Serial.println(statusCode);
  response = http->responseBody();
  Serial.print("Response: ");
  Serial.println(response);
  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN

  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
  if (!http->connected()) {
    Serial.println();
    http->stop();// Shutdown
    Serial.println("HTTP POST disconnected");
  }
  //NNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNN
}
//**************************************************************************************************


//**************************************************************************************************
void gps_loop()
{
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 2000;){
    while (neogps.available()){
      if (gps.encode(neogps.read())){
        newData = true;
        break;
      }
    }
  }

  if(newData){
    newData = false;

    // Check if GPS has a valid location fix
    if (gps.location.isValid()) {
      String latitude, longitude, timestamp;
      latitude = String(gps.location.lat(), 7); // Latitude in degrees
      longitude = String(gps.location.lng(), 7); // Longitude in degrees

      // Convert latitude and longitude strings back to floats for comparison
      float latValue = latitude.toFloat();
      float lngValue = longitude.toFloat();

      // Check if the latitude and longitude are not 0.0
      if (latValue != 0.0 && lngValue != 0.0) {

        // Construct timestamp from GPS time
        int hour = gps.time.hour();  // Hour in UTC
        int minute = gps.time.minute();  // Minute
        int second = gps.time.second();  // Second

        // Check if the GPS time data is valid
        if (gps.time.isValid()) {
          char timeBuffer[10];
          sprintf(timeBuffer, "%02d:%02d:%02d", hour, minute, second);
          timestamp = String(timeBuffer);
        } else {
          timestamp = "unknown";  // Fallback if GPS time is unavailable
        }

        Serial.print("Latitude= "); 
        Serial.print(latitude);
        Serial.print(" Longitude= "); 
        Serial.print(longitude);
        Serial.print(" Time= ");
        Serial.println(timestamp);

        // Construct the JSON payload with time
        String gpsData = "{";
        gpsData += "\"lat\":" + latitude + ",";
        gpsData += "\"lng\":" + longitude + ",";
        gpsData += "\"time\":\"" + timestamp + "\"";
        gpsData += "}";

        // Send data to Firebase
        PostToFirebase("POST", FIREBASE_PATH, gpsData, &http_client);

      } else {
        Serial.println("GPS data is invalid (latitude or longitude is 0.0). Skipping Firebase upload.");
      }

    } else {
      // If GPS fix is not valid, don't update the database
      Serial.println("GPS location is not valid. Skipping Firebase upload.");
      delay(10000);
    }
  }
}

void sendATCommand(String command) {
  // Send an AT command followed by a carriage return
  sim800.println(command);
  delay(1000);  // Wait for SIM800 to respond

  // Print response to the Serial Monitor
  while (sim800.available()) {
    Serial.write(sim800.read());
  }
}


String sendATCommandsim(String command) {
  sim800.println(command);
  delay(500); // Wait for response

  while (sim800.available()) {
    char c = (char)sim800.read();
    response += c;
    
  }
  return response;

}

bool checkGSMModule() {
  String response1 = sendATCommandsim("AT");
  //Serial.println(response1);
  delay(500);

  // Directly compare the response to "OK"
  return response1.indexOf("OK") != -1;
}



// Function to read GSM module response
String readGSMResponse() {
  String response = "";
  unsigned long timeout = millis() + 2000;  // 2-second timeout

  while (millis() < timeout) {
    while (sim800.available()) {
      char c = sim800.read();
      response += c;
    }
  }

  return response;
}
