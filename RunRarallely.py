###### IMPORTS ###################
# sudo apt install espeak
# pip install pyttsx3
import tensorflow as tf
import threading
import time
import sounddevice as sd
import librosa
import numpy as np
import keras
from keras.models import Sequential
from tensorflow.keras.models import load_model
import pyttsx3

#### SETTING UP TEXT TO SPEECH ###
engine = pyttsx3.init()
voices = engine.getProperty('voices')
engine.setProperty('voice', 'HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Speech\Voices\Tokens\TTS_MS_EN-US_DAVID_11.0')

def speak(audio):
    engine.say(audio)
    engine.runAndWait()
    engine.endLoop()

##### CONSTANTS ################
fs = 22050
seconds = 2

model = load_model("/home/namgabin/voice_recognition/saved_model/WWD2.h5")
'''
model = keras.models.Sequential()
model.call = tf.function(model.call)
'''
##### LISTENING THREAD #########
def listener():
    while True:
        myrecording = sd.rec(int(seconds * fs), samplerate=fs, channels=1)
        sd.wait()
        mfcc = librosa.feature.mfcc(y=myrecording.ravel(), sr=fs, n_mfcc=40)
        mfcc_processed = np.mean(mfcc.T, axis=0)
        prediction_thread(mfcc_processed)
        time.sleep(0.001)


def voice_thread():
    listen_thread = threading.Thread(target=listener, name="ListeningFunction")
    listen_thread.start()

##### PREDICTION THREAD #############
def prediction(y):
    prediction = model.predict(np.expand_dims(y, axis=0))
    if prediction[:, 1] > 0.99:
        if engine._inLoop:
            engine.endLoop()

        speak("Hello, What can I do for you?")
        

    time.sleep(0.1)

def prediction_thread(y):
    pred_thread = threading.Thread(target=prediction, name="PredictFunction", args=(y,))
    pred_thread.start()

voice_thread()


tf.compat.v1.disable_eager_execution()
gpus = tf.config.experimental.list_physical_devices('GPU')
if len(gpus) > 0:
    
    #print(f'GPUs {gpus}')
    try:
        tf.config.experimental.set_visible_devices(gpus[0], 'GPU')
        tf.config.experimental.set_memory_growth(gpus[0], True)
    except RuntimeError:
        pass