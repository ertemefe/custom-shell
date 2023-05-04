#!/usr/bin python3

import requests
from bs4 import BeautifulSoup


def main():
    response = requests.get("https://www.bbc.com/news")
    soup = BeautifulSoup(response.content, 'html.parser')

    # bbc news most read
    div = soup.find(id="orb-modules")
    div = div.find(id="latest-stories-tab-container")
    div = div.find(class_="nw-c-most-read gs-t-news gs-u-box-size no-touch b-pw-1280")

    print("Most Read Today on BBC News: ")
    i = 1
    for link in div.find_all('a'):
        print(str(i) + " " + link.get_text() + " - https://www.bbc.com" + link.get('href'))
        i += 1

if __name__ == "__main__":
    main()