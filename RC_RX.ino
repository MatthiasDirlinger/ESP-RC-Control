#include <ESP8266WiFi.h>
#include <Servo.h>

extern "C" { 
  #include <espnow.h>
  #include <user_interface.h>
}

uint8_t myMac[] = {0x10, 0x10, 0x10, 0x10, 0x10, 0x02};
#define WIFI_CHANNEL 4

#define CHANNELS 8
#define RCdataSize 16
#define RX_PPM_PULSE_LENGTH 300  //set the pulse length
#define RX_PPM_FRAME_LENGTH 22500  //set the PPM frame length in microseconds (1ms = 1000µs)

#define PWM1_PIN 16
#define PWM2_PIN 14
#define PWM3_PIN 12
#define PWM4_PIN 13
#define PWM5_PIN 5
#define PWM6_PIN 4
#define PWM7_PIN 0
#define PWM8_PIN 2

#define PPM_PIN 15                             //GPIO2 (D4)
#define PPM_POLARITY 0                        //Set polarity of the pulses: 1=Negative, 0=Positive. Default=1.

volatile uint16_t rcValue[CHANNELS];
volatile boolean gotRC;
volatile int peernum = 0;
volatile unsigned long rx_next_timer_int;
unsigned long timeout;

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

int usMin = 1000;                             // Minimal PWM value
int usMax = 2000;                             // Maximal PWM value
int usMiddle = usMin+((usMax-usMin)/2);       // Average PWM value
Servo servoCh[CHANNELS];
int chPin[] = {PWM1_PIN, PWM2_PIN, PWM3_PIN, PWM4_PIN, PWM5_PIN, PWM6_PIN, PWM7_PIN, PWM8_PIN};

void recv_cb(u8 *macaddr, u8 *data, u8 len)
{
  if (len == (CHANNELS*2))
  {
    for (int i=0;i<(CHANNELS*2);i++) 
    {
      RCdata.data[i] = data[i];
    }
    gotRC=true;
  }
  if (!esp_now_is_peer_exist(macaddr))
  {
    Serial.println("adding peer ");
    esp_now_add_peer(macaddr, ESP_NOW_ROLE_COMBO, WIFI_CHANNEL, NULL, 0);
    peernum++;
  }
};

void send_cb(uint8_t* mac, uint8_t sendStatus) 
{
};

void inline rx_ppmISR(void){
  static boolean rx_ppm_state = true;
  if (rx_ppm_state) {
    digitalWrite(PPM_PIN, !PPM_POLARITY);
    rx_next_timer_int = rx_next_timer_int + (RX_PPM_PULSE_LENGTH * (F_CPU/1000000));
    rx_ppm_state = false;
  } 
  else
  {
    static byte cur_chan_numb;
    static unsigned int calc_rest;
  
    digitalWrite(PPM_PIN, PPM_POLARITY);
    rx_ppm_state = true;

    if(cur_chan_numb >= CHANNELS){
      cur_chan_numb = 0;
      calc_rest = calc_rest + RX_PPM_PULSE_LENGTH;// 
      rx_next_timer_int = rx_next_timer_int + ((RX_PPM_FRAME_LENGTH - calc_rest) * (F_CPU/1000000));
      calc_rest = 0;
    }
    else
    {
      int val = rcValue[cur_chan_numb];
      rx_next_timer_int = rx_next_timer_int + ((val - RX_PPM_PULSE_LENGTH) * (F_CPU/1000000));
      calc_rest = calc_rest + rcValue[cur_chan_numb];
      cur_chan_numb++;
    }     
  }
  timer0_write(rx_next_timer_int);
}

void setup() 
{
  Serial.begin(115200); Serial.println();
  delay(10);
  
  WiFi.mode(WIFI_STA); // Station mode for esp-now 
  wifi_set_macaddr(STATION_IF, &myMac[0]);
  Serial.printf("This mac: %s, ", WiFi.macAddress().c_str()); 

  WiFi.disconnect();

  Serial.printf(", channel: %i\n", WIFI_CHANNEL); 

  if (esp_now_init() != 0) Serial.println("*** ESP_Now init failed");

  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);

  esp_now_register_recv_cb(recv_cb);
  esp_now_register_send_cb(send_cb);

  for(int i=0; i<CHANNELS; i++) {
   if (chPin[i]>-1) {
      pinMode(chPin[i], OUTPUT);
      servoCh[i].attach(chPin[i], usMin, usMax);
      {
        servoCh[i].writeMicroseconds( rcValue[i] );
      }
    }
  }
  for(int i=0; i<CHANNELS; i++){
    rcValue[i]= usMiddle;
  }

  pinMode(PPM_PIN,OUTPUT);
  digitalWrite(PPM_PIN, PPM_POLARITY);
  timer0_isr_init();
  timer0_attachInterrupt(rx_ppmISR);
  rx_next_timer_int=ESP.getCycleCount()+1000;
  timer0_write(rx_next_timer_int);
}

void loop() 
{
  if(gotRC)
  {
    gotRC=false;
    timeout=0;

    rcValue[0] = RCdata.chans.Ch1;
    rcValue[1] = RCdata.chans.Ch2;
    rcValue[2] = RCdata.chans.Ch3;
    rcValue[3] = RCdata.chans.Ch4;
    rcValue[4] = RCdata.chans.Ch5;
    rcValue[5] = RCdata.chans.Ch6;
    rcValue[6] = RCdata.chans.Ch7;
    rcValue[7] = RCdata.chans.Ch8;
  
    Serial.print(rcValue[0]);Serial.print("\t");
    Serial.print(rcValue[1]);Serial.print("\t");
    Serial.print(rcValue[2]);Serial.print("\t");
    Serial.print(rcValue[3]);Serial.print("\t");
    Serial.print(rcValue[4]);Serial.print("\t");
    Serial.print(rcValue[5]);Serial.print("\t");
    Serial.print(rcValue[6]);Serial.print("\t");
    Serial.print(rcValue[7]);Serial.print("\n");
  }
  if(timeout>=10)
  {
    servoCh[0].writeMicroseconds(1500);
    servoCh[1].writeMicroseconds(1500);
    servoCh[2].writeMicroseconds(1500);
    servoCh[3].writeMicroseconds(1500);
    servoCh[4].writeMicroseconds(1500);
    servoCh[5].writeMicroseconds(1500);
    servoCh[6].writeMicroseconds(1500);
    servoCh[7].writeMicroseconds(1500);
  } else {
    servoCh[0].writeMicroseconds(rcValue[0]);
    servoCh[1].writeMicroseconds(rcValue[1]);
    servoCh[2].writeMicroseconds(rcValue[2]);
    servoCh[3].writeMicroseconds(rcValue[3]);
    servoCh[4].writeMicroseconds(rcValue[4]);
    servoCh[5].writeMicroseconds(rcValue[5]);
    servoCh[6].writeMicroseconds(rcValue[6]);
    servoCh[7].writeMicroseconds(rcValue[7]);
  }
  timeout++;

  yield();
  delay(10);
}
