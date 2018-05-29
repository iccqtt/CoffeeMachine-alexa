import threading
import time
import ServiceAWS as aws
import PyGattBLE as gatt
import pygatt
from time import sleep

class time_thread (threading.Thread):
    def __init__(self):
        super(time_thread, self).__init__()
        self._stop = threading.Event()
        self.stopping = None
        self.message = None
        self.status = None
        self.client = None
    def run(self):
        #print "Starting "
        time.sleep(10)
        if self.stopping is None:
        #    print "Error"
            aws.sendError(self.message)
        self.stopping = None
        self.status = None
        #print "Exiting "

    def stop(self):
        self.stopping = 1
        self._stop.set()

    def start(self):
        threading.Thread.start(self)
        self.status = 1

    def topic(self, message):
        self.message = message

    def setClient(self, client):
        self.client = client

class Keep_alive(threading.Thread):
    def __init__(self):
        super(Keep_alive, self).__init__()

    def run(self):
        while True:
            time.sleep(5)
            print "[bluetooth]: Keep Alive"
            data = aws.getByteArray(10)
            gatt.send_keepalive(aws.handle, data)

class connect_ble(threading.Thread):
    def __init__(self):
        super(connect_ble, self).__init__()

    def run(self):
        threading.Thread.run(self)

    def connect(self):
        sleep(0.5)
        adapter = pygatt.GATTToolBackend()
        adapter.start()
        device = adapter.connect('00:02:5B:00:15:10', 10)
        return device
