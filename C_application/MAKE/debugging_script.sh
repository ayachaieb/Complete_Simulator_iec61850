#!/bin/bash
curl -v -X POST -H "Content-Type: application/json" --data @start_config.json http://localhost:3000/api/verify-config
sleep 3
echo "verif CONFIG."
i=0
while [ $i -lt 10 ]; do
    ((i++))
    echo "-------------------------- RUN #$i --------------------------"

    ulimit -c unlimited
    sudo ../BIN/sv_simulator &
    sleep 3

    echo " application launched..."
    curl -v -X POST -H "Content-Type: application/json" --data @start_config.json http://localhost:3000/api/start-simulation
    echo "STARTED SIMULATION"
    sleep 10
    curl -s -X POST  http://localhost:3000/api/stop-simulation
    echo "STOPPED SIMULATION"
    sleep 3
    CORE_INFO=$(coredumpctl list --since "30 sec ago" | grep "sv_simulator" | tail -n 1)

    if [ -n "$CORE_INFO" ]; then
        # If CORE_INFO is not empty, a crash was found!
        PID=$(echo "$CORE_INFO" | awk '{print $1}')
        echo
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        echo "  SUCCESS: CRASH DETECTED on run #$i"
        echo "  The PID of the crashed process was: $PID"
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        echo
        break # Exit the loop, our work is done.
    else
        # No crash was found, loop again.
        echo "Run #$i completed without a crash."
        killall sv_simulator
        sleep 3
        echo "Killed sv_simulator process."
        echo "No crash detected, continuing to next run..."
    fi
done

echo "Script finished."
echo "You can now analyze the captured crash dump."
echo "Run this final command:"
echo "sudo coredumpctl debug $PID"