#include <Wire.h>
#define CTRL_REG1 0x20
#define CTRL_REG2 0x21
#define CTRL_REG3 0x22
#define CTRL_REG4 0x23
#define CTRL_REG5 0x24
int Addr = 105; // I2C address of gyro
int x, y, z;

const int numGyroReadings = 10;
float gyroReadings[numGyroReadings];
int gyroIndex = 0;
int gyroTotal = 0;
int gyroAverage = 0;


#include <SPI.h>
#include <Ethernet.h>
byte mac[] = { 0x90, 0xA2, 0xDA, 0x0E, 0xCB, 0x51 };
IPAddress ip(192,168,0,13);
//IPAddress myDns(1,1,1,1);
boolean hasEthernet = false;

EthernetClient client;
char server[] = "jimmybyrum.com";

unsigned long lastConnectionTime = 0;
boolean lastConnected = false;
const unsigned long postingInterval = 5*1000;




#include <NewPing.h>
#define TRIGGER_PIN  6
#define ECHO_PIN     5
#define MAX_DISTANCE 200
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);



boolean sonarFree = true;
boolean gyroFree = false;


int statusLed = 13;


void setup(){
  Wire.begin();
  
  delay(5000); // give wifi a moment to sort itself out
  
  Serial.begin(9600);
//  while (!Serial) ;
  Serial.println("Starting");
  

  writeI2C(CTRL_REG1, 0x1F);    // Turn on all axes, disable power down
  writeI2C(CTRL_REG3, 0x08);    // Enable control ready signal
  writeI2C(CTRL_REG4, 0x80);    // Set scale (500 deg/sec)
  delay(100);                   // Wait to synchronize
  
  for (int i = 0; i < numGyroReadings; i++) {
    gyroReadings[i] = 0;
  }
  
  Serial.println("Ready");
}

void loop() {
  
  getGyroValues();
  // In following Dividing by 114 reduces noise
  int valX = abs(x);
  int valY = abs(y);
  int valZ = abs(z);
  int gyroVals = valX + valY + valZ;
  
  gyroTotal = gyroTotal - gyroReadings[gyroIndex];
  gyroReadings[gyroIndex] = gyroVals;
  gyroTotal = gyroTotal + gyroReadings[gyroIndex];
  gyroIndex = gyroIndex + 1;
  if (gyroIndex >= numGyroReadings) {
    gyroIndex = 0;
  }
  int gyroAverage = abs(gyroTotal / numGyroReadings);
  int adjustedGyroAverage = map(gyroAverage, 0, 150, 0, 10);

  gyroFree = adjustedGyroAverage < 10; // average was 10
  
  
  
  unsigned int uS = sonar.ping();
  long cm = uS / US_ROUNDTRIP_CM;
  if (cm > 60) { cm = 60; }
  
  sonarFree = cm > 0 && cm < 10; // average was 3
  
    
  
  
  String s = "";
  s += "X:" + String(valX);
  s += "\tY:" + String(valY);
  s += "\tZ:" + String(valZ);
  s += "\tAverage:" + String(adjustedGyroAverage);
  s += "\tSonar:" + String(cm);
  Serial.println(s);
  
  
  
  
  
  if (!client.connected() && lastConnected) {
    Serial.println();
    Serial.println("disconnecting.");
    client.stop();
  }

  long timePassed = millis() - lastConnectionTime;
  if(!client.connected() && timePassed > postingInterval) {
    Serial.println("--------------");
    httpRequest();
  }
  lastConnected = client.connected();
  
  
  
  
  delay(300); // Short delay between reads
}

void blinkLed(int pin, int blinks, int interval) {
  int current = digitalRead(pin);
  int _first = HIGH;
  int _second = LOW;
  if (current == HIGH) {
    _first = LOW;
    _second = HIGH;
  }
  for (int i = 0; i < blinks; i++) {
    digitalWrite(pin, _first);
    delay(interval);
    digitalWrite(pin, _second);
    delay(interval);
  }
}

void getGyroValues () {
  byte MSB, LSB;

  MSB = readI2C(0x29);
  LSB = readI2C(0x28);
  x = ((MSB << 8) | LSB);

  MSB = readI2C(0x2B);
  LSB = readI2C(0x2A);
  y = ((MSB << 8) | LSB);

  MSB = readI2C(0x2D);
  LSB = readI2C(0x2C);
  z = ((MSB << 8) | LSB);
}

int readI2C (byte regAddr) {
    Wire.beginTransmission(Addr);
    Wire.write(regAddr);                // Register address to read
    Wire.endTransmission();             // Terminate request
    Wire.requestFrom(Addr, 1);          // Read a byte
    while(!Wire.available()) { };       // Wait for receipt
    return(Wire.read());                // Get result
}

void writeI2C (byte regAddr, byte val) {
    Wire.beginTransmission(Addr);
    Wire.write(regAddr);
    Wire.write(val);
    Wire.endTransmission();
}

void startEthernet() {
  if ( ! hasEthernet) {
    blinkLed(13, 7, 60);
    if (Ethernet.begin(mac) > 0) {
      hasEthernet = true;
      Serial.println("[Ethernet using DHCP]");
    } else {
      hasEthernet = false;
      Serial.println("[Ethernet failed]");
    }
  }
}

void httpRequest() {
  startEthernet();
  if (hasEthernet && client.connect(server, 80)) {
    digitalWrite(statusLed, HIGH);
    String s = "GET /340HarvardSt/?";
    if (gyroFree) {
      s += "gyro=free";
    } else {
      s += "gyro=busy";
    }
    s += "&";
    if (sonarFree) {
      s += "sonar=free";
    } else {
      s += "sonar=busy";
    }
    s += " HTTP/1.1";
    Serial.println(s);
    client.println(s);
    client.println("Host: jimmybyrum.com");
    client.println("Connection: close");
    client.println();
    client.stop();
  } else {
    hasEthernet = false;
    digitalWrite(statusLed, LOW);
    Serial.println("FAILED");
    client.stop();
  }
  Serial.println("--------------");
  lastConnectionTime = millis();
}

