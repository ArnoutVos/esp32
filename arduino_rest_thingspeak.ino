#include <WiFi.h>
#include <CCS811.h>
#include <BME280I2C.h>

#define LEDPin 25
#define ADDR      0x5A
#define WAKE_PIN  13

// Network information
const char* ssid = "xxx";
const char* password = "xxx";

// ThingSpeak Settings
const char server[] = "api.thingspeak.com";
const String writeAPIKey = "xxx";

// Constants
const unsigned long postingInterval = 15L * 1000L;
const unsigned long measureInterval = 5L * 1000L;
const int numberPoints = 7;

// Global Variables
unsigned long lastUpdateThingspeakTime = 0;
unsigned long lastMeasureTime = 0;
int numberOfWifiRetry = 0;
int numberOfFailedHttp = 0;
int numberOfSamples = 0;
float avgWifiStrength, avgTemp, avgHum, avgPres, avgCO2, avgTVOC, avgLightVal;

CCS811 sensor;
BME280I2C bme;

void setup() {

  Serial.begin(115200);
  Serial.print(millis());
  Serial.println(" Starting setup");

  connectWiFi();

  if (!sensor.begin(uint8_t(ADDR), uint8_t(WAKE_PIN)))
    Serial.println("Initialization failed.");

  while (!bme.begin())
  {
    Serial.println("Could not find BME280 sensor!");
    delay(1000);
  }

  switch (bme.chipModel())
  {
    case BME280::ChipModel_BME280:
      Serial.println("Found BME280 sensor! Success.");
      break;
    case BME280::ChipModel_BMP280:
      Serial.println("Found BMP280 sensor! No Humidity available.");
      break;
    default:
      Serial.println("Found UNKNOWN sensor! Error!");
  }
  Serial.print(millis());
  Serial.println(" Finished setup");
}

void loop() {

  // In each loop, make sure there is always an internet connection.
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    numberOfWifiRetry++;
  }

  if ((millis() - lastMeasureTime > measureInterval) or lastMeasureTime == 0)
  {
    Serial.print(millis());
    Serial.println(" start measure");

    float temp, hum, pres,TVOC,CO2,wifiStrength;

    BME280::TempUnit tempUnit(BME280::TempUnit_Celsius);
    BME280::PresUnit presUnit(BME280::PresUnit_Pa);

    bme.read(pres, temp, hum, tempUnit, presUnit);

    sensor.compensate(temp - .7, hum);
    sensor.getData();

    CO2 = sensor.readCO2();
    TVOC = sensor.readTVOC();

    wifiStrength = getStrength(numberPoints);

    int lightVal = analogRead(34);

    numberOfSamples++;

    if (numberOfSamples != 1) {
      avgWifiStrength = (.66666 * avgWifiStrength) + (.33333 * wifiStrength);
      avgTemp = (.66666 * avgTemp) + (.33333 * (temp - .7));
      avgHum = .66666 * avgHum + .33333 * hum;
      avgPres = .66666 * avgPres + .33333 * (pres / 100);
      avgCO2 = .66666 * avgCO2 + .33333 * CO2;
      avgTVOC = .66666 * avgTVOC + .33333 * TVOC;
      avgLightVal = .66666 * avgLightVal + .33333 * lightVal;
    }
    else
    {
      avgWifiStrength = wifiStrength;
      avgTemp = (temp - .7);
      avgHum = hum ;
      avgPres = pres / 100;
      avgCO2 = CO2;
      avgTVOC = TVOC;
      avgLightVal = lightVal ;
    }

    lastMeasureTime = millis();

    Serial.print("temp: ");
    Serial.print(temp - .7);
    Serial.print(" avgTemp: ");
    Serial.println(avgTemp);

    Serial.print(millis());
    Serial.print(" end measure. number of samples: ");
    Serial.println(numberOfSamples);
  }

  if ((millis() - lastUpdateThingspeakTime > postingInterval))
  {
    Serial.print(millis());
    Serial.println(" start update");

    httpRequest(avgWifiStrength, avgCO2, numberOfWifiRetry, avgPres, avgTemp, avgHum, avgLightVal, millis() / 1000 );

    Serial.print(millis());
    Serial.println(" end update");
  }
}

void connectWiFi() {

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(millis());
    Serial.println(" WiFi Connect...");
    WiFi.begin(ssid, password);
    delay(3000);
  }
}

void httpRequest(float field1Data, float field2Data, float field3Data, float field4Data, float field5Data, float field6Data, float field7Data, float field8Data) {

  WiFiClient client;

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    numberOfWifiRetry++;
  }

  if (!client.connect(server, 80)) {
    Serial.print(millis());
    Serial.println(" connection failed");
    lastUpdateThingspeakTime = millis();
    numberOfFailedHttp++;
    client.stop();
    return;
  }

  else {

    // create data string to send to ThingSpeak
    String data = "field1=" + String(field1Data) + "&field2=" + String(field2Data) + "&field3=" + String(field3Data) + "&field4=" + String(field4Data) + "&field5=" + String(field5Data) + "&field6=" + String(field6Data) + "&field7=" + String(field7Data) + "&field8=" + String(field8Data);

    // POST data to ThingSpeak
    Serial.print(millis());
    Serial.println(" start Data posted to ThingSpeak");

    client.println("POST /update HTTP/1.1");
    client.println("Host: api.thingspeak.com");
    client.println("Connection: close");
    client.println("User-Agent: ESP32WiFi/1.1");
    client.println("X-THINGSPEAKAPIKEY: " + writeAPIKey);
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.print(data.length());
    client.print("\n\n");
    client.print(data);

    lastUpdateThingspeakTime = millis();
  }
  client.stop();
}

// Take a number of measurements of the WiFi strength and return the average result.
int getStrength(int points) {
  long rssi = 0;
  long averageRSSI = 0;

  for (int i = 0; i < points; i++) {
    rssi += WiFi.RSSI();
    delay(20);
  }

  averageRSSI = rssi / points;
  return averageRSSI;
}
