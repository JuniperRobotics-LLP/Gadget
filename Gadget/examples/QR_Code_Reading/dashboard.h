const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=no' />
  <title>Gadget Control Panel</title>
  <style>
    body {
      font-family: sans-serif;
      text-align: center;
      padding-top: 30px;
    }
    h1 {
      font-size: 36px;
      font-weight: bold;
      margin-bottom: 20px;
      text-decoration: underline;
    }
    #qrResult,
    #operationStatus {
      margin-top: 20px;
      font-size: 24px;
      font-weight: bold;
      color: #333;
    }
    .text_box {
      width: 100%;
      max-width: 640px;
      background: #eee;
      padding: 6px;
      margin-top: 12px;
      border-radius: 4px;
      text-align: center;
      font-weight: bold;
      font-size: 20px;
    }
  </style>
</head>
<body>

  <h1>QR Code Reading</h1>

  <div style="margin-top: 40px;">
    <div id="qrResult">Last QR Code Detected: None</div>
  </div>
  
  <div id="operationStatus">Status: Idle</div>

  <div style="margin-top: 20px;">
    <p id="text_0" class="text_box">--</p>
    <p id="text_1" class="text_box">--</p>
    <p id="text_2" class="text_box">--</p>
  </div>

  <script>
    const WS_URL = "ws://" + window.location.host + ":82";
    const ws = new WebSocket(WS_URL);

    ws.onmessage = message => {
      const msg = message.data;
      if (msg.startsWith("qr:")) {
        const payload = msg.substring(3);
        document.getElementById("qrResult").textContent =
          "Last QR Code Detected: " + payload;
      } else if (msg.startsWith("op:")) {
        const op = msg.substring(3);
        document.getElementById("operationStatus").textContent =
          "Status: " + op;
      } else if (msg.startsWith("text_0:")) {
        const payload = msg.substring(7);
        document.getElementById("text_0").textContent = payload;
      } else if (msg.startsWith("text_1:")) {
        const payload = msg.substring(7);
        document.getElementById("text_1").textContent =
          "Turn Duration: " + payload;
      } else if (msg.startsWith("text_2:")) {
        const payload = msg.substring(7);
        document.getElementById("text_2").textContent =
          "Rotation Speed: " + payload;
      }
    };
  </script>
</body>
</html>
)rawliteral";
