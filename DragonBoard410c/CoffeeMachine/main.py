import PyGattBLE, ServiceAWS
from Utils import Keep_alive
import time
import AlexaService

if __name__ == "__main__":

    time.sleep(5)

    # Create Amazon IOT client
    client = ServiceAWS.createClient()
    client.connect()

    #Subscribe all topics
    ServiceAWS.subscribeTopics(client)

    # Connect to CSR bluetooth device
    device = PyGattBLE.getConnection("connection_error")

    if device is None:
        print "Error to connect "

    alive = Keep_alive()
    alive.start()

    # Start up AlexaPi service
    AlexaService.setupAlexaService()

    # Notify user about program being ready to work
    AlexaService.readySound()

    ServiceAWS.updateCallback(None, None, None)



