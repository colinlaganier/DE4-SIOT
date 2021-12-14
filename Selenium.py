#!/usr/bin/env python

from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver import DesiredCapabilities
from time import sleep
from paho.mqtt import client as mqtt_client
import config
import random
from datetime import datetime

broker = '146.169.220.92'
# broker = "localhost"
port = 1883
topic = "smellStation/occupancy"
# generate client ID with pub prefix randomly
client_id = f'python-mqtt-{random.randint(0, 1000)}'
# username = 'emqx'
# password = 'public'


def ConnectMQTT():
    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
        else:
            print("Failed to connect, return code %d\n", rc)

    client = mqtt_client.Client(client_id)
    # client.username_pw_set(username, password)
    client.on_connect = on_connect
    client.connect(broker, port)
    return client


def Publish(client):

    current_occupancy = GetOccupancy()
    # if current_occupancy != -1:
    result = client.publish(topic, current_occupancy)
    # result: [0, 1]
    status = result[0]
    if status == 0:
        print(f"Sent message - sleep for 60 seconds")
        sleep(60)
        print("wake")
    else:
        print(f"Failed to send message")


def PublishExpected():
    current_time = datetime.now()
    if (current_time.minute % 10 == 0):
        return True
    else:
        return False


def MQTT():
    client = ConnectMQTT()
    # client.loop_start()
    if PublishExpected():
        Publish(client)


def GetOccupancy():
    attempt = 0
    opts = webdriver.ChromeOptions()
    d = DesiredCapabilities.CHROME
    browser = webdriver.Chrome(desired_capabilities=d, options=opts)
    for attempt in range(5):
        try:
            browser.get(config.url)
            sleep(5)
            login_elem = browser.find_elements(
                By.XPATH, "//*[@class='align-center background-green display-block clickable mb4 px2 py3 font-size-3 font-normal']")[0]
            login_elem.click()
            sleep(5)
            browser.find_element(By.ID, "username").send_keys(config.username)
            browser.find_element(
                By.ID, "password_label").send_keys(config.password)
            browser.find_element(By.ID, "submitButton").submit()
            sleep(5)
            occupancy = browser.find_element(By.CSS_SELECTOR,
                                             "#root > div > div > main > div > div.flex__1.overflow-auto.mb3 > div > div:nth-child(4) > div > div > div > div > span").text
            print(occupancy)
            browser.quit()
            return occupancy
        except:
            attempt += 1
            if attempt == 5:
                browser.quit()
                print("Scraping failed too many times")
                # SendWarningEmail()
                return -1


if __name__ == '__main__':
    # GetOccupancy()
    while True:
        MQTT()
