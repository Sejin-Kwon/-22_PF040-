######## IMPORTS ##########
# -*- coding: utf-8 -*-
#!/usr/bin/env python
import os
import sounddevice as sd
from scipy.io.wavfile import write
import librosa
import numpy as np
from tensorflow.keras.models import load_model
from gtts import gTTS
import playsound
from pydub import AudioSegment

def speak(text):

	tts = gTTS(text=text, lang='ko')
	tts.save("prediction.mp3")
	playsound.playsound("prediction.mp3")


####### ALL CONSTANTS #####
fs = 44100
seconds = 2
filename = "prediction.wav"
class_names = ["Wake Word NOT Detected", "Wake Word Detected"]

##### LOADING OUR SAVED MODEL and PREDICTING ###
model = load_model('/home/namgabin/voice_recognition/saved_model/WWD.h5')

print("Prediction Started: ")
i = 0
while True:
    print("Say Now: ")
    myrecording = sd.rec(int(seconds * fs), samplerate=fs, channels=2)
    sd.wait()
    write(filename, fs, myrecording)

    audio, sample_rate = librosa.load(filename)
    mfcc = librosa.feature.mfcc(y=audio, sr=sample_rate, n_mfcc=40)
    mfcc_processed = np.mean(mfcc.T, axis=0)

    prediction = model.predict(np.expand_dims(mfcc_processed, axis=0))
    if prediction[:, 1] > 0.99:
        print(f"Wake Word Detected for ({i})") #사니
        print("Confidence:", prediction[:, 1])
        i += 1
        speak("네, 듣고 있어요.")
    
    else:
        print(f"Wake Word NOT Detected")
        print("Confidence:", prediction[:, 0])

        #sudo apt-get install python3-h5py