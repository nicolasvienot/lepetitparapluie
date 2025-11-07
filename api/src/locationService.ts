import axios from 'axios';

export interface LocationInfo {
  ip: string;
  city: string;
  region: string;
  country: string;
  timezone: string;
  latitude: number;
  longitude: number;
}

/**
 * Get location information based on IP address
 * Uses ip-api.com which is free for non-commercial use
 */
export async function getLocationFromIP(): Promise<LocationInfo> {
  try {
    // Using ip-api.com - free for non-commercial use, no API key required
    const response = await axios.get('http://ip-api.com/json/?fields=status,message,country,countryCode,region,regionName,city,zip,lat,lon,timezone,query');
    
    const data = response.data;
    
    if (data.status === 'fail') {
      throw new Error(data.message || 'Failed to get location data');
    }
    
    return {
      ip: data.query,
      city: data.city || 'Unknown',
      region: data.regionName || data.region || 'Unknown',
      country: data.country || 'Unknown',
      timezone: data.timezone || 'Unknown',
      latitude: data.lat || 0,
      longitude: data.lon || 0
    };
  } catch (error) {
    throw new Error(`Failed to detect location: ${error instanceof Error ? error.message : 'Unknown error'}`);
  }
}

/**
 * Get city name from IP address
 */
export async function getCityFromIP(): Promise<string> {
  const location = await getLocationFromIP();
  return location.city;
}

