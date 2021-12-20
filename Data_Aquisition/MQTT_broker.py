#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""

    @file     MQTT_broker.py
    @author   Colin Laganier
    @version  V0.5
    @date     2021-12-05
    @brief    This script serves as the MQTT broker running on the Rasperry Pi, receiving the data from the sensor clients and pushing it to the MongoDB database.

"""

from posix import times_result
from time import time
import paho.mqtt.client as mqtt
from pymongo import MongoClient
from pprint import pprint
from time import sleep
import os
import socket
import time
import config
import smtplib


def VerifyData():
    print("verifying data")
    for _elem in data:
        if data[_elem] == -2:
            return False
    return True


def ConvertValues(key, value):
    if key in intKeys:
        return int(value)
    else:
        return float(value)


def on_connect(client, userdata, flags, rc):
    print("Connected with result code " + str(rc))
    client.subscribe([("smellStation/occupancy", 1), ("smellStation/bin1",
                                                      1), ("smellStation/bin2", 1), ("smellStation/H2S", 1), ("smellStation/temperature", 1), ("smellStation/humidity", 1)])
    print("MQTT subscription")


def on_message(client, userdata, message):
    global timestamp, data
    print("Message received: " + message.topic + " : " + str(message.payload))

    _key = message.topic.split("/", 1)[1]
    if (data[_key] == -2 or data[_key] == -1):
        if timestamp == False:
            _timestamp = time.time()
            timestamp = _timestamp
            data["timestamp"] = int(_timestamp)
        data[_key] = ConvertValues(_key, message.payload)
        print(timestamp)
        print(data)
        if VerifyData():
            SendData()
            # ResetData()


def OnLoopVerification():
    VerifyTime()
    try:
        VerifyIPAddress()
    except:
        print("IP Check Failed")
    print("verifying things")

# Verifies if more than 5 minutes have elapsed since the first communication


def VerifyTime():
    global data
    if timestamp != False:
        _currentTime = time.time()
        if (_currentTime - timestamp) > timeThreshold:
            for _elem in data:
                if data[_elem] == -2:
                    data[_elem] = -1
            print("verify time sending")
            SendData()
            # ResetData()

# Sends data to the mongo database


def SendData():
    print("sending")
    db.sensorData.insert_one(data)
    sleep(1)
    ResetData()


def SendEmail(msg):
    server = smtplib.SMTP('smtp.gmail.com', 587)
    server.starttls()
    server.login(config.email_address, config.email_password)
    #msg = "IP Address Changed to " + newAddress
    server.sendmail(config.email_address, config.email_address, msg)
    server.quit()

# Resets the temporary data container and timestamp after it has been sent to the database


def ResetData():
    global data, timestamp
    data = {
        "occupancy": -2,
        "bin1": -2,
        "bin2": -2,
        "H2S": -2,
        "temperature": -2,
        "humidity": -2,
        "timestamp": 0
    }
    timestamp = False

# Gets IP Address from command line call


def GetIPAddress():
    _gw = os.popen("ip -4 route show default").read().split()
    _s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    _s.connect((_gw[2], 0))
    return _s.getsockname()[0]

# Verifies if the current IP is the same as the previously saved one


def VerifyIPAddress():
    _currentIP = GetIPAddress()
    if _currentIP != IPAddress:
        _data = {address: _currentIP, timestamp: time.time()}
        # db.IPAddress.insert_one(_data)
        msg = "IP Address changed to " + _currentIP
        SendEmail(msg)


if __name__ == '__main__':
    try:
        data = {
            "occupancy": -2,
            "bin1": -2,
            "bin2": -2,
            "H2S": -2,
            "temperature": -2,
            "humidity": -2,
            "timestamp": 0
        }

        intKeys = ["occupancy", "H2S"]
        IPAddress = GetIPAddress()
        timestamp = False
        timeThreshold = 500

        # Connection to MongoDB
        client = MongoClient(config.mongo_uri)
        db = client.smellStation
        serverStatusResult = db.command("serverStatus")
        pprint(serverStatusResult)
        print("MongoDB ready")

        # MQTT Setup
        broker_address = "localhost"
        port = 1883
        client = mqtt.Client()  # create new instance
        client.on_connect = on_connect  # attach function to callback
        client.on_message = on_message
        # client.on_loop = on_loop
        client.connect(broker_address, port=port)
        while True:
            OnLoopVerification()
            client.loop()
        # client.loop_forever()
    except Exception as e:
        SendEmail(e)
