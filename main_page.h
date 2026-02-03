#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H
#include <pgmspace.h>

const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
<html>
<head>
  <title>Settings</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
camera { position:relative; display:inline-block; }
body {
  font-family: Arial, sans-serif;
  text-align: center;
  background: white;
  color: black;
}
h1 {
  font-size: 1.8em;
  text-align: center;
}
.main-layout {
  display: flex;
  justify-content: center;
  align-items: flex-start;
  gap: 20px;
  flex-wrap: wrap;
}
.settings-table {
  flex: 1 1 300px;
  max-width: 640px;
}
.grid-container {
  display: grid;
  grid-template-areas:
    'header'
    'network'
    'ota'
    'footer';
  gap: 2px;
  background-color: #65b32e;
  border: 1px solid black;
  border: outset;
  border-radius: 10px;
  padding: 5px;
}
.grid-container > div {
  background-color: #f1f1f1;
  border-radius: 10px;
  padding: 2px 20px;
  text-align: center;
}
.item1 {
  grid-area: header;
  text-align: center;
  color: black;
}
.item2 {
  grid-area: network;
}
.item3 {
  grid-area: ota;
}
.item4 { 
  grid-area: footer;
  text-align: center;
  color: black;
}
.container {
  display: grid;
  grid-template-columns: 1fr auto;
  gap: 10px;
  max-width: 300px;
  margin: 20px auto;
  text-align: left;
}
.btn {
  display:inline-block; 
  margin: 20px 5px;
  padding: 8px 16px;
  border-radius: 5px;
  background-color: #2cb84d;
  color: black;
  border: single;
  cursor: pointer;
  text-decoration: none;
}
.btn:hover { background-color: #0078d7; }
.btn:active {
  background-color: #2cb84d;
  box-shadow: 0 5px #366e36;
  transform: translateY(4px);
}
.btn2 {
  display:inline-block; 
  margin: 20px 5px;
  padding: 8px 16px;
  border-radius: 5px;
  background-color: yellow;
  color: black;
  border: single;
  cursor: pointer;
  text-decoration: none;
}
.btn2:hover { background-color: red; }
.btn2:active {
  background-color: lightred;
  color: white;
  box-shadow: 0 5px pink;
  transform: translateY(4px);
}
.label1 { display: block; width: 150px; text-align: left }
.input {
  display: block;
  width: 60px;
  padding: 5px 5px;
  border-radius: 5px;
  text-align: center;
}
.input2 {
  display: block;
  padding: 5px 5px;
  border-radius: 5px;
  text-align: center;
  width: 60px;
}
  </style>
</head>
<body>
  <h1>Settings</h1>
  <div class="grid-container">
    <div class="item1">
      <h1>Wi-Fi Networks</h1>
    </div>        
    <div class="item2">
      <h2>Available Networks</h2>
      <button class="btn" onclick="scanWiFi()">Scan Networks</button>
      <div class="container">
<div id="wifiList" style="margin-top:10px;"></div>
</div>
<p>Select SSID & enter password</p>
<hr>
      <h2>Add new network</h2>
      <div class="container">
        <input id="ssid" placeholder="SSID">
        <input id="pass" placeholder="Password">
        <button class="btn" onclick="addWiFi()">Add</button>
        <button class="btn" onclick="clearWiFi()">Clear All</button>
      </div>
    </div>

    <div class="item3">
      <h2>Firmware Update</h2>
      <hr>
      <br>
      <form method="POST" action="/update" enctype="multipart/form-data">
        <input type="file" name="update">
        <input type="submit" value="Upload & Update">
      </form>
      <br>
    </div>
    <br>
  </div>
  <br>
  <a href="/"><button>Back</button></a>

<script>
function scanWiFi() {
  fetch('/wifi_scan')
    .then(response => response.json())
    .then(data => {
      const list = document.getElementById('wifiList');
      list.innerHTML = '';
      if (!data.networks || data.networks.length === 0) {
        list.innerHTML = '<p>No networks found</p>';
      } else {
        data.networks.forEach(net => {
          const secureIcon = net.secure ? 'Locked' : 'Open';
          const div = document.createElement('div');
          div.style.cursor = 'pointer';
          div.innerHTML = `${net.ssid} (${net.rssi} dBm) ${secureIcon}`;
          div.onclick = () => {
            document.getElementById('ssid').value = net.ssid;
            document.getElementById('pass').focus();
          };
          list.appendChild(div);
        });
      }
    })
    .catch(() => {
      document.getElementById('wifiList').innerHTML = '<p>Error scanning networks</p>';
    });
}

function addWiFi() {
  const ssid = document.getElementById('ssid').value;
  const pass = document.getElementById('pass').value;
  fetch('/wifi_add', {
    method: 'POST',
    headers: {'Content-Type':'application/x-www-form-urlencoded'},
    body: 'ssid='+encodeURIComponent(ssid)+'&password='+encodeURIComponent(pass)
  });
}

function clearWiFi() {
  fetch('/wifi_clear', {method: 'POST'});
}

// Auto-scan on page load
document.addEventListener('DOMContentLoaded', scanWiFi);
</script>
</body>
</html>
)rawliteral";

#endif
