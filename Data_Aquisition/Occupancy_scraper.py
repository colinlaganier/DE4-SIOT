#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""

    @file     Occupancy_scraper.py
    @author   Colin Laganier
    @version  V0.3
    @date     2021-12-05
    @brief    This script scrapes the Imperial Occupancy platform to retrieve the occupancy of Level 2.
"""

from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver import DesiredCapabilities
from time import sleep
from paho.mqtt import client as mqtt_client
import config
import random
from datetime import datetime

broker = config.IPAddress
port = 1883
topic = "smellStation/occupancy"
# generate client ID with pub prefix randomly
client_id = f'python-mqtt-{random.randint(0, 1000)}'


def ConnectMQTT():
    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print("Connected to MQTT Broker!")
        else:
            print("Failed to connect, return code %d\n", rc)

    client = mqtt_client.Client(client_id)
    # client.username_pw_set(config.username, config.password)
    client.on_connect = on_connect
    client.connect(broker, port)
    return client


def Publish(client):

    current_occupancy = GetOccupancy()
    result = client.publish(topic, current_occupancy)
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
    while True:
        MQTT()
