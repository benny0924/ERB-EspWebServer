//  EspNode

  /*
   * Copyright (c) 2015, Eldon R. Brown - ERB - WA0UWH - eldonb@ebcon.com
   *
   * See Blog at: http://WA0UWH.blogspot.com
   *
   * All rights reserved. See FULL Copyright in Main.h or Info Page for Exclusions
   *
   * Inspired by work by: Majenko Technologies - 2015 Esp8266 AdvancedWebServer
   *
   * See: https://github.com/esp8266/Arduino
   */

#define INTRODUCTION PSTR("\
    /* \
     * This Program Provides the following for the Esp8266 as a WebServer:\
     * But CAUTION, some functionallity may not have been throughly tested.\
     * \
     * The purpose of the program is to provide an Experimental Enviroment in which\
     * to create new functions that can be used in my other planned projects.\
     * \
     * Currently, all Arduino IDE TABS are '.ino' type files, which are compiled as a single blob.\
     * The structure of this progame could be better, as normally, the TABS should be set up\
     * as '*.h', '*.c', or '*.cpp' files, but this is easier and quicker for my experimental efforts.\
     * Future revisions may be properly constructed.\
     * \
     *      All Web Pages are Dynamically Generated by The Esp8266 Processor\
     *      Implemented Full (1460 bytes) WIFI Buffered Transfers\
     *      All String Constants are saved in Flash, freeing RAM/Heap, via F(), sF(), and FMT() macros.\
     *      Uses ICACHE_FLASH_ATTR to dynamically load seldom used program functions\
     *      Copyright and this Text (Introduction) is Compiled into the Executable as a Web Page\
     *      Home Page Dashboard\
     *      Help Page\
     *      Admin Page, with WiFi Scan and Network Selection\
     *      Page Navigator and Resource Links\
     *      Raw Data Query\
     *      NTP Network Time (Unix Epoch)\
     *      mDNS Service/Client\
     *      Optional Page AutoRefresh\
     *      Auto AP Timeout\
     *      Implements and uses Html 'meter' function for FreeHeap and Vdd\
     *      Implemented SVG 12/24 Hour Clock\
     *      Implemented SVG Gauge for FreeHeap\
     *      Implemented SVG Gauge for Vdd\
     *      Implemented SVG Line Graph for FreeHeap Historical Data, with Start and End of Page Markers\
     *           and reduced Data Transfer by removing Duplicate Y Data Points\
     *      Implemented Data Creation Rate Per Web Page Function\
     *      Implemented a Used CPU Seconds Value for Display\
     *      Implemented a link to a Use Map Display\
     *      Implemented Slider Bars, Currently used for Page Refresh Interval\
     */ \
 ")



#include "AA_Main.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

#include <WiFiClient.h>
#include <ESP8266mDNS.h>


extern "C" {
    #include "user_interface.h"
    uint16 readvdd33(void);
}

// Note: By my convention, All Global Variables and Constants start with a letter "g".


const char *gRev = "ERB_MC";  // Software Revision Code


String gSsid = ".....";     // Change to your SSID
String gPasswd = "........";   // Change to your Passwd


String gSsidPrev = gSsid;
String gPasswdPrev = gPasswd;

char gDeviceName[16] = "Nod000000"; // Default Unit ID "Node" Name, This will be changed in Setup

const String gSoftAPAddress = "192.168.4.1"; // Default Soft AP IP Address
const int gDeviceApChannelDefault = 8; // Used when in SoftAP only Mode
int gApChannel = gDeviceApChannelDefault;

unsigned long gApTimeout = 0;
boolean gStnLive = false;
int gWifiStnAttempts = 0;
int gWifiReStarts = 0;
boolean gReStartSTN = false;

byte gCurrentPage = HOMEPAGE;

// Buffer, Used with FMT() to store constants in program space (FLASH)
// See Main.h file\
char gFmtBuf[64+2];

char gTmpBuf[32+2];  // Generic Temp Buffer

// Line Graphic will be plotted with this data
byte gFreeHeapLog[512]; // Log Storage
int gFreeHeapLogIndex = 0;

const boolean ON = 0;  // Used to set LED state
const boolean OFF = !ON;

boolean gVerbose = false;        // Web Page Verbose Mode

boolean gVisitorMapState = true; // Show Visitor Map
String  gSharedVisitorMapIdKey = "2q435yktjwx";   // A shared "IdKey" for sites that want to participate in shared Global Reverse Beacon Map
String  gVisitorMapIdKey = gSharedVisitorMapIdKey;
int gVisitorMapStyle = 1;
 
long gHits = 0;                  // Web page Hit Counter

// Initial Automatic Page Refresh Intervals
int gAutoHomeRefresh = 60;
int gAutoHelpRefresh = 180;
int gAutoAdminRefresh = 180;


unsigned long gNextNTPSchd = millis();

unsigned long gNextMDnsSchd = millis();

byte gMacBuf[6]; // A buffer to hold Mac Address conversion

unsigned long gPageStartTime = millis();

long gIdleL = 0;  // Idle counter
long gUpTimeSec = 0; 

const long gTimeZONE = -(7*HRs/SECs); // PDT
boolean gEpochAvailable = false;
unsigned long gEpochDelta = 0; // The Time ADDED to millis() to get Epoch

boolean gMarkFlag = false;

long gSentSize = 0; // WEB Page Size

long gLoadAvgL = 0;
long gLoadAvgCountL = 0;
String gLoadAvgS = "1.00";
long gIdlePrevL = 0;
unsigned long gOneSecSchedule = millis() + 1 * SECs;

MDNSResponder gMDns;
boolean gMDnsResponderReady = false;

ESP8266WebServer gServer ( 80 );
WiFiClient gClient;


// Pin numbers for LEDs
const int gGrnLED = 0;
const int gRedLED = 2;
const int gBluLED = 4;
boolean gRedLedState = OFF;

// Some often used Text Strings as Constants
const String gTextPlain = "text/plain";
const String gTextHtml  = "text/html";

// ###########################################################
void
ICACHE_FLASH_ATTR
setup ( void )
{
     
    Serial.begin ( 115200 );
    delay(3000);
    
    for (int i = 3; i>0; i--) {
      Serial.println ( F("Start: setup") );
    }
   
    Serial.setDebugOutput(true);
    
    pinMode ( gRedLED, OUTPUT ); digitalWrite ( gRedLED, OFF );
    pinMode ( gGrnLED, OUTPUT ); digitalWrite ( gGrnLED, OFF );
    pinMode ( gBluLED, OUTPUT ); digitalWrite ( gBluLED, OFF );
    
    // Create a "gDeviceName" from chip_id, this could be the MAC address, or the least digits of the Chip ID, (or anything you like)
    // Also, See: try2StartWifiStn() for using Least Digits of IPA
    // sprintf(gDeviceName+3, FMT("%04d"), ESP.getChipId() % 10000); // Append ID Number at position 3
    sprintf(gDeviceName+3, FMT("%X"), ESP.getChipId());              // Append Hex ID Number at position 3

    logFreeHeapMarkDn();
      
    // Startup WIFI
    initWifiAp();
    
//    Serial.print   ( F("TimeZONE: ") );
//    Serial.println ( TimeZONE );
    
    Serial.println ( F("End: setup") );
 
}

// ###########################################################
void
loop ( void )
{

      gIdleL++;  // Idle Loop Counter
      
      gServer.handleClient(); 
      
      // Short Cut the Loop, so Support Tasks only run once per second
      // This keeps the "loop" very fast most of the time, which is needed
      // for WIFI processes.
      if (gOneSecSchedule > millis()) return;


      
      gOneSecSchedule += 1 * SECs;
      gUpTimeSec++;
      
      
      if ( gReStartSTN == true  ) {
        try2StartWifiStn();
      }
   
      if ( gStnLive == true ) {
        if ( gApTimeout && millis() > gApTimeout ) WiFi.mode(WIFI_STA);  // After AP Timeout, Switch to STN-only mode    
        updateMDNS();  // Scheduled MDNS Update
        updateNTP();   // Scheduled NTP Update
      }
      
      digitalWrite ( gGrnLED, ON );  // Show Loop Activity
      // The following provides enough time for a quick LED Blink 
      logFreeHeap();       // This logs data for the Free Heap Display
      logPseudoLoadAvg();  // This logs data for the Pseudo Load Average
      digitalWrite ( gGrnLED, OFF );

      // Dubug
//      Serial.println ( sF("Loops Per Sec: ") + String( gIdleL - gIdlePrevL) );
//      gIdlePrevL = gIdleL;
      
}

// ###########################################################
// End
