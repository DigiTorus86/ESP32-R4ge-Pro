/***************************************************
 ESP32 BLE Tank Commander (central role)
  
 Sketch for operating a remote control tank in a BLE central role.
 This sketch runs on the ESP32 attached to the tank chassis 
 via a dual H-bridge control board such as the  L298N DC Stepper Motor Driver Module. 
 See the esp32-r4ge-ble-controller sketch for the remote control peripheral unit.

Requires:
- ESP32 (i.e. DevKitC or other similar board)
- Dual motorized tank chassis (though can be adapted to single motors)
- H-Bridge Driver board
- An ESP32 R4ge Pro running the BLE Controller sketch

Copyright (c) 2021 Paul Pagel
This is free software; see the license.txt file for more information.
There is no warranty; not even for merchantability or fitness for a particular purpose.
*****************************************************/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define BLE_DEVICE_ID   "Cmdr1"  // Adjust as desired, but keep short

// UPDATE PERIPHERAL ADDRESS PRIOR TO RUNNING THIS SKETCH!!
// Specific MAC address of the peripheral controller device you're looking for.
// Should be in ff:ff:ff:ff:ff:ff format (6 bytes plus colon separators)
// This value is displayed at the bottom of the screen by the 
// esp32-r4ge-ble-controller sketch.
String esp32_peripheral_address = "24:6f:28:b6:19:22"; 

// UUIDs for the BLE service and characteristic - must match the central
// Generate new GUIDs if you modify the data packet size/format
#define SERVICE_UUID        "788fce9d-5ba6-4e8f-9e98-5965b20ab856"
#define CHARACTERISTIC_UUID "e2cdb570-6340-4aa2-9ac3-ab32cfa371f3"

#define MOTOR_L_POS  19  // L289N IN1 = tank left tread  Motor A +
#define MOTOR_L_NEG  18  // L289N IN2 = tank left tread  Motor A -
#define MOTOR_L_EN    5  // L289N ENA = tank left tread  Motor Enable (PWM/speed)

#define MOTOR_R_POS  25  // L289N IN3 = tank right tread Motor B +
#define MOTOR_R_NEG  26  // L289N IN4 = tank right tread Motor B -  
#define MOTOR_R_EN   23  // L289N ENA = tank right tread Motor Enable (PWM/speed)

#define CAM_ELEV8     2  // camera elevation servo (PWM)r
#define PAIR_LED      0  

uint8_t channel_left = 0;
uint8_t channel_right = 1;
uint8_t channel_elev8 = 2;
uint8_t duty_left = 128;
uint8_t duty_right = 128;
uint8_t duty_elev8 = 8;

static BLERemoteCharacteristic* 
                    remote_characteristic;
BLEScan*            ble_scan; //BLE scanning device 
BLEScanResults      found_devices;
static BLEAddress*  server_ble_address;
String              server_ble_address_str;
bool                server_paired = false;
bool                client_connected = false;
bool                data_received = false;


#define PACKET_SIZE 4
#define DP_CMND     0
#define DP_LTRK     1
#define DP_RTRK     2
#define DP_ELEV     3

#define DP_CMND_LTRKFWD   1  
#define DP_CMND_LTRKBWD   2
#define DP_CMND_RTRKFWD   4
#define DP_CMND_RTRKBWD   8
#define DP_CMND_PICTURE  16

uint8_t data_packet[] = 
{
   0x00,  // command flags
   0x00,  // left tread (0 = fwd, 127 = stop, 255 = bkwd 
   0x00,  // right tread
   0x00,  // elevation position
};

/*
 * Callback used when scanning for BLE peripheral/server devices
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks
{
    void onResult(BLEAdvertisedDevice advertisedDevice) 
    {
      Serial.printf("Scan Result: %s \n", advertisedDevice.toString().c_str());
      BLEAddress *foundAddress = new BLEAddress(advertisedDevice.getAddress());
      String bleAddr = foundAddress->toString().c_str();

      Serial.println(bleAddr);

      if (bleAddr == esp32_peripheral_address)
      {
        Serial.printf("Found specified peripheral\n");
        server_ble_address = new BLEAddress(advertisedDevice.getAddress());
        server_ble_address_str = server_ble_address->toString().c_str();
      }
    }
};

/* 
 * Callbacks used with BLE control device 
 */
class MyClientCallbacks: public BLEClientCallbacks
{
  void onConnect(BLEClient *pClient)
  {
    client_connected = true;
  }
  
  void onDisconnect(BLEClient *pClient)
  {
    client_connected = false;
    server_paired = false;
  }
};

/*
 * Called every time we get BLE Control data from the peripheral server device
 */
static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) 
{
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.println((char*)pData);
    
    if (length != PACKET_SIZE)
      return;
    
    for (int i = 0; i < length; i++)
    {
        data_packet[i] = pData[i];
        Serial.println(data_packet[i]);
    }
    data_received = true;
}

/*
 * Connect to a peripheral BLE control server address that we found during the scan
 */
bool connectToServer(BLEAddress pAddress){
    
    BLEClient*  pClient  = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallbacks()); 
    Serial.println(" - Created client");

    // Connect to the BLE Server.
    pClient->connect(pAddress);
    Serial.println(" - Connected to ESP32 Peripheral");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr)
    {
      Serial.println(" - Unable to find control data service.");
      return false;
    }
     Serial.println(" - data control service found.");
    
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    remote_characteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (remote_characteristic == nullptr)
    {
      Serial.println(" - Unable to find Control characteristic.");
      return false;
    }
    Serial.println(" - Control characteristic found.");
    
    if(remote_characteristic->canNotify())
      remote_characteristic->registerForNotify(notifyCallback);
    
    return true;
}

/*
 * Process the received control data and map it to internal state variables
 */
void processDataPacket()
{
  // TODO: data_packet[] values to control
  // 0=cmd, 1=left track, 2=right track, 3=elevate

  duty_left = data_packet[DP_LTRK] << 1;
  duty_right = data_packet[DP_RTRK] << 1;
  ledcWrite(channel_left, duty_left);  
  ledcWrite(channel_right, duty_right);   

  // Control left track direction
  if (data_packet[DP_CMND] & DP_CMND_LTRKFWD)
  {
    digitalWrite(MOTOR_L_POS, LOW);
    digitalWrite(MOTOR_L_NEG, HIGH); 
  }
  else if (data_packet[DP_CMND] & DP_CMND_LTRKBWD)
  {
    digitalWrite(MOTOR_L_POS, HIGH);
    digitalWrite(MOTOR_L_NEG, LOW); 
  }
  else
  {
    digitalWrite(MOTOR_L_POS, LOW);
    digitalWrite(MOTOR_L_NEG, LOW); 
  }

  // Control right track direction
  if (data_packet[DP_CMND] & DP_CMND_RTRKFWD)
  {
    digitalWrite(MOTOR_R_POS, LOW);
    digitalWrite(MOTOR_R_NEG, HIGH); 
  }
  else if (data_packet[DP_CMND] & DP_CMND_RTRKBWD)
  {
    digitalWrite(MOTOR_R_POS, HIGH);
    digitalWrite(MOTOR_R_NEG, LOW); 
  }
  else
  {
    digitalWrite(MOTOR_R_POS, LOW);
    digitalWrite(MOTOR_R_NEG, LOW); 
  }

  // TODO: handle other commands here
}

/*
 * Kick off the process of finding and paring with the Control server.
 * Returns true if successfully found and paired.
 */
bool pairBlePeripheral()
{
  server_ble_address_str = "";
  digitalWrite(PAIR_LED, LOW); 
  
  ble_scan = BLEDevice::getScan(); //create new scan
  ble_scan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks()); //Call the class that is defined above
  ble_scan->setActiveScan(true); //active scan uses more power, but get results faster
  
  found_devices = ble_scan->start(3); //Scan for 3 seconds to find the Control Gauntlet

  if (found_devices.getCount() >= 1 && !server_paired)
  {
    if (server_ble_address_str.length() > 0)
    {
      if (connectToServer(*server_ble_address))
      {
        server_paired = true;
        digitalWrite(PAIR_LED, HIGH);
        Serial.print("Control server paired: ");
        Serial.println(server_ble_address_str.c_str());
        
        return true; // found peripheral controller
      }
    }
  }
  return false;
}
 
void setup() 
{
  Serial.begin(115200);
  Serial.println("ESP32 Tank Commander"); 
  delay(100);
  
  pinMode(MOTOR_L_POS, OUTPUT);
  pinMode(MOTOR_L_NEG, OUTPUT);
  pinMode(MOTOR_L_EN, OUTPUT);
  
  pinMode(MOTOR_R_POS, OUTPUT);
  pinMode(MOTOR_R_NEG, OUTPUT);
  pinMode(MOTOR_R_EN, OUTPUT);

  pinMode(CAM_ELEV8, OUTPUT);
  pinMode(PAIR_LED, OUTPUT);

  ledcSetup(channel_left, 20000, 8);  // 20 kHz max, 8 bit resolution
  ledcAttachPin(MOTOR_L_EN, channel_left);
  ledcWrite(channel_left, duty_left);  

  ledcSetup(channel_right, 20000, 8);  // 20 kHz max, 8 bit resolution
  ledcAttachPin(MOTOR_R_EN, channel_right);
  ledcWrite(channel_right, duty_right);   

  ledcSetup(channel_elev8, 50, 8);  // 50 Hz max, 8 bit resolution
  ledcAttachPin(CAM_ELEV8, channel_elev8);
  ledcWrite(channel_elev8, duty_elev8);   

  if (esp32_peripheral_address.length() != 17)
  {
    Serial.println("Please configure a valid ESP32 peripheral address!");
    Serial.println("Valid fmt: 01:02:03:f1:f2:f3");
    while(true);
  }

  BLEDevice::init("");
  
  while (!pairBlePeripheral())
  {
    digitalWrite(PAIR_LED, HIGH);
    delay(100);
    digitalWrite(PAIR_LED, LOW);
    delay(3000);
  }
}

void loop() 
{
  if (!client_connected)
  {
    digitalWrite(PAIR_LED, LOW);
    while (!pairBlePeripheral())
    {
      digitalWrite(PAIR_LED, HIGH);
      delay(100);
      digitalWrite(PAIR_LED, LOW);
      delay(3000);
    }
  }

  
  if (data_received)
  {
    //digitalWrite(DATA_LED_PIN, HIGH);
    processDataPacket();
  
    data_received = false;
    delay(10);
    //digitalWrite(DATA_LED_PIN, LOW);
  }
}
