#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <Ethernet.h>
#include <Servo.h>

#define RST_PIN         5          // Configurable, see typical pin layout above
#define SS_PIN          53         // Configurable, see typical pin layout above

byte sector         = 0;
byte blockAddr      = 0; ////////Access certain sector/blocks in the card, trailer block is the last block
byte trailerBlock   = 1;

EthernetClient client;

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

MFRC522::MIFARE_Key key; //Set key instance

LiquidCrystal lcd(22,23,24,25,26,27); // instance lcd

Servo servo;

String statusSaldo="0"; // dana kurang

char server[] = "192.168.1.110"; // ip server local ( web service  etol )

signed long timeout; //TIMEOUT SO IT DOESN'T SIT THERE FOREVER


void setup() {
  Serial.begin(9600);   // Initialize serial communications with the PC
  SPI.begin();          // Init SPI bus

  // Ethernet initial
  byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
  IPAddress ip(192, 168, 1, 108); // set ip ethernet
  Ethernet.begin(mac, ip);
  // end of ethernet

  //Rfid Initial
  mfrc522.PCD_Init();   // Initiate MFRC522
  for (byte i = 0; i < 6; i++) {   // Prepare the key (used both as key A and as key B)
        key.keyByte[i] = 0xFF;        // using FFFFFFFFFFFFh which is the default at chip delivery from the factory
        }
    
    Serial.println(F("Scan a Card"));
    dump_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);     //Get key byte size
   timeout = 0;  
   delay(2000);
   //End Rfid Initial

  // initial LCD
  lcd.begin(16,2);
  lcd.print("Selamat Datang "); // test

  // initial Servo
  servo.attach(28); // pin 28 in arduino mega
  servo.write(0);
      
}

void loop() {

  servo.write(180);  // tutup palang
  
    // Look for new cards
  if ( ! mfrc522.PICC_IsNewCardPresent()) 
  {
    return;
  }
  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) 
  {
    return;
  }
  byte piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);

    // Check for compatibility with Mifare card
    if (    piccType != MFRC522::PICC_TYPE_MIFARE_MINI
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
        &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
        return;
    }
    
  byte status;
  byte buffer[18];
  byte size = sizeof(buffer);


  status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("PCD_Authenticate() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
    return;
  }

  // Read data from the block
  status = mfrc522.MIFARE_Read(blockAddr, buffer, &size);
  if (status != MFRC522::STATUS_OK) {
    Serial.print(F("MIFARE_Read() failed: "));
    Serial.println(mfrc522.GetStatusCodeName(status));
  }
      // Halt PICC 
    mfrc522.PICC_HaltA();
    // Stop encryption on PCD
    mfrc522.PCD_StopCrypto1();

    // port server 8084
    if (client.connect(server, 8084)) {
      timeout = millis()+1000;
      const String ID = dump_byte_array(buffer, size);
      Serial.println("RFID code :" + ID);

      httpRequest(ID);
//      lcd.clear();
//      lcd.setCursor(0,0);
//      lcd.print("Code ID :");
//      lcd.setCursor(0,1);
//      lcd.print(ID);
      delay(10);

      while(client.available() == 0 )
      {
        if(timeout - millis() < 0 )
        goto close;
      }

       statusSaldo = cekSaldo();
       if(statusSaldo == "1") {
          servo.write(90);  // buka palang
          delay(3000);
       }
       
       delay(1000);
       lcd.clear();
       lcd.setCursor(0,0);
       lcd.print("Selamat Datang");
       lcd.setCursor(0,1);
       lcd.print("Tempelkan Kartu");
  
        
       
      close:
        client.stop();
  
    }else{
      // if you didn't get a connection to the server:
      Serial.println("connection failed");
    }
  } 

 // TURN THE BUFFER ARRAY INTO A SINGLE STRING THAT IS UPPERCASE WHICH EQUALS OUR ID OF THE SECTOR AND BLOCK
String dump_byte_array(byte *buffer, byte bufferSize) {
          String out = "";
    for (byte i = 0; i < bufferSize; i++) {
//        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
//        Serial.print(buffer[i], HEX);
        out += String(buffer[i] < 0x10 ? " 0" : " ") + String(buffer[i], HEX);
    }
    out.toUpperCase();
    out.replace(" ", "");
    return out;
}

void httpRequest(String rfidID){

  Serial.println("connected");
    // Make a HTTP request:
    client.print("GET /etol/resources/generic/getsaldo/");
    client.print(rfidID);  //kode Kartu ( FRID scan )
    client.println("/CB HTTP/1.1"); // CB kodeMesin ( cikarang Barat Rp 4000)
    client.println("Host: 192.168.1.110"); // ip server local ( etol web service )
    client.println("Connection: close");
    client.println();
  
}


String cekSaldo(){

  String content = "";
  String statusSaldo = "0"; // dana kurang

  while(client.available() != 0 )
  {
    content = content + String(char((client.read())));
  }

  Serial.println("Response : ");
  Serial.println(content);

  content = content.substring(158);
  content.trim();

  statusSaldo = content.substring(0,1);
  statusSaldo.trim();  
  Serial.println(statusSaldo);
  content = content.substring(1,content.length()-1);
  content.trim();

  if(statusSaldo == "1"){ 

    //tampilkan di lcd
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Silakan Masuk");
    lcd.setCursor(0,1);
    lcd.print("Saldo :");
    lcd.print(content);
    
  }else{

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Saldo Kurang");
    lcd.setCursor(0,1);
    lcd.print("Saldo :");
    lcd.print(content);

  }
  
  return statusSaldo;
  
}


//END DUMP_BYTE_ARRAY
//https://github.com/machfudh/tutorial-etol/blob/gh-pages/etol_rfidrc522_tahap1.ino
//etol_rfid522_tahap1 

