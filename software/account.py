#!/usr/bin/python
import requests
import logging
from pprint import pprint
from datetime import datetime, date
import calendar
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
parser.add_argument('--filename', default="rec.csv")
parser.add_argument('--month', type=int, default=0)
args = parser.parse_args()

cutting = False
stopped = False
current_user = None
total_mins = 0
year = 2017

if args.fetch:
    if args.month == 0:
        print("specify a --month")
        exit(1)
    print("fetching for month %d" % args.month)
    _, num_days = calendar.monthrange(year, args.month)
    first_day = date(year, args.month, 1)
    last_day = date(year, args.month, num_days)
    filter = 'gt[timestamp]=%s&lt[timestamp]=%s' % (first_day.strftime('%Y%m%d'), last_day.strftime('%Y%m%d'))
    
    print(filter)
    url = 'http://phant.cursivedata.co.uk/output/%s.csv?%s' % (key, filter)
    r = requests.get(url)
    with open("rec.csv",'w') as fh:
        fh.write(r.text)
    
#print("got %d records from %s to %s" % (len(records), records[-1]['timestamp'], records[0]['timestamp']))


current_user = None
cutting = False
start = None

with open(args.filename,'r') as fh:
    reader = csv.DictReader(fh, delimiter=',')
    for record in reader:

        # start case
        if not cutting and record['laser'] == '1':
            cutting = True
            # find user
            for user in users:
                if user['rfid'] == record['rfid']:
                    current_user = user
            assert current_user is not None
            start = datetime.strptime(record['timestamp'], "%Y-%m-%dT%H:%M:%S.%fZ")
        
        # end case
        if cutting == True and record['laser'] == '0':
            cutting = False;
            end = datetime.strptime(record['timestamp'], "%Y-%m-%dT%H:%M:%S.%fZ")
            delta = start - end
            minutes = int(delta.total_seconds() / 60)
            current_user['minutes'] += minutes
#            print("logging %4d minutes (total %4d) for %10s at %s" % (minutes, current_user['minutes'], current_user['name'], start))
            total_mins += minutes

for user in users:
    print("%20s : %4d = %.2f euro" % (user['name'], user['minutes'], user['minutes'] * cost_per_minute))

print("%20s : %4d = %.2f euro" % ("total", total_mins, total_mins * cost_per_minute))

