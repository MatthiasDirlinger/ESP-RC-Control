#include <ESP8266WiFi.h>

extern "C" { 
  #include <espnow.h> 
}


uint8_t remoteMac1[] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x03};
uint8_t remoteMac2[] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x02};
uint8_t remoteMac3[] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x03};
uint8_t remoteMac4[] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x04};
#define WIFI_CHANNEL 4

#define PPM_PIN 2 //GPIO2, D4 on wemos D1 mini
#define CHANNELS 8
#define RCdataSize 16

static unsigned long PPMDuration;
volatile uint16_t rcValue[CHANNELS];
volatile boolean gotRC;

typedef struct 
{
  uint16_t Ch1     : 11; 
  uint16_t Ch2     : 11;
  uint16_t Ch3     : 11;
  uint16_t Ch4     : 11;
  uint16_t Ch5     : 11;
  uint16_t Ch6     : 11;
  uint16_t Ch7     : 11;
  uint16_t Ch8     : 11;
  uint8_t spare    : 8;
}Payload;
typedef union
{
  Payload chans;
  uint8_t data[CHANNELS*2];
} RCdataTY;
RCdataTY RCdata;

void recv_cb(u8 *macaddr, u8 *data, u8 len)
{
};

void send_cb(uint8_t* mac, uint8_t sendStatus) 
{
};

void setup() 
{
  Serial.begin(115200); Serial.println();

  WiFi.mode(WIFI_STA); // Station mode for esp-now 
  WiFi.disconnect();

  Serial.printf("This mac: %s, ", WiFi.macAddress().c_str()); 
  Serial.printf(", channel: %i\n", WIFI_CHANNEL); 

  if (esp_now_init() != 0) Serial.println("*** ESP_Now init failed");

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

  pinMode(5, INPUT_PULLUP);
  pinMode(4, INPUT_PULLUP);
  if(digitalRead(4) == HIGH && digitalRead(5) == HIGH) { esp_now_add_peer(remoteMac1, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, NULL, 0); }
  else if (digitalRead(4) == HIGH) { esp_now_add_peer(remoteMac2, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, NULL, 0); }
  else if (digitalRead(5) == HIGH) { esp_now_add_peer(remoteMac3, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, NULL, 0); }
  else { esp_now_add_peer(remoteMac4, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, NULL, 0); }

  esp_now_register_recv_cb(recv_cb);
  esp_now_register_send_cb(send_cb);

  pinMode(PPM_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PPM_PIN), PPM_read_channel, CHANGE);  
}

void loop() 
{
  delay(10);
  
  if (gotRC)
  {
    gotRC = false;
    RCdata.chans.Ch1 = rcValue[0];   
    RCdata.chans.Ch2 = rcValue[1];   
    RCdata.chans.Ch3 = rcValue[2];   
    RCdata.chans.Ch4 = rcValue[3];   
    RCdata.chans.Ch5 = rcValue[4];   
    RCdata.chans.Ch6 = rcValue[5];   
    RCdata.chans.Ch7 = rcValue[6];   
    RCdata.chans.Ch8 = rcValue[7];
    
    Serial.print(rcValue[0]);Serial.print("\t");
    Serial.print(rcValue[1]);Serial.print("\t");
    Serial.print(rcValue[2]);Serial.print("\t");
    Serial.print(rcValue[3]);Serial.print("\t");
    Serial.print(rcValue[4]);Serial.print("\t");
    Serial.print(rcValue[5]);Serial.print("\t");
    Serial.print(rcValue[6]);Serial.print("\t");
    Serial.print(rcValue[7]);Serial.print("\n");

    esp_now_send(NULL, RCdata.data, CHANNELS*2); 
  }    
}

ICACHE_RAM_ATTR void PPM_read_channel(){
  static unsigned int pulse;
  static byte i;
  static unsigned long last_micros;

  PPMDuration = micros() - last_micros;
  last_micros = micros();

  if(PPMDuration < 510){
    pulse = PPMDuration;
  }
  else if(PPMDuration > 1910){
    i = 0;
  }
  else{
    if (i<CHANNELS) {
      rcValue[i] = PPMDuration + pulse;
    }
    if (i == 5) {
      gotRC = true;
    }
    i++;
  }
}
