/******************************************************************************
 A decentralized networking system for the Tyria World communicating through 
 OOCSI. The database is built around the OOCSI framework by Mathias Funk. 
 
 TU/e course of faculty of Industrial Design by Mathias Funk
 DBSU10 (2018-3) - Tyria Team 05
 
 Author: Danny Yang
 Author: Glenn Mossel
 Author: Sylvan Brons
 Author: Kirsten Tensen
 
 For more information on OOCSI refer to: https://iddi.github.io/oocsi/
 ******************************************************************************/

// Include libraries
#include "OOCSI.h"
#include <EEPROM.h>

//---------------------------------- Group Specific libraries----------------------------------//

//---------------------------------------------------------------------------------------------//

// Values needed to connect to oocsi
const char* ssid = "SSID";
const char* password = "PASSWORD";
const char* hostserver = "OOCSI SERVER";
const char* channel = "Tyria";
const char* OOCSIName = "GROUPNAME";    //<-- CHANGE THIS

// Create OOCSI object
OOCSI oocsi = OOCSI();

// Group identifiers and routes (pointers)
String groups[5] = {"T01", "T02", "T03", "T04", "T05"};
String routes[3][5] = {
  {"T04", "T05", "T01", "T02", "T03"},
  {"T02", "T04", "T05", "T03", "T01"},
  {"T03", "T05", "T04", "T01", "T02"}
};

int currentRoute = 0;
int state = 0;
int oldState = -1;
boolean worldUnlocked = false;

// Group names and array for unlocks received
String group(OOCSIName); //converts char array OOCSIName to string
String beforeNeighbour;
int beforeNeighbourPos;
int beforeNeighbourIndex;
String afterNeighbour;
int afterNeighbourPos;
int afterNeighbourIndex;
boolean unlocks[5];
boolean online[5];
unsigned long timeMillis = 0;

//---------------------------------- Group Specific variables----------------------------------//

//---------------------------------------------------------------------------------------------//


void setup() {
  //---------------------------------- Group Specific Setup----------------------------------//
  
  //---------------------------------------------------------------------------------------------//

  //Read currentRoute from EEPROM
  int readEEPROM = EEPROM.read(0);
  if (readEEPROM <= (sizeof(routes) / sizeof(routes[0]))) currentRoute = EEPROM.read(0);

  Serial.begin(115200);
  Serial.println("Start setup");

  // Set all unlocks to false
  for (int i = 0; i < sizeof(unlocks) / sizeof(boolean); i++) {
    unlocks[i] = false;
  }

  // Set on board pin as acitivity indicator for OOCSI
  pinMode(LED_BUILTIN, OUTPUT);
  oocsi.setActivityLEDPin(LED_BUILTIN);

  // Connect to OOCSI
  oocsi.connect(OOCSIName, hostserver, ssid, password, processOOCSI);

  // Subscribe to a channel
  Serial.print("subscribing to "); Serial.println(channel);
  oocsi.subscribe(channel);

  // Send ready signal to channel
  oocsi.newMessage(channel);
  oocsi.addBool("Setup_finished", true);
  oocsi.sendMessage();
}

//Reset function
void(* resetFunc) (void) = 0;

//----------------------------------Here every groups individual code can be posted----------------------------------//
bool moduleUnlocked() {

  return false;
}
//-------------------------------------------------------------------------------------------------------------------//

void loop() {
  oocsi.check();

  //Prints state when changed
  if (state != oldState) {
    Serial.print("State ");
    Serial.println(state);
    oldState = state;
  }

  //---------------------------- State 0 - Before ----------------------------//
  if (state == 0) {

    // Check online status every 10 seconds
    if (millis() > timeMillis) {
      timeMillis = millis() + 10000;
      checkOnline();
    }

    // Do something if two or more modules after self are offline
    while (numberOnline() < 4) {
      Serial.println("Waiting for neighbours");
      printBooleanArray(online, sizeof(online) / sizeof(boolean), "Online:");
      if (millis() > timeMillis) {
        timeMillis = millis() + 10000;
        checkOnline();
      }

      // Wait for other modules to come online
    }

    // Continue to state 1 if first in route
    if (determinePos(currentRoute, 0, 0) == 0) {
      state = 1;
    }

    // Wait for beforeNeighbour to be activated
    else {
      activationCheck(0);

      if (!online[beforeNeighbourIndex]) {
        activationCheck(1);
      }
      //do nothing
    }
  }

  //---------------------------- State 1 - During ----------------------------//
  else if (state == 1) {
    boolean unlocked = moduleUnlocked();
    if (unlocked) {
      Serial.println("UNLOCKED!");
      // Set own module to unlocked
      unlocks[determineIndex(group)] = true;

      // Send all unlocked modules to OOCSI
      oocsi.newMessage(channel);
      if (unlocks[0]) oocsi.addBool("T01_unlocked", true);
      if (unlocks[1]) oocsi.addBool("T02_unlocked", true);
      if (unlocks[2]) oocsi.addBool("T03_unlocked", true);
      if (unlocks[3]) oocsi.addBool("T04_unlocked", true);
      if (unlocks[4]) oocsi.addBool("T05_unlocked", true);
      // Send currentRoute to OOCSI
      oocsi.addInt("route", currentRoute);
      oocsi.sendMessage();

      state = 2;
    }
  }

  //---------------------------- State 2 - After -----------------------------//
  else if (state == 2) {
    if (millis() > timeMillis) {
      timeMillis = millis() + 10000;
      checkOnline();
    }

    // Unlock world if last in route
    if (determinePos(currentRoute, 0, 0) == 4) {
      unlockWorld();
    }

    // Unlock world if second to last in route and neighbour is offline
    else if (determinePos(currentRoute, 0, 0) == 3) {
      afterNeighbour = determineNeighbour(currentRoute, 1, 0);
      afterNeighbourPos = determinePos(currentRoute, 1, 0);
      afterNeighbourIndex = determineIndex(afterNeighbour);
      if (!online[afterNeighbourPos]) {
        unlockWorld();
      }
    }

    // Reset Arduino if World unlocked
    if (worldUnlocked) {
      EEPROM.write(0, (currentRoute + 1) % 3);
      resetFunc();
    }
  }
}


//Determines name of a Neighbour. Side defines neighbour before or after.
//Offset is used when the neighbour is offline to look for its neighbour
String determineNeighbour(int routeNr, int side, int offset) {
  for (int i = 0; i < sizeof(routes[routeNr]) / sizeof(routes[routeNr][0]); i++) {
    if (group.equals(routes[routeNr][i])) {
      return routes[routeNr][i + side * (1 + offset)];
    }
  }
}

//Determines position. Side defines self or neighbour before or after.
//Offset is used when the neighbour is offline to look for its neighbour
int determinePos(int routeNr, int side, int offset) {
  for (int i = 0; i < sizeof(routes[routeNr]) / sizeof(routes[routeNr][0]); i++) {
    if (group.equals(routes[routeNr][i])) {
      return i + side * (1 + offset);
    }
  }
}

//Determines index in "group" string
int determineIndex(String neighbour) {
  for (int i = 0; i < sizeof(groups) / sizeof(groups[0]); i++) {
    if (neighbour.equals(groups[i])) {
      return i;
    }
  }
}

//Checks who is in the list of online clients
void checkOnline() {
  online[0] = oocsi.containsClient("T01");
  online[1] = oocsi.containsClient("T02");
  online[2] = oocsi.containsClient("T03");
  online[3] = oocsi.containsClient("T04");
  online[4] = oocsi.containsClient("T05");
}

//Returns number of online devices
int numberOnline() {
  int total = 0;
  for (int i = 0; i < sizeof(online) / sizeof(boolean); i++) {
    if (online[i]) {
      total++;
    }
  }
  return total;
}

//Checks if ready for activation
void activationCheck(int offset) {
  beforeNeighbour = determineNeighbour(currentRoute, -1, offset);
  beforeNeighbourPos = determinePos(currentRoute, -1, offset);
  beforeNeighbourIndex = determineIndex(beforeNeighbour);

  // Continue to state 1 if beforeNeighbour is activated
  if (online[beforeNeighbourIndex] && unlocks[beforeNeighbourIndex]) {
    state = 1;
    return;
  }
}

void unlockWorld() {
  oocsi.newMessage(channel);
  oocsi.addInt("route", currentRoute);
  oocsi.addBool("World_unlocked", true);
  oocsi.sendMessage();
  worldUnlocked = true;
}

//Prints positive value of a boolean array
void printBooleanArray(boolean input[], int inputSize, String what) {
  Serial.println(what);
  for (int i = 0; i < inputSize; i++) {
    if (input[i]) {
      Serial.print("T0");
      Serial.println(i + 1);
    }
  }
}

//Triggered when a message is received, unlocked state saved in the unlocks array
void processOOCSI() {
  Serial.println("--------------OOCSI Triggered--------------");
  unlocks[0] = oocsi.getBool("T01_unlocked", false);
  unlocks[1] = oocsi.getBool("T02_unlocked", false);
  unlocks[2] = oocsi.getBool("T03_unlocked", false);
  unlocks[3] = oocsi.getBool("T04_unlocked", false);
  unlocks[4] = oocsi.getBool("T05_unlocked", false);
  worldUnlocked = oocsi.getBool("World_unlocked", false);
  currentRoute = oocsi.getInt("route", 0);
}
