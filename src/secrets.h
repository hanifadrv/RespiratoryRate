#include <pgmspace.h>
 
#define SECRET
#define THINGNAME ""                         //change this
 
const char WIFI_SSID[] = "";               //change this
const char WIFI_PASSWORD[] = "";           //change this
const char AWS_IOT_ENDPOINT[] = "";       //change this
 
// Amazon Root CA 1
static const char AWS_CERT_CA[] PROGMEM = R"EOF(

)EOF";
 
// Device Certificate                                               //change this
static const char AWS_CERT_CRT[] PROGMEM = R"KEY(

)KEY";
 
// Device Private Key                                               //change this
static const char AWS_CERT_PRIVATE[] PROGMEM = R"KEY(

)KEY";
