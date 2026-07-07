#!/usr/bin/env python3
"""
NFCAttend PC Card Management Tool v1.0
======================================
PC 上位机卡片管理工具 — 用于 NFC 考勤系统.
通过 COM 口(USART1)与 STM32 下位机通信,支持:
  · 读卡 (READ)          · 发卡 (ISSUE + 图像块)
  · 写头像 (IMGAxx)      · 写姓名图像 (IMGNxx)
  · 写部门图像 (IMGDxx)  · 清卡 (CLEAR)
  · 时钟显示

依赖: Python 3.8+, tkinter(内置), pyserial (pip install pyserial)
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import serial
import serial.tools.list_ports
import threading
import time
import struct
import os

# ──── 全局配置 ────
BAUDRATE = 115200
TIMEOUT = 2.0

# ──── 主窗口 ────
class NFCAttendApp:
    def __init__(self, root):
        self.root = root
        self.root.title("NFCAttend Card Manager v1.0")
        self.root.geometry("860x620")
        self.root.resizable(False, False)

        self.ser: serial.Serial | None = None
        self.serial_lock = threading.Lock()
        self.last_uid = ""
        self.last_sid = ""
        self.last_type = ""
        self.last_pts = ""

        self._build_ui()
        self._refresh_ports()

    # ──── UI 构建 ────
    def _build_ui(self):
        # 顶部: 串口控制
        top = ttk.Frame(self.root, padding=5)
        top.pack(fill=tk.X)

        ttk.Label(top, text="COM:").pack(side=tk.LEFT)
        self.combo_port = ttk.Combobox(top, width=10, state="readonly")
        self.combo_port.pack(side=tk.LEFT, padx=2)
        ttk.Button(top, text="刷新", width=5, command=self._refresh_ports).pack(side=tk.LEFT, padx=2)
        ttk.Button(top, text="连接", width=6, command=self._toggle_connect).pack(side=tk.LEFT, padx=2)

        self.lbl_status = ttk.Label(top, text="● 未连接", foreground="red")
        self.lbl_status.pack(side=tk.LEFT, padx=10)

        # 时间显示(右上)
        self.lbl_time = ttk.Label(top, text="", font=("Courier", 12))
        self.lbl_time.pack(side=tk.RIGHT, padx=5)

        ttk.Separator(self.root, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=2)

        # 中部: 三栏布局
        mid = ttk.Frame(self.root, padding=5)
        mid.pack(fill=tk.BOTH, expand=True)

        # 左栏: 读卡信息
        left = ttk.LabelFrame(mid, text="卡片信息", padding=5)
        left.pack(side=tk.LEFT, fill=tk.BOTH, expand=True, padx=(0,3))

        ttk.Label(left, text="UID:").grid(row=0, column=0, sticky=tk.W)
        self.lbl_uid = ttk.Label(left, text="----", font=("Courier", 11))
        self.lbl_uid.grid(row=0, column=1, sticky=tk.W, pady=2)

        ttk.Label(left, text="SID(工号):").grid(row=1, column=0, sticky=tk.W)
        self.entry_sid = ttk.Entry(left, width=12)
        self.entry_sid.grid(row=1, column=1, sticky=tk.W, pady=2)

        ttk.Label(left, text="姓名:").grid(row=2, column=0, sticky=tk.W)
        self.entry_name = ttk.Entry(left, width=12)
        self.entry_name.grid(row=2, column=1, sticky=tk.W, pady=2)

        ttk.Label(left, text="部门:").grid(row=3, column=0, sticky=tk.W)
        self.entry_dept = ttk.Entry(left, width=12)
        self.entry_dept.grid(row=3, column=1, sticky=tk.W, pady=2)

        ttk.Label(left, text="点数:").grid(row=4, column=0, sticky=tk.W)
        self.entry_pts = ttk.Entry(left, width=8)
        self.entry_pts.insert(0, "100")
        self.entry_pts.grid(row=4, column=1, sticky=tk.W, pady=2)

        ttk.Label(left, text="类型:").grid(row=5, column=0, sticky=tk.W)
        self.combo_type = ttk.Combobox(left, width=10, state="readonly",
                                        values=["0-未发卡", "1-员工", "2-管理员"])
        self.combo_type.current(1)
        self.combo_type.grid(row=5, column=1, sticky=tk.W, pady=2)

        ttk.Separator(left, orient=tk.HORIZONTAL).grid(row=6, column=0, columnspan=2, sticky=tk.EW, pady=5)

        self.btn_read = ttk.Button(left, text="🔍 读卡", command=self._read_card)
        self.btn_read.grid(row=7, column=0, pady=5)
        self.btn_issue = ttk.Button(left, text="✏️ 发卡(ISSUE)", command=self._issue_card)
        self.btn_issue.grid(row=7, column=1, pady=5)
        self.btn_clear = ttk.Button(left, text="🗑️ 清卡", command=self._clear_card)
        self.btn_clear.grid(row=8, column=0, pady=2)
        self.btn_read_all = ttk.Button(left, text="📋 全部读出", command=self._read_full)
        self.btn_read_all.grid(row=8, column=1, pady=2)

        self.txt_log = tk.Text(left, height=8, width=32, font=("Courier", 8), bg="#1e1e1e", fg="#d4d4d4")
        self.txt_log.grid(row=9, column=0, columnspan=2, pady=5, sticky=tk.NSEW)
        left.rowconfigure(9, weight=1)

        # 中栏: 头像预览
        center = ttk.LabelFrame(mid, text="头像预览 48×64", padding=5)
        center.pack(side=tk.LEFT, fill=tk.BOTH, padx=3)

        self.canvas_avatar = tk.Canvas(center, width=144, height=192, bg="white", highlightthickness=1)
        self.canvas_avatar.pack(pady=5)
        ttk.Label(center, text="Phase2: 卡内图像数据\nIMGAxx 命令写入", justify=tk.CENTER).pack()

        ttk.Button(center, text="加载图片文件...", command=self._load_avatar_img).pack(pady=3)

        # 右栏: 姓名/部门图像预览
        right = ttk.LabelFrame(mid, text="姓名/部门 80×16", padding=5)
        right.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True, padx=(3,0))

        ttk.Label(right, text="姓名图像预览:").pack()
        self.canvas_name = tk.Canvas(right, width=240, height=48, bg="white", highlightthickness=1)
        self.canvas_name.pack(pady=3)
        ttk.Button(right, text="加载姓名图片...", command=self._load_name_img).pack()

        ttk.Separator(right, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=5)

        ttk.Label(right, text="部门图像预览:").pack()
        self.canvas_dept = tk.Canvas(right, width=240, height=48, bg="white", highlightthickness=1)
        self.canvas_dept.pack(pady=3)
        ttk.Button(right, text="加载部门图片...", command=self._load_dept_img).pack()

        ttk.Separator(right, orient=tk.HORIZONTAL).pack(fill=tk.X, pady=5)

        # 进度条
        self.progress = ttk.Progressbar(right, length=220, mode='determinate')
        self.progress.pack(pady=5)

        # 底部: 状态栏
        bot = ttk.Frame(self.root, padding=3)
        bot.pack(fill=tk.X)
        self.status_bar = ttk.Label(bot, text="就绪 | 专业实践综合设计II — NFC考勤系统 | HDU 杭州电子科技大学",
                                     relief=tk.SUNKEN)
        self.status_bar.pack(fill=tk.X)

        self._tick_clock()

    # ──── 串口操作 ────
    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.combo_port['values'] = ports
        if ports and not self.combo_port.get():
            self.combo_port.current(0)

    def _toggle_connect(self):
        if self.ser and self.ser.is_open:
            self._disconnect()
        else:
            self._connect()

    def _connect(self):
        port = self.combo_port.get()
        if not port:
            messagebox.showwarning("警告", "请选择 COM 端口")
            return
        try:
            self.ser = serial.Serial(port, BAUDRATE, timeout=TIMEOUT)
            self.lbl_status.config(text=f"● 已连接 {port}", foreground="green")
            self.status_bar.config(text=f"已连接 {port} @ {BAUDRATE} baud")
            self._log(f"已连接 {port}")
        except Exception as e:
            messagebox.showerror("错误", f"无法打开 {port}: {e}")

    def _disconnect(self):
        if self.ser:
            self.ser.close()
            self.ser = None
        self.lbl_status.config(text="● 未连接", foreground="red")
        self._log("已断开")

    def _send_cmd(self, cmd: str) -> str:
        """发送命令,返回所有响应行(直到 OK/ERR/空行超时)"""
        with self.serial_lock:
            if not self.ser or not self.ser.is_open:
                return "ERR:NOT_CONNECTED"
            try:
                self.ser.reset_input_buffer()
                self.ser.write((cmd + "\n").encode())
                lines = []
                deadline = time.time() + TIMEOUT
                while time.time() < deadline:
                    line = self.ser.readline().decode(errors='replace').strip()
                    if line:
                        lines.append(line)
                        if line.startswith("OK") or line.startswith("ERR") or line.startswith("LIST:END"):
                            if not line.startswith("OK"):
                                continue
                            break
                return "\n".join(lines) if lines else "ERR:TIMEOUT"
            except Exception as e:
                return f"ERR:SERIAL:{e}"

    def _log(self, msg):
        self.txt_log.insert(tk.END, msg + "\n")
        self.txt_log.see(tk.END)

    # ──── 卡片操作 ────
    def _read_card(self):
        self._log(">>> READ")
        resp = self._send_cmd("READ")
        self._log(resp)
        self._parse_read_response(resp)

    def _parse_read_response(self, resp):
        for line in resp.split("\n"):
            if line.startswith("UID:"):
                self.last_uid = line[4:].strip()
                self.lbl_uid.config(text=self.last_uid)
            elif line.startswith("SID:"):
                self.last_sid = line[4:].strip()
                self.entry_sid.delete(0, tk.END)
                self.entry_sid.insert(0, self.last_sid)
            elif line.startswith("TYPE:"):
                self.last_type = line[5:].strip()
                try:
                    t = int(self.last_type)
                    self.combo_type.current(t if t <= 2 else 0)
                except: pass
            elif line.startswith("PTS:"):
                self.last_pts = line[4:].strip()
                self.entry_pts.delete(0, tk.END)
                self.entry_pts.insert(0, self.last_pts)

    def _issue_card(self):
        sid = self.entry_sid.get().strip()
        pts = self.entry_pts.get().strip()
        ctype = self.combo_type.current()
        if not sid:
            messagebox.showwarning("警告", "请输入工号(SID)")
            return
        # 先用 READ 获取 UID
        resp = self._send_cmd("READ")
        self._parse_read_response(resp)
        uid = self.last_uid
        if not uid or len(uid) < 8:
            messagebox.showerror("错误", "未检测到卡片,请先放卡再发卡")
            return
        cid = uid  # CID 即 UID
        try:
            sid_int = int(sid)
            pts_int = int(pts) if pts else 0
        except:
            messagebox.showerror("错误", "SID/点数必须为数字")
            return

        cmd = f"ISSUE:{cid},{sid_int},{pts_int},{ctype}"
        self._log(f">>> {cmd}")
        resp = self._send_cmd(cmd)
        self._log(resp)
        if "OK" in resp:
            self.status_bar.config(text=f"发卡成功! SID={sid_int} TYPE={ctype}")
            messagebox.showinfo("成功", f"卡片已发行!\nCID={cid}\nSID={sid_int}\nTYPE={ctype}")
        else:
            messagebox.showerror("失败", f"发卡失败:\n{resp}")

    def _clear_card(self):
        uid = self.last_uid
        if not uid:
            resp = self._send_cmd("READ")
            self._parse_read_response(resp)
            uid = self.last_uid
        if not uid:
            messagebox.showerror("错误", "未检测到卡片")
            return
        if not messagebox.askyesno("确认", f"确定清空卡片 UID={uid} ?"):
            return
        cmd = f"CLEAR:{uid}"
        self._log(f">>> {cmd}")
        resp = self._send_cmd(cmd)
        self._log(resp)

    def _read_full(self):
        """全卡读出(含图像块),显示在日志"""
        self._read_card()
        self._log("--- 图像块读取(Phase2) ---")
        # TODO Phase2: 循环读 IMGA00~IMGA23, IMGN00~IMGN09, IMGD00~IMGD09
        self._log("image blocks Phase2 reserved")

    def _write_image_blocks(self, prefix: str, data: bytes, total_blocks: int):
        """写入图像块序列 IMGA/IMGN/IMGD"""
        for i in range(total_blocks):
            chunk = data[i*16:(i+1)*16]
            if len(chunk) < 16:
                chunk = chunk + b'\x00' * (16 - len(chunk))
            hexstr = chunk.hex().upper()
            cmd = f"{prefix}{i:02d}:{hexstr}"
            self._log(f">>> {cmd}")
            resp = self._send_cmd(cmd)
            if "ERR" in resp:
                self._log(f"ERROR at block {i}: {resp}")
                return False
            self.progress['value'] = (i+1) / total_blocks * 100
            self.root.update()
        return True

    # ──── 图像加载 ────
    @staticmethod
    def _img_to_mono(path: str, w: int, h: int) -> bytes | None:
        """将图片文件转为单色位图字节数组"""
        try:
            from PIL import Image
            img = Image.open(path).convert('L')  # 灰度
            img = img.resize((w, h), Image.LANCZOS)
            # 二值化
            pixels = list(img.getdata())
            threshold = 128
            data = bytearray()
            for y in range(h):
                for x_byte in range(0, w, 8):
                    byte = 0
                    for bit in range(8):
                        if x_byte + bit < w:
                            px = pixels[y * w + x_byte + bit]
                            if px < threshold:
                                byte |= (1 << (7 - bit))
                    data.append(byte)
            return bytes(data)
        except ImportError:
            return None
        except Exception as e:
            messagebox.showerror("错误", f"图片处理失败: {e}")
            return None

    def _load_avatar_img(self):
        path = filedialog.askopenfilename(
            title="选择头像图片",
            filetypes=[("Image files", "*.png *.jpg *.bmp *.gif"), ("All", "*.*")])
        if not path: return
        data = self._img_to_mono(path, 48, 64)
        if data is None:
            self._log("需要安装 Pillow: pip install Pillow")
            messagebox.showwarning("提示", "需要安装 Pillow 库来加载图片:\npip install Pillow")
            return
        self._draw_on_canvas(self.canvas_avatar, data, 48, 64, 3)
        # 写入卡
        if self.ser and self.ser.is_open and messagebox.askyesno("写入卡片", "将头像写入卡片? (需确保卡片已放置)"):
            self.progress['maximum'] = 24
            self._write_image_blocks("IMGA", data, 24)

    def _load_name_img(self):
        path = filedialog.askopenfilename(
            title="选择姓名图片", filetypes=[("Image files", "*.png *.jpg *.bmp"), ("All", "*.*")])
        if not path: return
        data = self._img_to_mono(path, 80, 16)
        if data is None:
            messagebox.showwarning("提示", "需要安装 Pillow: pip install Pillow")
            return
        self._draw_on_canvas(self.canvas_name, data, 80, 16, 3)
        if self.ser and self.ser.is_open and messagebox.askyesno("写入卡片", "将姓名图像写入卡片?"):
            self.progress['maximum'] = 10
            self._write_image_blocks("IMGN", data, 10)

    def _load_dept_img(self):
        path = filedialog.askopenfilename(
            title="选择部门图片", filetypes=[("Image files", "*.png *.jpg *.bmp"), ("All", "*.*")])
        if not path: return
        data = self._img_to_mono(path, 80, 16)
        if data is None:
            messagebox.showwarning("提示", "需要安装 Pillow: pip install Pillow")
            return
        self._draw_on_canvas(self.canvas_dept, data, 80, 16, 3)
        if self.ser and self.ser.is_open and messagebox.askyesno("写入卡片", "将部门图像写入卡片?"):
            self.progress['maximum'] = 10
            self._write_image_blocks("IMGD", data, 10)

    @staticmethod
    def _draw_on_canvas(cv: tk.Canvas, data: bytes, w: int, h: int, scale: int):
        """在 Canvas 上绘制单色位图"""
        cv.delete("all")
        for y in range(h):
            for x_byte in range(0, w, 8):
                idx = y * ((w + 7) // 8) + x_byte // 8
                if idx >= len(data): continue
                byte = data[idx]
                for bit in range(8):
                    if x_byte + bit >= w: break
                    if byte & (1 << (7 - bit)):
                        x1 = (x_byte + bit) * scale
                        y1 = y * scale
                        x2 = x1 + scale
                        y2 = y1 + scale
                        cv.create_rectangle(x1, y1, x2, y2, fill="black", outline="")

    # ──── 时钟 ────
    def _tick_clock(self):
        now = time.strftime("%Y-%m-%d %H:%M:%S")
        self.lbl_time.config(text=now)
        self.root.after(1000, self._tick_clock)


# ──── 入口 ────
def main():
    root = tk.Tk()
    app = NFCAttendApp(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (app._disconnect(), root.destroy()))
    root.mainloop()


if __name__ == "__main__":
    print("NFCAttend PC Card Manager v1.0")
    print("依赖: pyserial, Pillow(可选,用于图片加载)")
    print("用法: python NFCAttend_PC_Tool.py")
    main()
