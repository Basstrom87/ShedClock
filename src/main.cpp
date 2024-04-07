#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

// DHT Sensor Setup
#define DHTPIN 25
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// NTP Variables
WiFiUDP ntpUDP;
// DST
//NTPClient timeClient(ntpUDP, "au.pool.ntp.org", 37800, 60000);
// Non-DST
NTPClient timeClient(ntpUDP, "au.pool.ntp.org", 34200, 60000);

// Screen and Fonts Variables
#include "Free_Fonts.h" // Include the header file attached to this sketch
TFT_eSPI tft = TFT_eSPI();

// Screen Refresh Boolean
bool screenRefresh = false;

// MQTT Variables
const char* mqtt_server = "xxx.xxx.xxx.xxx";
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];

// MQTT Callback to update sensor information on screen.
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = 0;
  String recvPayload = String(( char *) payload);
  // Serial.println(recvPayload);

  // Split recieved data into array of size 3
  String incomingData[3];
  int stringCount = 0;  
  while (recvPayload.length() > 0){
    int index = recvPayload.indexOf(' ');
    if (index == -1) // No space found
    {
      incomingData[stringCount++] = recvPayload;
      break;
    }
    else
    {
      incomingData[stringCount++] = recvPayload.substring(0, index);
      recvPayload = recvPayload.substring(index+1);
    }
  }
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  // Display on Screen
  if (incomingData[0] == "OutdoorSensor"){
    tft.drawString("Outside:", 0, 270, 4);
    String tempHumid = "Temperature: " + incomingData[1] + "`C" + "   Humidity: " + incomingData[2] + "%";
    tft.drawString(tempHumid, 0, 300, 2);
    // Serial.println("Outside Data Recv.");
  }
  else if (incomingData[0] == "InsideSensor"){
    tft.drawString("Inside:", 0, 203, 4);
    String tempHumid = "Temperature: " + incomingData[1] + "`C" + "   Humidity: " + incomingData[2] + "%";
    tft.drawString(tempHumid, 0, 233, 2);
    // Serial.println("Inside Data Recv.");
  }
}

// MQTT Reconnect
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ShedClock";
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("weatherData");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// Network Setup
void initWiFi() {
  const char* ssid = "ssidGoesHere";
  const char* password = "WiFiPassWord";
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  String hostname = "ShedClock";
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname.c_str());
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());

  timeClient.begin();
}

// Function to get the text centred on the screen based on the length of the text in pixels.
int centerText(String textToCenter, int fontNumber){
  int textWidth = tft.textWidth(textToCenter, fontNumber);
  int textPosition = 120 - round(textWidth / 2);

  return textPosition;
}


// CODE START //
String timeString = "";
String currentMinutesStr = "";

void setup() {
  Serial.begin(115200);
  initWiFi();
  tft.begin();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.begin();
}

void loop() {
  // Check if MQTT Is connected
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  // Update time
  timeClient.update();

  // Get current hours and Minutes.
  int currentHours = timeClient.getHours();
  int currentMinutes = timeClient.getMinutes();
  
  int timePadding = -40;
  
  // Convert to 12 hour time
  if (currentHours > 12){
    currentHours = currentHours - 12;
  }
  
  // Add on 0 for less than 10 past the hour
  if (currentMinutes < 10){
    currentMinutesStr = "0";
    currentMinutesStr += currentMinutes;
  }
  else{
    currentMinutesStr = currentMinutes;
  }

  // Refresh the screen to fix the extra numbers when past 12 o'clock

  if (currentHours == 1 && screenRefresh == false){
    tft.fillScreen(TFT_BLACK);
    screenRefresh = true;
  }
  else if (currentHours == 2 && screenRefresh == true){
    screenRefresh = false;
  }
  

  // Build up time string.
  timeString = "";
  timeString += currentHours;
  timeString += ":";
  timeString += currentMinutesStr;

  // Double text size and display time on screen
  tft.setTextSize(2);
  // Get the padding correct to display time centred on the screen
  if (currentHours < 10){
    timePadding = centerText(timeString, 7);
  }
  else {
    timePadding = -40;
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(timeString, timePadding, 0, 7);
  
  // Restore text time and put date on screen.
  tft.setTextSize(1);
  String date = timeClient.getFormattedDate();
  tft.drawString(date, centerText(date, 4), 105, 4);

  // Display Temps and humidities.
  tft.setTextColor(TFT_DARKCYAN, TFT_BLACK);
  tft.drawString("Shed:", 0, 140, 4);
  String tempHumid = "Temperature: ";
  int shedTemp = round(dht.readTemperature());
  int shedHumid = round(dht.readHumidity());
  tempHumid += shedTemp;
  tempHumid += "\`C";
  tempHumid += "   Humidity: ";
  tempHumid += shedHumid;
  tempHumid += "%";
  tft.drawString(tempHumid, 0, 170, 2);
  
  delay(1000);
}