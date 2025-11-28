/**
 * FILE: static/dashboard.js
 * VERSION: Final Integrated
 * DESCRIPTION: Quản lý toàn bộ logic Frontend, SocketIO, ChartJS và API
 */

const socket = io();
const moneyFmt = new Intl.NumberFormat('vi-VN', { style: 'currency', currency: 'VND' });

// Biến toàn cục
let allHistory = [];        // Cache dữ liệu lịch sử để search nhanh
let chartInstance = null;   // Lưu instance biểu đồ để destroy khi vẽ lại
let sensorWatchdog = null;  // Timer kiểm tra kết nối cảm biến
let cameraWatchdog = null;  // Timer kiểm tra kết nối camera

// ============================================================
// 1. QUẢN LÝ KẾT NỐI & TRẠNG THÁI HỆ THỐNG (SIDEBAR)
// ============================================================

socket.on('connect', () => {
    console.log("✅ Connected to Server");
    updateStatus('server', 'Online', 'green');
});

socket.on('disconnect', () => {
    console.warn("❌ Disconnected from Server");
    updateStatus('server', 'Offline', 'red');
    updateStatus('sensor', 'Mất kết nối', 'red');
});

/**
 * Hàm cập nhật dấu chấm trạng thái bên Sidebar
 * @param {string} type - 'server', 'cam', 'sensor', 'mq135'
 * @param {string} text - Chữ hiển thị
 * @param {string} color - 'green', 'red', 'yellow'
 */
function updateStatus(type, text, color) {
    const dot = document.getElementById(`st-${type}`);
    const txt = document.getElementById(`txt-${type}`);
    
    if(dot && txt) {
        dot.className = "status-dot"; // Reset
        
        if(color === 'green') dot.classList.add('dot-green');
        else if(color === 'red') dot.classList.add('dot-red');
        else if(color === 'yellow') dot.style.backgroundColor = '#eab308';
        
        txt.innerText = text;
        if(color === 'red') txt.classList.add('text-danger');
        else txt.classList.remove('text-danger');
    }
}

// ============================================================
// 2. SOCKET: NHẬN DỮ LIỆU REALTIME
// ============================================================

// --- A. SỰ KIỆN XE RA VÀO (CAMERA/RFID) ---
socket.on('new_log', (data) => {
    console.log("🔔 New Log:", data);
    
    // 1. Cập nhật trạng thái Camera (Heartbeat)
    updateStatus('cam', 'Đang xử lý', 'green');
    clearTimeout(cameraWatchdog);
    cameraWatchdog = setTimeout(() => {
        updateStatus('cam', 'Sẵn sàng', 'green');
    }, 5000);

    // 2. XỬ LÝ HIỂN THỊ ẢNH (SNAPSHOT)
    const camBox = document.querySelector('.camera-box');
    
    // Kiểm tra xem đây là sự kiện RFID hay Camera
    if (data.plate.includes("RFID") || data.image.includes("rfid_icon")) {
        // === TRƯỜNG HỢP RFID: Hiển thị Icon lớn ===
        camBox.innerHTML = `
            <div class="d-flex flex-column align-items-center justify-content-center h-100 w-100 bg-dark text-white">
                <i class="fas fa-wifi fa-5x mb-3 text-primary animate-pulse"></i>
                <h4 class="fw-bold">QUẸT THẺ THÀNH CÔNG</h4>
                <div class="text-warning font-monospace fs-5">${data.plate.replace('RFID:', '')}</div>
            </div>
        `;
    } else {
        // === TRƯỜNG HỢP CAMERA: Hiển thị ảnh chụp ===
        // Tạo lại thẻ img để đảm bảo load ảnh mới nhất
        camBox.innerHTML = `
            <img id="live-img" src="${data.image}?t=${new Date().getTime()}" 
                 alt="Live Capture" 
                 style="max-width: 100%; max-height: 100%; object-fit: contain;">
        `;
    }
    
    // 3. Cập nhật Thông tin Text
    document.getElementById('live-plate').innerText = data.plate;
    document.getElementById('live-time').innerText = data.time.split(' ')[1];
    
    const statusEl = document.getElementById('live-status');
    const msgEl = document.getElementById('live-msg');
    const typeEl = document.getElementById('live-type');
    const st = (data.status || "").toUpperCase();

    // 4. Phân loại trạng thái & Màu sắc
    if(st.includes('IN') || st.includes('ALLOWED')) {
        statusEl.innerHTML = '<span class="badge bg-success fs-5">MỜI VÀO (IN)</span>';
        msgEl.className = "alert alert-success mt-3 mb-0";
        msgEl.innerText = "✔ Xe hợp lệ / Vé lượt đã tạo";
        typeEl.innerText = data.plate.includes("RFID") ? "Vé Lượt (RFID)" : "Vé Tháng (Cam)";
    } 
    // ... (Đoạn code IN giữ nguyên) ...

    else if (st.includes('OUT')) {
        // Kiểm tra loại vé gửi từ Server xuống
        if (data.ticket_type === 'Monthly') {
            // === TRƯỜNG HỢP VÉ THÁNG RA ===
            statusEl.innerHTML = '<span class="badge bg-success fs-5">VÉ THÁNG (RA)</span>';
            msgEl.className = "alert alert-success mt-3 mb-0";
            msgEl.innerText = "✔ Xe hợp lệ / Đã đăng ký tháng";
            typeEl.innerText = "Vé Tháng";
            // Vé tháng thì không hiện 0đ cho đỡ phản cảm, hiện chữ "Miễn phí" hoặc ẩn
            const feeText = document.getElementById('live-msg');
            msgEl.innerText = "✔ Chúc quý khách thượng lộ bình an";
        } else {
            // === TRƯỜNG HỢP KHÁCH VÃNG LAI RA ===
            statusEl.innerHTML = '<span class="badge bg-primary fs-5">ĐÃ THU PHÍ (OUT)</span>';
            msgEl.className = "alert alert-primary mt-3 mb-0";
            msgEl.innerText = `💰 Phí: ${data.fee ? moneyFmt.format(data.fee) : '0đ'}`;
            typeEl.innerText = "Vé Lượt (Check-out)";
        }
    } 
    
    // ... (Đoạn code DENIED giữ nguyên) ...
    else {
        statusEl.innerHTML = '<span class="badge bg-danger fs-5">TỪ CHỐI (DENIED)</span>';
        msgEl.className = "alert alert-danger mt-3 mb-0";
        msgEl.innerText = "⛔ Biển số chưa đăng ký / Lỗi";
        typeEl.innerText = "Unknown";
    }

    // 5. Tải lại lịch sử nếu cần
    if (!document.getElementById('hist-start').value) {
        loadHistory();
    }
});

// --- B. SỰ KIỆN CẢM BIẾN (ESP32 GỬI LÊN) ---
socket.on('sensor_update', (data) => {
    // Heartbeat cho cảm biến
    updateStatus('sensor', 'Hoạt động', 'green');
    clearTimeout(sensorWatchdog);
    sensorWatchdog = setTimeout(() => {
        updateStatus('sensor', 'Mất tín hiệu', 'red');
    }, 5000); // 5s không gửi là coi như mất kết nối

    // 1. Cập nhật 4 Slot
    if(data.slots) {
        let freeCount = 0;
        data.slots.forEach((val, idx) => {
            updateSlot(idx+1, val);
            if(val == 0) freeCount++;
        });
        // Cập nhật text tổng quan bên sidebar
        const txtFree = document.getElementById('txt-free');
        if(txtFree) txtFree.innerText = `${freeCount} Trống`;
    }
    
    // 2. Cập nhật Chất lượng không khí (MQ135)
    if(data.mq135 !== undefined) {
        const mqVal = parseInt(data.mq135);
        let qualityText = "Tốt";
        let qualityColor = "green";

        if(mqVal > 400) {
            qualityText = `Kém (${mqVal})`;
            qualityColor = "red";
        } else if (mqVal > 200) {
            qualityText = `TB (${mqVal})`;
            qualityColor = "yellow";
        } else {
            qualityText = `Tốt (${mqVal})`;
        }
        updateStatus('mq135', qualityText, qualityColor);
    }
    
    document.getElementById('last-sensor-update').innerText = "Cập nhật: " + new Date().toLocaleTimeString();
});

function updateSlot(id, isBusy) {
    const el = document.getElementById('slot-' + id);
    if(!el) return;
    
    // 1 hoặc true nghĩa là CÓ XE (Busy)
    if(isBusy == 1 || isBusy === true) {
        el.className = 'slot-box busy';
        el.querySelector('span:last-child').innerText = "CÓ XE";
        el.querySelector('i').className = "fas fa-car fa-2x mb-2";
    } else {
        el.className = 'slot-box free';
        el.querySelector('span:last-child').innerText = "TRỐNG";
        el.querySelector('i').className = "fas fa-car-side fa-2x mb-2";
    }
}

// ============================================================
// 3. NAVIGATION & INITIALIZATION
// ============================================================

document.addEventListener("DOMContentLoaded", () => {
    loadHistory(); // Load dữ liệu mặc định (50 dòng)
});

function switchTab(tabId) {
    // 1. Ẩn hiện Tab Content
    document.querySelectorAll('.section-view').forEach(el => el.classList.remove('active'));
    document.getElementById(tabId).classList.add('active');
    
    // 2. Highlight Sidebar Menu
    document.querySelectorAll('.nav-item').forEach(el => el.classList.remove('active'));
    
    // Map ID tab với thứ tự menu (Monitor=0, History=1, Vehicles=2, Revenue=3)
    const menuMap = { 'monitor': 0, 'history': 1, 'vehicles': 2, 'revenue': 3 };
    const navItems = document.querySelectorAll('.nav-item');
    if(navItems[menuMap[tabId]]) {
        navItems[menuMap[tabId]].classList.add('active');
    }

    // 3. Load dữ liệu đặc thù
    if(tabId === 'vehicles') loadRegistered();
    
    // Nếu vào tab Doanh thu, reset về "Hôm nay" cho tiện theo dõi
    if(tabId === 'revenue') {
        document.getElementById('rev-quick-select').value = 'today';
        quickRevenueFilter('today');
    }
}

// ============================================================
// 4. API: LỊCH SỬ & BỘ LỌC (HISTORY + REVENUE)
// ============================================================

/**
 * Hàm trung tâm để lấy dữ liệu lịch sử
 * @param {string} start - Ngày bắt đầu (YYYY-MM-DD)
 * @param {string} end - Ngày kết thúc (YYYY-MM-DD)
 */
async function fetchHistoryData(start = '', end = '') {
    try {
        let url = '/api/history';
        if(start && end) {
            url += `?start=${start}&end=${end}`;
        }
        
        const res = await fetch(url);
        const data = await res.json();
        allHistory = data; // Cache lại để search local
        
        // Vẽ lại bảng lịch sử
        renderHistory(data);
        
        // Tính toán doanh thu (Dùng chung data này)
        calculateRevenue(data);

    } catch(e) { 
        console.error("Fetch error:", e); 
    }
}

// --- Render Bảng Lịch Sử (Cập nhật: Icon RFID vs Ảnh Camera) ---
function renderHistory(data) {
    const tbody = document.getElementById('history-table-body');
    if(!tbody) return;
    tbody.innerHTML = '';
    
    // Giới hạn 200 dòng để tránh lag nếu dữ liệu quá lớn
    const displayData = data.length > 200 ? data.slice(0, 200) : data;

    displayData.forEach(row => { 
        // 1. XỬ LÝ HIỂN THỊ ẢNH / ICON
        let imgUrl = row.image_path || "";
        let visualContent = "";

        // Điều kiện: Nếu đường dẫn chứa "rfid_icon" HOẶC biển số bắt đầu bằng "RFID"
        // -> Hiển thị Icon thẻ từ (FontAwesome)
        if(imgUrl.includes("rfid_icon") || imgUrl.includes("placeholder") || (row.plate && row.plate.startsWith("RFID"))) {
            visualContent = `
                <div class="d-flex align-items-center justify-content-center bg-light rounded" 
                     style="width:60px; height:40px; border:1px solid #cbd5e1;" title="Thẻ từ RFID">
                    <i class="fas fa-id-card text-primary fs-4"></i>
                </div>
            `;
        } 
        // -> Ngược lại: Hiển thị ảnh chụp từ Camera
        else {
            // Fix đường dẫn: chuyển từ "static/captures/..." sang "/captures/..." để Flask serve đúng
            if(imgUrl.includes("static")) {
                const filename = imgUrl.split(/[\\/]/).pop();
                imgUrl = "/captures/" + filename;
            }
            // Thêm sự kiện onclick để phóng to ảnh nếu cần
            visualContent = `
                <img src="${imgUrl}" height="40" 
                     style="border-radius:4px; border:1px solid #ddd; cursor:pointer;" 
                     title="Click để xem ảnh gốc"
                     onclick="window.open(this.src, '_blank')">
            `;
        }

        // 2. XỬ LÝ MÀU SẮC TRẠNG THÁI
        let badge = 'bg-secondary';
        let st = (row.status || "").toUpperCase();
        if(st.includes('IN')) badge = 'bg-success';
        else if(st.includes('OUT')) badge = 'bg-primary';
        else if(st.includes('DENIED')) badge = 'bg-danger';

        // 3. CHÈN VÀO BẢNG
        tbody.innerHTML += `
            <tr>
                <td>${row.entry_time || row.time}</td>
                <td>${visualContent}</td> <td class="fw-bold font-monospace">${row.plate}</td>
                <td><span class="badge ${badge}">${row.status}</span></td>
                <td>${row.fee ? moneyFmt.format(row.fee) : '-'}</td>
            </tr>
        `;
    });
}

// --- Chức năng Lọc ở Tab History ---
function loadHistoryWithFilter() {
    const start = document.getElementById('hist-start').value;
    const end = document.getElementById('hist-end').value;
    
    if(!start || !end) {
        alert("Vui lòng chọn đủ ngày bắt đầu và kết thúc!");
        return;
    }
    fetchHistoryData(start, end);
}

function resetFilter() {
    document.getElementById('hist-start').value = '';
    document.getElementById('hist-end').value = '';
    // Load lại mặc định (không tham số)
    fetchHistoryData();
}

// Search biển số (Client-side)
function filterHistoryLocal() {
    const term = document.getElementById('search-input').value.toUpperCase();
    const filtered = allHistory.filter(x => (x.plate || "").includes(term));
    renderHistory(filtered);
}

// ============================================================
// 5. API: BÁO CÁO DOANH THU (REVENUE)
// ============================================================

// Xử lý Dropdown chọn nhanh (Hôm nay, Hôm qua, Tháng này)
function quickRevenueFilter(type) {
    const customDiv = document.getElementById('rev-custom-date');
    const today = new Date();
    const formatDate = (date) => {
        // Trả về YYYY-MM-DD theo giờ địa phương
        const offset = date.getTimezoneOffset() * 60000;
        return new Date(date.getTime() - offset).toISOString().split('T')[0];
    };

    let start = "", end = "";

    if(type === 'custom') {
        customDiv.classList.remove('d-none');
        return; 
    } else {
        customDiv.classList.add('d-none');
    }

    if(type === 'today') {
        start = end = formatDate(today);
        document.getElementById('lbl-rev-money').innerText = "Doanh Thu Hôm Nay";
    } 
    else if (type === 'yesterday') {
        const yest = new Date(today);
        yest.setDate(today.getDate() - 1);
        start = end = formatDate(yest);
        document.getElementById('lbl-rev-money').innerText = "Doanh Thu Hôm Qua";
    } 
    else if (type === 'this_month') {
        // Ngày 1 của tháng hiện tại
        start = formatDate(new Date(today.getFullYear(), today.getMonth(), 1));
        // Ngày cuối của tháng hiện tại (Ngày 0 của tháng sau)
        end = formatDate(new Date(today.getFullYear(), today.getMonth() + 1, 0));
        document.getElementById('lbl-rev-money').innerText = "Doanh Thu Tháng Này";
    }

    // Gọi API
    fetchHistoryData(start, end);
}

// Xử lý nút Xem Tùy Chọn
function loadRevenueCustom() {
    const start = document.getElementById('rev-start').value;
    const end = document.getElementById('rev-end').value;
    if(!start || !end) { alert("Chọn ngày!"); return; }
    
    document.getElementById('lbl-rev-money').innerText = `Doanh Thu (${start} -> ${end})`;
    fetchHistoryData(start, end);
}

// Tính toán và Vẽ biểu đồ
function calculateRevenue(data) {
    let rev = 0;
    let cntIn = 0;
    let cntOut = 0;

    // Vì Data đã được lọc theo ngày từ Server, nên ta chỉ việc cộng tổng
    data.forEach(r => {
        const st = (r.status || "").toUpperCase();
        
        if (st.includes('IN') || st.includes('ALLOWED')) cntIn++;
        if (st.includes('OUT') || st.includes('PAID')) {
            cntOut++;
            rev += (r.fee || 0);
        }
    });

    // Hiển thị số liệu
    if(document.getElementById('rev-total')) 
        document.getElementById('rev-total').innerText = moneyFmt.format(rev);
    if(document.getElementById('rev-count-in')) 
        document.getElementById('rev-count-in').innerText = cntIn;
    if(document.getElementById('rev-count-out')) 
        document.getElementById('rev-count-out').innerText = cntOut;

    // Vẽ biểu đồ: Lấy tối đa 20 giao dịch OUT gần nhất để vẽ
    const chartData = data
        .filter(x => (x.status || "").toUpperCase().includes('OUT'))
        .slice(0, 20)
        .reverse();
        
    updateChart(chartData);
}

// Hàm vẽ biểu đồ Chart.js
function updateChart(recentData) {
    const ctx = document.getElementById('revenueChart');
    if(!ctx) return;

    // 1. Hủy biểu đồ cũ để tránh lỗi chồng đè
    if (chartInstance) {
        chartInstance.destroy();
    }

    // 2. Xử lý dữ liệu
    let labels = [];
    let dataValues = [];

    if (recentData.length === 0) {
        labels = ["Không có dữ liệu"];
        dataValues = [0];
    } else {
        // Label là Giờ hoặc Ngày tùy vào khoảng thời gian (ở đây lấy giờ cho chi tiết)
        labels = recentData.map(x => (x.exit_time || x.entry_time).split(' ')[1]);
        dataValues = recentData.map(x => x.fee || 0);
    }

    // 3. Tạo biểu đồ mới
    chartInstance = new Chart(ctx.getContext('2d'), {
        type: 'bar', // Biểu đồ cột
        data: {
            labels: labels,
            datasets: [{
                label: 'Phí thu được (VNĐ)',
                data: dataValues,
                backgroundColor: 'rgba(37, 99, 235, 0.7)', // Xanh dương
                borderColor: '#2563eb',
                borderWidth: 1,
                borderRadius: 4,
                barThickness: 25,
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false, // Cho phép tự do chiều cao theo CSS cha
            plugins: {
                legend: { display: true }
            },
            scales: {
                y: {
                    beginAtZero: true,
                    ticks: {
                        callback: function(value) { return value.toLocaleString('vi-VN'); }
                    }
                }
            }
        }
    });
}

// ============================================================
// 6. QUẢN LÝ XE ĐĂNG KÝ (CRUD)
// ============================================================

async function loadRegistered() {
    const res = await fetch('/api/registered');
    const data = await res.json();
    const tbody = document.getElementById('reg-table-body');
    if(!tbody) return;
    
    tbody.innerHTML = '';
    data.forEach(r => {
        tbody.innerHTML += `
            <tr>
                <td class="fw-bold text-uppercase">${r.plate}</td>
                <td>${r.owner || '-'}</td>
                <td><span class="badge bg-info text-dark">${r.vehicle_type || 'Car'}</span></td>
                <td>
                    <button class="btn btn-sm btn-outline-danger" onclick="delReg('${r.plate}')" title="Xóa">
                        <i class="fas fa-trash"></i>
                    </button>
                </td>
            </tr>
        `;
    });
}

// Form Submit
const regForm = document.getElementById('reg-form');
if(regForm) {
    regForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const plate = document.getElementById('reg-plate').value.trim();
        const owner = document.getElementById('reg-owner').value.trim();
        const type = document.getElementById('reg-type').value;

        if(!plate) return alert("Vui lòng nhập biển số!");

        const res = await fetch('/api/registered', {
            method: 'POST', 
            headers: {'Content-Type':'application/json'},
            body: JSON.stringify({ plate, owner, type })
        });
        
        const json = await res.json();
        if(json.status === 'ok') {
            alert("Đăng ký thành công!");
            document.getElementById('reg-plate').value = '';
            document.getElementById('reg-owner').value = '';
            loadRegistered();
        } else {
            alert("Lỗi: " + (json.msg || "Không thể thêm"));
        }
    });
}

async function delReg(plate) {
    if(confirm(`Bạn chắc chắn muốn xóa xe biển số [${plate}] khỏi danh sách vé tháng?`)) {
        await fetch(`/api/registered/${plate}`, {method: 'DELETE'});
        loadRegistered();
    }
}

// ============================================================
// 7. ĐIỀU KHIỂN THỦ CÔNG (MANUAL CONTROL)
// ============================================================

function manualControl(action) {
    // action: 'open_entry' | 'open_exit'
    fetch(`/api/control/${action}`, {method: 'POST'})
        .then(res => res.json())
        .then(data => {
            if(data.status === 'ok') {
                alert("✅ " + data.msg);
            } else {
                alert("❌ Lỗi: " + data.msg);
            }
        })
        .catch(e => alert("Lỗi kết nối Server!"));
}