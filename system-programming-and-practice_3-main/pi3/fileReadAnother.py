from firebase import firebase
import json

firebase_url = 'https://my-weather-rpi-default-rtdb.asia-southeast1.firebasedatabase.app/'

firebase = firebase.FirebaseApplication(firebase_url, None)

result = firebase.get('/', None)

with open('./data.json', 'w') as file:
    json.dump(result, file)
    