#!/bin/sh

BROKER="localhost"
PORT="1883"

echo "Scanning retained Home Assistant discovery configs..."
echo

current_topic=""
current_payload=""

mosquitto_sub -h "$BROKER" -p "$PORT" \
    -t "homeassistant/sensor/#" \
    -v --retained-only | \
while IFS= read -r line; do

    # Detect topic line: must start with homeassistant/
    case "$line" in
        homeassistant/*)
            # If we have a previous message, process it
            if [ -n "$current_topic" ]; then
                case "$current_payload" in
                    *"telldus/"*"/sensor/"*)
                        echo "Deleting retained defaulted config:"
                        echo "  $current_topic"
                        mosquitto_pub -h "$BROKER" -p "$PORT" -t "$current_topic" -n -r
                        echo
                        ;;
                esac
            fi

            # Start new message
            current_topic="${line%% *}"
            current_payload="${line#* }"
            ;;
        *)
            # Continuation of payload (JSON lines)
            current_payload="$current_payload
$line"
            ;;
    esac

done

# Process last retained message
if [ -n "$current_topic" ]; then
    case "$current_payload" in
        *"telldus/"*"/sensor/"*)
            echo "Deleting retained defaulted config:"
            echo "  $current_topic"
            mosquitto_pub -h "$BROKER" -p "$PORT" -t "$current_topic" -n -r
            echo
            ;;
    esac
fi

echo "Cleanup complete."
