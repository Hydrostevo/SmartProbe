#ifndef SD_BROWSER_PAGE_H
#define SD_BROWSER_PAGE_H
#include <pgmspace.h>

const char SD_BROWSER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>SD Card Browser - Smart Probe</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css">
  <style>
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background: white;
      color: black;
      margin: 0;
      padding: 20px;
    }
    h1 {
      font-size: 1.8em;
      color: #333;
    }
    .container {
      max-width: 900px;
      margin: 0 auto;
      background: #f9f9f9;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    }
    .sd-info {
      background: #e8f5e9;
      padding: 15px;
      border-radius: 8px;
      margin-bottom: 20px;
      border: 1px solid #4caf50;
    }
    .sd-info h3 {
      margin-top: 0;
      color: #2e7d32;
    }
    .stats {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
      gap: 10px;
      margin-top: 10px;
    }
    .stat-box {
      background: white;
      padding: 10px;
      border-radius: 5px;
      border: 1px solid #a5d6a7;
    }
    .stat-label {
      font-size: 0.9em;
      color: #666;
    }
    .stat-value {
      font-size: 1.3em;
      font-weight: bold;
      color: #2e7d32;
    }
    .controls {
      margin: 20px 0;
      display: flex;
      gap: 10px;
      justify-content: center;
      flex-wrap: wrap;
    }
    .btn {
      padding: 10px 20px;
      border-radius: 5px;
      background-color: #2cb84d;
      color: white;
      border: none;
      cursor: pointer;
      font-size: 1em;
      transition: all 0.3s;
    }
    .btn:hover {
      background-color: #0078d7;
      transform: translateY(-2px);
    }
    .btn:active {
      transform: translateY(0);
    }
    .btn-danger {
      background-color: #f44336;
    }
    .btn-danger:hover {
      background-color: #d32f2f;
    }
    .btn-secondary {
      background-color: #666;
    }
    .btn-secondary:hover {
      background-color: #444;
    }
    .file-list {
      background: white;
      border-radius: 8px;
      margin-top: 20px;
      max-height: 500px;
      overflow-y: auto;
      border: 1px solid #ddd;
    }
    .file-item {
      display: grid;
      grid-template-columns: 40px 1fr 150px 120px 200px;
      align-items: center;
      padding: 15px;
      border-bottom: 1px solid #eee;
      transition: background 0.2s;
    }
    .file-item:hover {
      background: #f5f5f5;
    }
    .file-item:last-child {
      border-bottom: none;
    }
    .file-checkbox {
      width: 20px;
      height: 20px;
      cursor: pointer;
    }
    .file-name {
      text-align: left;
      font-weight: 500;
      color: #333;
      word-break: break-all;
    }
    .file-size {
      color: #666;
      font-size: 0.9em;
    }
    .file-date {
      color: #999;
      font-size: 0.85em;
    }
    .file-actions {
      display: flex;
      gap: 5px;
      justify-content: flex-end;
    }
    .action-btn {
      padding: 5px 12px;
      border-radius: 4px;
      border: none;
      cursor: pointer;
      font-size: 0.9em;
      transition: all 0.2s;
    }
    .view-btn {
      background: #2196F3;
      color: white;
    }
    .view-btn:hover {
      background: #1976D2;
    }
    .download-btn {
      background: #4CAF50;
      color: white;
    }
    .download-btn:hover {
      background: #45a049;
    }
    .delete-btn {
      background: #f44336;
      color: white;
    }
    .delete-btn:hover {
      background: #da190b;
    }
    .empty-state {
      padding: 60px 20px;
      color: #999;
      font-size: 1.1em;
    }
    .empty-state i {
      font-size: 3em;
      margin-bottom: 20px;
      color: #ccc;
    }
    .loading {
      padding: 40px;
      color: #666;
    }
    .spinner {
      border: 4px solid #f3f3f3;
      border-top: 4px solid #2cb84d;
      border-radius: 50%;
      width: 40px;
      height: 40px;
      animation: spin 1s linear infinite;
      margin: 20px auto;
    }
    @keyframes spin {
      0% { transform: rotate(0deg); }
      100% { transform: rotate(360deg); }
    }
    .modal {
      display: none;
      position: fixed;
      z-index: 1000;
      left: 0;
      top: 0;
      width: 100%;
      height: 100%;
      background-color: rgba(0,0,0,0.8);
      justify-content: center;
      align-items: center;
    }
    .modal-content {
      max-width: 90%;
      max-height: 90%;
    }
    .modal-content img {
      max-width: 100%;
      max-height: 90vh;
      border-radius: 8px;
    }
    .close-modal {
      position: absolute;
      top: 20px;
      right: 40px;
      color: white;
      font-size: 40px;
      cursor: pointer;
      z-index: 1001;
    }
    .select-all-container {
      background: #f0f0f0;
      padding: 10px 15px;
      border-bottom: 2px solid #ddd;
      display: flex;
      align-items: center;
      gap: 10px;
    }
    .progress-bar {
      display: none;
      width: 100%;
      height: 30px;
      background: #f0f0f0;
      border-radius: 5px;
      overflow: hidden;
      margin: 20px 0;
    }
    .progress-fill {
      height: 100%;
      background: linear-gradient(90deg, #2cb84d, #4CAF50);
      width: 0%;
      transition: width 0.3s;
      display: flex;
      align-items: center;
      justify-content: center;
      color: white;
      font-weight: bold;
    }
  </style>
</head>
<body>
  <h1>SD Card Browser</h1>
  
  <div class="container">
    <div class="sd-info">
      <h3>Storage Information</h3>
      <div class="stats">
        <div class="stat-box">
          <div class="stat-label">Total Images</div>
          <div class="stat-value" id="total-images">-</div>
        </div>
        <div class="stat-box">
          <div class="stat-label">Total Size</div>
          <div class="stat-value" id="total-size">-</div>
        </div>
        <div class="stat-box">
          <div class="stat-label">Free Space</div>
          <div class="stat-value" id="free-space">-</div>
        </div>
        <div class="stat-box">
          <div class="stat-label">Used Space</div>
          <div class="stat-value" id="used-space">-</div>
        </div>
      </div>
    </div>

    <div class="controls">
      <button class="btn" onclick="refreshFiles()">
        <i class="fas fa-sync"></i> Refresh
      </button>
      <button class="btn btn-secondary" onclick="downloadSelected()">
        <i class="fas fa-download"></i> Download Selected
      </button>
      <button class="btn btn-danger" onclick="deleteSelected()">
        <i class="fas fa-trash"></i> Delete Selected
      </button>
      <button class="btn btn-secondary" onclick="location.href='/'">
        <i class="fas fa-home"></i> Back to Home
      </button>
    </div>

    <div class="progress-bar" id="progress-bar">
      <div class="progress-fill" id="progress-fill">0%</div>
    </div>

    <div class="file-list" id="file-list">
      <div class="loading">
        <div class="spinner"></div>
        Loading files...
      </div>
    </div>
  </div>

  <!-- Image Modal -->
  <div id="imageModal" class="modal" onclick="closeModal()">
    <span class="close-modal">&times;</span>
    <div class="modal-content">
      <img id="modalImage" src="" alt="Preview">
    </div>
  </div>

  <script>
    let allFiles = [];
    
    window.onload = function() {
      loadSDStatus();
      loadFiles();
    };

    function loadSDStatus() {
      fetch('/sd_status')
        .then(response => response.json())
        .then(data => {
          document.getElementById('total-images').textContent = data.imageCount || 0;
          document.getElementById('total-size').textContent = (data.totalMB || 0) + ' MB';
          document.getElementById('free-space').textContent = (data.freeMB || 0) + ' MB';
          document.getElementById('used-space').textContent = (data.usedMB || 0) + ' MB';
        })
        .catch(error => console.error('Error loading SD status:', error));
    }

    function loadFiles() {
      document.getElementById('file-list').innerHTML = '<div class="loading"><div class="spinner"></div>Loading files...</div>';
      
      fetch('/sd_list')
        .then(response => response.json())
        .then(data => {
          allFiles = data.files || [];
          displayFiles();
        })
        .catch(error => {
          console.error('Error loading files:', error);
          document.getElementById('file-list').innerHTML = 
            '<div class="empty-state"><i class="fas fa-exclamation-circle"></i><br>Error loading files</div>';
        });
    }

    function displayFiles() {
      const fileList = document.getElementById('file-list');
      
      if (allFiles.length === 0) {
        fileList.innerHTML = '<div class="empty-state"><i class="fas fa-folder-open"></i><br>No images found on SD card</div>';
        return;
      }

      let html = '<div class="select-all-container">';
      html += '<input type="checkbox" id="select-all" onchange="toggleSelectAll()" class="file-checkbox">';
      html += '<label for="select-all"><strong>Select All (' + allFiles.length + ' files)</strong></label>';
      html += '</div>';

      allFiles.forEach((file, index) => {
        const sizeKB = (file.size / 1024).toFixed(1);
        html += '<div class="file-item">';
        html += '<input type="checkbox" class="file-checkbox" id="file-' + index + '" data-filename="' + file.name + '">';
        html += '<div class="file-name">' + file.name + '</div>';
        html += '<div class="file-size">' + sizeKB + ' KB</div>';
        html += '<div class="file-date">' + (file.date || 'Unknown') + '</div>';
        html += '<div class="file-actions">';
        html += '<button class="action-btn view-btn" onclick="viewImage(\'' + file.name + '\')"><i class="fas fa-eye"></i> View</button>';
        html += '<button class="action-btn download-btn" onclick="downloadFile(\'' + file.name + '\')"><i class="fas fa-download"></i></button>';
        html += '<button class="action-btn delete-btn" onclick="deleteFile(\'' + file.name + '\')"><i class="fas fa-trash"></i></button>';
        html += '</div>';
        html += '</div>';
      });

      fileList.innerHTML = html;
    }

    function toggleSelectAll() {
      const selectAll = document.getElementById('select-all').checked;
      const checkboxes = document.querySelectorAll('.file-item .file-checkbox');
      checkboxes.forEach(cb => cb.checked = selectAll);
    }

    function getSelectedFiles() {
      const checkboxes = document.querySelectorAll('.file-item .file-checkbox:checked');
      return Array.from(checkboxes).map(cb => cb.dataset.filename);
    }

    function viewImage(filename) {
      const modal = document.getElementById('imageModal');
      const img = document.getElementById('modalImage');
      img.src = '/sd_download?file=' + encodeURIComponent(filename);
      modal.style.display = 'flex';
    }

    function closeModal() {
      document.getElementById('imageModal').style.display = 'none';
    }

    function downloadFile(filename) {
      window.location.href = '/sd_download?file=' + encodeURIComponent(filename);
    }

    function downloadSelected() {
      const selected = getSelectedFiles();
      if (selected.length === 0) {
        alert('Please select at least one file to download');
        return;
      }
      
      if (selected.length === 1) {
        downloadFile(selected[0]);
      } else {
        alert('Downloading ' + selected.length + ' files...\nFiles will download one at a time.');
        selected.forEach((filename, index) => {
          setTimeout(() => downloadFile(filename), index * 500);
        });
      }
    }

    function deleteFile(filename) {
      if (!confirm('Are you sure you want to delete "' + filename + '"?')) {
        return;
      }

      fetch('/sd_delete', {
        method: 'POST',
        headers: {'Content-Type': 'application/x-www-form-urlencoded'},
        body: 'file=' + encodeURIComponent(filename)
      })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          alert('File deleted successfully');
          refreshFiles();
        } else {
          alert('Error deleting file: ' + (data.error || 'Unknown error'));
        }
      })
      .catch(error => {
        console.error('Error:', error);
        alert('Error deleting file');
      });
    }

    function deleteSelected() {
      const selected = getSelectedFiles();
      if (selected.length === 0) {
        alert('Please select at least one file to delete');
        return;
      }

      if (!confirm('Are you sure you want to delete ' + selected.length + ' file(s)?\nThis cannot be undone!')) {
        return;
      }

      const progressBar = document.getElementById('progress-bar');
      const progressFill = document.getElementById('progress-fill');
      progressBar.style.display = 'block';
      
      let deleted = 0;
      
      selected.forEach((filename, index) => {
        fetch('/sd_delete', {
          method: 'POST',
          headers: {'Content-Type': 'application/x-www-form-urlencoded'},
          body: 'file=' + encodeURIComponent(filename)
        })
        .then(response => response.json())
        .then(data => {
          deleted++;
          const progress = Math.round((deleted / selected.length) * 100);
          progressFill.style.width = progress + '%';
          progressFill.textContent = progress + '%';
          
          if (deleted === selected.length) {
            setTimeout(() => {
              progressBar.style.display = 'none';
              alert('Deleted ' + deleted + ' file(s)');
              refreshFiles();
            }, 500);
          }
        })
        .catch(error => {
          console.error('Error deleting ' + filename + ':', error);
        });
      });
    }

    function refreshFiles() {
      loadSDStatus();
      loadFiles();
    }
  </script>
</body>
</html>
)rawliteral";

#endif
