# -*- coding: utf-8 -*-
#!/usr/bin/env python
#vs code로 안됨. 그냥 

import os
import sys
'''
reload(sys)
sys.setdefaultencoding('utf-8')
# 위 세 줄 없으면 컴파일 안 됨. 유니코드 에러 생김. 한글.
'''
import playsound
from pydub import AudioSegment
import speech_recognition as sr

r = sr.Recognizer()
with sr.Microphone() as source:
    print("Say Something")
    speech = r.listen(source)

sys.stdout = open('audio_output.txt', 'w') #-- 텍스트 저장시 사용

try:
    audio = r.recognize_google(speech, language="ko-KR")
    print("Your speech thinks like\n " + audio)
except sr.UnknownValueError:
    print("Your speech can not understand")
except sr.RequestError as e:
    print("Request Error!; {0}".format(e))

sys.stdout.close() #-- 텍스트 저장시 사용
