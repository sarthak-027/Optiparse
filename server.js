const express = require('express');
const path = require('path');
const multer = require('multer');
const fs = require('fs');
const http = require('http');
const { Server } = require('socket.io');
const { spawn } = require('child_process');

const app = express();
// Upgrade to an HTTP server to support WebSockets
const server = http.createServer(app);
const io = new Server(server); 
const PORT = 3000;

const upload = multer({ dest: 'uploads/' });

app.use(express.static(path.join(__dirname, 'public')));
app.use(express.json());

// Step 1: Handle the heavy file upload via standard HTTP
app.post('/api/upload', upload.single('dataFile'), (req, res) => {
    if (!req.file) return res.status(400).send("No file uploaded.");
    res.json({ filePath: req.file.path }); // Send the temp file path back to the browser
});

// NEW: Secure Download Endpoint
app.get('/api/download', (req, res) => {
    const extractedFilePath = path.join(__dirname, 'extracted_data.txt');
    
    // Check if the file exists before trying to send it
    if (fs.existsSync(extractedFilePath)) {
        // res.download automatically forces the browser to save the file
        res.download(extractedFilePath, 'OptiParse_Triage_Log.txt');
    } else {
        res.status(404).send("File not found. Please run the analyzer first.");
    }
});

// Step 2: Handle the live C++ execution via WebSockets
io.on('connection', (socket) => {
    socket.on('start-engine', (data) => {
        const { filePath, targetWord } = data;
        const engine = process.platform === 'win32' ? 'datadrill.exe' : './datadrill';

        // 'spawn' allows us to stream data live, unlike 'exec'
        const child = spawn(engine, [filePath, targetWord]);

        // Stream C++ std::cout directly to the frontend in real-time
        child.stdout.on('data', (chunk) => {
            socket.emit('terminal-stream', chunk.toString());
        });

        child.stderr.on('data', (chunk) => {
            socket.emit('terminal-error', chunk.toString());
        });

        // Step 3: Trigger the AI Agent when C++ finishes
    
        child.on('close', (code) => {
            // FIXED: Swapped \033 for \x1B to satisfy JavaScript's strict mode
            socket.emit('terminal-stream', `\n\x1B[1;36m[SYSTEM]\x1B[0m C++ Thread pool closed. Engaging AI Agent on extracted data...\n`);
            
            // Simulating an AI API call reading the extracted_data.txt
            setTimeout(() => {
                const mockAIResponse = `\n🤖 AI Agent Triage Report:\n--------------------------\nThe anomaly pattern "${targetWord}" was successfully isolated. Preliminary analysis suggests this is a recurring localized event. I have bundled the relevant data into 'extracted_data.txt' for the Data Science team. System integrity remains stable.\n`;
                
                socket.emit('ai-report', mockAIResponse);

                // Cleanup massive uploaded file
                fs.unlink(filePath, (err) => {
                    if(err) console.error(err);
                });
            }, 2000); 
        });
    });
});

server.listen(PORT, () => {
    console.log(`⚡ Autonomous Pipeline running on http://localhost:${PORT}`);
});