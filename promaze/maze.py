import os
os.environ["GOOGLE_APPLICATION_CREDENTIALS"]="/home/namgabin/promaze/pro-masze-idwc-766df8ed87b0.json"
print(os.environ["GOOGLE_APPLICATION_CREDENTIALS"])

def implicit():
    from google.cloud import storage
     
        #!/usr/bin/env python3
# -*- coding: utf-8 -*-

    import uuid
    import queue
    import random
    import wave
    import time
    from collections import deque
    from google.cloud import dialogflow_v2 as dialogflow
    from google.protobuf import struct_pb2
    from google.type import latlng_pb2
    from google.api_core import exceptions

    project_id ="pro-masze-idwc"
    session_id = str(uuid.uuid4())
    language = 'ko'
    disable_audio =  False

    time_before_start = 0.8
    save_audio_requests = True

    session_client = dialogflow.SessionsClient()
    session = session_client.session_path(project_id, session_id)
    
      
    query_params = dialogflow.QueryParameters(geo_location = latlng_pb2.LatLng(latitude=58.4106611, longitude=15.6198244), contexts = [dialogflow.Context(lifespan_count=100,name="projects/"+project_id+"/agent/sessions/"+session_id+"/contexts/linkoping"
)])

    audio_chunk_queue = deque(maxlen=int(time_before_start * 31.25))# 16000/512 = 31.25,  # Times 7.8 since the data is sent in 7.8Hz (16000 / 2048)

# Note: hard coding audio_encoding and sample_rate_hertz for simplicity.
    audio_encoding = dialogflow.AudioEncoding.AUDIO_ENCODING_LINEAR_16
    sample_rate_hertz = 16000
    #audio_config = dialogflow.InputAudioConfig
    '''(
    audio_encoding=audio_encoding,
    language_code=language,
    sample_rate_hertz=sample_rate_hertz,
    ingle_utterance=True)
    '''
    volume = 0
    is_talking = False
    is_in_dialog = False
    detected_wake_word = False
    head_visible = False
    waiting_for_wake_word = False
    cancel_stream_intent = False
    skip_audio = False
    texts={"콜라"}
    for text in texts:
        text_input = dialogflow.TextInput(text=text, language_code="en")

        query_input = dialogflow.QueryInput(text=text_input)

        response = session_client.detect_intent(
            request={"session": session, "query_input": query_input}
        )
        #/home/namgabin/promaze/pro-masze-idwc-766df8ed87b0.json

        print("=" * 20)
        print("Query text: {}".format(response.query_result.query_text))
        print(
            "Detected intent: {} (confidence: {})\n".format(
                response.query_result.intent.display_name,
                response.query_result.intent_detection_confidence,
            )
        )
        print("Fulfillment text: {}\n".format(response.query_result.fulfillment_text))



def create_intent(project_id, display_name, training_phrases_parts, message_texts):
    """Create an intent of the given intent type."""
    from google.cloud import dialogflow

    intents_client = dialogflow.IntentsClient()

    parent = dialogflow.AgentsClient.agent_path("pro-masze-idwc")
    training_phrases = []
    for training_phrases_part in training_phrases_parts:
        part = dialogflow.Intent.TrainingPhrase.Part(text="")
        # Here we create a new training phrase for each provided part.
        training_phrase = dialogflow.Intent.TrainingPhrase(parts=[part])
        training_phrases.append(training_phrase)

    text = dialogflow.Intent.Message.Text(text=message_texts)
    message = dialogflow.Intent.Message(text=text)

    intent = dialogflow.Intent(
        display_name=display_name, training_phrases=training_phrases, messages=[message]
    )

    response = intents_client.create_intent(
        request={"parent": parent, "intent": intent}
    )

    print("Intent created: {}".format(response))

# [START dialogflow_list_intents]
def list_intents(project_id):
    from google.cloud import dialogflow
    #pip install --upgrade google-api-python-client
    #python -m pip install dialogflow
    intents_client = dialogflow.IntentsClient()

    parent = dialogflow.AgentsClient.agent_path(project_id)

    intents = intents_client.list_intents(request={"parent": parent})
    
    #print(intents)
'''
    for intent in intents:
        print("=" * 20)
        print("Intent name: {}".format(intent.name))
        print("Intent display_name: {}".format(intent.display_name))
        print("Action: {}\n".format(intent.action))
        print("Root followup intent: {}".format(intent.root_followup_intent_name))
        print("Parent followup intent: {}\n".format(intent.parent_followup_intent_name))

        print("Input contexts:")
        for input_context_name in intent.input_context_names:
            
            print("\tName: {}".input_context_name)

        print("Output contexts:")
        for output_context in intent.output_contexts:
            print("\tName: {}".format(output_context.name))
'''
list_intents("pro-masze-idwc")
implicit()