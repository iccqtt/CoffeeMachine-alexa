from __future__ import division
import logging
import ServiceAWS as aws
from pygatt.util import uuid16_to_uuid
from pygatt.exceptions import NotConnectedError
from Utils import connect_ble
from time import sleep

logging.basicConfig()
logging.getLogger('pygatt').setLevel(logging.DEBUG)

uuid_heart_service = uuid16_to_uuid(0x2A37)
uuid_battery_service = uuid16_to_uuid(0x2A19)
uuid_set_sensor_location = uuid16_to_uuid(0x2A39)
uuid_get_sensor_location = uuid16_to_uuid(0x2A38)

device = None

cf_control_point_turn_off = 0
cf_control_point_turn_on = 1
cf_control_point_short_coffee = 2
cf_control_point_long_coffee = 3
cf_control_point_level_water = 4
cf_control_point_level_coffee = 5
cf_control_point_glass_position = 6
cf_control_point_update = 7

ON = '1'
OFF = '0'

inUse = 2

MACHINE_IN_USE = "coffee_in_progress"

hasWater = 1
empty = 0

COFFEE_READY = 'coffee_ready'

queue = []

conn = connect_ble()
conn.start()
def getConnection(task):
    try:
        global device
        device = conn.connect()
        if device is not None:
            subscribeCharacteristics(device)
            if task == "write_char_error":
                aws.sendError("reconnected")
            elif task == "reconnecting":
                aws.sendError("reconnected")
        sleep(0.5)
        return device
    except NotConnectedError:
        print (NotConnectedError.message)
        aws.sendError(task)
        getConnection("connection_error")

def subscribeCharacteristics(device):
    try:
        device.subscribe(uuid_heart_service, messageCSRCallback, False)
        device.subscribe(uuid_battery_service, batteryCallback, False)
    except NotConnectedError:
        print "[!] "+NotConnectedError.message

def write_char(handle, data):
    if device == None:
        getConnection("write_char_error")
    else:
        try:
            device.char_write_handle(handle, data, False)
        except NotConnectedError:
            print "[bluetooth]: Error to connect!"

def send_keepalive(handle, data):
    if device == None:
        getConnection("connection_error")
    else:
        try:
            global device
            device1 = device
            device = device1
            device.char_write_handle(handle, data, False)
        except NotConnectedError:
            getConnection("connection_error")

def batteryCallback(handle, message):
    if handle == 19:
        print "[batteryCallback] Received battery level. "
        #for i in range(len(message)):
        #    print str(message[i])

        write_char(aws.handle, aws.getByteArray(aws.SEND_UPDATE))
        aws.sendError("reconnected")

def messageCSRCallback(handle, message):
    #print("[messageCSRCallback]: Received commands")
    #for i in range(len(message)):
    #    print str(message[i])

    if len(message) == 2:
        if message[0] == cf_control_point_turn_off:
            aws.sendOnOff(OFF)
            print("[messageCSRCallback]: Turn off machine.")
            aws.stopTimeoutOnOff()

        if message[0] == cf_control_point_turn_on:
            aws.sendOnOff(ON)
            print("[messageCSRCallback]: Turn on machine.")
            aws.stopTimeoutOnOff()

        if message[0] == cf_control_point_short_coffee:
            print("[messageCSRCallback]: Short coffee request")
            if message[1] == inUse:
                aws.sendShortCoffee(MACHINE_IN_USE)
                aws.stopTimeoutShortCoffee()
            elif message[1] == 1:
                aws.sendShortCoffee(COFFEE_READY)
                aws.coffeeDoing = None
                write_char(aws.handle, aws.getByteArray(aws.SEND_UPDATE))
            elif message[1] == 3:
                aws.stopTimeoutShortCoffee()

        if message[0] == cf_control_point_long_coffee:
            print("[messageCSRCallback]: Long coffee request")
            if message[1] == inUse:
                aws.sendLongCoffee(MACHINE_IN_USE)
                aws.stopTimeoutLongCoffee()
            elif message[1] == 1:
                aws.sendLongCoffee(COFFEE_READY)
                aws.coffeeDoing = None
            elif message[1] == 3:
                aws.stopTimeoutLongCoffee()

        if message[0] == cf_control_point_level_water:
            aws.sendLevelWater(str(message[1]))
            print("[messageCSRCallback]: Sending water status")
            aws.stopTimeoutLevelWater()

        if message[0] == cf_control_point_glass_position:
            aws.sendGlassPostion(str(message[1]))
            aws.stopTimeoutGlassPostion()
    if len(message) == 3:
        if message[0] == cf_control_point_level_coffee:
            print("[messageCSRCallback]: Sending coffee status")
            global queue
            sleep(1)

            #Converts sensor input to percentage
            res = message[2]*256 + message[1]
            res = res * 340
            res = res / 20000
            res = round(res, 1)
            #print "Tamanho: " + str(res)

            total = 8.85
            percent = (res/total)*100
            percent = round(percent)
            percent = 100 - int(percent)
            print "[messageCSRCallback]: Percentage read -> " + str(percent)
            if percent < 0:
                write_char(aws.handle, aws.getByteArray(aws.SEND_GET_COFFEE_LEVEL))
                print "[messageCSRCallback]: Nova leitura sensor HC"
            elif percent > 100:
                write_char(aws.handle, aws.getByteArray(aws.SEND_GET_COFFEE_LEVEL))
                print "[messageCSRCallback]: Nova leitura sensor HC"


            aws.sendCoffeelevel(int(percent))
            aws.stopTimeoutLevelCoffee()


    if len(message) == 4:
        print("[messageCSRCallback]: Sending all sensor update")
        aws.sendUpdate(message)
        aws.stopTimeout()
