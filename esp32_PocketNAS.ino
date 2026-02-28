#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SdFat.h>
#include <ArduinoJson.h>
#include <SPI.h> // Added for explicit SPI routing

// ================= CONFIG =================
const char* ssid     = "SSID_NAME";      // ← CHANGE THIS
const char* password = "SSID_PASSWORD";  // ← CHANGE THIS

// SD SPI Pins (standard for most ESP32 + external module)
#define SD_CS_PIN   5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN  18
#define SD_SPEED    SD_SCK_MHZ(16)  // Lower to 16 if unstable

AsyncWebServer server(80);
SdFat sd;

// Your frontend HTML from Gemini — paste the FULL <html>...</html> here
const char index_html[] PROGMEM = R"rawliteral(
  <!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP32 Pocket NAS</title>
    <style>
        :root {
            --bg-dark: #0f111a;
            --bg-panel: #1e2130;
            --bg-hover: #292d3e;
            --text-main: #e0e0e0;
            --text-muted: #8a91a6;
            --accent: #00d2ff;
            --danger: #ff4b4b;
            --border: #2e3246;
        }
        * { box-sizing: border-box; font-family: 'Segoe UI', system-ui, sans-serif; }
        body { background: var(--bg-dark); color: var(--text-main); margin: 0; padding: 20px; display: flex; justify-content: center; }
        .app-container { width: 100%; max-width: 900px; display: flex; flex-direction: column; gap: 20px; }
        
        /* Header & Controls */
        header { display: flex; justify-content: space-between; align-items: center; padding-bottom: 20px; border-bottom: 1px solid var(--border); }
        h1 { margin: 0; font-size: 1.8rem; font-weight: 600; color: var(--accent); }
        .controls { display: flex; gap: 10px; width: 100%; margin-top: 10px; }
        input[type="text"] { flex: 1; padding: 12px; border-radius: 8px; border: 1px solid var(--border); background: var(--bg-panel); color: white; outline: none; }
        input[type="text"]:focus { border-color: var(--accent); }
        
        /* Drag & Drop Zone */
        #drop-zone { border: 2px dashed var(--border); border-radius: 12px; padding: 30px; text-align: center; color: var(--text-muted); transition: 0.3s; background: var(--bg-panel); cursor: pointer; }
        #drop-zone.dragover { border-color: var(--accent); background: rgba(0, 210, 255, 0.1); color: var(--accent); }
        #upload-progress { width: 100%; height: 8px; border-radius: 4px; display: none; margin-top: 15px; }
        
        /* File List */
        .file-list { background: var(--bg-panel); border-radius: 12px; box-shadow: 0 8px 16px rgba(0,0,0,0.4); overflow: hidden; }
        .file-item { display: flex; align-items: center; justify-content: space-between; padding: 15px 20px; border-bottom: 1px solid var(--border); transition: 0.2s; }
        .file-item:last-child { border-bottom: none; }
        .file-item:hover { background: var(--bg-hover); }
        .file-info { display: flex; align-items: center; gap: 15px; flex: 1; overflow: hidden; }
        .file-icon { font-size: 1.5rem; }
        .file-details { display: flex; flex-direction: column; }
        .file-name { font-weight: 500; cursor: pointer; word-break: break-all; }
        .file-name:hover { color: var(--accent); }
        .file-size { font-size: 0.8rem; color: var(--text-muted); }
        .file-actions button { background: none; border: none; font-size: 1.2rem; cursor: pointer; opacity: 0.7; transition: 0.2s; }
        .file-actions button:hover { opacity: 1; transform: scale(1.1); }
        .btn-delete:hover { color: var(--danger); }

        /* Modal Player */
        .modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.9); justify-content: center; align-items: center; z-index: 1000; }
        .modal-content { width: 90%; max-width: 800px; position: relative; }
        .close-btn { position: absolute; top: -40px; right: 0; color: white; font-size: 2rem; cursor: pointer; background: none; border: none; }
        video, audio, img { width: 100%; border-radius: 8px; outline: none; background: black; }
        audio { height: 50px; }
    </style>
</head>
<body>

<div class="app-container">
    <header>
        <h1>☁️ Pocket NAS</h1>
        <div id="current-path" style="color: var(--text-muted);">/</div>
    </header>

    <div class="controls">
        <input type="text" id="search-bar" placeholder="🔍 Search files..." onkeyup="filterFiles()">
    </div>

    <div id="drop-zone" onclick="document.getElementById('fileInput').click()">
        📁 Drag & Drop files here or click to browse
        <input type="file" id="fileInput" style="display:none" multiple>
        <progress id="upload-progress" value="0" max="100"></progress>
    </div>

    <div class="file-list" id="file-list">
        <div class="file-item" style="justify-content:center; color: var(--text-muted);">Loading system...</div>
    </div>
</div>

<div id="media-modal" class="modal">
    <div class="modal-content">
        <button class="close-btn" onclick="closeModal()">×</button>
        <div id="media-container"></div>
    </div>
</div>

<script>
    let currentDir = "/";
    let allFiles = []; // Cache for search

    // Fetch Files
    async function loadFiles(dir = "/") {
        currentDir = dir;
        document.getElementById('current-path').innerText = "Path: " + dir;
        const container = document.getElementById('file-list');
        container.innerHTML = '<div class="file-item" style="justify-content:center;">Loading...</div>';

        try {
            // Mocking for preview, will use real endpoint on ESP32
            const response = await fetch(`/list?dir=${encodeURIComponent(dir)}`);
            allFiles = await response.json();
            renderFiles(allFiles);
        } catch (error) {
            console.error("Fetch error:", error);
            container.innerHTML = '<div class="file-item" style="justify-content:center; color: var(--danger);">Error loading files. Is the ESP32 online?</div>';
        }
    }

    // Render HTML
    function renderFiles(files) {
        const container = document.getElementById('file-list');
        container.innerHTML = '';
        
        // Add "Up" button if not in root
        if (currentDir !== "/") {
            let parentDir = currentDir.substring(0, currentDir.lastIndexOf('/')) || "/";
            container.innerHTML += `
                <div class="file-item">
                    <div class="file-info">
                        <span class="file-icon">⬅️</span>
                        <div class="file-details"><span class="file-name" onclick="loadFiles('${parentDir}')">.. (Go back)</span></div>
                    </div>
                </div>`;
        }

        if (files.length === 0) {
            container.innerHTML += '<div class="file-item" style="justify-content:center; color: var(--text-muted);">Folder is empty</div>';
            return;
        }

        files.forEach(file => {
            const icon = file.isDir ? '📁' : getIcon(file.name);
            const path = currentDir === "/" ? `/${file.name}` : `${currentDir}/${file.name}`;
            
            container.innerHTML += `
                <div class="file-item" data-name="${file.name.toLowerCase()}">
                    <div class="file-info">
                        <span class="file-icon">${icon}</span>
                        <div class="file-details">
                            <span class="file-name" onclick="${file.isDir ? `loadFiles('${path}')` : `openMedia('${path}')`}">${file.name}</span>
                            <span class="file-size">${file.isDir ? 'Directory' : file.size}</span>
                        </div>
                    </div>
                    <div class="file-actions">
                        ${!file.isDir ? `<button title="Download" onclick="window.open('/stream?file=${encodeURIComponent(path)}', '_blank')">⬇️</button>` : ''}
                        <button class="btn-delete" title="Delete" onclick="deleteFile('${path}')">🗑️</button>
                    </div>
                </div>`;
        });
    }

    // Search filter
    function filterFiles() {
        const query = document.getElementById('search-bar').value.toLowerCase();
        const items = document.querySelectorAll('.file-item[data-name]');
        items.forEach(item => {
            if (item.getAttribute('data-name').includes(query)) {
                item.style.display = 'flex';
            } else {
                item.style.display = 'none';
            }
        });
    }

    // Get Emoji based on extension
    function getIcon(name) {
        if (name.match(/\.(mp4|webm|mkv|avi)$/i)) return '🎞️';
        if (name.match(/\.(mp3|wav|flac|m4a)$/i)) return '🎵';
        if (name.match(/\.(jpg|jpeg|png|gif|svg)$/i)) return '🖼️';
        if (name.match(/\.(pdf|txt|md|csv)$/i)) return '📄';
        return '📦';
    }

    // Media Modal Logic
    function openMedia(path) {
        const modal = document.getElementById('media-modal');
        const container = document.getElementById('media-container');
        const streamUrl = `/stream?file=${encodeURIComponent(path)}`;
        
        container.innerHTML = ''; // Clear previous

        if (path.match(/\.(mp4|webm|mkv)$/i)) {
            container.innerHTML = `<video controls autoplay><source src="${streamUrl}"></video>`;
        } else if (path.match(/\.(mp3|wav|m4a)$/i)) {
            container.innerHTML = `<audio controls autoplay><source src="${streamUrl}"></audio>`;
        } else if (path.match(/\.(jpg|jpeg|png|gif)$/i)) {
            container.innerHTML = `<img src="${streamUrl}" alt="image">`;
        } else {
            window.open(streamUrl, '_blank'); // Download fallback
            return;
        }
        
        modal.style.display = 'flex';
    }

    function closeModal() {
        document.getElementById('media-modal').style.display = 'none';
        document.getElementById('media-container').innerHTML = ''; // Stops playback
    }

    // Drag and Drop Upload Logic
    const dropZone = document.getElementById('drop-zone');
    const fileInput = document.getElementById('fileInput');
    const progressBar = document.getElementById('upload-progress');

    ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
        dropZone.addEventListener(eventName, preventDefaults, false);
    });

    function preventDefaults(e) { e.preventDefault(); e.stopPropagation(); }

    ['dragenter', 'dragover'].forEach(eventName => {
        dropZone.addEventListener(eventName, () => dropZone.classList.add('dragover'), false);
    });

    ['dragleave', 'drop'].forEach(eventName => {
        dropZone.addEventListener(eventName, () => dropZone.classList.remove('dragover'), false);
    });

    dropZone.addEventListener('drop', (e) => handleUpload(e.dataTransfer.files));
    fileInput.addEventListener('change', function() { handleUpload(this.files); });

    function handleUpload(files) {
        if (!files.length) return;
        const formData = new FormData();
        // Append path so backend knows where to save it
        formData.append('path', currentDir);
        formData.append('file', files[0], files[0].name);

        progressBar.style.display = 'block';
        
        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/upload', true);
        
        xhr.upload.onprogress = (e) => {
            if (e.lengthComputable) progressBar.value = (e.loaded / e.total) * 100;
        };

        xhr.onload = () => {
            progressBar.style.display = 'none';
            progressBar.value = 0;
            loadFiles(currentDir);
        };

        xhr.send(formData);
    }

    // Delete
    async function deleteFile(path) {
        if(confirm(`Delete ${path}?`)) {
            await fetch(`/delete?file=${encodeURIComponent(path)}`, { method: 'DELETE' });
            loadFiles(currentDir);
        }
    }

    // Init
    window.onload = () => loadFiles("/");
</script>

</body>
</html>

  )rawliteral";

// Helpers
String formatSize(uint64_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  if (bytes < 1048576) return String(bytes / 1024.0, 1) + " KB";
  if (bytes < 1073741824ULL) return String(bytes / 1048576.0, 1) + " MB";
  return String(bytes / 1073741824.0, 1) + " GB";
}

String getMime(const String& fn) {
  if (fn.endsWith(".mp4")) return "video/mp4";
  if (fn.endsWith(".mp3")) return "audio/mpeg";
  if (fn.endsWith(".jpg") || fn.endsWith(".jpeg")) return "image/jpeg";
  if (fn.endsWith(".png")) return "image/png";
  return "application/octet-stream";
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting ESP32 Pocket NAS...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  // FIX 1: Manually route SPI pins and start SD safely
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!sd.begin(SdSpiConfig(SD_CS_PIN, SHARED_SPI, SD_SPEED))) {
    Serial.println("SD mount failed! Check wiring/card.");
    while(true) delay(1000);
  }
  Serial.println("SD ready.");

  // Serve UI at root
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  // GET /list?dir=/subfolder/
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    String dir = "/";
    if (request->hasParam("dir")) dir = request->getParam("dir")->value();
    if (!dir.startsWith("/")) dir = "/" + dir;
    if (!dir.endsWith("/")) dir += "/";

    FsFile root = sd.open(dir.c_str());
    if (!root || !root.isDirectory()) {
      request->send(404, "application/json", "[]");
      return;
    }

    DynamicJsonDocument doc(8192);
    JsonArray arr = doc.to<JsonArray>();

    FsFile entry;
    while ((entry = root.openNextFile())) {
      // FIX 2: Safely read file names for newer SdFat versions
      char nameBuf[256];
      entry.getName(nameBuf, sizeof(nameBuf));
      String name = String(nameBuf);
      
      // Skip hidden files or folder markers
      if (name == "" || name == "." || name == "..") {
        entry.close();
        continue;
      }

      JsonObject obj = arr.createNestedObject();
      obj["name"] = name;
      obj["isDir"] = entry.isDirectory();
      if (!entry.isDirectory()) obj["size"] = formatSize(entry.size());
      
      entry.close();
    }
    root.close();

    String json;
    serializeJson(arr, json);
    request->send(200, "application/json", json);
  });

  // POST /upload?path=/sub/
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    static FsFile file;
    static String path;

    if (!index) {
      String p = "/";
      if (request->hasParam("path")) p = request->getParam("path")->value();
      if (!p.startsWith("/")) p = "/" + p;
      if (!p.endsWith("/")) p += "/";
      path = p + (filename != "" ? filename : "file_" + String(millis()));
      if (sd.exists(path.c_str())) sd.remove(path.c_str());
      file = sd.open(path.c_str(), O_WRITE | O_CREAT | O_TRUNC);
    }
    if (file) file.write(data, len);
    if (final && file) file.close();
  });

  // GET /stream?file=/path/file.mp4
  server.on("/stream", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("file")) return request->send(400, "text/plain", "Missing file");

    String path = request->getParam("file")->value();
    if (!path.startsWith("/")) path = "/" + path;

    // FIX 3: ALLOCATE ON HEAP to prevent async crash
    FsFile* f = new FsFile(sd.open(path.c_str(), O_RDONLY));
    if (!*f || !f->isOpen()) {
      delete f; 
      return request->send(404, "text/plain", "Not found");
    }

    String mime = getMime(path);
    uint64_t size = f->size(), start = 0, end = size - 1;
    bool range = false;

    const AsyncWebHeader* header = request->getHeader("Range");
    if (header) {
      String r = header->value();
      if (r.startsWith("bytes=")) {
        r = r.substring(6);
        int dash = r.indexOf('-');
        if (dash >= 0) {
          start = r.substring(0, dash).toInt();
          end = r.substring(dash + 1).length() ? r.substring(dash + 1).toInt() : size - 1;
          if (start < size && end < size && start <= end) {
            range = true;
            f->seek(start);
          } else {
            f->close();
            delete f;
            auto resp = request->beginResponse(416);
            resp->addHeader("Content-Range", "bytes */" + String(size));
            return request->send(resp);
          }
        }
      }
    }

    auto resp = request->beginChunkedResponse(mime.c_str(), [f, start, end, size](uint8_t *buf, size_t max, size_t idx) mutable -> size_t {
      uint64_t rem = (end - start + 1) - idx;
      if (rem == 0 || !f->available()) { 
        f->close(); 
        delete f; 
        return 0; 
      }
      size_t rd = min(max, (size_t)rem);
      size_t got = f->read(buf, rd);
      if (got == 0) { 
        f->close(); 
        delete f; 
      }
      return got;
    });

    resp->addHeader("Accept-Ranges", "bytes");
    if (range) {
      resp->setCode(206);
      resp->addHeader("Content-Range", "bytes " + String(start) + "-" + String(end) + "/" + String(size));
      resp->addHeader("Content-Length", String(end - start + 1));
    } else {
      resp->addHeader("Content-Length", String(size));
    }
    request->send(resp);
  });

  // DELETE /delete?file=/path/file
  server.on("/delete", HTTP_DELETE, [](AsyncWebServerRequest *request){
    if (!request->hasParam("file")) return request->send(400);

    String path = request->getParam("file")->value();
    if (!path.startsWith("/")) path = "/" + path;

    if (sd.remove(path.c_str())) request->send(200, "text/plain", "Deleted");
    else request->send(500, "text/plain", "Failed");
  });

  server.begin();
  Serial.println("Ready! Open http://" + WiFi.localIP().toString());
}

void loop() {}