#include <WiFiClientSecure.h>   // Include the HTTPS library
#include <ESP8266WiFi.h>        // Include the Wi-Fi library
#include <ESP8266WiFiMulti.h>   // Include the Wi-Fi-Multi library
#include <Wire.h> 
#include "time.h"
#include <UnixTime.h>


const char* ntpServer = "pool.ntp.org";
unsigned long epochTime;

UnixTime stamp(0);


#include <LiquidCrystal_I2C.h> //This library you can add via Include Library > Manage Library > 

LiquidCrystal_I2C lcd(0x3F, 20, 4);

#define SDA_PIN D0
#define SCL_PIN D1

ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'

const char* host = "mail.google.com"; // the Gmail server
const char* url = "/mail/feed/atom";  // the Gmail feed url
const int httpsPort = 443;            // the port to connect to the email server

const int relay = 0;

const char* credentials = "(add credentials here)";



int button_switch = 16; // external interrupt  pin
bool buttonState;
int pressedCycleCount = 0;

unsigned long ignore_messages_before_time = 0;
bool showing_message = false;

String getUnread() {    
  WiFiClientSecure client; 
  Serial.printf("Connecting to %s:%d ... \r\n", host, httpsPort);
  client.setInsecure();
  if (!client.connect(host, httpsPort)) {   // Connect to the Gmail server, on port 443
    Serial.println("Connection failed");    // If the connection fails, stop and return
    Serial.println(client.getLastSSLError());
    return "-1";
  }

  Serial.print("Requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" + 
               "Host: " + host + "\r\n" +
               "Authorization: Basic " + credentials + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n"); // Send the HTTP request headers

  Serial.println("Request sent");

  String unread = "-1";
  int instance = 0;
  int modified_instance = 0;

  //Serial.println(client.readString());

  for (int i=0; i<30; i++) {                          // Wait for the response. The response is in XML format
    //Serial.println(i);
    client.readStringUntil('<');                        // read until the first XML tag
    String tagname = client.readStringUntil('>');       // read until the end of this tag to get the tag name
    //Serial.println(tagname);

    if (tagname == "title") {                       // if the tag is <fullcount>, the next string will be the number of unread emails
      instance++;
      //Serial.println(instance);
      if (instance == 2) {
        String unreadStr = client.readStringUntil('<');   // read until the closing tag (</fullcount>)
        unread = unreadStr;
      }
    }

    if (tagname == "modified") {
      modified_instance++;
      if (instance == 2) {
        String ts = client.readStringUntil('<');
        Serial.println(ts);
        Serial.println(ts.substring(0,4));
        stamp.setDateTime(
          ts.substring(0,4).toInt(),
          ts.substring(5,7).toInt(),
          ts.substring(8,10).toInt(),
          ts.substring(11,13).toInt(),
          ts.substring(14,16).toInt(),
          ts.substring(17,19).toInt()
          );
        uint32_t unix = stamp.getUnix();
        Serial.println(unix);

        if (unix < ignore_messages_before_time) {
          unread = "-1";
        }

      }
    }

    if (tagname == "/feed") {
        // marks the end of the response
        break;
    }
  }
  Serial.println("Connection closed");

  return unread;                                        // Return the number of unread emails
}


unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return(0);
  }
  time(&now);
  return now;
}


void stringToLCD(String stringIn) {
  int x = 0;
  int y = 0;
  byte stillProcessing = 1;
  byte charCount = 1;
  String currentChar;
  lcd.clear();
  lcd.setCursor(0, 0);

  while (stillProcessing) {

    currentChar = stringIn[charCount - 1];

    if (x == 20 || currentChar == "\n" || currentChar == "%") {
      y += 1;
      x = 0;
    }

    lcd.setCursor(x, y);

    if (currentChar != "%") {
      lcd.print(currentChar);
      x += 1;
    }

    if (!stringIn[charCount]) {   // no more chars to process?
      stillProcessing = 0;
    }

    charCount += 1;

  }

}



void setup() {

  delay(2000);

  pinMode(relay, OUTPUT);
  digitalWrite(relay, LOW);
  pinMode(button_switch, INPUT);

  lcd.init();   // initializing the LCD
  lcd.backlight(); // Enable or Turn On the backlight


  Serial.begin(115200);         // Start the Serial communication to send messages to the computer

  delay(10);

  Serial.println('\n');

  wifiMulti.addAP("ewl-internet.ch_33165", "L668ssbHq7s");   // add Wi-Fi networks you want to connect to
  wifiMulti.addAP("PSB", "fdus3701");

  Serial.println("Connecting ...");
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(250);
    Serial.print('.');
  }
  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer
  Serial.println('\n');


  // Init and get the time
  configTime(0, 0, ntpServer);

  epochTime = getTime();
  Serial.println(epochTime);

}

void loop() {

  /*
  if (read_button() == switched) {
    //led_status = HIGH - led_status;  // toggle state
    //digitalWrite(LED, led_status);
    Serial.println("BUTTON PRESSED");
  }
  */


  //digitalWrite(relay, LOW);
  String unread = getUnread();
  //digitalWrite(relay, HIGH);
  if (unread == "-1") {
    Serial.println("\r\nYou've got no unread emails");
    digitalWrite(relay, LOW);
    showing_message = false;
    lcd.clear();
  }
  else {
    Serial.println(unread);  
    stringToLCD(unread);
    digitalWrite(relay, HIGH);
    showing_message = true;
  }
  Serial.println('\n');

  for (int i=0; i<1000; i++) {
    delay(50);
    buttonState = digitalRead(button_switch);
    if (buttonState == HIGH) {
      Serial.println("HIGH");
      pressedCycleCount = 0;
    }
    else {
      Serial.println("LOW");
      pressedCycleCount ++;
      if (showing_message == true and pressedCycleCount > 3) {
        digitalWrite(relay, LOW);
        lcd.clear();
        showing_message = false;
        ignore_messages_before_time = getTime();
        Serial.println(ignore_messages_before_time);
        break;
      }
      else if (pressedCycleCount > 3) {
        ignore_messages_before_time = 0;
        stringToLCD("Message for         Samantha...");
        break;
      }

    }
  }

}
