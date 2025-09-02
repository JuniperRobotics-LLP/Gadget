const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
<meta name='viewport' content='width=device-width, initial-scale=1.0, user-scalable=no' />
<title>Gadget Control Panel</title>
<style> 
body {
    margin: 0;
    padding: 0;
    font-family: sans-serif;
    display: flex;
    flex-direction: column;
    align-items: center;
}
#stream-container {
     display: flex;
     justify-content: center;
     width: 100%;
     max-width: 640px;
     margin: 0 auto;
}

#stream-container img {
     display: block;
     max-width: 100%;
     height: auto;
     border-radius: 4px;
}

#coords_display {
     width: 100%;
     max-width: 640px;
     background: #fff;
     padding: 6px;
     margin-top: 8px;
     border-radius: 4px;
     text-align: center;
     font-weight: bold;
     border: 1px solid #ccc;
}

.text_box {
     width: 100%;
     max-width: 640px;
     padding: 6px;
     border-radius: 4px;
     margin-top: 0;
     text-align: center;
     font-weight: bold;
     margin: 0;
}

.text_box_title {
     width: 100%;
     max-width: 640px;
     background: #eee;
     padding: 6px;
     margin-top: 16px;
     border-radius: 4px;
     text-align: center;
     font-weight: bold;
     text-decoration: underline;
}
</style>
</head>
<body>

<!-- Face coordinates  -->
<p id="coords_display"></p>

<div id="stream-container" class="image-container">
   <img id="stream" src="">
</div>



<!-- Three text boxes stacked below the coordinates -->
<p id="text_0" class="text_box_title"></p>
<p id="text_1" class="text_box"></p>
<p id="text_2" class="text_box"></p>

<script>
const view = document.getElementById('stream');
const WS_URL = "ws://" + window.location.host + ":82";
const ws = new WebSocket(WS_URL);

ws.onmessage = message => {
  if (message.data instanceof Blob) {
    const urlObject = URL.createObjectURL(message.data);
    view.src = urlObject;
  } else if (typeof message.data === "string") {
    if (message.data.startsWith("coords:")) {
      const data = message.data.substring(7);
      if(data === "none"){
        document.getElementById('coords_display').innerText = `No Face Detected`;
      } else {
        const coords = data.split(",");
        document.getElementById('coords_display').innerText = `Face Center: X=${coords[0]}, Y=${coords[1]}`;
      }
    } else if (message.data.startsWith("text_0:")) {
      document.getElementById('text_0').innerText = message.data.substring(7);
    } else if (message.data.startsWith("text_1:")) {
      document.getElementById('text_1').innerText = "Dead Zone: " + message.data.substring(7);
    } else if (message.data.startsWith("text_2:")) {
      document.getElementById('text_2').innerText = "Rotation Speed: " + message.data.substring(7);
    }
  }
};
</script>

</body>
</html>
)rawliteral";
