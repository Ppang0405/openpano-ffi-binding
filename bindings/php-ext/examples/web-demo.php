<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OpenPano - Panorama Stitcher</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            padding: 2rem;
        }
        .container {
            max-width: 900px;
            margin: 0 auto;
            background: white;
            border-radius: 20px;
            padding: 3rem;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        h1 {
            color: #667eea;
            margin-bottom: 0.5rem;
            font-size: 2.5rem;
        }
        .subtitle {
            color: #666;
            margin-bottom: 2rem;
        }
        .upload-area {
            border: 3px dashed #667eea;
            border-radius: 10px;
            padding: 3rem;
            text-align: center;
            cursor: pointer;
            transition: all 0.3s;
            background: #f8f9ff;
        }
        .upload-area:hover {
            border-color: #764ba2;
            background: #fff;
        }
        .upload-icon {
            font-size: 4rem;
            color: #667eea;
            margin-bottom: 1rem;
        }
        input[type="file"] {
            display: none;
        }
        button {
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            padding: 1rem 2rem;
            font-size: 1rem;
            border-radius: 10px;
            cursor: pointer;
            margin-top: 1rem;
            width: 100%;
            font-weight: bold;
            transition: transform 0.2s;
        }
        button:hover { transform: translateY(-2px); }
        button:disabled {
            opacity: 0.5;
            cursor: not-allowed;
        }
        .preview {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
            gap: 1rem;
            margin-top: 2rem;
        }
        .preview img {
            width: 100%;
            height: 150px;
            object-fit: cover;
            border-radius: 10px;
            box-shadow: 0 4px 6px rgba(0,0,0,0.1);
        }
        .result {
            margin-top: 2rem;
            text-align: center;
        }
        .result img {
            max-width: 100%;
            border-radius: 10px;
            box-shadow: 0 10px 30px rgba(0,0,0,0.2);
        }
        .status {
            padding: 1rem;
            border-radius: 10px;
            margin-top: 1rem;
            font-weight: 500;
        }
        .status.success {
            background: #d4edda;
            color: #155724;
            border: 1px solid #c3e6cb;
        }
        .status.error {
            background: #f8d7da;
            color: #721c24;
            border: 1px solid #f5c6cb;
        }
        .status.processing {
            background: #fff3cd;
            color: #856404;
            border: 1px solid #ffeaa7;
        }
        .loading-overlay {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(102, 126, 234, 0.95);
            z-index: 9999;
            justify-content: center;
            align-items: center;
            flex-direction: column;
            opacity: 0;
            visibility: hidden;
            transition: opacity 0.3s, visibility 0s 0.3s;
        }
        .loading-overlay.active {
            display: flex;
            opacity: 1;
            visibility: visible;
            transition: opacity 0.3s, visibility 0s;
        }
        /* Show spinner immediately on form submission */
        body.processing .loading-overlay {
            display: flex;
            opacity: 1;
            visibility: visible;
        }
        .spinner {
            width: 80px;
            height: 80px;
            border: 8px solid rgba(255, 255, 255, 0.3);
            border-top: 8px solid white;
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .loading-text {
            color: white;
            font-size: 1.5rem;
            margin-top: 2rem;
            font-weight: bold;
        }
        .loading-subtext {
            color: rgba(255, 255, 255, 0.8);
            font-size: 1rem;
            margin-top: 0.5rem;
        }
    </style>
</head>
<body>
    <!-- Loading Overlay -->
    <div class="loading-overlay" id="loadingOverlay">
        <div class="spinner"></div>
        <div class="loading-text">Creating Your Panorama</div>
        <div class="loading-subtext">This may take a few seconds...</div>
    </div>

    <div class="container">
        <h1>🌄 OpenPano Stitcher</h1>
        <p class="subtitle">Upload 2+ photos to create a panorama</p>
        <div style="background: #e3f2fd; padding: 1rem; border-radius: 10px; margin-bottom: 1rem; font-size: 0.9rem;">
            <strong>💡 Tips for best results:</strong>
            <ul style="margin: 0.5rem 0 0 1.5rem;">
                <li>Take photos with 30-50% overlap between adjacent images</li>
                <li>Keep the camera level and rotate in one direction</li>
                <li>Use good lighting and avoid motion blur</li>
                <li>Keep the same exposure settings across all photos</li>
            </ul>
        </div>

        <form method="POST" enctype="multipart/form-data" id="uploadForm">
            <div class="upload-area" onclick="document.getElementById('fileInput').click()">
                <div class="upload-icon">📸</div>
                <h3>Click to select images</h3>
                <p>or drag and drop multiple photos here</p>
                <input type="file" id="fileInput" name="images[]" multiple accept="image/*" onchange="previewFiles()">
            </div>

            <div class="preview" id="preview"></div>

            <button type="submit" id="submitBtn" disabled>Create Panorama</button>
        </form>

        <?php
        if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_FILES['images'])) {
            // Processing happens here - no need for status message, results will show below
            $uploadDir = sys_get_temp_dir() . '/panorama_' . uniqid();
            mkdir($uploadDir, 0755, true);
            
            try {
                $imagePaths = [];
                foreach ($_FILES['images']['tmp_name'] as $key => $tmpName) {
                    if ($_FILES['images']['error'][$key] === UPLOAD_ERR_OK) {
                        $filename = $uploadDir . '/' . basename($_FILES['images']['name'][$key]);
                        move_uploaded_file($tmpName, $filename);
                        $imagePaths[] = $filename;
                    }
                }
                
                if (count($imagePaths) < 2) {
                    throw new Exception('Please upload at least 2 images');
                }
                
                $outputPath = $uploadDir . '/panorama.jpg';
                $startTime = microtime(true);
                
                // Use the OpenPano extension
                openpano_stitch_images($imagePaths, $outputPath, 90, true);
                
                $duration = round(microtime(true) - $startTime, 2);
                
                // Convert to base64 for display
                $imageData = base64_encode(file_get_contents($outputPath));
                
                echo '<div class="status success">✅ Panorama created in ' . $duration . ' seconds!</div>';
                echo '<div class="result">';
                echo '<h3>Your Panorama:</h3>';
                echo '<img src="data:image/jpeg;base64,' . $imageData . '" alt="Panorama">';
                echo '<p style="margin-top: 1rem;"><a href="data:image/jpeg;base64,' . $imageData . '" download="panorama.jpg">Download</a></p>';
                echo '</div>';
                
                // Cleanup
                array_map('unlink', $imagePaths);
                unlink($outputPath);
                rmdir($uploadDir);
                
            } catch (Exception $e) {
                echo '<div class="status error">❌ Error: ' . htmlspecialchars($e->getMessage()) . '</div>';
                
                // Cleanup on error
                if (isset($imagePaths)) {
                    foreach ($imagePaths as $path) {
                        if (file_exists($path)) unlink($path);
                    }
                }
                if (isset($outputPath) && file_exists($outputPath)) {
                    unlink($outputPath);
                }
                if (is_dir($uploadDir)) {
                    rmdir($uploadDir);
                }
            }
        }
        ?>
    </div>

    <script>
        // Check if we have results - if so, clear processing state immediately
        <?php if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_FILES['images'])): ?>
        // Results are being shown, clear localStorage immediately
        localStorage.removeItem('panoramaProcessing');
        localStorage.removeItem('panoramaImageCount');
        <?php else: ?>
        // No results yet, check if we're processing
        if (localStorage.getItem('panoramaProcessing') === 'true') {
            // Add class IMMEDIATELY, even before DOM loads
            document.documentElement.classList.add('processing');
        }
        <?php endif; ?>
        
        window.addEventListener('DOMContentLoaded', function() {
            <?php if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_FILES['images'])): ?>
            // Ensure spinner is hidden when results are shown
            document.body.classList.remove('processing');
            document.documentElement.classList.remove('processing');
            document.getElementById('loadingOverlay').classList.remove('active');
            <?php else: ?>
            // Show spinner if we're still processing
            if (localStorage.getItem('panoramaProcessing') === 'true') {
                document.body.classList.add('processing');
                const overlay = document.getElementById('loadingOverlay');
                const imageCount = localStorage.getItem('panoramaImageCount') || '?';
                const loadingText = overlay.querySelector('.loading-subtext');
                loadingText.textContent = `Processing ${imageCount} images... This may take a few seconds.`;
                overlay.classList.add('active');
            }
            <?php endif; ?>
        });

        // Show loading overlay on form submit
        document.getElementById('uploadForm').addEventListener('submit', function(e) {
            const files = document.getElementById('fileInput').files;
            if (files.length >= 2) {
                // IMMEDIATELY add processing class to body (CSS will show spinner instantly)
                document.body.classList.add('processing');
                
                // Store processing state in localStorage
                localStorage.setItem('panoramaProcessing', 'true');
                localStorage.setItem('panoramaImageCount', files.length);
                
                const overlay = document.getElementById('loadingOverlay');
                const loadingText = overlay.querySelector('.loading-subtext');
                
                // Update loading text with number of images
                loadingText.textContent = `Processing ${files.length} images... This may take a few seconds.`;
                overlay.classList.add('active');
                
                // Update button to show it's processing
                const submitBtn = document.getElementById('submitBtn');
                submitBtn.disabled = true;
                submitBtn.textContent = 'Processing...';
            }
        });

        function previewFiles() {
            const preview = document.getElementById('preview');
            const files = document.getElementById('fileInput').files;
            const submitBtn = document.getElementById('submitBtn');
            
            preview.innerHTML = '';
            
            if (files.length >= 2) {
                submitBtn.disabled = false;
                
                Array.from(files).forEach(file => {
                    const reader = new FileReader();
                    reader.onload = (e) => {
                        const img = document.createElement('img');
                        img.src = e.target.result;
                        preview.appendChild(img);
                    };
                    reader.readAsDataURL(file);
                });
            } else {
                submitBtn.disabled = true;
            }
        }
        
        // Drag and drop support
        const uploadArea = document.querySelector('.upload-area');
        
        ['dragenter', 'dragover', 'dragleave', 'drop'].forEach(eventName => {
            uploadArea.addEventListener(eventName, preventDefaults, false);
        });
        
        function preventDefaults(e) {
            e.preventDefault();
            e.stopPropagation();
        }
        
        ['dragenter', 'dragover'].forEach(eventName => {
            uploadArea.addEventListener(eventName, highlight, false);
        });
        
        ['dragleave', 'drop'].forEach(eventName => {
            uploadArea.addEventListener(eventName, unhighlight, false);
        });
        
        function highlight(e) {
            uploadArea.style.borderColor = '#764ba2';
            uploadArea.style.background = '#fff';
        }
        
        function unhighlight(e) {
            uploadArea.style.borderColor = '#667eea';
            uploadArea.style.background = '#f8f9ff';
        }
        
        uploadArea.addEventListener('drop', handleDrop, false);
        
        function handleDrop(e) {
            const dt = e.dataTransfer;
            const files = dt.files;
            document.getElementById('fileInput').files = files;
            previewFiles();
        }
    </script>
</body>
</html>

