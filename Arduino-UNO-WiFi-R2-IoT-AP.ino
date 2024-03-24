/*
  Board : Arduino UNO WiFi R2
  Mode  : AP
  Prog  : IoT

  Program is intended for teaching and learning (no https, no login)

  Using the program:
      Find board IP-address     : Check output from serial monitor when board is booting. Mind the serial monitor speed
      Monitor and control ports : Browse board IP for monitoring and changing port values
      Basic user configuration  : Edit digital port arrays DPU, DPO, DPV, PWO, PWV, DPN and DPC
                                  Edit analog port arrays APU, APO, APV, APN and APC

  Based on examples from https://docs.arduino.cc/tutorials/communication/wifi-nina-examples
    
  GitHub/OJStuff, march 24, 2024, v1.0
*/

#define SERIAL_MONITOR_BAUDRATE 115200  // Serial monitor speed
#define WL_RETRY_TIME 5000              // Wireless retry time in ms

#include <SPI.h>       // Library for communication with the WiFi module
#include <WiFiNINA.h>  // Library for using the WiFi module

const String Board = "Arduino UNO WiFi R2";  // Board type
const String Mode = "AP";                      // Board mode
const String Prog = "IoT";                     // Brogram name

//  IPAddress ip(192, 168, 1, 1);  // Define static IP here, 192.168.4.1 is the default. See also WiFiAPCreate() below
//  https://www.arduino.cc/reference/en/libraries/wifinina/wifi.config/

const char ssid[] = "Arduino-IoT-AP";  // Network AP SSID name.
const char pass[] = "12345678";      // Network AP SSID password.

byte mac[6];                  // MAC address for Wifi Module
int status = WL_IDLE_STATUS;  // WiFi system is starting in idle mode

WiFiServer server(80);  // Define server as a WiFiServer object on port 80 (HTTP)
WiFiClient client;      // Define client as a WiFiClient object

bool Touchcontrol = false;  // Switch to activate/deactivate touchcontrol support (for PWM sliders)
bool WiFiinfo = true;       // Switch to activate/deactivate WiFi information

// Digital ports     0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,11,12,13       Digital Port number
const bool DPU[] = { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };  // Port Used (1=YES, 0=NO)
const bool DPO[] = { 0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 0 };  // Port Output (1=YES, 0=INPUT)
const bool PWO[] = { 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0 };  // PWM Output (1=YES, 0=NO)
bool DPV[]       = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };  // Port Value (and default value)
int PWV[]        = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };  // PWM Value (and default value)

const String DPN[] = { "P0 (rx)", "P1 (tx)", "P2", "P3 ~",      // Digital Port Names
                       "P4", "P5 ~", "P6 ~", "P7", "P8",        // Digital Port Names
                       "P9 ~", "P10 ~", "P11", "P12", "P13" };  // Digital Port Names

const String DPC[] = { "(used by serial monitor)", "(used by serial monitor)",  // Digital Port Comments
                       "", "(%) PWM", "", "(%) PWM",                            // Digital Port Comments
                       "(%) PWM", "", "", "(%) PWM",                            // Digital Port Comments
                       "(%) PWM", "", "", "" };                                 // Digital Port Comments

const int DPnr = (sizeof(DPU) - 1);  // Highest digital port nr
const int PWM_max = 255;             // Max PWM value, according to board type

// Analog ports      0, 1, 2, 3, 4, 5     Analog Port number
const bool APU[] = { 1, 1, 1, 1, 1, 1 };  // Port Used (1=YES, 0=NO)
const bool APO[] = { 0, 0, 0, 0, 0, 0 };  // Port Output (1=YES, 0=NO)
float APV[]      = { 0, 0, 0, 0, 0, 0 };  // Port Value (and default value)

const String APN[] = { "A0", "A1", "A2", "A3", "A4", "A5" };  // Analog Port Names

const String APC[] = { "(0-5.0v)", "(0-5.0v)", "(0-5.0v)",    // Analog Port Comments. Rename as you wish
                       "(0-5.0v)", "(0-5.0v)", "(0-5.0v)" };  // Analog Port Comments. Rename as you wish

const int APnr = (sizeof(APU) - 1);  // Highest analog port nr
const int ADC_max = 1023;            // ADC is 10 bits, 0-1023
const float ADC_ref = 5.0;           // ADC voltage reference

const String PMN[] = { "Input", "Output" };  // Digital Port Mode Name
const String DPVN[] = { "Off", "On" };       // Digital Port state Value Name
const String DPVC[] = { "red", "green" };    // Digital Port state Value Color (HTML font color name)

void setup() {
  Serial.begin(SERIAL_MONITOR_BAUDRATE);  // Config serial monitor
  while (!Serial) {
  }

  DPorts_setup(DPnr);  // Setup digital ports according to arrays
  APorts_setup(APnr);  // Setup analog ports according to arrays

  if (WiFi.status() == WL_NO_MODULE) {  // Check if WiFi module is present
    Serial.println("Communication with WiFi module failed!");
    while (true)
      ;  // If no supported WiFi module found, stop program
  }

  String fv = WiFi.firmwareVersion();
  Serial.print(Board);
  Serial.print(" has WiFiNINA firmware ");
  Serial.print(fv);
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {  // Information about firmware update
    Serial.println();
    Serial.print("    Please update to the latest WiFiNINA firmware version ");
    Serial.println(WIFI_FIRMWARE_LATEST_VERSION);
  } else {  // Inform that existing firmware version is up to date
    Serial.println();
    Serial.println("    Installed firmware is the latest WiFiNINA firmware version");
    Serial.println();
  }

  WiFiAPCreate();     // Create AP
  server.begin();     // Start web server
  printWifiStatus();  // Print network status

  // Inform that Arduino server is ready
  Serial.print(Board);
  Serial.print(" ");
  Serial.print(Mode);
  Serial.print(" - ");
  Serial.print(Prog);
  Serial.println(" is ready...");
  Serial.println();
}

void loop() {  // Main loop handles client requests and server responses
  // compare the previous status to the current status
  if (status != WiFi.status()) {  // it has changed update the variable
    status = WiFi.status();
    if (status == WL_AP_CONNECTED) {  // a device has connected to the AP
      Serial.println("Device connected to AP");
    } else {  // a device has disconnected from the AP, and we are back in listening mode
      Serial.println("Device disconnected from AP");
    }
  }

  client = server.available();           // Listen for incoming client requests
  if (client) {                          // If we get a client request,
    Serial.println("client connected");  // inform about new client connected
    Serial.println();
    Serial.println("new client request");  // Inform about new client on the serial terminal
    String currentLine = "";               // Create a String to hold incoming data from the client
    while (client.connected()) {           // Loop while the client's connected
      if (client.available()) {            // If there's characters to read from the client,
        char c = client.read();            // read a character, and then
        Serial.write(c);                   // print it on the serial terminal
        if (c == '\n') {                   // If the character is a newline character,
          // that's the end of the client HTTP request, so send server response to client
          if (!currentLine.length()) {  // If the current line is blank, we got two newline characters in a row.
                                        // That's the end of the client HTTP request, so send server response to client
            ServerResponse();           // Call routine to handle response from server
            break;                      // After the response, break out of the loop
          } else {                      // If we got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // If we got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
      ClientRequest(currentLine);  // Call routine to handle client request
    }
    client.stop();                         // Close the connection
    Serial.println("client disonnected");  // Inform about client disconnection on the serial terminal
    Serial.println();
  }
}

void ClientRequest(String Request) {  // Handle request from client and update port if set up for output
  for (int i = 0; i <= DPnr; i++) {
    if (DPU[i] and DPO[i] and !PWO[i]) {                     // Check if port is in use and also set up for output
      if (Request.endsWith("GET /P" + str_dd(i) + "-on")) {  // Request for port ON
        DPV[i] = true;                                       // Set port value in table
        digitalWrite(i, DPV[i]);                             // Set port value
      }
      if (Request.endsWith("GET /P" + str_dd(i) + "-off")) {  // Request for port OFF
        DPV[i] = false;                                       // Set port value in table
        digitalWrite(i, DPV[i]);                              // Set port value
      }
    }
    if (DPU[i] and DPO[i] and PWO[i]) {                     // Check if port is in use and also set up for PWM output
      if (Request.endsWith("pwm")) {                        // If request contains PWM (like /GET P03-123pwm)
        int pwmS = Request.indexOf("-") + 1;                // Finf index of "-" in the /GET command
        int pwmE = Request.indexOf("pwm") + 1;              // Finf index of "pwm" in the /GET command
        String pwm_str = Request.substring(pwmS, pwmE);     // What we want is the % PWM bethween pwmS and pwmE
        if (Request.indexOf("GET /P" + str_dd(i)) != -1) {  // Do we have a request on a valid port?
          PWV[i] = pwm_str.toInt();                         // If so, set port % PWM value in table
          analogWrite(i, PWV[i] * PWM_max / 100);           // Set port PWM value (0 - 255)
        }
      }
    }
  }

  if (Request.indexOf("GET /touchcontrol-on") != -1) {  // Do we have a request for turning Touchcontrol on?
    Touchcontrol = true;                                // If so, set Touchcontrol switch on
  }
  if (Request.indexOf("GET /touchcontrol-off") != -1) {  // Do we have a request for turning Touchcontrol off?
    Touchcontrol = false;                                // If so, set Touchcontrol switch off
  }

  if (Request.indexOf("GET /wifiinfo-on") != -1) {  // Do we have a request for turning WiFiinfo on?
    WiFiinfo = true;                                // If so, set WiFiinfo switch on
  }
  if (Request.indexOf("GET /wifiinfo-off") != -1) {  // Do we have a request for turning WiFiinfo off?
    WiFiinfo = false;                                // If so, set WiFiinfo switch off
  }
}

void ServerResponse() {
  client.println("HTTP/1.1 200 OK");          // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  client.println("Content-type: text/html");  // Content-type is text/html
  client.println("Connection: close");        // Connection: ends after completion of the response
  client.println("Refresh: 60");              // Client/browser will refresh/update the page every 60 sec
  client.println();

  // The content of the HTTP response follows the header
  client.println("<!DOCTYPE html>");  //  Document is HTML5 type
  client.println("<html><head><style>");
  client.println("table {font-family: courier; font-size: 20px; border-collapse: collapse; width: 100%;}");
  client.println("button {width: 60px; height: 30px; font-size: 16px; font-weight: bold; text-align: center; border-radius: 8px;}");
  client.println("td, th {border: 1px solid #dddddd; text-align: left; padding: 8px;}");
  client.println("tr:nth-child(even) {background-color: #dddddd;}");
  client.println("</style></head>");

  client.println("<br>");
  client.println("<a href=/><button>Home</button></a>");  // "HOME" button

  client.println("<body><h2>" + Board + " " + Prog + " - " + Mode + "</h2>");  // Page header

  if (Touchcontrol) {  // Handle activation for touchcontrol for sliders
    client.print("<p><input checked type='checkbox' name='touch' value='off' id='radio1' ");
    client.println("onclick=\"{location.href='/touchcontrol-' + this.value;}\">Touchscreen</p>");
  } else {  // Handle activation for mousecontrol for sliders
    client.print("<p><input type='checkbox' name='touch' value='on' id='radio1' ");
    client.println("onclick=\"{location.href='/touchcontrol-' + this.value;}\">Touchscreen</p>");
  }

  if (WiFiinfo) {  // Handle activation for WiFi info on screen
    client.print("<p><input checked type='checkbox' name='touch' value='off' id='radio2' ");
    client.println("onclick=\"{location.href='/wifiinfo-' + this.value;}\">Network info</p>");
  } else {  // Handle deactivation for WiFi info on screen
    client.print("<p><input type='checkbox' name='touch' value='on' id='radio2' ");
    client.println("onclick=\"{location.href='/wifiinfo-' + this.value;}\">Network info</p>");
  }

  client.println("<table>");
  client.println("<tr><th>Port</th><th>Mode</th><th>Control</th><th>Value</th><th>Comment</th></tr>");  // Table header

  for (int i = 0; i <= DPnr; i++) {  // Show state for digital ports, and create control elements
    String Control = "";
    if (DPU[i]) {                                                                                // If port in use
      if (DPO[i] and !PWO[i]) {                                                                  // If output without PWM
        Control = "<a href=/P" + str_dd(i) + "-on><button>ON</button></a>" + " " + "<a href=/P"  // Create control element for port
                  + str_dd(i) + "-off><button>OFF</button></a>";
        client.println(HTML_DP(DPN[i], PMN[DPO[i]], Control, DPV[i], DPVN[DPV[i]], DPC[i]));  // Show HTML line with control
      }
      if (DPO[i] and PWO[i]) {  // If output with PWM,
        if (Touchcontrol) {     // create touch control element for PWM ports
          Control = "<input type='range' min='0' max='100' value='" + String(PWV[i]) + "' step='5' class='slider' id='pot"
                    + str_dd(i) + "'" + "ontouchend=\"{location.href='/P' + '" + str_dd(i) + "-' + this.value + 'pwm';}\">";
        } else {  // Create mouse control element for PWM ports
          Control = "<input type='range' min='0' max='100' value='" + String(PWV[i]) + "' step='5' class='slider' id='pot"
                    + str_dd(i) + "'" + "onmouseup=\"{location.href='/P' + '" + str_dd(i) + "-' + this.value + 'pwm';}\">";
        }
        client.println(HTML_PWO(DPN[i], PMN[DPO[i]], Control, PWV[i], DPC[i]));  // Show HTML line with control
      }
      if (!DPO[i]) {                                                                          // If ordinary input,
        client.println(HTML_DP(DPN[i], PMN[DPO[i]], Control, DPV[i], DPVN[DPV[i]], DPC[i]));  // show HTML line without control
      }
    }
  }

  for (int i = 0; i <= APnr; i++) {  // Show state for analog ports, and create control elements
    String Control = "";
    if (APU[i]) {                                  // For each Analog Port in Use,
      APV[i] = analogRead(i) * ADC_ref / ADC_max;  // take a sample and convert to volts
      //if (i == 1) {
      //  APV[i] = ((APV[i] * 1000) - 500) / 10;     // Convert from mV to degrees celsius
      //}
      client.println(HTML_AP(APN[i], PMN[APO[i]], Control, APV[i], APC[i]));  // Show HTML line with dummycontrol
    }
  }

  client.println("</table>");  // End of table

  if (WiFiinfo) {                                  // If WiFiinfo is wanted,
    client.println("<p>" + HTML_WiFi() + "</p>");  // print WiFi status on client
  }
  client.println("</body></html>");  // End of HTML code
  client.println();                  // The HTTP response ends with another blank line
}

String HTML_DP(String Name, String Mode, String Control, bool Value, String ValueName, String Comment) {  // Create HTML for Digital Ports
  return ("<tr><th>" + Name + "</th><th>" + Mode + "</th><th>" + Control + "</th><th><font style=color:"
          + DPVC[Value] + ";>" + ValueName + "</font></th><th>" + Comment + "</th></tr>");
}

String HTML_PWO(String Name, String Mode, String Control, int Value, String Comment) {  // Create HTML for PWM Ports
  return ("<tr><th>" + Name + "</th><th>" + Mode + "</th><th>" + Control + "</th><th><font style=color:"
          + "black" + ";>" + Value + "</font></th><th>" + Comment + "</th></tr>");
}

String HTML_AP(String Name, String Mode, String Control, float Value, String Comment) {  // Create HTML for Analog Ports
  return ("<tr><th>" + Name + "</th><th>" + Mode + "</th><th>" + Control + "</th><th>"
          + Value + "</th><th>" + Comment + "</th></tr>");
}

String HTML_WiFi() {  // Create HTML for WiFi status
  return ("* Arduino IP : " + WiFiIP_Str() + " (" + WiFiMAC_Str() + " * SSID : " + String(ssid) + " * Client IP : " + Client_IP_Str());
}

void WiFiAPCreate() {  // Create WiFi AP network
  Serial.print("Creating access point named: ");
  Serial.println(ssid);
  //  WiFi.config(ip);  // If you want static IP. Make shure WiFi.config is done before WiFi.begin
  //  https://www.arduino.cc/reference/en/libraries/wifinina/wifi.config/
  status = WiFi.beginAP(ssid, pass);  // Create WiFi network with gived SSID (ssid) and password (pass)
  if (status != WL_AP_LISTENING) {    // If creation of AP fails
    Serial.println("Creating access point failed");
    while (true)
      ;  // stop program
  }
  delay(WL_RETRY_TIME);  // Wait some time for AP to create
}

void printWifiStatus() {  // Print network status on serial monitor
  // Print board IP address
  Serial.print("    IP address  : ");
  Serial.println(WiFiIP_Str());

  // Print MAC address
  Serial.print("    MAC address : ");
  Serial.println(WiFiMAC_Str());

  // Print WiFi encryption type
  Serial.print("    Encryption  : ");
  Serial.println(WiFiEnc_Str());
  Serial.println();
}

String WiFiIP_Str() {  // Create WiFi IP address string
  IPAddress ip = WiFi.localIP();
  String IP0 = String(ip[0]);
  String IP1 = String(ip[1]);
  String IP2 = String(ip[2]);
  String IP3 = String(ip[3]);
  return (String(IP0 + "." + IP1 + "." + IP2 + "." + IP3));
}

String Client_IP_Str() {  // Create client IP address string
  IPAddress ip = client.remoteIP();
  String IP0 = String(ip[0]);
  String IP1 = String(ip[1]);
  String IP2 = String(ip[2]);
  String IP3 = String(ip[3]);
  return (String(IP0 + "." + IP1 + "." + IP2 + "." + IP3));
}

String WiFiMAC_Str() {  // Create WiFi MAC address string
  WiFi.macAddress(mac);
  String MAC0 = String(mac[0], HEX);
  String MAC1 = String(mac[1], HEX);
  String MAC2 = String(mac[2], HEX);
  String MAC3 = String(mac[3], HEX);
  String MAC4 = String(mac[4], HEX);
  String MAC5 = String(mac[5], HEX);
  return (MAC5 + ":" + MAC4 + ":" + MAC3 + ":" + MAC2 + ":" + MAC1 + ":" + MAC0);
}

String WiFiEnc_Str() {  // Create string for WiFi encryption type
  String WiFiEnc_name[] = { "", "", "WPA (TKIP)", "", "WPA (CCMP)", "WEP", "", "NONE", "AUTO" };
  return (WiFiEnc_name[WiFi.encryptionType()]);
}

void DPorts_setup(int Ports) {  // Setup digital ports
  for (int i = 0; i <= Ports; i++) {
    if (DPU[i]) {                 // Is port in use?
      pinMode(i, DPO[i]);         // Set input or output
      if (!DPO[i]) {              // If input,
        DPV[i] = digitalRead(i);  // read port value
      }
      if (DPO[i] and !PWO[i]) {   // If output without PWM,
        digitalWrite(i, DPV[i]);  // write port value
      }
      if (DPO[i] and PWO[i]) {   // If output with PWM,
        analogWrite(i, PWV[i]);  // write port PWM value
      }
    }
  }
}

void APorts_setup(int Ports) {  // Setup analog ports
  for (int i = 0; i <= Ports; i++) {
    if (APU[i]) {              // Is port in use?
      APV[i] = analogRead(i);  // Read port value
    }
  }
}

String str_dd(int nr) {  // Create numbers with 2 digits: 00, 01, 02..., 99
  if (nr >= 0; nr < 100) {
    String add100 = String(100 + nr);
    return (String(add100[1]) + String(add100[2]));
  }
}
