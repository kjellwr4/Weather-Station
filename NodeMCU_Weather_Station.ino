#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h> //Install "Adafruit Unified Sensor" in Manage Libraries.
#include <Adafruit_BMP280.h> //Install "Adafruit BMP280 Library" in Manage Libraries.
#include <DHT.h> //Install "DHT Sensor Library" in Manage Libraries.

#define DHTPIN 2 //Connect the signal pin of the DHT22 to D4 on the NodeMCU. Keep this at pin 2- the NodeMCU pin mapping is incorrect.
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
float elevation = ---.--;  //Elevation of deployment location in meters. https://developers.google.com/maps/documentation/javascript/examples/elevation-simple
float dewPointFast(float celcius, float humidity); //Function call declaration for dew point claculation.
float heatIndexPrecise(float fahrenheit, float humidity); //Function call declaration for heat index claculation.

Adafruit_BMP280 bme; //I2C Address for the GY-BMP280 is 0x77.

const char *ssid = "----------";  //ENTER YOUR WIFI SETTINGS
const char *password = "----------";
 
//Web/Server address to read/write from 
const char *host = "192.168.-.--"; // IP Address

void setup() {
  
  delay(1000);
  Serial.begin(115200);
  WiFi.mode(WIFI_OFF);        //Prevents reconnection issue (taking too long to connect)
  delay(1000);
  WiFi.mode(WIFI_STA);        //This line hides the viewing of ESP as wifi hotspot
  
  WiFi.begin(ssid, password);     //Connect to your WiFi router
  Serial.println("");
 
  Serial.print("Connecting");
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
 
  //If connection successful show IP address in serial monitor
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());  //IP address assigned to your ESP

  dht.begin();
  
  if (!bme.begin()) {  
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    while (1);
  }

}

void loop() {

  float h = dht.readHumidity(); //DHT22 relative humidity as a percent.
  float f = dht.readTemperature(true); //DHT22 Fahrenheit reading.
  float tempCelcius = dht.readTemperature(); //DHT22 Celcius reading.
  //float hif = dht.computeHeatIndex(f, h); //DHT22 heat index calculation. Not very precise.
  float bmp280Fah = ((bme.readTemperature()*1.8)+32); //GY-BMP280 temperature calculation.
  float tempAvg = (f + bmp280Fah)/2; //Average of DHT and BMP Fahrenheit temperatures.
  float relPressure = bme.readPressure(); //GY-BMP280 relative pressure calculation.
  float seaLevelPressure = ((relPressure/pow((1-((float)(elevation))/44330), 5.255))/100.0);
  float dewPointCelcius = dewPointFast(tempCelcius, h);
  float dewPointFah = ((dewPointCelcius*9)/5)+32;
  float heatIndex = heatIndexPrecise(tempAvg, h); //Precise heat index calculation based NOAA. https://www.wpc.ncep.noaa.gov/html/heatindex_equation.shtml

  String postData = "temp=" + String(tempAvg) //Change temp_dht. Change in log.php and MySQL database.
  + "&humidity=" + String(h)
  + "&heat_index=" + String(heatIndex)
  + "&dew_point=" + String(dewPointFah)
  + "&pressure=" + String(seaLevelPressure);

  Serial.println(tempAvg);
  Serial.println(h);
  Serial.println(heatIndex);
  Serial.println(dewPointFah);
  Serial.println(seaLevelPressure);
  Serial.print("postData String: ");
  Serial.println(postData);
  
  HTTPClient http; 
  http.begin("http://192.168.-.--/log.php"); // Change to include IP Address.
  http.addHeader("Content-Type", "application/x-www-form-urlencoded"); 
  int httpCode = http.POST(postData);   // Send the request.
  String payload = http.getString();    // Get the response payload.
  Serial.println(httpCode);   // Print HTTP return code. Greater than 0 means success.
  Serial.println(payload);    // Print request response payload.
  http.end();

  delay(300000); // Readings every 5 minutes.
}

//Function for calculating dew point (C). From https://gist.github.com/Mausy5043/4179a715d616e6ad8a4eababee7e0281
float dewPointFast(float celsius, float humidity) {
  double RATIO = 373.15 / (273.15 + celsius);
  double SUM = -7.90298 * (RATIO - 1);
  SUM += 5.02808 * log10(RATIO);
  SUM += -1.3816e-7 * (pow(10, (11.344 * (1 - 1/RATIO ))) - 1) ;
  SUM += 8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1))) - 1) ;
  SUM += log10(1013.246);
  double VP = pow(10, SUM - 3) * humidity;
  double T = log(VP/0.61078);   // temp var
  return (241.88 * T) / (17.558 - T);
}

//Function for calculating more precise heat index (F).
float heatIndexPrecise(float fahrenheit, float humidity) {
  float computedHI;
  double regressionHI = -42.379 + (2.04901523*fahrenheit);
  regressionHI += 10.14333127*humidity;
  regressionHI -= 0.22475541*fahrenheit*humidity;
  regressionHI -= 0.00683783*sq(fahrenheit);
  regressionHI -= 0.05481717*sq(humidity);
  regressionHI += 0.00122874*humidity*sq(fahrenheit);
  regressionHI += 0.00085282*fahrenheit*sq(humidity);
  regressionHI -= 0.00000199*sq(fahrenheit)*sq(humidity);
  double simpleHI = (0.5*(fahrenheit + 61 + ((fahrenheit - 68)*1.2) + (humidity*0.094)));
  double steadmanComp = (fahrenheit+simpleHI)/2;
  if (steadmanComp < 80) {
    computedHI = simpleHI;
    return computedHI;
  }
  else if (steadmanComp > 80 && fahrenheit >= 80 && fahrenheit <= 112 && humidity < 13) {
    double squareRoot = ((17-abs(fahrenheit-95))/17);
    double adjustment = ((13-humidity)/4)*sqrt(squareRoot);
    computedHI = regressionHI-adjustment;
    return computedHI;
  }
  else if (steadmanComp > 80 && fahrenheit >= 80 && fahrenheit < 87 && humidity > 85) {
    double adjustment = ((humidity-85)/10)*((87-fahrenheit)/5); 
    computedHI = regressionHI+adjustment;
    return computedHI;
  }
  else {
    computedHI = regressionHI;
    return computedHI;
  }
}
