from firebase import firebase
import json

firebase_url = 'https://testing-in-rpi-default-rtdb.asia-southeast1.firebasedatabase.app/'

firebase = firebase.FirebaseApplication(firebase_url, None)

# data.json에 읽어온 값 저장
result = firebase.get('/', None)
with open('./data.json', 'w') as file:
    json.dump(result, file)
