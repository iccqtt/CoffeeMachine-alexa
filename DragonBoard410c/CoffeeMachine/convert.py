#! /usr/bin/env python

import subprocess
from gtts import gTTS

input_text = 'ask coffee machine make short coffee'
filename = 'make_short.wav'
print("Converting text to "+input_text)
tts = gTTS(input_text, lang='en')
tts.save('file.mp3')
print("Generated MP3 file!")
print("Converting MP3 to WAV...")
subprocess.call(['ffmpeg','-y', '-i', 'file.mp3', '-acodec', 'pcm_s16le', '-ar', '16000', filename])
subprocess.call(['aplay', filename])
print("Done!")
