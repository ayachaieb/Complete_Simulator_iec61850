#!/bin/bash

# --- CONFIGURATION ---
# Replace with the port your Node.js server runs on
API_PORT=3000
# ---------------------

# Construct the API endpoints
START_URL="http://localhost:$API_PORT/api/start-simulation"
STOP_URL="http://localhost:$API_PORT/api/stop-simulation"

# Check if the config file exists
if [ ! -f "start_config.json" ]; then
    echo "Error: start_config.json not found. Please create it."
    exit 1
fi

# Initialize a counter
i=0
echo "Starting test loop. Will run until a crash occurs. Press Ctrl+C to stop the script."

# Loop indefinitely
while true; do
    ((i++))
    echo "-------------------------- RUN #$i --------------------------"

    # CRITICAL: We NO LONGER start sv_simulator here.
    # The Node.js server is responsible for that.
    
    # Enable core dumps for any process started by this user (or root if running script with sudo)
    # This setting will be inherited by the C app when the Node.js server launches it.
    ulimit -c unlimited
    sudo ../BIN/sv_simulator &
    
    # --- Send commands via curl ---
    echo "Sending START command to Node.js server..."
    # The -f flag makes curl treat server errors (like 500) as failures
    # The -w "%{http_code}" prints the HTTP status code
    HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST -H "Content-Type: application/json" --data @start_config.json "$START_URL")

    if [ "$HTTP_CODE" != "200" ]; then
        echo "Error: Server returned HTTP status $HTTP_CODE on START. Aborting this run."
        sleep 2
        continue # Skip to the next iteration of the loop
    fi

    echo "Simulation should be running. Waiting 4 seconds..."
    sleep 4

    echo "Sending STOP command..."
    curl -s -X POST "$STOP_URL" > /dev/null
    
    # Allow a moment for the crash to occur
    sleep 2 

    # --- Check for a Core Dump ---
    CORE_INFO=$(coredumpctl list --since "20 sec ago" | grep "sv_simulator" | tail -n 1)

    if [ -n "$CORE_INFO" ]; then
        PID=$(echo "$CORE_INFO" | awk '{print $1}')
        echo
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        echo "CRASH DETECTED on run #$i"
        echo "PID of crashed process: $PID"
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        echo
        break # Exit the loop
    else
        echo "Run #$i completed without a detectable crash."
        echo
    fi
done

echo "Script finished."
echo "You can now analyze the crash dump."
echo "Run this command:"
echo "sudo coredumpctl debug $PID"