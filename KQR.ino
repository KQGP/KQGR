#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <Servo.h>

#define TRIG_PIN D1
#define ECHO_PIN D2
#define SERVO_PIN D5

const char* ssid = "KQR";
const char* password = "88888888";

Servo myservo;
ESP8266WebServer server(80);

int angle = 0;
int step = 1;
int radarData[180];
unsigned long lastServoMove = 0;
const int servoDelay = 15;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta charset="UTF-8">
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        body {
            font-family: 'Segoe UI', Arial, sans-serif;
            background: #0a0f1e;
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 10px;
        }
        .container {
            max-width: 500px;
            width: 100%;
            background: #141b2b;
            border-radius: 30px;
            padding: 20px;
            box-shadow: 0 20px 40px rgba(0,0,0,0.4);
            border: 1px solid #2a3440;
        }
        .header {
            text-align: center;
            margin-bottom: 20px;
        }
        h1 {
            font-size: 4em;
            font-weight: 800;
            background: linear-gradient(135deg, #00ff88, #00ccff);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            letter-spacing: 4px;
            text-shadow: 0 0 20px rgba(0,255,136,0.3);
        }
        .radar-container {
            position: relative;
            width: 100%;
            aspect-ratio: 2/1;
            margin: 10px 0;
            overflow: hidden;
        }
        canvas {
            width: 100%;
            height: 100%;
            background: #0f1625;
            border-radius: 50%/100% 100% 0 0;
            box-shadow: 0 0 30px rgba(0,255,136,0.1);
            border: 2px solid #1e2a3a;
            border-bottom: none;
        }
        .info-panel {
            background: #0f1625;
            border-radius: 20px;
            padding: 15px;
            margin: 15px 0;
            border: 1px solid #1e2a3a;
        }
        .stats {
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        .stat-item {
            text-align: center;
            flex: 1;
        }
        .stat-label {
            font-size: 0.8em;
            color: #7f8c9f;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .stat-value {
            font-size: 2em;
            font-weight: bold;
            color: #00ff88;
            margin-top: 5px;
        }
        .stat-value.angle {
            color: #00ccff;
        }
        .controls {
            display: flex;
            gap: 10px;
            margin-top: 10px;
        }
        button {
            flex: 1;
            background: #1e2a3a;
            border: 1px solid #2a3440;
            color: white;
            padding: 15px;
            border-radius: 15px;
            font-size: 1em;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.2s;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        button:hover {
            background: #2a3440;
            border-color: #00ff88;
            color: #00ff88;
            transform: translateY(-2px);
        }
        .status-dot {
            display: inline-block;
            width: 10px;
            height: 10px;
            border-radius: 50%;
            background: #00ff88;
            margin-right: 8px;
            box-shadow: 0 0 10px #00ff88;
            animation: pulse 2s infinite;
        }
        @keyframes pulse {
            0% { opacity: 1; }
            50% { opacity: 0.5; }
            100% { opacity: 1; }
        }
        .status-text {
            display: flex;
            align-items: center;
            justify-content: center;
            color: #7f8c9f;
            font-size: 0.9em;
        }
        .range-indicator {
            display: flex;
            justify-content: space-between;
            padding: 0 20px;
            color: #7f8c9f;
            font-size: 0.8em;
            margin-top: 5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>KQR</h1>
            <div class="status-text">
                <span class="status-dot"></span>
                RADAR ACTIVE
            </div>
        </div>
        
        <div class="radar-container">
            <canvas id="radarCanvas" width="400" height="200"></canvas>
        </div>
        <div class="range-indicator">
            <span>0°</span>
            <span>90°</span>
            <span>180°</span>
        </div>
        
        <div class="info-panel">
            <div class="stats">
                <div class="stat-item">
                    <div class="stat-label">ANGLE</div>
                    <div class="stat-value angle" id="currentAngle">0°</div>
                </div>
                <div class="stat-item">
                    <div class="stat-label">DISTANCE</div>
                    <div class="stat-value" id="currentDistance">0cm</div>
                </div>
                <div class="stat-item">
                    <div class="stat-label">MAX RANGE</div>
                    <div class="stat-value" id="maxRange">400cm</div>
                </div>
            </div>
        </div>
        
        <div class="controls">
            <button onclick="setSpeed(20)">SLOW</button>
            <button onclick="setSpeed(10)">MED</button>
            <button onclick="setSpeed(5)">FAST</button>
        </div>
    </div>

    <script>
        let radarData = new Array(180).fill(0);
        let currentAngle = 0;
        let currentDistance = 0;
        const MAX_DISTANCE = 400;
        
        function drawRadar() {
            const canvas = document.getElementById('radarCanvas');
            const ctx = canvas.getContext('2d');
            const width = canvas.width;
            const height = canvas.height;
            const centerX = width / 2;
            const centerY = height;
            const maxRadius = height - 20;
            
            ctx.clearRect(0, 0, width, height);
            
            ctx.strokeStyle = '#1e2a3a';
            ctx.lineWidth = 1;
            
            for (let r = maxRadius/4; r <= maxRadius; r += maxRadius/4) {
                ctx.beginPath();
                ctx.arc(centerX, centerY, r, 0, Math.PI, false);
                ctx.stroke();
            }
            
            for (let angle = 0; angle <= 180; angle += 30) {
                const rad = (angle) * Math.PI / 180;
                const x = centerX - maxRadius * Math.cos(rad);
                const y = centerY - maxRadius * Math.sin(rad);
                
                ctx.beginPath();
                ctx.moveTo(centerX, centerY);
                ctx.lineTo(x, y);
                ctx.stroke();
            }
            
            ctx.fillStyle = '#00ff88';
            ctx.shadowColor = '#00ff88';
            ctx.shadowBlur = 10;
            
            for (let i = 0; i < 180; i++) {
                if (radarData[i] > 0 && radarData[i] < MAX_DISTANCE) {
                    const radius = (radarData[i] / MAX_DISTANCE) * maxRadius;
                    const rad = (i) * Math.PI / 180;
                    const x = centerX - radius * Math.cos(rad);
                    const y = centerY - radius * Math.sin(rad);
                    
                    ctx.beginPath();
                    ctx.arc(x, y, 4, 0, 2 * Math.PI);
                    ctx.fill();
                }
            }
            
            ctx.shadowBlur = 15;
            ctx.strokeStyle = '#00ccff';
            ctx.lineWidth = 2;
            ctx.beginPath();
            const scanRad = (currentAngle) * Math.PI / 180;
            ctx.moveTo(centerX, centerY);
            ctx.lineTo(centerX - maxRadius * Math.cos(scanRad), centerY - maxRadius * Math.sin(scanRad));
            ctx.stroke();
            
            if (currentDistance > 0 && currentDistance < MAX_DISTANCE) {
                ctx.fillStyle = '#ff3366';
                ctx.shadowColor = '#ff3366';
                ctx.beginPath();
                const distRadius = (currentDistance / MAX_DISTANCE) * maxRadius;
                ctx.arc(centerX - distRadius * Math.cos(scanRad), 
                       centerY - distRadius * Math.sin(scanRad), 6, 0, 2 * Math.PI);
                ctx.fill();
            }
        }
        
        function updateData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    radarData = data.radarData;
                    currentAngle = data.currentAngle;
                    currentDistance = data.currentDistance;
                    
                    document.getElementById('currentAngle').innerHTML = currentAngle + '°';
                    document.getElementById('currentDistance').innerHTML = currentDistance + 'cm';
                    
                    drawRadar();
                });
        }
        
        function setSpeed(delay) {
            fetch('/setspeed?delay=' + delay);
        }
        
        setInterval(updateData, 100);
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  myservo.attach(SERVO_PIN);
  myservo.write(0);
  
  WiFi.softAP(ssid, password);
  
  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });
  
  server.on("/data", []() {
    String json = "{";
    json += "\"currentAngle\":" + String(angle) + ",";
    json += "\"currentDistance\":" + String(readUltrasonic()) + ",";
    json += "\"radarData\":[";
    for (int i = 0; i < 180; i++) {
      json += String(radarData[i]);
      if (i < 179) json += ",";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });
  
  server.on("/setspeed", []() {
    if (server.hasArg("delay")) {
    }
    server.send(200, "text/plain", "OK");
  });
  
  server.begin();
  
  for (int i = 0; i < 180; i++) {
    radarData[i] = 0;
  }
}

void loop() {
  server.handleClient();
  
  unsigned long currentMillis = millis();
  if (currentMillis - lastServoMove >= servoDelay) {
    lastServoMove = currentMillis;
    
    myservo.write(angle);
    
    int distance = readUltrasonic();
    radarData[angle] = distance;
    
    angle += step;
    if (angle >= 179 || angle <= 0) {
      step = -step;
    }
  }
}

int readUltrasonic() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  if (duration == 0) {
    return 400;
  }
  
  int distance = duration * 0.034 / 2;
  
  if (distance > 400) {
    distance = 400;
  }
  
  return distance;
}