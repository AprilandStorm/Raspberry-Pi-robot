from LOBOROBOT import LOBOROBOT  # 载入机器人库
from gpiozero import DistanceSensor
from time import sleep

# 超声波模块
makerobo_sensor = DistanceSensor(echo=21, trigger=20,max_distance=3, threshold_distance=0.2)

clbrobot = LOBOROBOT() # 实例化机器人对象

def loop():
        while True: 
            dis = makerobo_sensor.distance * 100  # 测量距离值，并把m单位换成cm单位
            print(dis, 'cm')
            print('')
            sleep(0.5)            # ✅ 建议加个延时，避免打印太快

def destroy():
        clbrobot.t_stop(.1)
        # GPIO.cleanup()

if __name__ == "__main__":
        clbrobot.t_stop(.1)
        try:
                loop()
        except KeyboardInterrupt:
                destroy()