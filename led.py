# led.py - 控制 GPIO18 上的 LED
import RPi.GPIO as GPIO
import time

LED_PIN = 18

def setup():
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(LED_PIN, GPIO.OUT)
    print("LED 控制已启动")

def led_on():
    GPIO.output(LED_PIN, GPIO.HIGH)
    print("LED 开")

def led_off():
    GPIO.output(LED_PIN, GPIO.LOW)
    print("LED 关")

def blink(times=3):
    for _ in range(times):
        led_on()
        time.sleep(0.5)
        led_off()
        time.sleep(0.5)

def cleanup():
    GPIO.cleanup()

# 测试代码
if __name__ == '__main__':
    setup()
    blink(5)
    cleanup()