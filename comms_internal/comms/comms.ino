
#include <CRCx.h>
//Set beetle_id
#define beetle_id '0'

//Preset data to be sent
char h_msg[] = "HANDSHAKE";
char a_msg[] = "ACK";
//create variable to be used for packet and data processing
char temp_msg[17];
uint8_t data[17];
uint8_t packet[21];
//boolean checks for logic program
//check that data transfer has begin
bool data_started = false;
//This boolean would be check by the sensor
bool dance_start = false;
//check if receive error or handshake from laptop
bool error = false;
bool handshake = false;
bool handshake_confirmed = false;
bool next_set = true;
byte temp;
int current_set = 0;
int err_set = -1;
int error_num = 0;
String time;
unsigned long time_out = 99999999;

float data_set[8];

//Dummy data -> see if possible to convert to queue
float roll[] = {222.14, 222.30, 222.12, 221.88, 221.73};
float pitch[] = {273.05, 272.90, 272.91, 272.93, 272.95};
float yaw[] = {370.27, -370.24, -370.29, -370.37, -370.49};
float AccX[] = { -0.48, 0.47, 0.46, 0.46, 0.46};
float AccY[] = { -0.15, -0.13, -0.16, -0.20, -0.22};
float AccZ[] = {1, 2, 3, 4, 5};

void setup() {
  // opens serial port, sets data rate to 115200 -> also done so in AT cmd
  Serial.begin(115200);
  while (!Serial) {
  }
  delay(1000);
}

//Add # into data as buffer till 16 char.
void data_prepare(char msg[]) {
  int len = strlen(msg);
  int j = 0;
  for (int i = 0; i < 16; i = i + 1) {
    if (16 - i >  len ) {
      data[i] = '#';
    } else {
      data[i] = msg[j];
      j = j + 1;
    }
  }
}


//Attach packet_id, packet_no and crc to data
void packet_prepare(char packet_id) {
  uint8_t result8 = crcx::crc8(data, 16);
  String temp = String(result8, HEX);
  char crc[3];
  temp.toCharArray(crc, 3);
  int j = 0;
  packet[0] = beetle_id;
  packet[1] = packet_id;
  for (int i = 2; i < 18; i = i + 1) {
    packet[i] = data[j];
    j = j + 1;
  }
  if (strlen(crc) == 1) {
    packet[18] = '0';
    packet[19] = crc[0];
  } else {
    packet[18] = crc[0];
    packet[19] = crc[1];
  }
}

//function to send 1 dataset
void dance_data(float data_set[]) {
  String pac;
  int set_no = 0;
  int pid = 51; // 3 in ASCII
  String front;
  String back;
  char pac_msg[17];
  //loop 3 times due to 3 packets per data set
  while (set_no < 6) {
    front = String (data_set[set_no]);
    back = String (data_set[set_no + 1]);
    pac = front + "|" + back;
    pac.toCharArray(temp_msg, 17);
    data_prepare(temp_msg);
    packet_prepare(char(pid));
    Serial.write((char*)packet);
    Serial.flush();
    memset(temp_msg, 0, 17);
    memset(data, 0, 17);
    memset(data_set, 0, sizeof(float));
    set_no = set_no + 2;
    pid++;
    delay(20);
  }
  //last packet to be send as timestamp
  time = String(millis());
  time.toCharArray(temp_msg, 17);
  data_prepare(temp_msg);
  packet_prepare('6');
  Serial.write((char*)packet);
  Serial.flush();
  memset(temp_msg, 0, 17);
  memset(data, 0, 17);
  delay(20);
}
//Function to ensure all data_set max 2dp?
float convert_2dp (float big) {
  int x10;
  float num;
  x10 = (int)(big * 100.0);
  num = x10 / 100.0;
  return num;
}


void loop() {
  //if dont recieve ACK from laptop, send the next set
  if (millis() < time_out) {
    error = true;
  }
  if (Serial.available()) {
    byte cmd = Serial.read();
    switch (cmd) {
      case 'A': //Recieve ACK
        data_prepare(a_msg);
        packet_prepare('0');
        Serial.write((char*)packet);
        Serial.flush();
        memset(data, 0, 17);
        if (data_started) {
          next_set = true;
          error = false;
          err_set = -1;
          error_num = 0;
        }
        if (handshake) {
          error = false;
          dance_start = true;
          handshake = false;
          handshake_confirmed = true;
        }
        break;
      case 'H'://Recieve Handshake request
        data_prepare(h_msg);
        packet_prepare('1');
        Serial.write((char*)packet);
        Serial.flush();
        memset(data, 0, 17);
        //Reset all boolean to default
        data_started = false;
        dance_start = false;
        error = false;
        handshake = true;
        handshake_confirmed = false;
        next_set = true;
        current_set = 0;
        err_set = -1;
        error_num = 0;
        break;
      case 'T'://Recieve request for timesync, send millis()
        time = String(millis());
        time.toCharArray(temp_msg, 17);
        data_prepare(temp_msg);
        packet_prepare('9');
        Serial.write((char*)packet);
        Serial.flush();
        memset(temp_msg, 0, 17);
        memset(data, 0, 17);
        break;
      case 'E': //Recieve err
        error = true;
        break;
    }

    //if dance start send 1 dataset in 3 packets. if issue with any 3, resend packet. if error more than 3 skip current data set.
    if (dance_start && handshake_confirmed) {
      //if one of the 3 packets have issue resend all 3 again.
      if (error) {
        if (error_num > 2) {
          next_set = true;
          error_num = 0;
        } else {
          //Send data set as part of resolving error
          data_set[0] = convert_2dp(roll[err_set]);
          data_set[1] = convert_2dp(pitch[err_set]);
          data_set[2] = convert_2dp(yaw[err_set]);
          data_set[3] = convert_2dp(AccX[err_set]);
          data_set[4] = convert_2dp(AccY[err_set]);
          data_set[5] = convert_2dp(AccZ[err_set]);
          //Call function to send data_set
          dance_data(data_set);
          error_num++;
        }
        error = false;
      }

      data_started = true;
      //Only send next set if ack or err received
      if (next_set) {
        //prepare all data in to single array
        time_out = millis() + 1000;
        err_set = current_set;
        data_set[0] = convert_2dp(roll[current_set]);
        data_set[1] = convert_2dp(pitch[current_set]);
        data_set[2] = convert_2dp(yaw[current_set]);
        data_set[3] = convert_2dp(AccX[current_set]);
        data_set[4] = convert_2dp(AccY[current_set]);
        data_set[5] = convert_2dp(AccZ[current_set]);
        //Call function to send data_set
        dance_data(data_set);
        //loop dummy data
        if (current_set != 4) {
          current_set++;
        } else {
          current_set = 0;
        }
        next_set = false;
      }
    }
  }
}