#include <WiFi.h>
#include <WebServer.h>

// ================= WIFI =================
const char* ssid = "Moto";
const char* password = "vishal90";

WebServer server(80);

// ================= MOTOR PINS =================
const int IP1 = 26;
const int IP2 = 25;
const int IP3 = 33;
const int IP4 = 32;

// ================= MOTOR SPEED =================
// 0 to 255 PWM
int motorSpeed = 220;

// ================= PWM =================
#define PWM_FREQ 1000
#define PWM_RESOLUTION 8

// ================= SAFETY =================
unsigned long lastCommandTime = 0;
const unsigned long MOTOR_TIMEOUT = 1200;

// ================= GPS =================
HardwareSerial GPS(2);

#define GPS_RX 16 // ESP32 RX2 <- GPS TX
#define GPS_TX 17 // ESP32 TX2 -> GPS RX
#define GPS_BAUD 9600

// Set to true to print raw NMEA to Serial Monitor (for debugging wiring/baud)
#define GPS_DEBUG_RAW false

String gpsLine = "";

bool gpsFix = false;
int satellites = 0;

double latitude = 0.0;
double longitude = 0.0;

String latitudeText = "No Fix";
String longitudeText = "No Fix";

// ======================================================
// GPS NMEA CONVERSION
// ======================================================

double nmeaToDecimal(String value, String direction) {

if (value.length() < 3)
return 0.0;

double raw = value.toDouble();

int degrees = (int)(raw / 100);
double minutes = raw - (degrees * 100);

double decimal = degrees + (minutes / 60.0);

if (direction == "S" || direction == "W")
decimal = -decimal;

return decimal;
}

// ======================================================
// SPLIT NMEA SENTENCE INTO FIELDS (bounds-safe)
// returns number of fields stored
// ======================================================

int splitFields(String sentence, String fields[], int maxFields) {

int count = 0;

// strip checksum (*XX)
int star = sentence.indexOf('*');
if (star >= 0)
sentence = sentence.substring(0, star);

while (count < maxFields) {

int comma = sentence.indexOf(',');

if (comma < 0) {
fields[count++] = sentence;
break;
}

fields[count++] = sentence.substring(0, comma);
sentence = sentence.substring(comma + 1);
}

return count;
}

// ======================================================
// GPS PARSER
// ======================================================

void parseGPS(String sentence) {

// ---------- RMC: fix status + position ----------
if (sentence.startsWith("$GPRMC") ||
sentence.startsWith("$GNRMC")) {

String fields[16];
int count = splitFields(sentence, fields, 16);

if (count >= 7) {

if (fields[2] == "A") {

gpsFix = true;

latitude = nmeaToDecimal(fields[3], fields[4]);
longitude = nmeaToDecimal(fields[5], fields[6]);

latitudeText = String(latitude, 6);
longitudeText = String(longitude, 6);
}
else {

gpsFix = false;
}
}
}

// ---------- GGA: satellites + fix quality ----------
else if (sentence.startsWith("$GPGGA") ||
sentence.startsWith("$GNGGA")) {

String fields[16];
int count = splitFields(sentence, fields, 16);

if (count >= 8) {

int fixQuality = fields[6].toInt();

satellites = fields[7].toInt();

if (fixQuality > 0)
gpsFix = true;
}
}
}

// ======================================================
// READ GPS
// ======================================================

void readGPS() {

while (GPS.available()) {

char c = GPS.read();

#if GPS_DEBUG_RAW
Serial.write(c);
#endif

if (c == '\n') {

gpsLine.trim();

if (gpsLine.length() > 0)
parseGPS(gpsLine);

gpsLine = "";
}
else {

// guard against runaway line if data is garbage
if (gpsLine.length() < 120)
gpsLine += c;
else
gpsLine = "";
}
}
}

// ======================================================
// MOTOR FUNCTIONS
// ======================================================

void forward() {

ledcWrite(IP1, motorSpeed);
ledcWrite(IP2, 0);

ledcWrite(IP3, motorSpeed);
ledcWrite(IP4, 0);

lastCommandTime = millis();
}

void backward() {

ledcWrite(IP1, 0);
ledcWrite(IP2, motorSpeed);

ledcWrite(IP3, 0);
ledcWrite(IP4, motorSpeed);

lastCommandTime = millis();
}

void left() {

ledcWrite(IP1, 0);
ledcWrite(IP2, motorSpeed);

ledcWrite(IP3, motorSpeed);
ledcWrite(IP4, 0);

lastCommandTime = millis();
}

void right() {

ledcWrite(IP1, motorSpeed);
ledcWrite(IP2, 0);

ledcWrite(IP3, 0);
ledcWrite(IP4, motorSpeed);

lastCommandTime = millis();
}

void stopCar() {

ledcWrite(IP1, 0);
ledcWrite(IP2, 0);

ledcWrite(IP3, 0);
ledcWrite(IP4, 0);
}

// ======================================================
// SPEED CONTROL
// ======================================================

void handleSpeed() {

if (server.hasArg("value")) {

int newSpeed = server.arg("value").toInt();

newSpeed = constrain(newSpeed, 0, 255);

motorSpeed = newSpeed;

Serial.print("Motor Speed PWM: ");
Serial.println(motorSpeed);

server.send(200, "text/plain", String(motorSpeed));
}
else {

server.send(400, "text/plain", "Missing speed");
}
}

// ======================================================
// GPS WEB API
// ======================================================

void handleGPS() {

String json = "{";

json += "\"fix\":";
json += gpsFix ? "true" : "false";

json += ",\"satellites\":";
json += String(satellites);

json += ",\"latitude\":";
json += String(latitude, 6);

json += ",\"longitude\":";
json += String(longitude, 6);

json += ",\"latitudeText\":\"";
json += latitudeText;
json += "\"";

json += ",\"longitudeText\":\"";
json += longitudeText;
json += "\"";

json += "}";

server.send(200, "application/json", json);
}

// ======================================================
// WEBPAGE
// ======================================================

void handleRoot() {

String html = R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>ESP32 RC PRO</title>

<style>

body {
font-family: Arial;
background: #111;
color: white;
text-align: center;
margin: 0;
padding: 15px;
}

h1 {
margin-bottom: 10px;
}

.status {
padding: 10px;
margin-bottom: 15px;
background: #222;
border-radius: 10px;
}

.gps {
background: #222;
padding: 15px;
border-radius: 12px;
margin-bottom: 15px;
}

.speed {
background: #222;
padding: 15px;
border-radius: 12px;
margin-bottom: 20px;
}

.speedValue {
font-size: 25px;
font-weight: bold;
margin: 10px;
}

input[type=range] {
width: 90%;
height: 40px;
}

.controls {
display: grid;
grid-template-columns: 1fr 1fr 1fr;
gap: 10px;
max-width: 400px;
margin: auto;
}

button {
height: 70px;
font-size: 22px;
font-weight: bold;
border: none;
border-radius: 15px;
background: #333;
color: white;
touch-action: none;
}

button:active {
background: #555;
}

.stop {
background: #b00000;
}

.map {
margin-top: 15px;
padding: 12px 20px;
border-radius: 10px;
background: #333;
color: white;
text-decoration: none;
display: inline-block;
}

.ok { color: #4caf50; }
.bad { color: #ff5252; }

</style>

</head>

<body>

<h1>ESP32 RC PRO</h1>

<div class="status">

<div id="connection">
Connecting...
</div>

</div>

<div class="gps">

<h2>GPS</h2>

<p>
Fix:
<span id="fix">NO</span>
</p>

<p>
Satellites:
<span id="satellites">0</span>
</p>

<p>
Latitude:
<span id="latitude">---</span>
</p>

<p>
Longitude:
<span id="longitude">---</span>
</p>

<a
id="map"
class="map"
href="#"
target="_blank">

Open Map

</a>

</div>

<div class="speed">

<h2>Motor Speed</h2>

<div class="speedValue">

<span id="speedValue">86</span> %

</div>

<input
type="range"
id="speedSlider"
min="0"
max="100"
value="86">

<p>
PWM:
<span id="pwmValue">220</span>
</p>

</div>

<div class="controls">

<div></div>

<button id="forward">
&#9650;
</button>

<div></div>

<button id="left">
&#9664;
</button>

<button id="stop" class="stop">
STOP
</button>

<button id="right">
&#9654;
</button>

<div></div>

<button id="backward">
&#9660;
</button>

<div></div>

</div>

<script>

let activeButton = null;
let commandTimer = null;

const connectionEl = document.getElementById("connection");

function setConnection(ok) {

if (ok) {
connectionEl.innerText = "Connected";
connectionEl.className = "ok";
}
else {
connectionEl.innerText = "Disconnected";
connectionEl.className = "bad";
}

}

function sendCommand(command) {

fetch("/" + command)
.then(() => setConnection(true))
.catch(() => setConnection(false));

}

function startMove(command) {

stopSending();

activeButton = command;

sendCommand(command);

commandTimer = setInterval(function() {

sendCommand(command);

}, 250);

}

function stopSending() {

if (commandTimer !== null) {

clearInterval(commandTimer);

commandTimer = null;

}

activeButton = null;

sendCommand("stop");

}

function setupButton(id, command) {

const button = document.getElementById(id);

button.addEventListener("pointerdown", function(event) {

event.preventDefault();

startMove(command);

});

button.addEventListener("pointerup", function(event) {

event.preventDefault();

stopSending();

});

button.addEventListener("pointercancel", function() {

stopSending();

});

button.addEventListener("pointerleave", function() {

if (activeButton === command)
stopSending();

});

}

setupButton("forward", "forward");
setupButton("backward", "backward");
setupButton("left", "left");
setupButton("right", "right");

document.getElementById("stop").addEventListener("click", function() {

stopSending();

});

// ================= SPEED =================

const speedSlider = document.getElementById("speedSlider");
const speedValue = document.getElementById("speedValue");
const pwmValue = document.getElementById("pwmValue");

speedSlider.addEventListener("input", function() {

let percent = Number(this.value);

let pwm = Math.round(percent * 255 / 100);

speedValue.innerText = percent;

pwmValue.innerText = pwm;

fetch("/speed?value=" + pwm)
.then(() => setConnection(true))
.catch(() => setConnection(false));

});

// ================= GPS =================

function updateGPS() {

fetch("/gps")

.then(response => response.json())

.then(data => {

setConnection(true);

document.getElementById("satellites").innerText = data.satellites;

if (data.fix) {

document.getElementById("fix").innerText = "YES";

document.getElementById("latitude").innerText = data.latitudeText;

document.getElementById("longitude").innerText = data.longitudeText;

document.getElementById("map").href =
"https://www.openstreetmap.org/?mlat="
+ data.latitude
+ "&mlon="
+ data.longitude
+ "#map=18/"
+ data.latitude
+ "/"
+ data.longitude;

}
else {

document.getElementById("fix").innerText = "NO";

document.getElementById("latitude").innerText = "---";

document.getElementById("longitude").innerText = "---";

}

})

.catch(() => {

setConnection(false);

document.getElementById("fix").innerText = "GPS ERROR";

});

}

updateGPS();

setInterval(updateGPS, 5000);

// ================= SAFETY =================

window.addEventListener("blur", function() {

stopSending();

});

document.addEventListener("visibilitychange", function() {

if (document.hidden)
stopSending();

});

</script>

</body>

</html>

)rawliteral";

server.send(200, "text/html", html);
}

// ======================================================
// SETUP
// ======================================================

void setup() {

Serial.begin(115200);

delay(1000);

Serial.println();
Serial.println("================================");
Serial.println(" ESP32 RC PRO");
Serial.println("================================");

// ================= MOTOR =================

pinMode(IP1, OUTPUT);
pinMode(IP2, OUTPUT);
pinMode(IP3, OUTPUT);
pinMode(IP4, OUTPUT);

ledcAttach(IP1, PWM_FREQ, PWM_RESOLUTION);
ledcAttach(IP2, PWM_FREQ, PWM_RESOLUTION);
ledcAttach(IP3, PWM_FREQ, PWM_RESOLUTION);
ledcAttach(IP4, PWM_FREQ, PWM_RESOLUTION);

stopCar();

// ================= GPS =================

GPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);

// ================= WIFI =================

Serial.println();
Serial.println("Connecting to phone hotspot...");
Serial.print("SSID: ");
Serial.println(ssid);

WiFi.mode(WIFI_STA);

WiFi.begin(ssid, password);

int attempts = 0;

while (WiFi.status() != WL_CONNECTED && attempts < 30) {

delay(500);

Serial.print(".");

attempts++;

}

Serial.println();

if (WiFi.status() == WL_CONNECTED) {

Serial.println("WiFi connected!");
Serial.print("IP Address: ");
Serial.println(WiFi.localIP());

Serial.print("Signal RSSI: ");
Serial.print(WiFi.RSSI());
Serial.println(" dBm");

}
else {

Serial.println("WiFi connection FAILED!");
Serial.println("Check hotspot name/password.");

}

// ================= WEB SERVER =================

server.on("/", handleRoot);

server.on("/forward", []() {
forward();
server.send(200, "text/plain", "FORWARD");
});

server.on("/backward", []() {
backward();
server.send(200, "text/plain", "BACKWARD");
});

server.on("/left", []() {
left();
server.send(200, "text/plain", "LEFT");
});

server.on("/right", []() {
right();
server.send(200, "text/plain", "RIGHT");
});

server.on("/stop", []() {
stopCar();
server.send(200, "text/plain", "STOP");
});

server.on("/speed", handleSpeed);

server.on("/gps", handleGPS);

server.begin();

Serial.println();
Serial.println("Web server started");

if (WiFi.status() == WL_CONNECTED) {

Serial.print("Open this address in your phone browser: ");
Serial.println(WiFi.localIP());

}

Serial.println("================================");
}

// ======================================================
// LOOP
// ======================================================

void loop() {

server.handleClient();

readGPS();

// ================= MOTOR SAFETY =================

if (millis() - lastCommandTime > MOTOR_TIMEOUT) {

stopCar();

}

}
