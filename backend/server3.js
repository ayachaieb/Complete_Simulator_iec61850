const express = require('express');
const cors = require('cors');
const app = express();
const net = require('net');
const fs = require('fs');
const port = 3000;

// Sample data
const items = [
  { id: 1, name: 'Item 1' },
  { id: 2, name: 'Item 2' },
  { id: 3, name: 'Item 3' },
];

// Enable JSON parsing
app.use(express.json());

// CORS middleware
app.use(cors({
  origin: 'http://localhost:4200',
  methods: ['GET', 'POST', 'OPTIONS'],
  allowedHeaders: ['Origin', 'X-Requested-With', 'Content-Type', 'Accept'],
}));
app.use((req, res, next) => {
  console.log(`[${new Date().toISOString()}] ${req.method} ${req.url} - Body:`, req.body);
  next();
});

// Helper function to send message to C app with unified format

// API endpoints
app.get('/api/items', (req, res) => {
  console.log('Serving /api/items');
  res.json(items);
});

app.get('/', (req, res) => {
  console.log('Serving /');
  res.send('Welcome to the Node.js API! Use /api/items to get the items.');
});

app.post('/api/verify-config', (req, res) => {
  console.log('Processing /api/verify-config with config:', req.body);
  const config = req.body;
  const errors = [];

  // Validation rules
  if (!config.appID || !/^0[xX][0-9A-Fa-f]{1,4}$/.test(config.appID)) {
    errors.push('appID must be a hexadecimal value (e.g., 0x1000 or 0X1000)');
  }
  if (!config.macAddress || !/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(config.macAddress)) {
    errors.push('macAddress must be in format XX:XX:XX:XX:XX:XX or XX-XX-XX-XX-XX-XX');
  }
  if (!config.GOid || !/^[A-Za-z0-9_]+$/.test(config.GOid)) {
    errors.push('GOid must be alphanumeric with underscores');
  }
  if (!config.interface || !/^[A-Za-z0-9]+$/.test(config.interface)) {
    errors.push('interface must be alphanumeric (e.g., eth0)');
  }
  if (!config.cbref || !/^[A-Za-z0-9_]+$/.test(config.cbref)) {
    errors.push('cbref must be alphanumeric with underscores');
  }
  if (!config.svid || !/^[A-Za-z0-9_]+$/.test(config.svid)) {
    errors.push('svid must be alphanumeric with underscores');
  }
  if (!config.scenariofile || !/^[A-Za-z0-9_]+\.[xX][mM][lL]$/.test(config.scenariofile)) {
    errors.push('scenariofile must be a valid XML filename (e.g., scenario.xml or SCENARIO.XML)');
  }

  if (errors.length > 0) {
    console.log('Validation errors:', errors);
    return res.status(400).json({ errors });
  }
  console.log('Config valid');
  res.json({ message: 'Configuration is valid' });
});

app.post('/api/verify-goose-config', (req, res) => {
  console.log('Processing /api/verify-goose-config with config:', req.body);
  const config = req.body;
  const errors = [];

  // Validation rules
  if (!config.gocbRef || !/^[A-Za-z0-9_]+$/.test(config.gocbRef)) {
    errors.push('gocbRef must be alphanumeric with underscores');
  }
  if (!config.datSet || !/^[A-Za-z0-9_]+$/.test(config.datSet)) {
    errors.push('datSet must be alphanumeric with underscores');
  }
  if (!config.goID || !/^0[xX][0-9A-Fa-f]{1,4}$/.test(config.goID)) {
    errors.push('goID must be a hexadecimal value (e.g., 0x1000 or 0X1000)');
  }
  if (!config.macAddress || !/^([0-9A-Fa-f]{2}[:-]){5}([0-9A-Fa-f]{2})$/.test(config.macAddress)) {
    errors.push('macAddress must be in format XX:XX:XX:XX:XX:XX or XX-XX-XX-XX-XX-XX');
  }
  if (!config.appID || !/^[A-Za-z0-9_]+$/.test(config.appID)) {
    errors.push('appID must be alphanumeric with underscores');
  }
  if (!config.interface || !/^[A-Za-z0-9]+$/.test(config.interface)) {
    errors.push('interface must be alphanumeric (e.g., eth0)');
  }

  if (errors.length > 0) {
    console.log('Validation errors:', errors);
    return res.status(400).json({ errors });
  }
  console.log('goose-config valid');
  res.json({ message: 'Configuration is valid' });
});
function sendToCApp(messageType, data) {
  if (!clientSocket) {
    throw new Error('No C application connected');
  }

  const message = JSON.stringify({
    type: messageType,
    data: data
  });

  clientSocket.write(message);
}
// Store pending HTTP responses to match with C app responses
const pendingResponses = new Map();

app.post('/api/start-simulation', (req, res) => {
  console.log('Processing /api/start-simulation with config:', req.body);
  const requestId = Date.now().toString();
  
  try {
    pendingResponses.set(requestId, res);
    sendToCApp('start_simulation', {
      config: req.body,
      requestId: requestId
    });
  } catch (error) {
    console.error('Simulation error:', error.message);
    pendingResponses.delete(requestId);
    res.status(500).json({ error: 'Failed to start simulation: ' + error.message });
  }
});

app.post('/api/stop-simulation', (req, res) => {
  console.log('Processing /api/stop-simulation');
  const requestId = Date.now().toString();
  
  try {
    pendingResponses.set(requestId, res);
    sendToCApp('stop_simulation', {
      requestId: requestId
    });
  } catch (error) {
    console.error('Stopping Simulation error:', error.message);
    pendingResponses.delete(requestId);
    res.status(500).json({ error: 'Failed to stop simulation: ' + error.message });
  }
});

app.post('/api/send-goose-message', (req, res) => {
  console.log('Processing /api/send-goose-message with config:', req.body);
  const requestId = Date.now().toString();
  
  try {
    pendingResponses.set(requestId, res);
    sendToCApp('send_goose_message', {
      config: req.body,
      requestId: requestId
    });
  } catch (error) {
    console.error('goose sending error:', error.message);
    pendingResponses.delete(requestId);
    res.status(500).json({ error: 'Failed to send goose: ' + error.message });
  }
});



// Error handling middleware
app.use((err, req, res, next) => {
  console.error('Server error:', err.stack);
  res.status(500).json({ error: 'Internal server error' });
});

// Start HTTP server
app.listen(port, () => {
  console.log(`Server running at http://localhost:${port}`);
});

// Create Unix domain socket server
const socketServer = net.createServer((socket) => {
  console.log('C application connected');
  clientSocket = socket;

  socket.on('data', (data) => {
    try {
      const message = data.toString();
      console.log('Received from C app:', message);
      
      const parsed = JSON.parse(message);
      const { requestId, ...response } = parsed;
      
      const res = pendingResponses.get(requestId);
      if (res) {
        res.json(response);
        pendingResponses.delete(requestId);
      }
    } catch (err) {
      console.error('Error processing message:', err);
    }
  });

  socket.on('end', () => {
    console.log('C application disconnected');
    clientSocket = null;
  });

  socket.on('error', (err) => {
    console.error('Socket error:', err);
    clientSocket = null;
  });
});

// Clean up old socket file if it exists
try {
  fs.unlinkSync('/tmp/app.sv_simulator');
} catch (err) {
  if (err.code !== 'ENOENT') throw err;
}

// Start socket server
socketServer.listen('/tmp/app.sv_simulator', () => {
  console.log('Socket server listening on /tmp/app.sv_simulator');
});