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

filter = 'gt[timestamp]=20170201' 
url = 'http://phant.cursivedata.co.uk/output/%s.json?%s' % (key, filter)
if args.fetch:
    print("fetching")
    r = requests.get(url)
    records = r.json()
    with open("rec.json",'w') as fh:
        json.dump(records, fh)
else:
    print("loading cached data")
    with open("rec.json") as fh:
        records = json.load(fh)
    
print("got %d records from %s to %s" % (len(records), records[-1]['timestamp'], records[0]['timestamp']))
total_mins = 0

cutting = False
stopped = False
prev_record = None
current_user = None

for record in records:
#    print(record["rfid"], record['laser'], record['timestamp'])
    # start state
    if not cutting and record['laser'] == '1':
        for user in users:
            if user['rfid'] == record['rfid']:
                cutting = True
                start_time = record['timestamp']
                current_user = user
                #print("session started for %s at %s" % (current_user['name'], start_time))
        if current_user == None:
            print("no such user")

    if cutting and (record['rfid'] != current_user['rfid']):
        if record['rfid'] != "":
            print("error")

    # hack for non finished records (remove after Feb)
    end = datetime.strptime(record['timestamp'], "%Y-%m-%dT%H:%M:%S.%fZ")
    if prev_record:
        prev_record_end = datetime.strptime(prev_record['timestamp'], "%Y-%m-%dT%H:%M:%S.%fZ")
        delta = prev_record_end - end
        minutes = int(delta.total_seconds() / 60)
        if cutting and minutes > 5:
            print minutes
            end = prev_record_end
            stopped = True

    # end state
    if cutting and record['laser'] == '0':
        stopped = True

    if stopped:
        #2017-01-16T14:42:22.852Z
        start = datetime.strptime(start_time, "%Y-%m-%dT%H:%M:%S.%fZ")

        delta = start - end
        minutes = int(delta.total_seconds() / 60)
        current_user['minutes'] += minutes
        print("session for %10s started at %s was %d minutes" % (current_user['name'], start_time, minutes))
        
        current_user = None
        cutting = False
        stopped = False

    prev_record = record

for user in users:
    print("%20s : %4d = %.2f euro" % (user['name'], user['minutes'], user['minutes'] * cost_per_minute))

print("%20s : %4d = %.2f euro" % ("total", total_mins, total_mins * cost_per_minute))

