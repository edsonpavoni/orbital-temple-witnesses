#ifndef WEBPAGE_H
#define WEBPAGE_H

const char webPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Heart Sculpture Control</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      padding: 20px;
    }
    .container {
      max-width: 600px;
      margin: 0 auto;
      background: white;
      border-radius: 20px;
      padding: 30px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
    }
    h1 {
      text-align: center;
      color: #333;
      margin-bottom: 10px;
      font-size: 28px;
    }
    .status {
      background: #f0f4ff;
      padding: 15px;
      border-radius: 10px;
      margin-bottom: 20px;
      font-size: 14px;
    }
    .status-row {
      display: flex;
      justify-content: space-between;
      padding: 5px 0;
    }
    .status-label { font-weight: 600; color: #555; }
    .status-value { color: #667eea; font-weight: 700; }
    .btn-group {
      margin-bottom: 20px;
    }
    .btn-group h3 {
      color: #555;
      margin-bottom: 10px;
      font-size: 16px;
    }
    .btn {
      width: 100%;
      padding: 15px;
      margin: 8px 0;
      border: none;
      border-radius: 10px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      transition: all 0.3s;
      color: white;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .btn:active { transform: scale(0.95); }
    .btn-primary { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); }
    .btn-success { background: linear-gradient(135deg, #56ab2f 0%, #a8e063 100%); }
    .btn-danger { background: linear-gradient(135deg, #eb3349 0%, #f45c43 100%); }
    .btn-warning { background: linear-gradient(135deg, #f09819 0%, #edde5d 100%); color: #333; }
    .slider-group {
      margin: 20px 0;
      padding: 15px;
      background: #f8f9fa;
      border-radius: 10px;
    }
    .slider-label {
      display: flex;
      justify-content: space-between;
      margin-bottom: 8px;
      font-weight: 600;
      color: #555;
    }
    input[type="range"] {
      width: 100%;
      height: 8px;
      border-radius: 5px;
      background: #ddd;
      outline: none;
      -webkit-appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 20px;
      height: 20px;
      border-radius: 50%;
      background: #667eea;
      cursor: pointer;
    }
    .btn-row {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>&#10084; Heart Sculpture</h1>

    <div class="status" id="status">
      <div class="status-row">
        <span class="status-label">State:</span>
        <span class="status-value" id="state">IDLE</span>
      </div>
      <div class="status-row">
        <span class="status-label">Position:</span>
        <span class="status-value" id="position">0</span>
      </div>
      <div class="status-row">
        <span class="status-label">Distance:</span>
        <span class="status-value" id="distance">360&deg;</span>
      </div>
      <div class="status-row">
        <span class="status-label">Pause:</span>
        <span class="status-value" id="pause">0ms</span>
      </div>
    </div>

    <div class="btn-group">
      <h3>Main Controls</h3>
      <button class="btn btn-primary" onclick="sendCmd('i')">Initialize (2 Rotations)</button>
      <button class="btn btn-success" onclick="sendCmd('h')">Hold Position</button>
      <button class="btn btn-primary" onclick="sendCmd('l')">Start Loop</button>
      <button class="btn btn-warning" onclick="sendCmd('p')">Power Pull</button>
    </div>

    <div class="btn-group">
      <h3>Stop Controls</h3>
      <div class="btn-row">
        <button class="btn btn-warning" onclick="sendCmd('s')">Stop</button>
        <button class="btn btn-danger" onclick="sendCmd('e')">Emergency Stop</button>
      </div>
    </div>

    <div class="slider-group">
      <div class="slider-label">
        <span>Loop Distance</span>
        <span id="distVal">360&deg;</span>
      </div>
      <input type="range" min="45" max="1440" step="45" value="360" id="distSlider"
             oninput="updateDist(this.value)">
    </div>

    <div class="slider-group">
      <div class="slider-label">
        <span>Pause Time</span>
        <span id="pauseVal">0ms</span>
      </div>
      <input type="range" min="0" max="10000" step="100" value="0" id="pauseSlider"
             oninput="updatePause(this.value)">
    </div>
  </div>

  <script>
    function sendCmd(c) {
      fetch("/cmd?c=" + c)
        .then(r => r.text())
        .then(d => console.log(d))
        .catch(e => console.error(e));
    }

    function updateDist(val) {
      document.getElementById("distVal").innerHTML = val + "&deg;";
      fetch("/setdist?v=" + val);
    }

    function updatePause(val) {
      document.getElementById("pauseVal").innerText = val + "ms";
      fetch("/setpause?v=" + val);
    }

    function updateStatus() {
      fetch("/status")
        .then(r => r.json())
        .then(d => {
          document.getElementById("state").innerText = d.state;
          document.getElementById("position").innerText = d.position;
          document.getElementById("distance").innerHTML = d.distance + "&deg;";
          document.getElementById("pause").innerText = d.pause + "ms";
        })
        .catch(e => console.error(e));
    }

    setInterval(updateStatus, 500);
    updateStatus();
  </script>
</body>
</html>
)rawliteral";

#endif
