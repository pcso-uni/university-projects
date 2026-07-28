#include <SPI.h>
#include <MFRC522.h>
#include <WiFiEsp.h>
#include <ThingSpeak.h>
#include <ArduinoJson.h>

#include <Adafruit_SSD1306.h>
#include <Servo.h>
#include <SPI.h>


#define SS_PIN 53
#define RST_PIN 5
#define RED_LED_PIN 11
#define GREEN_LED_PIN 12
#define BLUE_LED_PIN 13

MFRC522 rfid(SS_PIN, RST_PIN);  // RFID reader instance
WiFiEspClient client;           // Wi-Fi client for ThingSpeak
unsigned long myChannelNumber = 2703166;
const char* myWriteAPIKey = "PG5FHB4IGTU7640G";
const char* myReadAPIKey = "496A6LEDENMNB6ED";

// Maximum number of registered cards
const int maxCards = 7;
byte registeredUIDs[maxCards][4];  // Array to store registered UIDs
int registeredCount = 0;           // Number of registered cards

// Recycling data and points for current user
String userid = "";
int temp = 0;
int userPoints = 0;
int userMetal = 0;
int userPlastic = 0;
int userPaper = 0;
int totalMetal = 0;
int totalPlastic = 0;
int totalPaper = 0;
int cardIndex = -1;  // Current user field index
String leastRecycledItem = "";

bool readyMessagePrinted = false;  // Flag to track if the "Ready for RFID" message has been printed
bool isRecyclingSession = false;   // Tracks if the user has started a recycling session
bool ready_tap_card = false;

/////////////////////////////////////////////////////////////////
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels

#define OLED_RESET -1  // Reset pin (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Servo servos[3];
int servoPins[] = { 2, 3, 4 };

bool readyToScan = false;
bool waitingForInput = false;
bool recycling = false;
bool scanning = false;
char rfidchar = '\0';
char rfidClose = '\0';
char input = '\0';
char inputChar = '\0';
char inputChar2 = '\0';
char signal = '\0';
char data2 = '\0';

////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();

  // Setup LED pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);

  // Initialize WiFi and ThingSpeak
  Serial1.begin(115200);  // Serial communication for ESP8266
  WiFi.init(&Serial1);
  ThingSpeak.begin(client);

  // Connect to WiFi
  connectToWiFi("EE3070_P1615_1", "EE3070P1615");

  // Print the ready message once
  Serial.println("Ready for RFID scan...");
  readyMessagePrinted = true;

  ////////////////////////////////////////////////////////////////////////////

  pinMode(33, INPUT);  // Push button pin
  pinMode(35, INPUT);
  pinMode(37, INPUT);
  // Initialize the display with 3.3V voltage generation
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {  // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;  // Don't proceed, loop forever
  }
  // Clear the buffer
  // display.clearDisplay();
  display.display();

  // Attach each servo to its respective pin
  for (int i = 0; i < 3; i++) {
    servos[i].attach(servoPins[i]);
    servos[i].write(0);  // Initialize all servos to 0 degrees
  }
  Serial.println(F("Servos initialized successfully"));
}

////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

void loop() {
  // Set text size and color
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);  // Start at top-left corner

  // This is the initial screen (start screen)
  if (!readyToScan && !waitingForInput && !recycling) {
    // Initial display: "Ready to use, tap card to start"
    display.clearDisplay();
    display.println(F("Ready to use,"));
    display.println(F("tap card to start"));
    display.fillTriangle(110, 45, 110, 55, 120, 50, SSD1306_WHITE);  // Arrow pointing right
    display.display();                                               // Show the arrow
    delay(500);                                                      // Wait for 500 milliseconds
    // Clear the arrow (blinking effect)
    display.fillTriangle(110, 45, 110, 55, 120, 50, SSD1306_BLACK);  // Clear the arrow by drawing it in black
    display.display();                                               // Show the cleared arrow
    delay(500);                                                      // Wait for 500 milliseconds
  }

  //////////////////////////////////////////////////////////////////////////////////
  // Check for a card

  if (!isRecyclingSession) {
    if (!rfid.PICC_IsNewCardPresent()) return;
    if (!rfid.PICC_ReadCardSerial()) return;

    getTotalRecycledData();
    // Find or register UID
    int fieldIndex = findOrRegisterUID(rfid.uid.uidByte);

    if (fieldIndex == -1) {
      Serial.println("Registration full. Cannot register new cards.");
      return;
    }

    // Display user data for existing or new user
    displayUserData(fieldIndex);

    isRecyclingSession = true;  // Start the recycling session
    readyToScan = true;
    input = 'R';
  }
  //////////////////////////////////////////////////////////////////////////////////

  if (readyToScan && !waitingForInput && !recycling) {  // After tapping RFID card
    // Display "Hello, [name]" and "Award points" from RFID
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print(F("Hello, "));
    display.println(userid);
    display.print(F("Award points: "));
    display.println(userPoints);
    // Display "Ready to scan..." with moving dots
    display.print(F("Ready to scan..."));
    display.display();
  }
  if (waitingForInput && !recycling) {  // After material scanning
    // Once 'A', 'B', or 'C' is pressed, display the question and instruction
    display.clearDisplay();
    display.setTextSize(1);               // Normal text size
    display.setTextColor(SSD1306_WHITE);  // Set text color to white
    display.setCursor(0, 0);              // Position at the top left
    display.print(F("Q. Which material do"));
    display.setCursor(0, 10);  // Move down a bit for the next line
    display.print(F("you want to recycle?"));
    // Print the instruction at the bottom of the screen
    display.setCursor(0, SCREEN_HEIGHT - 20);  // Position near the bottom
    display.print(F("(Push 1 button on the bottom)"));
    display.display();
  }
  //////////////////////////////////////////////////////////////////////////////////
  if (waitingForInput && recycling) {  // After button pushed
    if (signal == '1') {
      display.clearDisplay();
      displayMessage(F("Correct :)"), F("(Gate is opening...)"));  // Display correct message
      openServos(2);
      delay(1000);                                                              // Wait for 1 second
      displayMessage(F("Just wait to close the gate."), F(""));  // Show message to press 'R'
      display.display();
      input = 'R';
      R_Uplord();  // Wait for 'R' input to close the gate
      // Clear the display and show "Gate is closing..."
      //clearAndShowClosingMessage();
      closeServos(2);
      readyToScan = false;  // Reset the state for next cycle
      waitingForInput = false;
      recycling = false;
    } else if (signal == '2') {
      display.clearDisplay();
      displayMessage(F("Correct :)"), F("(Gate is opening...)"));  // Display correct message
      openServos(1);
      delay(1000);                                                              // Wait for 1 second
      displayMessage(F("Just wait to close the gate."), F(""));  // Show message to press 'R'
      display.display();
      input = 'R';
      R_Uplord();  // Wait for 'R' input to close the gate
      //clearAndShowClosingMessage();
      closeServos(1);
      readyToScan = false;
      waitingForInput = false;
      recycling = false;
    } else if (signal == '3') {
      display.clearDisplay();
      displayMessage(F("Correct :)"), F("(Gate is opening...)"));  // Display correct message
      openServos(0);
      delay(1000);                                                              // Wait for 10 seconds
      displayMessage(F("Just wait to close the gate."), F(""));  // Show message to press 'R'
      display.display();
      input = 'R';
      R_Uplord();  // Wait for 'R' input to close the gate
      // Clear the display and show "Gate is closing..."
      //clearAndShowClosingMessage();
      closeServos(0);
      readyToScan = false;      // Flag to track when to display the second screen
      waitingForInput = false;  // Flag to check if we're waiting for 'A', 'B', or 'C'
      recycling = false;
    } else if (signal == '4') {
      display.clearDisplay();
      displayMessage(F("Wrong, it should be plastic."), F("(Gate is opening...)"));  // Display correct message
      openServos(2);
      delay(1000);                                                              // Wait for 10 seconds
      displayMessage(F("Just wait to close the gate."), F(""));  // Show message to press 'R'
      display.display();
      input = 'R';
      R_Uplord();  // Wait for 'R' input to close the gate
      // Clear the display and show "Gate is closing..."
      //clearAndShowClosingMessage();
      closeServos(2);
      readyToScan = false;      // Flag to track when to display the second screen
      waitingForInput = false;  // Flag to check if we're waiting for 'A', 'B', or 'C'
      recycling = false;
    } else if (signal == '5') {
      display.clearDisplay();
      displayMessage(F("Wrong, it should be paper."), F("(Gate is opening...)"));  // Display correct message
      openServos(1);
      delay(1000);                                                              // Wait for 10 seconds
      displayMessage(F("Just wait to close the gate."), F(""));  // Show message to press 'R'
      display.display();
      input = 'R';
      R_Uplord();  // Wait for 'R' input to close the gate
      // Clear the display and show "Gate is closing..."
      //clearAndShowClosingMessage();
      closeServos(1);
      readyToScan = false;      // Flag to track when to display the second screen
      waitingForInput = false;  // Flag to check if we're waiting for 'A', 'B', or 'C'
      recycling = false;
    } else if (signal == '6') {
      display.clearDisplay();
      displayMessage(F("Wrong, it should be metal."), F("(Gate is opening...)"));  // Display correct message
      openServos(0);
      delay(1000);                                                              // Wait for 10 seconds
      displayMessage(F("Just wait to close the gate."), F(""));  // Show message to press 'R'
      display.display();
      input = 'R';
      R_Uplord();  // Wait for 'R' input to close the gate
      // Clear the display and show "Gate is closing..."
      //clearAndShowClosingMessage();
      closeServos(0);
      readyToScan = false;      // Flag to track when to display the second screen
      waitingForInput = false;  // Flag to check if we're waiting for 'A', 'B', or 'C'
      recycling = false;
    }

  }

  // Check if a valid input ('R', 'A', 'B', or 'C') is typed and store it
  //'R' means for RFID part
  if (readyToScan) {
    if (input == 'R') {
      scanning = true;
      //RFID part
      //RFID User Login System
    }
    while (scanning) {
      data2 = Serial.read();
      if (data2 == 'A') {
        data2 = '\0';
        input = 'A';
        scanning = false;
      }
      if (data2 == 'B') {
        data2 = '\0';
        input = 'B';
        scanning = false;
      }
      if (data2 == 'C') {
        data2 = '\0';
        input = 'C';
        scanning = false;
      }
    }
    while (waitingForInput) {
      bool valuee = digitalRead(33);  // Read button state
      bool valuen = digitalRead(35);
      bool valuet = digitalRead(37);

      if (valuee == 0) {  // Button 1 pressed
        Serial.println("Input set to: X");
        input = 'X';  // Set input to 'X'
        delay(200);
        break;
      }
      if (valuen == 0) {  // Button 2 pressed
        Serial.println("Input set to: Y");
        input = 'Y';  // Set input to 'Y'
        delay(200);
        break;
      }
      if (valuet == 0) {  // Button 3 pressed
        Serial.println("Input set to: Z");
        input = 'Z';  // Set input to 'Z'
        delay(200);
        break;
      }
    }
    if ((input == 'A' || input == 'B' || input == 'C') && readyToScan) {
      // Press 'A', 'B', or 'C' to record the answer
      Serial.print(F("Input received: "));
      Serial.println(input);  // Store and print input
      inputChar = input;
      input = '\0';
      waitingForInput = true;  // Display the question and instruction
    }
    if ((input == 'X' || input == 'Y' || input == 'Z') && waitingForInput) {
      Serial.print(F("Input received: "));
      Serial.println(input);  // Store and print input
      recycling = true;
      inputChar2 = input;
      int pointsEarned = 0;
      leastRecycledItem = getLeastRecycledItem();

      if (inputChar == 'A') {     // plastic
        if (inputChar2 == 'X') {  // plastic
          userPlastic++;
          totalPlastic++;
          pointsEarned = 1;
          if (leastRecycledItem == "plastic") {
            pointsEarned *= 2;  // Apply multiplier if plastic has the fewest count
            Serial.println("Plastic recycled with multiplier. Points +2");
          } else {
            Serial.println("Plastic recycled. Points +1");
          }
          userPoints += pointsEarned;
          signal = '1';
        } else {  // not plastic
          signal = '4';
        }
      }
      if (inputChar == 'B') {     // paper
        if (inputChar2 == 'Y') {  // paper
          userPaper++;
          totalPaper++;
          pointsEarned = 1;
          if (leastRecycledItem == "Paper") {
            pointsEarned *= 2;  // Apply multiplier if books have the fewest count
            Serial.println("Paper recycled with multiplier. Points +2");
          } else {
            Serial.println("Paper recycled. Points +1");
          }
          userPoints += pointsEarned;
          signal = '2';
        } else {
          signal = '5';
        }
      }
      if (inputChar == 'C') {     // metal
        if (inputChar2 == 'Z') {  // metal
          userMetal++;
          totalMetal++;
          pointsEarned = 1;
          if (leastRecycledItem == "metal") {
            pointsEarned *= 2;  // Apply multiplier if aluminum has the fewest count
            Serial.println("Metal can recycled with multiplier. Points +2");
          } else {
            Serial.println("Metal can recycled. Points +1");
          }
          userPoints += pointsEarned;
          signal = '3';
        } else {
          signal = '6';
        }
      }
    }
  }
  // Control blinking effect
  delay(500);  // Adjust delay for speed of the blinking
}
////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
// Function to turn off all LEDs
void turnOffLEDs() {
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
}

// Function to flash an LED for a specified duration
void flashLED(int pin, int duration) {
  digitalWrite(pin, HIGH);
  delay(duration);
  digitalWrite(pin, LOW);
}


// Function to connect to Wi-Fi
void connectToWiFi(const char* ssid, const char* password) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting to WiFi: ");
    Serial.println(ssid);
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, password);
      delay(5000);
      Serial.print(".");
    }
    Serial.println("\nConnected to WiFi");
  }
}

// Function to display user data (customize as needed)
void displayUserData() {
  Serial.print("User Points: ");
  Serial.println(userPoints);
  Serial.print("Metal: ");
  Serial.println(userMetal);
  Serial.print("Plastic: ");
  Serial.println(userPlastic);
  Serial.print("Paper: ");
  Serial.println(userPaper);
}

// Function to search for the UID in ThingSpeak fields and register if not found
int findOrRegisterUID(byte* uid) {
  turnOffLEDs();
  String uidString = "";
  for (int i = 0; i < 4; i++) {
    uidString += String(uid[i], HEX);
  }

  int maxRetries = 3;  // Maximum attempts to write data
  int attempt = 0;

  // Check each of the 7 user fields on ThingSpeak
  for (int i = 1; i <= 7; i++) {
    // Read the full data string from ThingSpeak
    String data = ThingSpeak.readStringField(myChannelNumber, i, myReadAPIKey);
    Serial.println("**Running findOrRegisterUID readStringField field: " + String(i));

    if (data == "-1") {  // Empty field detected (returns -1 for empty)
      Serial.println("Empty field detected, attempting to register UID...");

      // Json format create new entry for new user
      JsonDocument newDoc;
      newDoc["uid"] = uidString;
      newDoc["awardPoints"] = 0;
      newDoc["metal"] = 0;
      newDoc["plastic"] = 0;
      newDoc["paper"] = 0;
      String newEntry;
      serializeJson(newDoc, newEntry);

      while (attempt < maxRetries) {
        ThingSpeak.setField(i, newEntry);
        int statusCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

        Serial.println("**Running findOrRegisterUID writeFields");
        if (statusCode == 200) {
          Serial.println("New card registered to field " + String(i));
          flashLED(BLUE_LED_PIN, 2000);  // 1 second blue flash
          return i;                      // Registration successful, return field index
        } else {
          Serial.println("Failed to register new card, attempt " + String(attempt + 1) + " of " + String(maxRetries));
          delay(1000);  // Delay before retry
          attempt++;
        }
      }

      Serial.println("Max retries reached. Could not register new card.");
      return -1;  // Exceeded max retries without success
    } else {

      // Json decode uid
      JsonDocument doc1;
      deserializeJson(doc1, data);
      String storedUID = doc1["uid"];

      if (storedUID == uidString) {
        Serial.println("Card found in field " + String(i));

        // Always on green LED during the recycling session
        if (isRecyclingSession) {
          digitalWrite(GREEN_LED_PIN, HIGH);  // Green LED is on during recycling session
        } else {
          digitalWrite(GREEN_LED_PIN, LOW);  // Green LED is off when no session is active
        }
        return i;  // Return field index for existing UID
      }
    }
  }

  // All fields are occupied or registration failed after retries
  Serial.println("All fields occupied or registration failed.");
  return -1;
}

// Function to retrieve and parse user data from a specific field
void displayUserData(int fieldIndex) {
  // Check if fieldIndex is valid
  if (fieldIndex < 1 || fieldIndex > 7) {
    Serial.println("Invalid field index. Cannot retrieve data.");
    return;
  }

  // Retrieve data from ThingSpeak
  String data = ThingSpeak.readStringField(myChannelNumber, fieldIndex, myReadAPIKey);
  if (data == "-1") {  // Check for empty field
    Serial.println("No data found for user.");
    return;
  }

  // Json decode user data
  JsonDocument doc1;
  deserializeJson(doc1, data);
  String uid = doc1["uid"];
  int points = doc1["awardPoints"];
  int metal = doc1["metal"];
  int plastic = doc1["plastic"];
  int paper = doc1["paper"];

  // Display the UID and user data
  Serial.print("User UID: ");
  Serial.println(uid);
  Serial.print("User Points: ");
  Serial.println(points);
  Serial.print("Metal: ");
  Serial.println(metal);
  Serial.print("Plastic: ");
  Serial.println(plastic);
  Serial.print("Paper: ");
  Serial.println(paper);

  // Update global user data
  userid = uid;
  userPoints = points;
  userMetal = metal;
  userPlastic = plastic;
  userPaper = paper;

  Serial.println("**Running displayUserData");
  // Always on green LED during the recycling session
  digitalWrite(GREEN_LED_PIN, HIGH);  // Green LED is on during recycling session
}


void R_Uplord() {
  while (true) {
    if (Serial.available() > 0) {  // Check if any input is available
      if (input == 'R' || input == 'r') {
        if (isRecyclingSession) {
          // If the card is tapped again, complete the session and upload data
          int fieldIndex = findOrRegisterUID(rfid.uid.uidByte);  // Re-check UID field index

          if (fieldIndex != -1) {
            uploadUserDataToCloud(fieldIndex, rfid.uid.uidByte);
            uploadTotalRecycledCounts();  // Upload total recycled counts to field 8
          }

          isRecyclingSession = false;  // End the recycling session
        }

        // Halt communication with the RFID card
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();

        clearAndShowClosingMessage();
        break;  // Exit the loop when 'R' is pressed
      }
    }
  }
}


// Function to upload the user’s updated data to ThingSpeak
void uploadUserDataToCloud(int fieldIndex, byte* uid) {
  turnOffLEDs();
  // Create UID string
  String uidString = "";
  for (int i = 0; i < 4; i++) {
    uidString += String(uid[i], HEX);
  }

  // Json encode user data
  JsonDocument uploadDoc;
  uploadDoc["uid"] = uidString;
  uploadDoc["awardPoints"] = userPoints;
  uploadDoc["metal"] = userMetal;
  uploadDoc["plastic"] = userPlastic;
  uploadDoc["paper"] = userPaper;

  String dataString;
  serializeJson(uploadDoc, dataString);

  // Send data to ThingSpeak
  ThingSpeak.setField(fieldIndex, dataString);
  Serial.println("**Running uploadUserDataToCloud setField");
  int statusCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  Serial.println("**Running uploadUserDataToCloud writeFields");

  if (statusCode == 200) {
    Serial.println("Data sent to cloud successfully");
    delay(12000);

    // Turn off green LED as user has completed the session
    digitalWrite(GREEN_LED_PIN, LOW);

    // Flash red LED to indicate data upload completion
    flashLED(RED_LED_PIN, 3000);  // Flash red LED for 2 seconds
  } else {
    Serial.println("Failed to send data to cloud, retrying...");

    // Retry few times
    int maxRetries = 3;
    for (int attempt = 0; attempt < maxRetries; attempt++) {
      delay(2000);  // Wait before retrying
      statusCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
      if (statusCode == 200) {
        Serial.println("Data sent to cloud successfully after retrying");
        break;
      }
      Serial.println("Retry attempt " + String(attempt + 1) + " failed.");
    }

    if (statusCode != 200) {
      Serial.println("Max retries reached. Could not send data to cloud.");
    }
  }
  temp = userPoints;
  // Reset user data after upload
  userPoints = 0;
  userMetal = 0;
  userPlastic = 0;
  userPaper = 0;
}

// Function to retrieve the total recycled data from ThingSpeak field 8
void getTotalRecycledData() {
  String data = ThingSpeak.readStringField(myChannelNumber, 8, myReadAPIKey);
  if (data != "-1") {  // total recycled data do exist -> create a new one
    // Json decode total recycled data
    JsonDocument getTotalDoc;
    deserializeJson(getTotalDoc, data);

    // Set the global variable
    //leastRecycledItem = getTotalDoc["leastRecycledItem"].as<String>();
    leastRecycledItem = getTotalDoc["leastRecycledItem"].as<String>();
    totalMetal = getTotalDoc["metal"];
    totalPlastic = getTotalDoc["plastic"];
    totalPaper = getTotalDoc["paper"];
  }
}


// Function to upload total recycled item counts to ThingSpeak field 8
void uploadTotalRecycledCounts() {

  // Json encode total data (metal, plastic, paper)
  JsonDocument totalDoc;
  totalDoc["leastRecycledItem"] = leastRecycledItem;
  totalDoc["metal"] = totalMetal;
  totalDoc["plastic"] = totalPlastic;
  totalDoc["paper"] = totalPaper;

  String totalDataString;
  serializeJson(totalDoc, totalDataString);

  // Set the data string to field 8
  ThingSpeak.setField(8, totalDataString);
  Serial.println("**Running uploadTotalRecycledCounts setField");

  int maxRetries = 3;  // Maximum retries
  int attempt = 0;
  int statusCode = -1;

  while (attempt < maxRetries) {
    // Check WiFi status before each attempt
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi not connected, attempting to reconnect...");
      connectToWiFi("Coffee2.4_EXT", "coffee23410211");
    }

    // Send data to ThingSpeak and get status
    statusCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
    Serial.println("**Running uploadTotalRecycledCounts writeFields");

    if (statusCode == 200) {
      Serial.println("Total recycled counts sent to cloud successfully.");
      break;  // Successful, exit the loop
    } else {
      Serial.println("Failed to send total recycled counts to cloud, attempt " + String(attempt + 1));
      delay(2000);  // Delay before retrying
      attempt++;
    }
  }

  // Final check if all retries failed
  if (statusCode != 200) {
    Serial.println("Max retries reached. Could not send total recycled counts to cloud.");

    // Attempt to reset WiFi
    Serial.println("Resetting WiFi connection...");
    WiFi.disconnect();
    connectToWiFi("EE3070_P1615_1", "EE3070P1615");
  }
}


// Function to determine the least recycled item type
String getLeastRecycledItem() {
  getTotalRecycledData();
  uploadTotalRecycledCounts();
  String data = ThingSpeak.readStringField(myChannelNumber, 8);
  String createdTime = ThingSpeak.getCreatedAt();
  Serial.println("Created Time: " + createdTime);
  if (data != "-1" && createdTime.substring(8, 10).toInt() == 1) {
    if (totalMetal <= totalPlastic && totalMetal <= totalPaper) {
        leastRecycledItem = "metal";
    } else if (totalPlastic <= totalMetal && totalPlastic <= totalPaper) {
        leastRecycledItem = "plastic";
    } else {
        leastRecycledItem =  "paper";
    }
  }
  return leastRecycledItem;
}

////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

void openServos(int servo1) {  // Move two servos up to 85 degrees simultaneously
  for (int angle = servos[servo1].read(); angle <= 85; angle++) {
    servos[servo1].write(angle);
    delay(45);  // Delay to control speed
  }
}

void closeServos(int servo1) {  // Move two servos down to 0 degrees simultaneously
  for (int angle = servos[servo1].read(); angle >= 0; angle--) {
    servos[servo1].write(angle);
    delay(90);  // Delay to control speed
  }
}

void displayMessage(const __FlashStringHelper* topMessage, const __FlashStringHelper* bottomMessage) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);                   // Start at top-left corner
  display.println(topMessage);               // Display top message
  display.setCursor(0, SCREEN_HEIGHT - 10);  // Position near the bottom
  display.println(bottomMessage);            // Display bottom message
  display.display();
}

void clearAndShowClosingMessage() {
  display.clearDisplay();                    // Clear the display // Always show "Gate is closing..." at the bottom
  display.setCursor(0, SCREEN_HEIGHT - 10);  // Position near the bottom
  display.println(F("Gate is closing..."));
  display.display();
  delay(2000);                            // Wait for 2 seconds before showing the new message
  display.clearDisplay();                 // Clear the display again
  display.setCursor(0, 0);                // Start at the top-left corner
  display.println(F("Awards points: "));  // Display the awards points message
  display.print(temp);
  display.setCursor(0, SCREEN_HEIGHT - 10);  // Position near the bottom
  display.println(F("Thank you"));           // Display "Thank you"
  display.display();                         // Update the screen with the new message
}
