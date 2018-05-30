# importing the requests library
import requests
import json

# defining the api-endpoint
API_ENDPOINT = ''
header = {'Content-Type': 'application/json'}

def updateStatus(message):
    payload = {
        "request": {
            "type": "UPDATE",
            "on_off": message[1],
            "water": message[2],
            "glass": message[3],
        }
    }
    # sending post request and saving response as response object
    r = requests.post(API_ENDPOINT, json.dumps(payload))

    #print r.text


def sendGlassPosition(position):
    payload = {
        "request": {
            "type": "GLASS",
            "glass": position
        }
    }

    # sending post request and saving response as response object
    r = requests.post(API_ENDPOINT, json.dumps(payload))

    #print r.text


def sendLevelWater(level):
    payload = {
        "request": {
            "type": "WATER",
            "water": level
        }
    }

    # sending post request and saving response as response object
    r = requests.post(API_ENDPOINT, json.dumps(payload))

    #print r.text


def sendCoffeelevel(level):
    payload = {
        "request": {
            "type": "COFFEE",
            "coffee": level
        }
    }

    # sending post request and saving response as response object
    #r = requests.post(API_ENDPOINT, json.dumps(payload))

    #print r.text

def sendOnOff(status):
    payload = {
        "request": {
            "type": "ON_OFF",
            "on_off": status
        }
    }

    # sending post request and saving response as response object
    r = requests.post(API_ENDPOINT, json.dumps(payload))

    #print r.text
