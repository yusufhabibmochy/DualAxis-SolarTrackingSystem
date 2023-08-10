#if !defined(ESP32)
#error This code is intended to run on the ESP32 platform! Please check your Tools->Board setting.
#endif

#define TIMER_INTERRUPT_DEBUG       1
#define ESP32_ISR_Servos_DEBUG      1
#define USE_ESP32_TIMER_NO          0

#include "ESP32_ISR_Servo.h"
#define PIN_Servo_Ver   19
#define PIN_Servo_Hor   18

//------ Servo TD-8120MG
#define MIN_MICROS      550
#define MAX_MICROS      2450

int servo_vertikal   = -1;
int servo_horizontal = -1;

int sudut_servo_horizontal;
int sudut_servo_vertikal;
 
//------ ADC Filter Median
#include "RunningMedian.h"

RunningMedian samples_ADC_F = RunningMedian(10);
RunningMedian samples_ADC_B = RunningMedian(10);
RunningMedian samples_ADC_L = RunningMedian(10);
RunningMedian samples_ADC_R = RunningMedian(10);
 
#define Pin_ADC_F     35
#define Pin_ADC_B     34
#define Pin_ADC_L     33
#define Pin_ADC_R     32

int ADC_F_Raw = 0;
int ADC_B_Raw = 0;
int ADC_L_Raw = 0;
int ADC_R_Raw = 0;

int ADC_F_Filtered = 0;
int ADC_B_Filtered = 0;
int ADC_L_Filtered = 0;
int ADC_R_Filtered = 0;

//------ INA219
#include <Wire.h>
#include <Adafruit_INA219.h>

Adafruit_INA219 ina219_pv (0x41);
Adafruit_INA219 ina219_load (0x40);

float tegangan_pv, arus_pv, daya_pv;
float tegangan_load, arus_load, daya_load;

//------ Blynk
#define BLYNK_PRINT Serial
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>


#define LED_BUILTIN    2 // GPIO2

#define BLYNK_TEMPLATE_ID "TMPL0IZUt_sd"
#define BLYNK_DEVICE_NAME "solar tracking dual axis"
#define BLYNK_AUTH_TOKEN "b88zpynh2ZhT-Y0QSZEwWszSuLb0akvV"

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "";
char pass[] = "";

#define debug1  true
#define debug2  true

void setup()
{
  Serial.begin(115200); // 115200 bit per detik
  ina219_pv.begin();
  ina219_load.begin();
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  //------ Initialize the INA219.
  if (!ina219_pv.begin())
  {
    Serial.println("INA219 PV eror");
    while (1)
    {
      digitalWrite(LED_BUILTIN, LOW);
      delay(50);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(50);
    }
  }
  if (!ina219_load.begin())
  {
    Serial.println("INA219 Sistem eror");
    {
      digitalWrite(LED_BUILTIN, LOW);
      delay(500);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(500);
    }
  }

  ESP32_ISR_Servos.useTimer(USE_ESP32_TIMER_NO);

  servo_vertikal   = ESP32_ISR_Servos.setupServo(PIN_Servo_Ver, MIN_MICROS, MAX_MICROS);
  servo_horizontal = ESP32_ISR_Servos.setupServo(PIN_Servo_Hor, MIN_MICROS, MAX_MICROS);

  sudut_servo_horizontal  = 90;
  sudut_servo_vertikal    = 90;
   
  ESP32_ISR_Servos.setPosition(servo_vertikal, sudut_servo_vertikal);
  ESP32_ISR_Servos.setPosition(servo_horizontal, sudut_servo_horizontal);

  Blynk.begin(auth, ssid, pass, "blynk.cloud", 80);
  checkBlynkStatus();
}

void loop()
{
  ADC_F_Raw = analogRead(Pin_ADC_F);
  ADC_B_Raw = analogRead(Pin_ADC_B);
  ADC_L_Raw = analogRead(Pin_ADC_L);
  ADC_R_Raw = analogRead(Pin_ADC_R);

  samples_ADC_F.add(ADC_F_Raw);
  samples_ADC_B.add(ADC_B_Raw);
  samples_ADC_L.add(ADC_L_Raw);
  samples_ADC_R.add(ADC_R_Raw);

  ADC_F_Filtered = samples_ADC_F.getAverage();
  ADC_B_Filtered = samples_ADC_B.getAverage();
  ADC_L_Filtered = samples_ADC_L.getAverage();
  ADC_R_Filtered = samples_ADC_R.getAverage();

  if (debug1 == true)
  {
    Serial.print("ADC_F Raw: ");
    Serial.print(ADC_F_Raw);
    Serial.print("\tFiltered :");
    Serial.println(ADC_F_Filtered);

    Serial.print("ADC_B Raw: ");
    Serial.print(ADC_B_Raw);
    Serial.print("\tFiltered :");
    Serial.println(ADC_B_Filtered)

    Serial.print("ADC_L Raw: ");
    Serial.print(ADC_L_Raw);
    Serial.print("\tFiltered :");
    Serial.println(ADC_L_Filtered);

    Serial.print("ADC_R Raw: ");
    Serial.print(ADC_R_Raw);
    Serial.print("\tFiltered :");
    Serial.println(ADC_R_Filtered);

    Serial.print("Selisih F_B: ");
    Serial.print(ADC_F_Filtered - ADC_B_Filtered);
    Serial.print("\t Selisih L_R: ");
    Serial.println(ADC_L_Filtered - ADC_R_Filtered);
  }

  //------ Dual-Axis
  int toleransi = 30;
  if ( ADC_L_Filtered - ADC_R_Filtered > -toleransi && ADC_L_Filtered - ADC_R_Filtered <  toleransi )
  {
    ;
  }
  else
  {
    if (ADC_L_Filtered > ADC_R_Filtered)
    {
      sudut_servo_horizontal = sudut_servo_horizontal - 1;
      if (sudut_servo_horizontal > 180) sudut_servo_horizontal = 180;
    }
    else if (ADC_R_Filtered > ADC_L_Filtered)
    {
      sudut_servo_horizontal = sudut_servo_horizontal + 1;
      if (sudut_servo_horizontal < 0) sudut_servo_horizontal = 0;
    }
  }

  if ( ADC_F_Filtered - ADC_B_Filtered > -toleransi && ADC_F_Filtered - ADC_B_Filtered <  toleransi )
  {
    ;
  }
  else
  {
    if (ADC_B_Filtered > ADC_F_Filtered)
    {
      sudut_servo_vertikal = sudut_servo_vertikal - 1;
      if (sudut_servo_vertikal < 35) sudut_servo_vertikal = 35;
    }
    else if (ADC_F_Filtered > ADC_B_Filtered)
    {
      sudut_servo_vertikal = sudut_servo_vertikal + 1;
      if (sudut_servo_vertikal > 145) sudut_servo_vertikal = 145;
    }
  }

  Serial.print("sudut_servo_horizontal: ");
  Serial.print(sudut_servo_horizontal);
  Serial.print("\t sudut_servo_vertikal: ");
  Serial.println(sudut_servo_vertikal);

  ESP32_ISR_Servos.setPosition(servo_horizontal, sudut_servo_horizontal);
  delay(500);
  ESP32_ISR_Servos.setPosition(servo_vertikal, sudut_servo_vertikal);
  delay(500);
  kirim_data_ke_BLYNK();
  delay(500);
}

void kirim_data_ke_BLYNK()
{
  tegangan_pv = ina219_pv.getBusVoltage_V() + (ina219_pv.getShuntVoltage_mV() / 1000);
  arus_pv = ina219_pv.getCurrent_mA();
  daya_pv = ina219_pv.getPower_mW();
  float daya_pv_watt = daya_pv / 1000.0;

  tegangan_load = ina219_load.getBusVoltage_V() + (ina219_load.getShuntVoltage_mV() / 1000);
  arus_load = ina219_load.getCurrent_mA();
  daya_load = ina219_load.getPower_mW();
  float daya_load_watt = daya_load / 1000.0;

  if (debug2 == true)
  {
    Serial.print("Tegangan PV: "); Serial.print(tegangan_pv); Serial.println("V \t");
    Serial.print("Arus PV: "); Serial.print(arus_pv); Serial.println("mA \t");
    Serial.print("Daya PV: "); Serial.print(daya_pv); Serial.print("mW = "); Serial.print(daya_pv_watt); Serial.println("W");

    Serial.print("Tegangan Load: "); Serial.print(tegangan_load); Serial.println("V \t");
    Serial.print("Arus Load: "); Serial.print(arus_load); Serial.println("mA \t");
    Serial.print("Daya Load: "); Serial.print(daya_load); Serial.print("mW = "); Serial.print(daya_load_watt); Serial.println("W");

    Serial.println();
  }

  Blynk.virtualWrite(V0, tegangan_pv);
  Blynk.virtualWrite(V1, arus_pv);
  Blynk.virtualWrite(V2, daya_pv_watt);
  Blynk.virtualWrite(V3, tegangan_load);
  Blynk.virtualWrite(V4, arus_load);
  Blynk.virtualWrite(V5, daya_load_watt);
  Blynk.virtualWrite(V6, sudut_servo_vertikal);
  Blynk.virtualWrite(V7, sudut_servo_horizontal);
}

void checkBlynkStatus()
{
  bool isconnected = Blynk.connected();
  if (isconnected == false)
  {
    digitalWrite (LED_BUILTIN, LOW); //Turn off WiFi LED indikator
  }
  if (isconnected == true)
  {
    digitalWrite (LED_BUILTIN, HIGH); //Turn on WiFi LED indikator
  }
}
