#include <DHT11.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <secrets.h>
// Local WiFi Network
// const char* ssid = ****************;
// const char* password = "****************";

// API config for transmitting sensor readings to external server
// String endpoint = myapp.com/api/transmit
// String api_key = "****************************************************************";
// const int project_id = 123456;

WiFiServer server(80); // Initiate WiFi Server
DHT11 dht11(2);  // Initiate DHT11 Hygro Sensor object using pin 2

struct HygroData { // Data structure for storing sensor data locally
  unsigned int time;
  byte temp;
  byte humidity;
};
HygroData data[10000];

int readInterval = 60; // time (sec) to wait between taking sensor readings
int lastReading = -1 * readInterval; // time (sec) when next sensor reading should be recorded locally
int nextIndex = 0; // next row to store sensor data locally

unsigned int lastUpdate = 0; // time (sec) last data was transmitted to external server
unsigned int updateInterval = 300; // time (sec) to wait between transmitting data to external server
	 
char buff[100]; // resuable buffer for formatting text with sprintf

// Define Reset Function
void(* resetDevice) (void) = 0;

// Command I/O
String readString;
const unsigned char MAX_WORDS = 10;
String userInput[MAX_WORDS];

void setup() {  // put your setup code here, to run once:
  Serial.begin(115200);
  // while (!Serial) ; // wait until Arduino Serial Monitor opens, FOR DEBUG ONLY
  delay(2000); // Wait for Serial buffer to catch up
  
  Serial.println("Available commands: reset, uptime, temp, humidity, readInterval <newValue>, updateInterval <newValue>");
  Serial.println();
  Serial.println();
  Serial.print("Connecting to network");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }

  Serial.println("");
  Serial.print("WiFi network connected: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
 
  server.begin();
}

void loop() {
  // Check serial input for commands from user
  processInput();

  // Read Sensor Data
  int tempc = 0;
  int humidity = 0;
  if (uptime() >= lastReading + readInterval) {
	  takeReading(tempc, humidity);
  }

  /////////////////////////////////////////////
  // Transmit latest data to external server //
  /////////////////////////////////////////////
  if ((uptime() - lastUpdate) > updateInterval) {
    // int last_temp = data[nextIndex - 1].temp;
    // int last_humidity = data[nextIndex - 1].humidity;
    String requestUrl = endpoint + "?temp=" + latestTemp() + "&humidity=" + latestHumidity() + "&project_id=" + project_id + "&sensor_id=" + sensor_id + "&API_KEY=" + api_key;
    // Serial.println(requestUrl); // For Debugging
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String serverNameStr = requestUrl;
      String serverPath = serverNameStr;
    
      // Your Domain name with URL path or IP address with path
      http.begin(serverPath.c_str());
    
      // If you need Node-RED/server authentication, insert user and password below
      //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
    
      // Send HTTP GET request
      int httpResponseCode = http.GET();
    
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastUpdate = uptime();
  }

  ////////////////////////////////
  // Respond to server requests //
  ////////////////////////////////
  WiFiClient client = server.available();   // listen for incoming clients
  if (client) {                         	// if you get a client,
    Serial.println("New Client.");      	// print a message out the serial port
    String currentLine = "";            	// make a String to hold incoming data from the client
    while (client.connected()) {        	// loop while the client's connected
      if (client.available()) {         	// if there's bytes to read from the client,
        char c = client.read();         	// read a byte, then
        Serial.write(c);                	// print it out the serial monitor
        if (c == '\n') {                	// if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            printTable(client);

            // The HTTP response ends with another blank line:
            client.println();
            break;
          } else {	// if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;  	// add it to the end of the currentLine
        }
      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}

int uptime() {
  return millis() / 1000; // Measured in seconds
}

void takeReading(int tempc, int humidity) {
  int result = dht11.readTemperatureHumidity(tempc, humidity);
  int tempf = c_to_f(tempc);
  int time = uptime();
  if (result == 0) {
    sprintf(buff, "Temperature: %i\u00B0F, Humidity: %i%%, Recorded at: %i seconds", tempf, humidity, time);
    Serial.println(buff);
    data[nextIndex].time = time;
    data[nextIndex].temp = tempf;
    data[nextIndex].humidity = humidity;
    nextIndex++;
    lastReading = time;
    // printData(); // For Debugging
  } else {
	Serial.println(DHT11::getErrorString(result));
  }
}

int c_to_f(int tempc) {
  int tempf = ((float)tempc * 9.0 / 5.0) + 32;
  return tempf;
}

void printTable(WiFiClient client) {
  client.print("<table><thead><tr><th>Temperature</th><th>Humidity</th><th>Time</th></thead><tbody>");
  for (int i=0; i<nextIndex; i++) {
    client.print("<tr>");
    client.print("<td>");
    client.print(data[i].temp);
    client.print("&deg;F</td>");

    client.print("<td>");
    client.print(data[i].humidity);
    client.print("%</td>");

    client.print("<td>");
    client.print(data[i].time);
    client.print(" seconds</td>");
    client.println("</tr>");
  }
  client.print("</tbody></table>");
}

void printData() {
  for (int i=0; i<15; i++) {
    Serial.print(data[i].temp);
    Serial.print("\u00B0F, ");
    Serial.print(data[i].humidity);
    Serial.print("%, ");
    Serial.println(data[i].time);
  }
}

int latestTemp() {
  return data[nextIndex - 1].temp;
}
int latestHumidity() {
  return data[nextIndex - 1].humidity;
}
//////////////////////////////////////////
// Command I/O                          //
// Syntax: command <arg1> <arg2> <etc.> //
//////////////////////////////////////////
void processInput() {
  // Try reading from input buffer
  while (Serial.available()) {
    delay(2);  // delay to allow byte to arrive in input buffer
    char c = Serial.read();
    readString += c;
  }
  if (readString.length() > 0) {
    delay(1); // allow time for message to reach output buffer
    int wordCount = 0;
    while (readString.length() > 0) { // Convert input string to array of words
      int index = readString.indexOf(' ');
      if (index == -1) { // Last or only word
        userInput[wordCount++] = readString;
        break;
      }
      userInput[wordCount++] = readString.substring(0, index);
      readString = readString.substring(index + 1);
    }
    String command = userInput[0];
    // Match available commands:
    if (command == "reset") {
      resetDevice();
    } else if (command == "readInterval") {
      if (is_valid_integer(userInput[1])) {
        readInterval = userInput[1].toInt();
        // if (uptime() - lastReading < readInterval)
        //   lastReading = 0;
        Serial.print("New Read Interval: ");
        Serial.print(readInterval);
        Serial.println(" seconds");
      } else {
        Serial.print("Current Interval: ");
        Serial.print(readInterval);
        Serial.println(" seconds");
      }
    } else if (command == "updateInterval") {
      if (is_valid_integer(userInput[1])) {
        updateInterval = userInput[1].toInt();
        // if (uptime() - lastUpdate < updateInterval)
        //   lastUpdate = 0;
        Serial.print("New Update Interval: ");
        Serial.print(updateInterval);
        Serial.println(" seconds");
      } else {
        Serial.print("Current Interval: ");
        Serial.print(updateInterval);
        Serial.println(" seconds");
      }
    } else if (command == "uptime") {
      Serial.print("Current Uptime: ");
      Serial.print(uptime());
      Serial.println(" seconds");
    } else if (command == "temp") {
      Serial.print("Latest Temperature: ");
      Serial.print(latestTemp());
      Serial.println("\u00B0F");
    } else if (command == "humidity") {
      Serial.print("Latest Humidity: ");
      Serial.print(latestHumidity());      
      Serial.println("%");
    } else {
      Serial.print("Command not recognized: "); 
      Serial.println(command);
    }

    // reset vars for next input
    readString="";
    for (int i=0; i<MAX_WORDS; i++) {
      userInput[i] = "";
    }
  }
}

bool is_valid_integer(String str) {
  if (str == "")
    return false; // int can't be blank
  size_t i = 0;
  if (str[0] == '-' || str[0] == '+') {
    i = 1; // begins with +/- sign
    if (str.length() == 1)
      return false;
  }
    
  for (; i < str.length(); ++i) {
    if (!isdigit(str[i])) // check if each character is a digit
      return false;
  }
  return true;
}