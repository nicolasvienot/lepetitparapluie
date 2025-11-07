import express, { Request, Response } from 'express';
import { getWeatherData } from './weatherService';
import { getCityFromIP } from './locationService';

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(express.json());

// GET route to check if it will rain (full weather data)
app.get('/api/weather', async (req: Request, res: Response) => {
  try {
    const location = req.query.location as string | undefined;
    const weatherData = await getWeatherData(location);
    res.json(weatherData);
  } catch (error) {
    console.error('Error fetching weather data:', error);
    res.status(500).json({ 
      error: 'Failed to fetch weather data',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
});

// GET route to check only if it will rain today (simple boolean)
// If no location is provided, automatically detects city from IP
app.get('/api/will-it-rain', async (req: Request, res: Response) => {
  try {
    let location = req.query.location as string | undefined;
    
    // If no location provided, detect from IP
    if (!location) {
      location = await getCityFromIP();
    }
    
    const weatherData = await getWeatherData(location);
    res.json({ will_rain_today: weatherData.will_rain_today });
  } catch (error) {
    console.error('Error fetching weather data:', error);
    res.status(500).json({ 
      error: 'Failed to fetch weather data',
      message: error instanceof Error ? error.message : 'Unknown error'
    });
  }
});

// Simple GET route
app.get('/api/hello', (req: Request, res: Response) => {
  res.json({
    message: 'Hello from Express with TypeScript!',
    timestamp: new Date().toISOString()
  });
});

// Start server
app.listen(PORT, () => {
  console.log(`🚀 Server is running on http://localhost:${PORT}`);
  console.log(`📡 Full weather data: http://localhost:${PORT}/api/weather`);
  console.log(`📡 Simple rain check: http://localhost:${PORT}/api/will-it-rain`);
});

