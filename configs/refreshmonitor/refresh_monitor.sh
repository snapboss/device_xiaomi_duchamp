#!/system/bin/sh

# Initial user-selected refresh rate from settings (strip off the decimal part)
prev_rate=$(settings get system peak_refresh_rate)
prev_rate_int=$(echo "$prev_rate" | awk '{print int($1)}')  # Convert to integer by stripping the decimal part

while true; do
    # Get the current user-set refresh rate (as float)
    current_rate=$(settings get system peak_refresh_rate)
    current_rate_int=$(echo "$current_rate" | awk '{print int($1)}')  # Convert to integer

    # If the refresh rate has changed, update SurfaceFlinger and sysfs
    if [ "$current_rate_int" != "$prev_rate_int" ]; then
        # Update SurfaceFlinger with the new rate
        /vendor/bin/set_refresh.sh "$current_rate_int"
        prev_rate_int=$current_rate_int
    fi

    sleep 1
done

