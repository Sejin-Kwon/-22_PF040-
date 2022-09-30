#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
A dialogflow integration for ROS
The node has support for events, intents, contexts, parameters and even dialogflow actions
License: BSD
  https://raw.githubusercontent.com/samiamlabs/dyno/master/LICENCE
"""
import uuid
import queue
import random
import wave
import time
from collections import deque
from google.cloud import dialogflow
from google.protobuf import struct_pb2
from google.type import latlng_pb2
from google.api_core import exceptions

project_id = "pro-masze-idwc"
session_id = str(uuid.uuid4())
language = 'en'
disable_audio =  False
        
time_before_start = 0.8
save_audio_requests = True

session_client = dialogflow.SessionsClient()
"""       
query_params = dialogflow.QueryParameters(geo_location = latlng_pb2.LatLng(latitude=58.4106611, longitude=15.6198244), contexts = [dialogflow.Context(lifespan_count=100,name="projects/"+project_id+"/agent/sessions/"+session_id+"/contexts/linkoping"
)])

audio_chunk_queue = deque(maxlen=int(time_before_start * 31.25))# 16000/512 = 31.25,  # Times 7.8 since the data is sent in 7.8Hz (16000 / 2048)

# Note: hard coding audio_encoding and sample_rate_hertz for simplicity.
audio_encoding = dialogflow.AudioEncoding.AUDIO_ENCODING_LINEAR_16
sample_rate_hertz = 16000
audio_config = dialogflow.InputAudioConfig(
audio_encoding=audio_encoding,
language_code=language,
sample_rate_hertz=sample_rate_hertz,
single_utterance=True)

volume = 0
is_talking = False
is_in_dialog = False
detected_wake_word = False
head_visible = False
waiting_for_wake_word = False
cancel_stream_intent = False
skip_audio = False

"""