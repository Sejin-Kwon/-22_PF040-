# -*- coding: utf-8 -*-
#!/usr/bin/env python
import os
import sys
reload(sys)
sys.setdefaultencoding('utf-8')
# 위 세 줄 없으면 컴파일 안 됨. 유니코드 에러 생김. 한글.

from gtts import gTTS
import playsound
from pydub import AudioSegment

#sound = AudioSegment.from_adts("voice.wav")
#sound.export("voice.wav", format="wav")

def speak(text):

	tts = gTTS(text=text, lang='ko')
	#tts.save("voice.wav")
	tts.save("voice.mp3")
	playsound.playsound("voice.mp3")


speak("TTS 끗")





'''
저장하지 못할 수도 있습니다. gTTS는 오디오 클립을 mp3로 저장하는 옵션을 제공합니다. 이름을 .wav로 지정해도 인식하지 못하고 기본 저장 옵션을 사용합니다. wav 파일만 필요한 경우 pydub 모듈을 사용하여 파일 형식을 변경하십시오.
'''

