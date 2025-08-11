# main.py - 主控制程序
import time
import led
import motor  # 假设你写了 motor.py

def main():
    print("机器人启动！")
    led.setup()
    # motor.setup()

    while True:
        print("闪烁 LED...")
        led.blink(2)
        time.sleep(1)

        # 这里可以加电机、传感器等控制
        # motor.forward()

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("程序被用户中断")
    finally:
        led.cleanup()