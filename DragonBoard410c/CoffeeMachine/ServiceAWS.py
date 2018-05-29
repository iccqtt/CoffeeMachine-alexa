import PyGattBLE as gatt
import ClientAWS
import Utils as utils
import json
import AlexaService
import ServiceHTTP as Http

TOPIC_TURN_ON_OFF = "qualcomm/CoffeeMachine/TurnOnOff"
TOPIC_SHORT_COFFEE = "qualcomm/CoffeeMachine/ShortCoffee"
TOPIC_LONG_COFFEE = "qualcomm/CoffeeMachine/LongCoffee"
TOPIC_LEVEL_COFFEE = "qualcomm/CoffeeMachine/CoffeeLevel"
TOPIC_LEVEL_WATER = "qualcomm/CoffeeMachine/WaterLevel"
TOPIC_GLASS_POSITION = "qualcomm/CoffeeMachine/GlassPosition"
TOPIC_UPDATE = "qualcomm/CoffeeMachine/Update"
TOPIC_ERROR = "qualcomm/CoffeeMachine/Error"
TOPIC_ANDROIDHANDLER = 'qualcomm/CoffeeMachine/TurnMachine/Android'
TOPIC_DROIDCOFFEE = 'qualcomm/CoffeeMachine/MakeCoffee/Android'


SEND_OFF = 0x00
SEND_ON = 0x01
SEND_SHORT_COFFEE = 0x02
SEND_LONG_COFFEE = 0x03
SEND_GET_WATER_LEVEL = 0x04
SEND_GET_COFFEE_LEVEL = 0x05
SEND_GET_GLASS_POSITION = 0x06
SEND_UPDATE = 0x07

ON = '1'
OFF = '0'
IOT = "/IoT"
ANDROID = "/Android"

MACHINE_IN_USE = "coffee_in_progress"

client = None

handle = 16

timeoutOnOff = None
timeoutShortCoffee = None
timeoutShortCoffeeInUse = None
timeoutLongCoffeeInUse = None
timeoutLongCoffee = None
timeoutLevelCoffee = None
timeoutLevelWater = None
timeoutGlassPostion = None
timeout = None

coffeeDoing = None

flag_error = 0

def getByteArray(message):
    i = [message]
    data = bytearray(i)
    return data

def turnOnOffCallback(client, userdata, message):
    #print("[turnOnOffCallback]: "+message.payload)
    jsonData = json.loads(message.payload)
    data = jsonData['state']

    if data == ON:
        if coffeeDoing is None:
            global timeoutOnOff
            timeoutOnOff = utils.time_thread()

            if timeoutOnOff is not None:
                timeoutOnOff.topic(message.topic)
                timeoutOnOff.start()

            print ("[turnOnOffCallback]: Turning on the coffee machine")
            gatt.write_char(handle, getByteArray(SEND_ON))

        else:
            sendOnOff("busy")
            print "The coffee machine is busy!"


    elif data == OFF:
        if coffeeDoing is None:
            global timeoutOnOff
            timeoutOnOff = utils.time_thread()

            if timeoutOnOff is not None:
                timeoutOnOff.topic(message.topic)
                timeoutOnOff.start()

            gatt.write_char(handle, getByteArray(SEND_OFF))
            print ("[turnOnOffCallback]: Turning off the coffee machine")
        else:
            sendOnOff("busy")
            print "The coffee machine is busy!"

    else:
        global timeout

        timeout = utils.time_thread()

        if timeout is not None:
            timeout.topic(message.topic)
            timeout.start()

        print ("[turnOnOffCallback]: Sending machine status")
        gatt.write_char(handle, getByteArray(SEND_UPDATE))


def sendOnOff(message):
    client.publish(TOPIC_TURN_ON_OFF + IOT, message, 0)
    Http.sendOnOff(message)

def stopTimeoutOnOff():
    global timeoutOnOff
    if timeoutOnOff is not None:
        #print "Stopping on/off..."
        timeoutOnOff.stop()

def shortCoffeeCallback(client, userdata, message):
    global timeoutShortCoffee, timeoutShortCoffeeInUse, coffeeDoing

    jsonData = json.loads(message.payload)
    data = jsonData['state']

    if coffeeDoing is None:
        coffeeDoing = 1
        timeoutShortCoffee = utils.time_thread()
        timeoutShortCoffee.setClient(data)
        #print timeoutShortCoffee.client
        timeoutShortCoffee.topic(message.topic)
        timeoutShortCoffee.start()

        print "[shortCoffeeCallback] Received short coffee request."
        gatt.write_char(handle, getByteArray(SEND_SHORT_COFFEE))

    else:

        timeoutShortCoffeeInUse = data
        sendShortCoffee(MACHINE_IN_USE)

def sendShortCoffee(message):
    if message == MACHINE_IN_USE:
        client.publish(TOPIC_LONG_COFFEE + IOT, timeoutShortCoffeeInUse, 0)
    else:
        client.publish(TOPIC_LONG_COFFEE + IOT, message, 0)
    levelCoffeeCallback(None, None, None)

def stopTimeoutShortCoffee():
    global timeoutShortCoffee
    if timeoutShortCoffee is not None:
        #print "Stopping short coffee..."
        timeoutShortCoffee.stop()

def longCoffeeCallback(client, userdata, message):
    global timeoutLongCoffee, timeoutLongCoffeeInUse, coffeeDoing

    jsonData = json.loads(message.payload)
    data = jsonData['state']

    if coffeeDoing is None:
        coffeeDoing = 1
        timeoutLongCoffee = utils.time_thread()

        timeoutLongCoffee.setClient(data)
        #print timeoutLongCoffee.client
        timeoutLongCoffee.topic(message.topic)
        timeoutLongCoffee.start()

        print "[longCoffeeCallback] Received long coffee."
        gatt.write_char(handle, getByteArray(SEND_LONG_COFFEE))

    else:
        timeoutLongCoffeeInUse = data
        sendLongCoffee(MACHINE_IN_USE)


def sendLongCoffee(message):
    if message == MACHINE_IN_USE:
        client.publish(TOPIC_LONG_COFFEE + IOT, timeoutLongCoffeeInUse, 0)
    else:
        client.publish(TOPIC_LONG_COFFEE + IOT, message, 0)
    levelCoffeeCallback(None, None, None)

def stopTimeoutLongCoffee():
    global timeoutLongCoffee
    if timeoutLongCoffee is not None:
        #print "Stopping long coffee..."
        timeoutLongCoffee.stop()

def levelCoffeeCallback(client, userdata, message):
    global timeoutLevelCoffee
    timeoutLevelCoffee = utils.time_thread()

    if timeoutLevelCoffee is not None:
        if client is not None:
            timeoutLevelCoffee.topic(message.topic)
        timeoutLevelCoffee.start()

    gatt.write_char(handle, getByteArray(SEND_GET_COFFEE_LEVEL))
    print "[levelCoffeeCallback]: Received coffee levels"

def sendCoffeelevel(level):
    print "Sending coffee level: " + str(level)
    Http.sendCoffeelevel(level)
    client.publish(TOPIC_LEVEL_COFFEE + IOT, level, 0)
    Http.sendCoffeelevel(level)


def stopTimeoutLevelCoffee():
    global timeoutLevelCoffee
    if timeoutLevelCoffee is not None:
        #print "Stopping level coffee"
        timeoutLevelCoffee.stop()

def levelWaterCallback(client, userdata, message):
    global timeoutLevelWater
    timeoutLevelWater = utils.time_thread()

    if timeoutLevelWater is not None:
        timeoutLevelWater.topic(message.topic)
        timeoutLevelWater.start()

    gatt.write_char(handle, getByteArray(SEND_GET_WATER_LEVEL))
    #print "Received level water"

def sendLevelWater(level):
    print "Sending water level: "+str(level)
    client.publish(TOPIC_LEVEL_WATER + IOT, level, 0)
    Http.sendLevelWater(level)


def stopTimeoutLevelWater():
    global timeoutLevelWater
    if timeoutLevelWater is not None:
        #print "Stopping water"
        timeoutLevelWater.stop()

def glassPositionCallback(client, userdata, message):
    global timeoutGlassPostion
    timeoutGlassPostion = utils.time_thread()

    if timeoutGlassPostion is not None:
        timeoutGlassPostion.topic(message.topic)
        timeoutGlassPostion.start()

    gatt.write_char(handle, getByteArray(SEND_GET_GLASS_POSITION))

def sendGlassPostion(position):
    client.publish(TOPIC_GLASS_POSITION + IOT, position, 0)
    Http.sendGlassPosition(position)
    print "Sending glass position: "+str(position)

def stopTimeoutGlassPostion():
    global timeoutGlassPostion
    if timeoutGlassPostion is not None:
        #print "Stopping cup"
        timeoutGlassPostion.stop()

def updateCallback(client, userdata, message):
    global timeout
    timeout = utils.time_thread()

    if timeout is not None:
        if message is not None:
            timeout.topic(message.topic)
        timeout.start()

    gatt.write_char(handle, getByteArray(SEND_UPDATE))


def sendUpdate(message):
    sendOnOff(message[1])
    sendLevelWater(message[2])
    sendGlassPostion(message[3])
    Http.updateStatus(message)
    levelCoffeeCallback(None, None, None)


def sendError(message):
    global coffeeDoing, flag_error
    coffeeDoing = None
    #print sendError
    if (message != None):
        print "[sendError]: Sending -> "+message

    if(message == "connection_error" or message =="write_char_error"):
        if flag_error == 0:
            flag_error = 1
            print "Sending error..."
            client.publish(TOPIC_ERROR + IOT, message, 0)
    elif(message == "reconnected"):
        flag_error = 0
        client.publish(TOPIC_ERROR + IOT, message, 0)

def stopTimeout():
    global timeout
    if timeout is not None:
        #print "Stopping stat update..."
        timeout.stop()

def androidCoffeeCallback(client, userdata, message):
	#print('[androidCoffeeCallback]: '+message.payload)
	if message.payload == '1':
		print('[androidCoffeeCallback]: Received long coffee request')
		prerec = open('make_long.wav', 'r')
		AlexaService.alexa_speech_recognizer(prerec)
	elif message.payload == '0':
		print('[androidCoffeeCallback]: Received short coffee request')
		prerec = open('make_short.wav', 'r')
		AlexaService.alexa_speech_recognizer(prerec)

def androidMachineHandlerCallback(client, userdata, message):
	#print('[androidMachineHandlerCallback]: '+message.payload)

	if message.payload == '1':
		print('[androidMachineHandlerCallback]: Turning on coffee machine')
		prerec = open('turn_on.wav', 'r')
		AlexaService.alexa_speech_recognizer(prerec)

	elif message.payload == '0':
		print('[androidMachineHandlerCallback]: Turning off coffee machine')
		prerec = open('turn_off.wav', 'r')
		AlexaService.alexa_speech_recognizer(prerec)


def subscribeTopics(client):
    # Register the client to a topic and callback

	client.subscribe(TOPIC_TURN_ON_OFF + ANDROID, 1, turnOnOffCallback)
	client.subscribe(TOPIC_SHORT_COFFEE + ANDROID, 1, shortCoffeeCallback)
	client.subscribe(TOPIC_LONG_COFFEE + ANDROID, 1, longCoffeeCallback)
	client.subscribe(TOPIC_LEVEL_COFFEE + ANDROID, 1, levelCoffeeCallback)
	client.subscribe(TOPIC_LEVEL_WATER + ANDROID, 1, levelWaterCallback)
	client.subscribe(TOPIC_GLASS_POSITION + ANDROID, 1, glassPositionCallback)
	client.subscribe(TOPIC_UPDATE + ANDROID, 1, updateCallback)

	client.subscribe(TOPIC_DROIDCOFFEE, 1, androidCoffeeCallback)
	client.subscribe(TOPIC_ANDROIDHANDLER, 1, androidMachineHandlerCallback)

def createClient():

    global client
    client = ClientAWS.createClient()

    return client
