from flask import Flask, render_template, request, jsonify, send_file, abort
from flask_socketio import SocketIO
from datetime import datetime, timedelta
import sqlite3
import os
import cv2
import requests
import numpy as np
from PIL import Image
from io import BytesIO
from ultralytics import YOLO
import easyocr
import re
import time

# ======================================
# 1. CONFIGURATION
# ======================================
AI_SERVER_IP = "0.0.0.0"  # Nghe trên mọi IP trong mạng LAN
PORT = 5000

# IP Camera (ESP32-CAM)
CAM_ENTRY_IP = "172.31.106.40"

CAM_EXIT_IP  = "172.31.106.41"

# Đường dẫn file
CAPTURE_FOLDER = "static/captures"
DB_FILE = "database.db"
YOLO_MODEL_PATH = "models/best.pt"

# Tạo thư mục lưu ảnh
os.makedirs(CAPTURE_FOLDER, exist_ok=True)

# Khởi tạo Flask & SocketIO
app = Flask(__name__, static_folder="static", template_folder="templates")
app.config['SECRET_KEY'] = 'secret!'
socketio = SocketIO(app, cors_allowed_origins="*")

# ======================================
# 2. LOAD AI MODELS
# ======================================
print("--- LOADING AI MODELS ---")
model = None
try:
    if os.path.exists(YOLO_MODEL_PATH):
        print(f"[YOLO] Loading custom model: {YOLO_MODEL_PATH}")
        model = YOLO(YOLO_MODEL_PATH)
    else:
        print("[YOLO] Custom model not found, loading standard yolov8n.pt...")
        model = YOLO("yolov8n.pt")
except Exception as e:
    print(f"[YOLO ERROR] {e}")

reader = None
try:
    print("[OCR] Loading EasyOCR...")
    reader = easyocr.Reader(['en'], gpu=False)
except Exception as e:
    print(f"[OCR ERROR] {e}")

# ======================================
# 3. DATABASE HELPER
# ======================================
def get_db_connection():
    conn = sqlite3.connect(DB_FILE, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    """Khởi tạo database với cấu trúc mới nhất (có RFID)"""
    with get_db_connection() as conn:
        c = conn.cursor()
        
        # Bảng lịch sử ra vào
        c.execute("""
            CREATE TABLE IF NOT EXISTS parking_log (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                plate TEXT,
                rfid_uid TEXT,
                entry_time TEXT,
                exit_time TEXT,
                fee REAL,
                image_path TEXT,
                status TEXT
            )
        """)
        
        # Bảng xe đăng ký vé tháng
        c.execute("""
            CREATE TABLE IF NOT EXISTS registered_vehicles (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                plate TEXT UNIQUE,
                vehicle_type TEXT,
                owner TEXT,
                expiry_date TEXT
            )
        """)
        conn.commit()
    print("[DB] Database initialized.")

# Gọi khởi tạo ngay khi chạy
init_db()

# ======================================
# 4. LOGIC FUNCTIONS
# ======================================

def normalize_plate(text):
    """Chuẩn hóa biển số xe: Viết hoa và bỏ ký tự đặc biệt"""
    if not text: return None
    
    # 1. Chuyển thành chữ hoa
    plate = text.upper()
    
    # 2. Chỉ giữ lại Chữ (A-Z) và Số (0-9), bỏ hết dấu chấm, gạch ngang, khoảng trắng
    plate = re.sub(r'[^A-Z0-9]', '', plate) 

    # 3. Kiểm tra độ dài cơ bản (Biển VN thường từ 7-9 ký tự sau khi bỏ dấu)
    if len(plate) < 6 or len(plate) > 12:
        return None
        
    return plate

def calculate_fee(entry_str, exit_str=None):
    """Tính phí gửi xe"""
    if not entry_str: return 0, None
    
    fmt = "%Y-%m-%d %H:%M:%S"
    t1 = datetime.strptime(entry_str, fmt)
    
    if exit_str:
        t2 = datetime.strptime(exit_str, fmt)
    else:
        t2 = datetime.now()
        exit_str = t2.strftime(fmt)

    duration = (t2 - t1).total_seconds() / 60.0 # Phút
    
    # Logic: Miễn phí 15p đầu, sau đó 100đ/phút
    if duration <= 15:
        return 0, exit_str
    
    money = round(duration * 100) 
    return money, exit_str
def smart_capture_loop(cam_ip, label_prefix, max_retries=3):
    """
    Chụp liên tiếp tối đa 3 ảnh.
    Nếu ảnh nào đọc được biển số thì DỪNG NGAY và trả về kết quả.
    """
    final_plate = "UNKNOWN"
    final_img_path = None
    
    print(f"[SMART CAM] Bắt đầu chu trình chụp liên tiếp ({max_retries} shots)...")

    for i in range(max_retries):
        print(f"--- SHOT {i+1}/{max_retries} ---")
        
        # 1. Chụp ảnh (Dùng hàm capture nhanh)
        img_path = capture_and_save(cam_ip, f"{label_prefix}_shot{i+1}")
        
        if not img_path:
            continue # Lỗi mạng, thử lại
            
        # 2. Nhận diện
        plate, status = process_plate_ai(img_path)
        
        # Lưu lại đường dẫn ảnh mới nhất để hiển thị web (dù có đọc được hay không)
        final_img_path = img_path
        
        # 3. Kiểm tra kết quả
        if plate:
            print(f"[SMART CAM] => ĐÃ TÌM THẤY: {plate} (Dừng chụp)")
            return plate, final_img_path # Thành công -> Thoát vòng lặp ngay
        else:
            print(f"[SMART CAM] => Shot {i+1} thất bại/mờ. Thử lại...")
            # Chờ 1 chút xíu để xe nhích vị trí hoặc camera ổn định lại
            time.sleep(0.2) 

    print("[SMART CAM] => Đã hết lượt thử. Không đọc được biển.")
    return "UNKNOWN", final_img_path
def is_plate_registered(plate):
    """Kiểm tra biển số có trong danh sách đăng ký không"""
    if not plate: return False
    with get_db_connection() as conn:
        row = conn.execute("SELECT * FROM registered_vehicles WHERE plate = ?", (plate,)).fetchone()
        if row: return True # Có thể thêm logic check expiry_date tại đây
    return False

def capture_and_save(cam_ip, label):
    """
    Chụp ảnh từ Camera IP và lưu file.
    Phiên bản tối ưu cho ESP32-CAM "No Delay".
    """
    url = f"http://{cam_ip}/capture"
    filename = f"{label}_{datetime.now().strftime('%Y%m%d_%H%M%S')}.jpg"
    filepath = os.path.join(CAPTURE_FOLDER, filename)

    print(f"[CAM] Requesting {url}...")
    
    # Thử 3 lần, nhưng thử rất nhanh
    for attempt in range(3):
        try:
            # Timeout 2s là đủ vì mạng LAN rất nhanh
            resp = requests.get(url, timeout=2) 
            
            if resp.status_code == 200:
                with open(filepath, 'wb') as f:
                    f.write(resp.content)
                # Chụp thành công -> Thoát ngay lập tức
                return filepath
        except Exception as e:
            # Nếu lỗi mạng, chờ xíu rồi thử lại (0.1s)
            print(f"[CAM] Retry {attempt+1} due to error: {e}")
            time.sleep(0.1) 
            
    print(f"[CAM] Failed to capture from {cam_ip}")
    return None

def process_plate_ai(image_path):
    """
    Xử lý AI: YOLO Detect -> Crop -> Upscale -> Gray -> EasyOCR
    """
    if not model or not reader:
        print("[AI] Model chưa sẵn sàng")
        return None, "AI_NOT_READY"
    
    # 1. Đọc ảnh
    frame = cv2.imread(image_path)
    if frame is None: return None, "READ_ERR"

    # 2. YOLO Detect
    # conf=0.25: Giảm ngưỡng tự tin xuống để bắt được biển số dễ hơn
    results = model.predict(frame, conf=0.25, verbose=False)
    boxes = results[0].boxes

    if len(boxes) == 0:
        print("[AI] Không thấy đối tượng nào (No bounding box)")
        return None, "NO_DETECTION"

    # Lấy box có độ tin cậy cao nhất
    best_box = max(boxes, key=lambda b: float(b.conf[0]))
    x1, y1, x2, y2 = map(int, best_box.xyxy[0].tolist())
    
    # Cắt ảnh biển số
    plate_img = frame[y1:y2, x1:x2]

    # --- BƯỚC NÂNG CẤP XỬ LÝ ẢNH ---
    
    # 3. Phóng to ảnh (Upscale) - Rất quan trọng cho EasyOCR
    # Tăng kích thước lên gấp 3 lần
    plate_img = cv2.resize(plate_img, None, fx=3, fy=3, interpolation=cv2.INTER_CUBIC)

    # 4. Chuyển sang thang độ xám (Grayscale)
    gray = cv2.cvtColor(plate_img, cv2.COLOR_BGR2GRAY)
    
    # 5. Tăng độ tương phản (Tùy chọn: Dùng GaussianBlur để giảm nhiễu hạt)
    gray = cv2.GaussianBlur(gray, (5, 5), 0)

    # Lưu ảnh đã xử lý ra để kiểm tra (Debug)
    debug_path = image_path.replace(".jpg", "_debug.jpg")
    cv2.imwrite(debug_path, gray)
    # -------------------------------

    # 6. Đọc OCR (Thêm allowlist để chỉ đọc chữ và số)
    try:
        ocr_res = reader.readtext(gray, detail=0, allowlist='0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.-')
    except Exception as e:
        print(f"[OCR ERR] {e}")
        return None, "OCR_FAIL"
    
    if not ocr_res:
        print(f"[OCR] Không đọc được chữ nào trong box {x1,y1,x2,y2}")
        return None, "OCR_EMPTY"
    
    # Ghép các từ lại (VD: đọc ra ['76A', '222.22'] -> '76A22222')
    raw_text = "".join(ocr_res)
    print(f"[OCR RAW] Đọc được: '{raw_text}'") # In ra xem nó đọc được gì

    final_plate = normalize_plate(raw_text)
    
    if final_plate:
        print(f"[AI SUCCESS] Biển số chuẩn: {final_plate}")
        return final_plate, "SUCCESS"
    else:
        print(f"[AI FAIL] Chuẩn hóa thất bại từ: {raw_text}")
        return None, "INVALID_FORMAT"
def emit_realtime(event, payload):
    socketio.emit(event, payload)

# ======================================
# 5. WEB ROUTES (UI)
# ======================================

@app.route('/')
def dashboard():
    return render_template('dashboard.html')

@app.route('/history')
def history_page():
    return render_template('history.html')

@app.route('/registered')
def registered_page():
    return render_template('registered.html')

@app.route('/captures/<path:filename>')
def serve_capture(filename):
    return send_file(os.path.join(CAPTURE_FOLDER, filename))

# ======================================
# 6. API ROUTES (QUẢN LÝ DỮ LIỆU)
# ======================================
# ======================================
# API ROUTES (QUẢN LÝ DỮ LIỆU)
# ======================================
@app.route('/api/history', methods=['GET'])
def api_history():
    # Lấy tham số từ URL: ?start=2023-11-01&end=2023-11-05
    start_date = request.args.get('start')
    end_date = request.args.get('end')
    
    conn = get_db_connection()
    
    # Nếu có chọn ngày, lọc theo ngày
    if start_date and end_date:
        # Thêm giờ vào để lấy trọn vẹn ngày bắt đầu và ngày kết thúc
        # VD: 2023-11-01 00:00:00 đến 2023-11-01 23:59:59
        start_str = f"{start_date} 00:00:00"
        end_str = f"{end_date} 23:59:59"
        
        sql = """
            SELECT * FROM parking_log 
            WHERE entry_time >= ? AND entry_time <= ? 
            ORDER BY id DESC
        """
        rows = conn.execute(sql, (start_str, end_str)).fetchall()
        
    else:
        # Mặc định: Lấy 50 tin mới nhất (như cũ)
        rows = conn.execute("SELECT * FROM parking_log ORDER BY id DESC LIMIT 50").fetchall()
        
    conn.close()
    return jsonify([dict(r) for r in rows])

@app.route('/api/registered', methods=['GET'])
def api_get_registered():
    conn = get_db_connection()
    rows = conn.execute("SELECT * FROM registered_vehicles ORDER BY id DESC").fetchall()
    conn.close()
    return jsonify([dict(r) for r in rows])

@app.route('/api/registered', methods=['POST'])
def api_add_registered():
    data = request.json
    plate = normalize_plate(data.get('plate'))
    if not plate: return jsonify({"status": "error", "msg": "Invalid Plate"}), 400
    
    with get_db_connection() as conn:
        try:
            conn.execute("INSERT INTO registered_vehicles (plate, owner, vehicle_type) VALUES (?, ?, ?)",
                         (plate, data.get('owner'), data.get('type')))
            conn.commit()
        except:
            return jsonify({"status": "error", "msg": "Plate exists"}), 400
    return jsonify({"status": "ok"})

@app.route('/api/registered/<plate>', methods=['DELETE'])
def api_del_registered(plate):
    with get_db_connection() as conn:
        conn.execute("DELETE FROM registered_vehicles WHERE plate=?", (plate,))
        conn.commit()
    return jsonify({"status": "ok"})

# ======================================
# 7. API ROUTES (HARDWARE LOGIC)
# ======================================

# --- API CẬP NHẬT CẢM BIẾN (ESP32 gửi lên) ---
@app.route('/api/update_data', methods=['POST'])
def api_update_data():
    data = request.json or {}
    slots = [int(data.get(f"s{i}", 0)) for i in range(1, 5)]
    
    payload = {
        "slots": slots,
        "free_slots": slots.count(0),
        "mq135": data.get("mq135", 0)
    }
    emit_realtime("sensor_update", payload)
    return jsonify({"status": "ok"})
# ======================================
# THÊM BIẾN TOÀN CỤC ĐỂ LƯU HÀNG ĐỢI LỆNH
# ======================================
command_queue = []

# ======================================
# API: NHẬN LỆNH TỪ WEB (ADMIN BẤM NÚT)
# ======================================
@app.route('/api/control/<action>', methods=['POST'])
def api_manual_control(action):
    global command_queue
    
    if action == "open_entry":
        command_queue.append("OPEN_ENTRY") # Bỏ lệnh vào hộp thư
        print("[MANUAL] Lệnh đã vào hàng đợi: OPEN_ENTRY")
        return jsonify({"status": "ok", "msg": "Đang mở cổng VÀO..."})
        
    elif action == "open_exit":
        command_queue.append("OPEN_EXIT") # Bỏ lệnh vào hộp thư
        print("[MANUAL] Lệnh đã vào hàng đợi: OPEN_EXIT")
        return jsonify({"status": "ok", "msg": "Đang mở cổng RA..."})
        
    return jsonify({"status": "error", "msg": "Lệnh không hợp lệ"}), 400

# ======================================
# API MỚI: ĐỂ ESP32 KIỂM TRA LỆNH (POLLING)
# ======================================
@app.route('/api/get_command', methods=['GET'])
def api_get_command():
    global command_queue
    if len(command_queue) > 0:
        # Lấy lệnh đầu tiên ra và gửi cho ESP32
        cmd = command_queue.pop(0)
        return jsonify({"command": cmd})
    else:
        return jsonify({"command": "none"})
# --- API CAMERA (Nhận diện biển số - Multi Shot) ---
@app.route('/api/parking/<action>', methods=['POST'])
def api_parking_camera(action):
    cam_ip = CAM_ENTRY_IP if action == 'entry' else CAM_EXIT_IP
    
    # Dùng hàm chụp thông minh (3 shots)
    plate, img_path = smart_capture_loop(cam_ip, f"CAM_{action}")
    
    if not img_path:
        return jsonify({"status": "error", "msg": "Cam Fail", "action": f"deny_{action}"}), 500

    now_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    web_img = f"/captures/{os.path.basename(img_path)}"
    
    # 1. ENTRY
    if action == "entry":
        is_reg = is_plate_registered(plate)
        status = "Allowed" if is_reg else "Denied (Unregistered)"
        
        with get_db_connection() as conn:
            conn.execute("INSERT INTO parking_log (plate, entry_time, image_path, status) VALUES (?, ?, ?, ?)",
                         (plate, now_str, img_path, "IN" if is_reg else "DENIED"))
            conn.commit()

        # Update UI
        emit_realtime("new_log", {
            "plate": plate, 
            "action": "ENTRY (Cam)", 
            "status": status, 
            "time": now_str, 
            "image": web_img,
            "ticket_type": "Monthly" if is_reg else "Unknown"
        })

        if is_reg:
            return jsonify({"status": "ok", "action": "allow_entry", "plate": plate})
        else:
            return jsonify({"status": "denied", "action": "deny_entry", "plate": plate}), 403

    # 2. EXIT
    elif action == "exit":
        with get_db_connection() as conn:
            row = conn.execute("SELECT * FROM parking_log WHERE plate=? AND status='IN' ORDER BY id DESC LIMIT 1", (plate,)).fetchone()
            
            fee = 0
            ticket_type = "Guest"
            status = "Not Found"

            if row:
                is_reg = is_plate_registered(plate)
                if is_reg:
                    fee = 0
                    ticket_type = "Monthly"
                else:
                    fee, exit_time = calculate_fee(row['entry_time'])
                    ticket_type = "Guest"

                conn.execute("UPDATE parking_log SET exit_time=?, fee=?, status='OUT' WHERE id=?", 
                             (now_str, fee, row['id']))
                conn.commit()
                status = "Out"

        emit_realtime("new_log", {
            "plate": plate, 
            "action": f"EXIT ({ticket_type})", 
            "status": status, 
            "time": now_str, 
            "image": web_img,
            "fee": fee,
            "ticket_type": ticket_type # Gửi loại vé để Web hiện đúng màu
        })

        if status == "Out":
            action_resp = "allow_exit" if fee == 0 else "payment_due"
            return jsonify({"status": "ok", "action": action_resp, "fee": fee, "plate": plate})
        else:
             return jsonify({"status": "error", "action": "deny_exit", "msg": "No Entry Record"}), 404

    return jsonify({"status": "err"}), 400
# --- API RFID (Vé lượt / Thẻ từ - Có chụp ảnh xác thực) ---
@app.route('/api/rfid/<action>', methods=['POST'])
def api_rfid_handler(action):
    data = request.json or {}
    uid = data.get("uid", "").strip()
    if not uid: return jsonify({"status": "error"}), 400

    cam_ip = CAM_ENTRY_IP if action == 'entry' else CAM_EXIT_IP
    
    # Kích hoạt chụp ảnh thông minh (Multi-shot)
    current_plate, img_path = smart_capture_loop(cam_ip, f"RFID_{action}_{uid}")
    
    web_img = "/static/placeholder.jpg"
    if img_path:
        web_img = f"/captures/{os.path.basename(img_path)}"
    
    now_str = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    # Hiển thị trên web: Nếu đọc được biển thì hiện biển, không thì hiện mã thẻ
    display_plate = f"{current_plate} (Thẻ: {uid})" if current_plate != "UNKNOWN" else f"RFID: {uid}"

    # 1. ENTRY
    if action == "entry":
        with get_db_connection() as conn:
            exist = conn.execute("SELECT * FROM parking_log WHERE rfid_uid=? AND status='IN'", (uid,)).fetchone()
            if exist:
                 return jsonify({"status": "error", "msg": "Card busy", "action": "deny_entry"}), 400
            
            # Lưu cả UID và Biển số (nếu đọc được)
            conn.execute("INSERT INTO parking_log (plate, rfid_uid, entry_time, image_path, status) VALUES (?, ?, ?, ?, ?)",
                         (current_plate, uid, now_str, img_path, "IN"))
            conn.commit()
        
        emit_realtime("new_log", {
            "plate": display_plate, 
            "action": "ENTRY (RFID)", 
            "status": "Allowed", 
            "time": now_str, 
            "image": web_img,
            "ticket_type": "Guest"
        })
        return jsonify({"status": "ok", "action": "allow_entry", "uid": uid, "plate": current_plate})

    # 2. EXIT
    elif action == "exit":
        with get_db_connection() as conn:
            # Tìm lượt vào chưa ra (status='IN')
            row = conn.execute("SELECT * FROM parking_log WHERE rfid_uid=? AND status='IN' ORDER BY id DESC LIMIT 1", (uid,)).fetchone()
            
            if not row:
                # Không tìm thấy lượt vào -> Chặn luôn
                return jsonify({"status": "error", "action": "deny_exit", "msg": "Card not inside"}), 404
            
            # --- KIỂM TRA BIỂN SỐ (LOGIC SỬA ĐỔI) ---
            plate_in = row['plate']
            
            # Chỉ so sánh khi CẢ HAI đều đọc được biển số (Khác UNKNOWN)
            if plate_in != "UNKNOWN" and current_plate != "UNKNOWN" and plate_in != current_plate:
                print(f"[ALERT] CHẶN CỔNG: Biển số lệch! Vào: {plate_in} - Ra: {current_plate}")
                
                # Gửi cảnh báo lên Web Dashboard ngay lập tức
                emit_realtime("new_log", {
                    "plate": f"{current_plate} (Gốc: {plate_in})", 
                    "action": "EXIT BLOCKED", 
                    "status": "Biển số sai!", 
                    "time": now_str, 
                    "image": web_img,
                    "fee": 0,
                    "ticket_type": "ALERT" # Màu đỏ cảnh báo
                })

                # QUAN TRỌNG: Trả về deny_exit và return ngay lập tức để không chạy code mở cổng phía dưới
                return jsonify({
                    "status": "error", 
                    "action": "deny_exit", 
                    "msg": f"Mismatch: {plate_in} vs {current_plate}"
                }), 403
            # ----------------------------------------

            # Nếu biển số khớp (hoặc 1 trong 2 không đọc được), tiếp tục tính tiền
            fee, exit_time = calculate_fee(row['entry_time'])
            
            # Cập nhật DB: Đã ra
            conn.execute("UPDATE parking_log SET exit_time=?, fee=?, status='OUT' WHERE id=?", 
                         (now_str, fee, row['id']))
            conn.commit()

        # Gửi thông tin ra web (Hợp lệ)
        emit_realtime("new_log", {
            "plate": display_plate, 
            "action": "EXIT (RFID)", 
            "status": "Out", 
            "fee": fee, 
            "time": now_str, 
            "image": web_img,
            "ticket_type": "Guest"
        })
        
        # Nếu có phí -> payment_due, nếu miễn phí -> allow_exit
        action_resp = "payment_due" if fee > 0 else "allow_exit"
        return jsonify({"status": "ok", "action": action_resp, "fee": fee, "uid": uid, "plate": current_plate})

    return jsonify({"status": "err"}), 400
# ======================================
# 8. RUN SERVER
# ======================================
if __name__ == '__main__':
    print(f"Server starting on {AI_SERVER_IP}:{PORT}")
    socketio.run(app, host=AI_SERVER_IP, port=PORT, debug=True)