import axios from 'axios';
import { getCityFromIP } from './locationService';

// Types for weather API response
export interface WeatherResponse {
  query_location: string;
  resolved_location: string;
  date: string;
  is_raining_now: boolean;
  will_rain_today: boolean;
  current: {
    temp_C: number;
    description: string;
    precipMM: number;
    humidity: number;
    feels_like_C: number;
  };
  today: {
    max_temp_C: number;
    min_temp_C: number;
    total_precipMM: number;
    chance_of_rain_max: number;
    rain_hours: string[];
  };
}

export async function getWeatherData(location?: string): Promise<WeatherResponse> {
  // If no location provided, use location service to find the city
  if (!location) {
    location = await getCityFromIP();
  }

  console.log(`🌤️  Fetching weather for: ${location}`);
  
  // Build URL based on location parameter
  const encodedLocation = location.replace(/\s+/g, '+');
  const url = `https://wttr.in/${encodedLocation}?format=j1`;
  
  // Fetch weather data
  const response = await axios.get(url);
  const data = response.data;
  
  // Process the data similar to the bash script
  const now = data.current_condition?.[0];
  const area = data.nearest_area?.[0];
  const today = data.weather?.[0];
  
  if (!now || !area || !today) {
    throw new Error('Invalid weather data received');
  }
  
  // Filter hourly data for rain hours
  const rainHours = today.hourly
    .filter((hour: any) => {
      const chanceOfRain = parseFloat(hour.chanceofrain);
      const precipMM = parseFloat(hour.precipMM);
      return chanceOfRain >= 30 || precipMM > 0;
    })
    .map((hour: any) => hour.time);
  
  // Calculate total precipitation
  const totalPrecipMM = today.hourly
    .reduce((sum: number, hour: any) => sum + parseFloat(hour.precipMM), 0);
  
  // Calculate max chance of rain
  const chanceOfRainMax = Math.max(
    ...today.hourly.map((hour: any) => parseFloat(hour.chanceofrain))
  );
  
  // Build resolved location string
  const resolvedLocation = [
    area.areaName?.[0]?.value,
    area.region?.[0]?.value,
    area.country?.[0]?.value
  ].filter(Boolean).join(', ');
  
  // Check if it's currently raining
  const isRainingNow = 
    parseFloat(now.precipMM) > 0 ||
    /rain|drizzle|shower/i.test(now.weatherDesc?.[0]?.value || '');
  
  // Build response object
  const weatherData: WeatherResponse = {
    query_location: location || 'auto',
    resolved_location: resolvedLocation,
    date: today.date,
    is_raining_now: isRainingNow,
    will_rain_today: rainHours.length > 0,
    current: {
      temp_C: parseFloat(now.temp_C),
      description: now.weatherDesc?.[0]?.value || '',
      precipMM: parseFloat(now.precipMM),
      humidity: parseFloat(now.humidity),
      feels_like_C: parseFloat(now.FeelsLikeC)
    },
    today: {
      max_temp_C: parseFloat(today.maxtempC),
      min_temp_C: parseFloat(today.mintempC),
      total_precipMM: totalPrecipMM,
      chance_of_rain_max: chanceOfRainMax,
      rain_hours: rainHours
    }
  };
  
  const rainIcon = weatherData.will_rain_today ? '☔' : '✨';
  console.log(`${rainIcon} ${resolvedLocation}: ${parseFloat(now.temp_C)}°C, Rain: ${weatherData.will_rain_today ? 'Yes' : 'No'}`);
  
  return weatherData;
}

