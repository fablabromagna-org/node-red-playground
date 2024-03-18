#include <Arduino.h>

const int LDR = A0;
const int BUTTON = 4;
const int RED = 15;
const int GREEN = 12;
const int BLUE = 13;

#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"



/**************** Configuration from external file *******************/
#define MYCONFIG
#include "myconfig.h"



/************************ Local config if myconfig.h is not used *********************************/
#ifndef MYCONFIG

/* WiFi Access Point */
#define WLAN_SSID       "myssid"
#define WLAN_PASS       "mypassword"

/* MQTT  */
#define AIO_SERVER      "test.mosquitto.org"
#define AIO_SERVERPORT  1883                   // use 8883 for SSL
#define AIO_USERNAME    ""
#define AIO_KEY         ""
#define MQTT_BASE "flr/ws20240316/witty1"

#endif
/************************/


// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
// or... use WiFiFlientSecure for SSL
//WiFiClientSecure client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Setup a topic for publishing photocell.
Adafruit_MQTT_Publish mqtt_photocell = Adafruit_MQTT_Publish(&mqtt, MQTT_BASE "/light");

// Setup a topic for publishing button.
Adafruit_MQTT_Publish mqtt_button = Adafruit_MQTT_Publish(&mqtt, MQTT_BASE "/button");


// subscrive for onoff and rgb values changes
Adafruit_MQTT_Subscribe mqtt_led_onoff = Adafruit_MQTT_Subscribe(&mqtt, MQTT_BASE "/led_onoff");
Adafruit_MQTT_Subscribe mqtt_led_rgb = Adafruit_MQTT_Subscribe(&mqtt, MQTT_BASE "/led_rgb");
bool networking_ok = true;

void MQTT_connect();
void set_rgb_led(char* rgb_val) ;
void set_rgb_led_n(uint32_t rgb_val) ;

void setup()
{
  pinMode(LDR, INPUT);
  pinMode(BUTTON, INPUT);
  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);

  set_rgb_led(strdup("#00ff00"));
  Serial.begin(115200);
  delay(200);


  Serial.println(F("Adafruit MQTT demo"));

  

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);
  //Serial.println(WLAN_PASS);

  set_rgb_led(strdup("#ff0000"));
 

  int retry = 20;
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    retry--;
    if (retry == 0) { networking_ok = false; break;}
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.print("IP address: "); Serial.println(WiFi.localIP());

  set_rgb_led(strdup("#0000ff"));
  delay(500);
  set_rgb_led(strdup("#00ff00"));
  delay(500);
  set_rgb_led(strdup("#0000ff"));
  delay(500);
  set_rgb_led(strdup("#00ff00"));

  delay(1000);
  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&mqtt_led_rgb);
  mqtt.subscribe(&mqtt_led_onoff);
}


int photocell_threshold = 5;
int photocell_val_prev = -1;
int photocell_millis_prev = -1;
int button_val_prev = -1;
uint32_t g_rgb_val= 0;


void loop()
{
  bool mqtt_msg_sent = false;
  int photocell_val = analogRead(LDR);
  int button_val = digitalRead(BUTTON);

  Serial.print("LIGHT: ");
  Serial.println(photocell_val);
  Serial.print("BUTTON: ");
  Serial.println(button_val);

  Serial.print(F("Networking ok: "));
  Serial.println(networking_ok);
  if (networking_ok) {

    // Ensure the connection to the MQTT server is alive (this will make the first
    // connection and automatically reconnect when disconnected).  See the MQTT_connect
    // function definition further below.
    MQTT_connect();

    // this is our 'wait for incoming subscription packets' busy subloop
    // try to spend your time here

    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt.readSubscription(100))) {
      if (subscription == &mqtt_led_rgb) {
        char* rgb_val = (char*) mqtt_led_rgb.lastread;
        *(rgb_val+7)= 0x00;
        long rgb =  strtol(rgb_val, 0, 16);
        g_rgb_val = rgb;
        Serial.print(F(">>>>>>> Got RGB: "));
        Serial.println(rgb_val);

        Serial.print(F(">>>>>>> Got RGB VAL: "));
        Serial.println(rgb);

        set_rgb_led_n(rgb);

      }
      else if (subscription == &mqtt_led_onoff) {
        char* led_onoff_val = (char*) mqtt_led_onoff.lastread;

        if (strcmp(led_onoff_val, "true") == 0) {
          Serial.println("Switch Led ON");
          set_rgb_led_n(g_rgb_val);
        }
        else {
          Serial.println("Switch Led OFF");
          set_rgb_led_n(0);
        }

      }
    }

    // Now we can publish stuff, if changed or every n sec
  }

  long diff = millis()-photocell_millis_prev;
  if ((abs(photocell_val - photocell_val_prev) > photocell_threshold) ||
      (diff > 3000)) {
    Serial.print(F("\nMQTT > Send photocell val:"));
    Serial.print(photocell_val);
    Serial.print(" -> ");
    


    if (networking_ok) {
      if (! mqtt_photocell.publish(photocell_val)) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("OK!"));
        photocell_val_prev = photocell_val;
        photocell_millis_prev = millis();
        mqtt_msg_sent = true;
      }
    }
  }
    

  if (button_val != button_val_prev) {
    Serial.print(F("\nMQTT > Send button val:"));
    Serial.print(button_val);
    Serial.print(" -> ");

    if (networking_ok) {
      if (! mqtt_button.publish(!button_val)) {
        Serial.println(F("Failed"));
      } else {
        button_val_prev = button_val;
        Serial.println(F("OK!"));
        mqtt_msg_sent = true;
      }
    }
  }


  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  if (!mqtt_msg_sent) {
    if(! mqtt.ping()) {
      mqtt.disconnect();
    }
  }

  delay(500);
}



void set_rgb_led_n(uint32_t rgb_val) {

  Serial.println(rgb_val);
  int r = ((rgb_val>>16) & 0xff );
  int g = ((rgb_val>>8) & 0xff);
  int b = ((rgb_val) & 0xff);
  Serial.printf("Set RGB %d -> %d, %d, %d\n", rgb_val, r,g,b);
  
  Serial.println(r);
  // Serial.println(g);
  // Serial.println(b);

  analogWrite(RED, r*1023/255 );
  analogWrite(GREEN, g*1023/255 );
  analogWrite(BLUE, b*1023/255 );
}

// rgb_val = '#ffffff'
void set_rgb_led(char* rgb_val) {

  uint32 rgb =  strtol(rgb_val+1, 0, 16);

  g_rgb_val = rgb;
 
  set_rgb_led_n(rgb);
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");


  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    set_rgb_led(strdup("#ff00ff")); 
    
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
  set_rgb_led(strdup("#00ff00"));
}
