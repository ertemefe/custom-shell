#! /usr/bin/python3
import sys
import requests

def handle_response(response):
    response = requests.get(complete_url)
    x = response.json()
    if x["cod"] != "404":
        main = x["main"]
        curr_temp = round((main["temp"]-273.15), 2)
        curr_pres = main["pressure"]
        curr_hum = main["humidity"]

        weather = x["weather"]
        curr_main = weather[0]["main"]
        curr_desc = weather[0]["description"]
        
        print(
              " temperature = " + str(curr_temp) + "°C"
            + "\n atmospheric pressure (hPa) = " + str(curr_pres)
            + "\n humidity= %" + str(curr_hum)
            + "\n weather= " + str(curr_main)
            + "\n description = " + str(curr_desc)
            )

base_url = "http://api.openweathermap.org/data/2.5/weather"
api_key = "a8f1f8bfc7692bf1f95e83d75cf139d7"
#https://api.openweathermap.org/data/2.5/weather?q={city name}&appid={API key}
city_name = sys.argv[1]

complete_url = base_url + "?q=" + city_name + "&appid=" + api_key

handle_response(complete_url)