import requests
import logging
from pprint import pprint

cost_per_minute = 10.0 / 60
key = 'kBd098lqXzu3mYJOX9YYckorrEy'
users = [
        { 'name' : "sirius", 'rfid' : "b6c7b158", 'minutes' : 0},
        { 'name' : "matt",   'rfid' : "967fa958", 'minutes' : 0},
        { 'name' : "justo",  'rfid' : "d613a458", 'minutes' : 0},
        { 'name' : "guillem",'rfid' : "962fb158", 'minutes' : 0},
        { 'name' : "carlos", 'rfid' : "36d9b058", 'minutes' : 0},
        { 'name' : "oldtag", 'rfid' : "c184932b", 'minutes' : 0}
    ];

filter = 'gt[timestamp]=now%20-30day' 
url = 'http://phant.cursivedata.co.uk/output/%s.json?%s' % (key, filter)
r = requests.get(url)
print("got %d records from %s to %s" % (len(r.json()), r.json()[-1]['timestamp'], r.json()[0]['timestamp']))
total_mins = 0

for record in r.json():
    if record['laser'] == '1':
        found_user = False
        for user in users:
            if user['rfid'] == record['rfid']:
                user['minutes'] += 1
                total_mins += 1
                found_user = True
        if found_user == False:
            print("no such user")

for user in users:
    print("%20s : %4d = %.2f euro" % (user['name'], user['minutes'], user['minutes'] * cost_per_minute))

print("%20s : %4d = %.2f euro" % ("total", total_mins, total_mins * cost_per_minute))
