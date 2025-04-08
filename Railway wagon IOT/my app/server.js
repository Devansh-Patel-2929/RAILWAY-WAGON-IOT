const express = require('express');
const cors = require('cors');
const helmet = require('helmet');
const app = express();
const port = 3000;

// Middleware
app.use(helmet());
app.use(cors());
app.use(express.json());

// Data storage
let latestData = {
  timestamp: null,
  temperature: null,
  humidity: null
};

// Routes
app.post('/update', (req, res) => {
  latestData = {
    timestamp: new Date().toISOString(),
    ...req.body
  };
  console.log('Received:', latestData);
  res.status(200).json({ status: 'success' });
});

app.get('/data', (req, res) => {
  res.json(latestData);
});

// Start server
app.listen(port, '0.0.0.0', () => {
  console.log(`Server running at:`);
  console.log(`- Local:  http://localhost:${port}`);
  console.log(`- Network: http://YOUR_LOCAL_IP:${port}`);
});