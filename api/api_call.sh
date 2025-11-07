#!/usr/bin/env bash

# Usage:
#   ./raincheck_json.sh            # auto location from IP
#   ./raincheck_json.sh Paris
#   ./raincheck_json.sh "New York"

LOCATION="$*"

if [ -n "$LOCATION" ]; then
  # replace spaces with +
  ENCODED_LOCATION="${LOCATION// /+}"
  URL="https://wttr.in/${ENCODED_LOCATION}?format=j1"
else
  URL="https://wttr.in/?format=j1"
fi

DATA=$(curl -s "$URL")

echo "$DATA" | jq -r '
  . as $root
  | $root.current_condition[0] as $now
  | $root.nearest_area[0] as $area
  | $root.weather[0] as $today
  | ($today.hourly 
      | map(
          .chanceofrain as $c
          | .precipMM as $p
          | ( ($c|tonumber) >= 30 or ($p|tonumber) > 0 )
            as $is_rain
          | select($is_rain)
        )
    ) as $rain_hours
  | {
      query_location: "'"${LOCATION:-auto}"'",
      resolved_location: (
        ($area.areaName[0].value // "") 
        + (if $area.region[0].value then ", " + $area.region[0].value else "" end)
        + (if $area.country[0].value then ", " + $area.country[0].value else "" end)
      ),
      date: $today.date,
      is_raining_now: (
        ($now.precipMM|tonumber) > 0
        or (
          ($now.weatherDesc[0].value|ascii_downcase)
          | test("rain|drizzle|shower")
        )
      ),
      will_rain_today: ($rain_hours | length > 0),
      current: {
        temp_C: ($now.temp_C|tonumber),
        description: $now.weatherDesc[0].value,
        precipMM: ($now.precipMM|tonumber),
        humidity: ($now.humidity|tonumber),
        feels_like_C: ($now.FeelsLikeC|tonumber)
      },
      today: {
        max_temp_C: ($today.maxtempC|tonumber),
        min_temp_C: ($today.mintempC|tonumber),
        total_precipMM: (
          $today.hourly
          | map(.precipMM|tonumber)
          | add
        ),
        chance_of_rain_max: (
          $today.hourly
          | map(.chanceofrain|tonumber)
          | max
        ),
        rain_hours: (
          $rain_hours
          | map(.time)
        )
      }
    }'
