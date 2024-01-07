/*
   This script for automation shelf which has a projector. Idea: We have a shelf which has 2 levels/sections. Each level/section has linear actuators for opening section and also shelf has a two legs which also
   will be open by actuators. By the idea shelf must have possibility to open by buttons and from special software via LAN and via HomeAssistant
*/
#include <IRremote.h>
#include <Ethernet.h>
#include <SPI.h>
#include <EEPROM.h>
#include <SD.h>
#include <ArduinoHA.h>
#include <ArduinoHADefines.h>
#include <HADevice.h>
#include <HAMqtt.h>
#include <DS3231.h>

#define IR_RECEIVE_PIN 12 //Arduino pin IR receiver connected to
#define IR_BUTTON_MINUS 7 //IR code from remote control for close shelf
#define IR_BUTTON_PLUS 21 //IR code from remote control for open shelf
#define BROKER_ADDR IPAddress(192,168,128,249) //Address for MQTT

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //MAC address for controller.
//byte ip[] = { 10,0,0,200 }; //IP address for controller.
//byte gateway[] = { 10,0,0,1 };
//byte subnet[] = { 255,255,255,0 }; //Subnet mask

byte ip[] = { 192,168,128,244 }; //IP address for controller.
byte gateway[] = { 192,168,128,1 }; // Internet access via router
byte subnet[] = { 255,255,224,0 }; //Subnet mask
EthernetServer server(55); // Initialize the Ethernet server with port 55.

// This block for MQTT
EthernetClient cl;
HADevice device(mac, sizeof(mac));
HAMqtt mqtt(cl, device);
// "myButtonA" and "myButtonB" are unique IDs of buttons. We should define our own ID.
HAButton buttonOpen("myButtonA");
HAButton buttonClose("myButtonB");

String readString;
int ir_command = 0;
const String API_Key = "6036899570953d0e4c923143c429b1e7a53b75f0799b30f2cd948986b76c6314";
int duration_doors = 19200; //Duration of working (open/close doors) actuators
int duration_main_section = 29200; //Duration of working (open/close main section) actuators
int duration_inner_section = 27800; //Duration of working (open/close inner section) actuators
int duration_legs = 9100; //Duration of working (open/close legs) actuators
const int chipSelect = 4; //Select kind of chip for SD. 4 means SD on etherner board
File myFile;
DS3231  rtc(SDA, SCL); //Declaring RTC module for date/time
const int ADDR = 100; //Address of EEPROM
const int ALLOWED_VAL = 500; //Max value for storing in EEPROM

//This block of variables for IR receiver. Uses to prevent controller read many IR signals one by one.
unsigned long btn1_code_rec_time = 0;
unsigned long btn2_code_rec_time = 0;
unsigned long no_repeat_delay = 300; // do not respond to a button press for 500ms
unsigned long btn1_times_pressed = 0;
unsigned long btn2_times_pressed = 0;


void setup() {
pinMode(7, OUTPUT); // Relay
pinMode(8, OUTPUT); // Relay
pinMode(2, OUTPUT); // Relay
pinMode(3, OUTPUT); // Relay 
pinMode(4, OUTPUT); // Relay
pinMode(5, OUTPUT); // Relay
pinMode(10, OUTPUT); // Relay
pinMode(11, OUTPUT); // Relay
pinMode(9, OUTPUT);
pinMode(6, OUTPUT);

digitalWrite(6, LOW);
digitalWrite(9, LOW);
digitalWrite(7, HIGH); // Relay 
digitalWrite(8, HIGH); // Relay
digitalWrite(2, HIGH); // Relay
digitalWrite(3, HIGH); // Relay
digitalWrite(4, HIGH); // Relay
digitalWrite(5, HIGH); // Relay
digitalWrite(10, HIGH); // Relay 
digitalWrite(11, HIGH); // Relay

Serial.begin(9600);

IrReceiver.begin(IR_RECEIVE_PIN); //Start IR receiver

//Start Ethernet
Ethernet.begin(mac, ip, gateway, subnet);
server.begin();
Serial.print("Server is at ");
Serial.println(Ethernet.localIP());

//Check SD card Initialization
if (!SD.begin(chipSelect)) {
  Serial.println("initialization SD failed!");
} 
else {
  Serial.println("initialization SD success!");  
}

//Check if we have file for storing data and variables
if (!SD.exists("data.txt")) {
  Serial.println("File data.dat doesn't exist. We'll create it.");
  myFile = SD.open("data.txt",  FILE_WRITE);
  myFile.close();
}

//Initialize the rtc object (date/time)
rtc.begin();

// The following lines can be uncommented to set the date and time
//rtc.setDOW(WEDNESDAY);     // Set Day-of-Week to SUNDAY
//rtc.setTime(19, 35, 00);     // Set the time to 12:00:00 (24hr format)
//rtc.setDate(2, 8, 2023);   // Set the date to January 1st, 2014

//EEPROM.update(ADDR, 1); //Here we can set value to EEPROM if need. For it just temporaly uncomment this line and set value

//Print in serial cmd some data
Serial.println(getLastState());
Serial.println(currentTime());
Serial.print("EEPROM value = ");
Serial.println(EEPROM.read(ADDR));


// Optional device's details MQTT
device.setName("Automatic shelf");
device.setSoftwareVersion("1.0.0");

// Optional properties MQTT
buttonOpen.setIcon("mdi:boom-gate-up-outline");
buttonOpen.setName("Open");
buttonClose.setIcon("mdi:boom-gate-outline");
buttonClose.setName("Close");

// Press callbacks MQTT
buttonOpen.onCommand(onButtonCommand);
buttonClose.onCommand(onButtonCommand);

mqtt.begin(BROKER_ADDR); //Start MQTT

//checkShelfState(); //Check state of shelf. If it's not fully closed or opened (because of for example lost of power) then when power back we close shelf at first
}


////////////////MQTT button
void onButtonCommand(HAButton* sender)
{
  if (sender == &buttonOpen) {
      // button A was clicked, do your logic here
  }
  else if (sender == &buttonClose) {
        // button B was clicked, do your logic here
  }
}
/////////////////////////////////////////////////////////


/////////////////Working with EEPROM
void increaseEEPROM() {
  int e = EEPROM.read(ADDR);
  e++;
  EEPROM.update(ADDR, e);  
}
/////////////////////////////////////////////////////////

/////////////////Check state of shelf
void checkShelfState() {
  if (getLastState() == "Closed|Start_opening"  || getLastState() == "Opened|Start_closing") {
    closeShelf();
    return;
  } 
}
/////////////////////////////////////////////////////////

/////////////////Get current date/time
String currentTime() {
 String dt_tm = rtc.getDOWStr();// + ' ' + rtc.getDateStr() + ' ' + rtc.getTimeStr();
 dt_tm.concat(" ");
 dt_tm.concat(rtc.getDateStr());
 dt_tm.concat(" ");
 dt_tm.concat(rtc.getTimeStr());
 return dt_tm;
}
//////////////////////////////////////////////////////////
 

/////////////////Get last state of shelf. For it we read last line in file.
String getLastState() { 
  myFile = SD.open("data.txt",  FILE_READ);
  String DataTemp;
  String newString;
  if (myFile)
  {
    while (myFile.available())
    {
      char c = myFile.read(); // Get the next character
      if (isPrintable(c)) { 
        DataTemp.concat(c);        
      }  
    }
    newString = DataTemp.substring(DataTemp.lastIndexOf("(") + 1, DataTemp.length() - 1); //Get last string in data.txt file. For it we read file char by char to string and get last data in brackets.
    //Serial.println(DataTemp);
    //Serial.println(newString);
  }
  myFile.close();
  return newString;
}
/////////////////////////////////////////////////////////////


//////////////////Insert/save position of shelf in file
void insert_state (String str) {
  myFile = SD.open("data.txt", FILE_WRITE | O_TRUNC); //Clear file each time during opening. It more easier to keep only last state of shelf
  //myFile = SD.open("data.txt",  FILE_WRITE);
  if (myFile)
  {
    myFile.println(str); //Position of shelf by default
  }
  myFile.close();
}
//////////////////////////////////////


/////////////////////// Display dashboard page with on/off button for relay
void dashboardPage(EthernetClient &client) {
  // Now output HTML data starting with standard header
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println();
  client.println("<!DOCTYPE HTML><html><head>");
  client.println("<title> Shelf projector automation</title>");
  client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>");
  client.println("<style> .footer { position: fixed; left: 0; bottom: 0; width: 100%; background-color: grey; color: white; text-align: center; }</style>");
  client.print("<body>");
  client.println("<center><h1 style=\"color:blue;\">Automation projector shelf - OPEN/CLOSE page</h1></center>");
  client.println("<hr>");
  client.println("<br>");
  //Form Shelf open
  client.println("<form method=\"get\" name=\"shelf\">");
  client.println("<input type=\"hidden\" name=\"shelf\" value=\"Open\">");
  client.println("<input type=\"hidden\" name=\"api\" value=\"6036899570953d0e4c923143c429b1e7a53b75f0799b30f2cd948986b76c6314\">");
  client.println("<input type=\"submit\" value=\"Open\">");
  client.println("</form>");
  //Form Shelf close
  client.println("<form method=\"get\" name=\"shelf1\">");
  client.println("<input type=\"hidden\" name=\"shelf\" value=\"Close\">");
  client.println("<input type=\"hidden\" name=\"api\" value=\"6036899570953d0e4c923143c429b1e7a53b75f0799b30f2cd948986b76c6314\">");
  client.println("<input type=\"submit\" value=\"Close\">");
  client.println("</form>");
  client.println("<div class=\"footer\"><p>Automation and programming by <a href = \"mailto: highsoft.soi@com\">Oleg Skvortsov</a></p></div>");
  client.println("</body></html>");
}
//////////////////////////////////////////////////////////////////////////////////


////////////////////////Open shelf
void openShelf() {
  increaseEEPROM();
  if (EEPROM.read(ADDR) < ALLOWED_VAL) {
    if (getLastState() != "Opened|Finish_opening") {  
      insert_state(currentTime() + " -- (Closed|Start_opening)");
      ////// Extend actuators for opening doors (Open doors)
      digitalWrite(2, LOW);
      digitalWrite(3, HIGH);
    
      delay(duration_doors); // Duration of opening doors before stop
    
      // Stop actuators for opening doors
      digitalWrite(2, HIGH);
      digitalWrite(3, HIGH);
      ////////////////////////////////////////////////////////
      
      
      ////// Extend 1st level of Linear Actuators (Main section of shelf)
      digitalWrite(7, LOW);
      digitalWrite(8, HIGH);
    
      delay(duration_main_section); // Duration of opening 1st level of actuators
    
      // Stop 1st level of Linear Actuators (Main section of shelf)
      digitalWrite(8, HIGH);
      digitalWrite(7, HIGH);
      ////////////////////////////////////////////////////////

            
      ////// Extend 2nd level of Linear Actuators (Inner section of shelf)
      digitalWrite(4, LOW);
      digitalWrite(5, HIGH);
    
      delay(duration_inner_section); // Duration of opening 2nd level of actuators
    
      // Stop 2nd level of Linear Actuators (Inner section of shelf)
      digitalWrite(4, HIGH);
      digitalWrite(5, HIGH);
      ////////////////////////////////////////////////////////
    
    
      ////// Extend Legs Linear Actuators
      //digitalWrite(10, LOW);
      //digitalWrite(11, HIGH);
    
      //delay(duration_legs); // Duration of opening legs
    
      // Stop Legs Linear Actuators
      //digitalWrite(10, HIGH);
      //digitalWrite(11, HIGH);
      ////////////////////////////////////////////////////////

      insert_state(currentTime() + " -- (Opened|Finish_opening)"); 
    }  
  }
}
//////////////////////////////////////////////////////////////////////////////////


////////////////////////Close shelf
void closeShelf() {
  increaseEEPROM();
  if (EEPROM.read(ADDR) < ALLOWED_VAL) {
    insert_state(currentTime() + " -- (Opened|Start_closing)");
    ////// Retract Legs Linear Actuators
    //digitalWrite(11, LOW);
    //digitalWrite(10, HIGH);
  
    //delay(duration_legs); // Duration of closing doors
  
    // Stop Legs Linear Actuators
    //digitalWrite(11, HIGH);
    //digitalWrite(10, HIGH);
    ////////////////////////////////////////////////////////
    
    
    ////// Retract 2nd level of Linear Actuators (Inner section of shelf)
    digitalWrite(5, LOW);
    digitalWrite(4, HIGH);
  
    delay(duration_inner_section); // Duration of closing 2nd level of actuators
  
    // Stop 2nd level of Linear Actuators (Inner section of shelf)
    digitalWrite(5, HIGH);
    digitalWrite(4, HIGH);
    ////////////////////////////////////////////////////////
  
  
    ////// Retract 1st level of Linear Actuators (Main section of shelf)
    digitalWrite(8, LOW);
    digitalWrite(7, HIGH);
  
    delay(duration_main_section); // Duration of closing 1st level (Main section) of actuators
  
    // Stop 1st level of Linear Actuators (Main section of shelf)
    digitalWrite(8, HIGH);
    digitalWrite(7, HIGH);
    ////////////////////////////////////////////////////////
  
  
    ////// Retract actuators for opening doors (Close doors)
    digitalWrite(3, LOW);
    digitalWrite(2, HIGH);
  
    delay(duration_doors); // Duration of closing doors before stop
  
    // Stop actuators for closing doors
    digitalWrite(3, HIGH);
    digitalWrite(2, HIGH);
    //////////////////////////////////////////////////////// 
    insert_state(currentTime() + " -- (Closed|Finish_closing)");
  }
}
//////////////////////////////////////////////////////////////////////////////////


void loop() {  
  // Create a client connection
  EthernetClient client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        //read char by char HTTP request
        if (readString.length() < 100) {
          //store characters to string
          readString += c;
        }
        Serial.print(c);
        if (c == '\n') {
          if (readString.indexOf("?") < 0) {
            delay(1000); //do nothing            
          }
          else {
            if ((readString.indexOf("shelf=Open") >0) && (readString.indexOf("api=6036899570953d0e4c923143c429b1e7a53b75f0799b30f2cd948986b76c6314") > 0)) {
              openShelf();
              //return;
            }
            else if ((readString.indexOf("shelf=Close") >0) && (readString.indexOf("api=6036899570953d0e4c923143c429b1e7a53b75f0799b30f2cd948986b76c6314") > 0)) {
                   closeShelf();
                   //return;
                 }
          }
          //Run procedure for opennibg html page
          dashboardPage(client);
          //clearing string for next read
          readString="";
          //wait until all data has been sent
          client.flush();
          //stopping client
          client.stop(); 
        }
      }
    }
  }
  

  //////Start IR receiver
  if (IrReceiver.decode()) {
    IrReceiver.resume();
    //Serial.println(IrReceiver.decodedIRData.command);
    ir_command = IrReceiver.decodedIRData.command;

    if ((ir_command == IR_BUTTON_PLUS) && ((millis() - btn1_code_rec_time) > no_repeat_delay)) {
      btn1_code_rec_time = millis(); //record the time the button was pressed
      btn1_times_pressed += 1;
      if (btn1_times_pressed == 1) {
        openShelf();
        btn2_times_pressed = 0;
        return;
      }  
    }
    
    if ((ir_command == IR_BUTTON_MINUS) && ((millis() - btn2_code_rec_time) > no_repeat_delay)) {
      btn2_code_rec_time = millis(); //record the time the button was pressed
      btn2_times_pressed += 1;
      if (btn2_times_pressed == 1) {
        closeShelf();
        btn1_times_pressed = 0;
        return;
      }  
    }
  }
  

  ////////Buttons
  if (digitalRead(9) == HIGH) {
    openShelf();
    Serial.println("Open button pressed"); 
  }

  if (digitalRead(6) == HIGH) {
    closeShelf();
    Serial.println("Close button pressed"); 
  }


  ////////For MQTT
  //Ethernet.maintain();
  //mqtt.loop();


}
