import requests
import logging
from pprint import pprint
from datetime import datetime
import json
import argparse
import csv

cost_per_minute = 10.0 / 60
key = 'kBd098lqXzu3mYJOX9YYckorrEy'

users = []

with open("users.csv",'r') as fh:
    reader = csv.reader(fh, delimiter=',')
    for row in reader:
        users.append({ 'name': row[0], 'rfid': row[1], 'minutes': 0})

parser = argparse.ArgumentParser(description="fetch and process laser records")
parser.add_argument('--fetch', action='store_const', const=True)
parser.add_argument('--hack', action='store_const', const=True) # for old firmware
args = parser.parse_args()

cutting = False
stopped = False
current_user = None
total_mins = 0
month = 4

filter = 'gt[timestamp]=2017%02d01&lt[timestamp]=2017%02d01' % (month - 1, month)
url = 'http://phant.cursivedata.co.uk/output/%s.csv?%s' % (key, filter)
if args.fetch:
    print("fetching")
    r = requests.get(url)
    with open("rec.csv",'w') as fh:
        fh.write(r.text)
    
#print("got %d records from %s to %s" % (len(records), records[-1]['timestamp'], records[0]['timestamp']))


with open("rec.csv",'r') as fh:
    reader = csv.DictReader(fh, delimiter=',')
    for record in reader:

        # end state
        if record['laser'] == '1':
            for user in users:
                if user['rfid'] == record['rfid']:
                    current_user = user
            current_user['minutes'] += 1
            total_mins += 1


for user in users:
    print("%20s : %4d = %.2f euro" % (user['name'], user['minutes'], user['minutes'] * cost_per_minute))

print("%20s : %4d = %.2f euro" % ("total", total_mins, total_mins * cost_per_minute))

