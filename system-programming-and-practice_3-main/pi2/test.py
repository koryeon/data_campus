from firebase import firebase
import json

firebase_url = 'https://my-weather-rpi-default-rtdb.asia-southeast1.firebasedatabase.app/'

 # Firebase 객체를 생성합니다.          fb.post(path here, data here)       fb.get(path here)       fb.patch(path here, new data here)
fb = firebase.FirebaseApplication(firebase_url, None)

with open('/home/pi/sensor_data.json', 'r') as file:
    data = json.load(file)

#fb.post('/data', {'message': 'Hello, Firebase!'})

#result = fb.get('/', None)
fb.patch('/', data)

result = fb.get('/', None)

print("patching to firebase is done!")