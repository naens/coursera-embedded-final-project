import RPi.GPIO as GPIO

def falling(channel):
    print(f"Falling edge detected on GPIO {channel}")

def rising(channel):
    print(f"Rising edge detected on GPIO {channel}")

GPIO.setmode(GPIO.BCM)
GPIO.setup(17, GPIO.IN, pull_up_down=GPIO.PUD_UP)
GPIO.add_event_detect(17, GPIO.FALLING, callback=falling, bouncetime=200)
GPIO.add_event_detect(17, GPIO.RISING, callback=rising, bouncetime=200)
