from flask import Flask, Response, send_file
from picamera2 import Picamera2
import cv2, time

app = Flask(__name__)
picam2 = Picamera2()
picam2.start()

def gen_frames():
    while True:
        frame = picam2.capture_array()
        ret, buffer = cv2.imencode('.jpg', frame)
        if not ret:
            continue
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + buffer.tobytes() + b'\r\n')

@app.route('/video')
def video():
    return Response(gen_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/capture')
def capture():
    filename = f"capture_{int(time.time())}.jpg"
    frame = picam2.capture_array()
    cv2.imwrite(filename, frame)
    return send_file(filename, mimetype='image/jpeg')

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000)