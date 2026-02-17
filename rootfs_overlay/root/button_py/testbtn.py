import gpiod
import time

# Use the chip name identified via gpiodetect (usually gpiochip0 for Pi 3)
CHIP_NAME = "gpiochip0"
# GPIO 17 (BCM numbering)
LINE_OFFSET = 17

with gpiod.request_lines(
    path=f"/dev/{CHIP_NAME}",
    consumer="button-monitor",
    config={
        LINE_OFFSET: gpiod.LineSettings(
            direction=gpiod.Direction.INPUT,
            edge_detection=gpiod.Edge.FALLING, # Or Edge.BOTH / Edge.RISING
            bias=gpiod.Bias.PULL_UP           # Standard for buttons connecting to GND
        )
    }
) as request:
    print(f"Waiting for falling edge on GPIO {LINE_OFFSET}...")
    while True:
        # Wait for an event (blocks until the edge is detected)
        if request.wait_edge_events():
            event_batch = request.read_edge_events()
            for event in event_batch:
                print(f"Event detected at {event.timestamp_ns}ns")
