from flask import Flask, request
import csv
import numpy as np
from datetime import datetime
import math

app = Flask(__name__)

CSV_FILE = 'temp.csv'
current_m = 0.0
current_c = 22.0


def retrain_ai():
    global current_m, current_c
    times = []
    temps = []

    try:
        with open(CSV_FILE, 'r') as f:
            reader = csv.reader(f)
            next(reader, None)  # Skip header

            for row in reader:
                if not row: continue
                try:
                    # 1. READ TIME (Handle both 23:45 and 1425 formats)
                    if ':' in str(row[0]):
                        h, m = map(int, row[0].split(':'))
                        minutes = h * 60 + m
                    else:
                        minutes = float(row[0])  # Read as number

                    # 2. READ TEMP (Crucial: Skip if it says 'nan')
                    temp_val = float(row[1])
                    if math.isnan(temp_val):
                        continue  # SKIP BAD DATA

                    times.append(minutes)
                    temps.append(temp_val)
                except ValueError:
                    continue

        if len(set(times)) > 1:
            current_m, current_c = np.polyfit(times, temps, 1)
            print(f"--> AI UPDATED: y = {current_m:.4f}x + {current_c:.2f}")
        else:
            print("(!) Waiting for more valid data points...")

    except Exception as e:
        print(f"Error training: {e}")


@app.route('/update', methods=['GET'])
def send_model():
    # Never send 'nan' to Arduino. Send defaults if AI is broken.
    if math.isnan(current_m) or math.isnan(current_c):
        return "0.0,22.0"
    return f"{current_m},{current_c}"


@app.route('/feedback', methods=['POST'])
def get_feedback():
    try:
        data = request.data.decode('utf-8')
        action, current_temp_str = data.split(',')

        # SAFETY CHECK: Did Arduino send 'nan'?
        if 'nan' in current_temp_str.lower():
            print("(!) BLOCKED: Arduino sent 'nan'. Ignoring.")
            return "Bad Data", 400

        current_temp = float(current_temp_str)
        new_target = current_temp - 1.0 if action == 'hot' else current_temp + 1.0

        now = datetime.now()
        minutes_now = now.hour * 60 + now.minute

        # Append safe data
        with open(CSV_FILE, 'a', newline='') as f:
            writer = csv.writer(f)
            writer.writerow([minutes_now, round(new_target, 2)])

        print(f"Feedback Logged: {new_target}°C at minute {minutes_now}")
        retrain_ai()
        return "OK"
    except Exception as e:
        print(f"Error: {e}")
        return "Error", 400


if __name__ == '__main__':
    retrain_ai()
    app.run(host='0.0.0.0', port=5000)