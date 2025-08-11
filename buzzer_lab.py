# 导入库文件
from gpiozero import TonalBuzzer
from gpiozero.tones import Tone
from time import sleep

# 定义无源蜂鸣器管脚
makerobo_Buzzer = 17   # 无源蜂鸣器管脚定义

#音谱定义
Tone_CL = [220, 220, 220, 220, 220, 220, 220, 248]		# 低C音符的频率
Tone_CM = [220, 262, 294, 330, 350, 393, 441, 495]		# 中C音的频率
Tone_CH = [220, 525, 589, 661, 700, 786, 800, 880]		# 高C音符的频率

## 第一首歌音谱
makerobo_song_1 = [	Tone_CM[3], Tone_CM[5], Tone_CM[6], Tone_CM[3], Tone_CM[2], Tone_CM[3], Tone_CM[5], Tone_CM[6], 
			        Tone_CH[1], Tone_CM[6], Tone_CM[5], Tone_CM[1], Tone_CM[3], Tone_CM[2], Tone_CM[2], Tone_CM[3], 
			        Tone_CM[5], Tone_CM[2], Tone_CM[3], Tone_CM[3], Tone_CL[6], Tone_CL[6], Tone_CL[6], Tone_CM[1],
			        Tone_CM[2], Tone_CM[3], Tone_CM[2], Tone_CL[7], Tone_CL[6], Tone_CM[1], Tone_CL[5]	]

## 第1首歌的节拍，1表示1/8拍
makerobo_beat_1 = [	1, 1, 3, 1, 1, 3, 1, 1, 			
			        1, 1, 1, 1, 1, 1, 3, 1, 
			        1, 3, 1, 1, 1, 1, 1, 1, 
			        1, 2, 1, 1, 1, 1, 1, 1, 
			        1, 1, 3	]
## 第二首歌音谱
makerobo_song_2 = [	Tone_CM[1], Tone_CM[1], Tone_CM[1], Tone_CL[5], Tone_CM[3], Tone_CM[3], Tone_CM[3], Tone_CM[1],
			        Tone_CM[1], Tone_CM[3], Tone_CM[5], Tone_CM[5], Tone_CM[4], Tone_CM[3], Tone_CM[2], Tone_CM[2], 
			        Tone_CM[3], Tone_CM[4], Tone_CM[4], Tone_CM[3], Tone_CM[2], Tone_CM[3], Tone_CM[1], Tone_CM[1], 
			        Tone_CM[3], Tone_CM[2], Tone_CL[5], Tone_CL[7], Tone_CM[2], Tone_CM[1]	]

## 第2首歌的节拍，1表示1/8拍
makerobo_beat_2 = [	1, 1, 2, 2, 1, 1, 2, 2, 			
			        1, 1, 2, 2, 1, 1, 3, 1, 
			        1, 2, 2, 1, 1, 2, 2, 1, 
			        1, 2, 2, 1, 1, 3 ]

# 初始化工作
def makerobo_setup():
    global bz
    bz = TonalBuzzer(makerobo_Buzzer)
    bz.stop()

