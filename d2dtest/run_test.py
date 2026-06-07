#!/usr/bin/env python3
"""
D2D 集成测试编排脚本

流程:
1. 启动 kv_server
2. writer_client 预写入 100 个 key
3. 并发启动 4 个客户端执行固定流程:
   - burst_client: 写入 key_00~49（无 sleep）
   - steady_client: 写入 key_50~79（有 sleep）
   - delete_client: 删除 key_80~89 并重写
   - modify_client: 读取 key_90~99 并修改
4. reader_client 读取所有 key 到 result.txt
5. 验证 result.txt
"""
import subprocess
import time
import sys
import os
import re

# 配置
BUILD_DIR = os.path.join(os.path.dirname(__file__), "..", "build", "d2dtest")
SERVER_BIN = os.path.join(BUILD_DIR, "..", "bin", "kv_server.exe")
WRITER_BIN = os.path.join(BUILD_DIR, "writer_client.exe")
BURST_BIN = os.path.join(BUILD_DIR, "burst_client.exe")
STEADY_BIN = os.path.join(BUILD_DIR, "steady_client.exe")
DELETE_BIN = os.path.join(BUILD_DIR, "delete_client.exe")
MODIFY_BIN = os.path.join(BUILD_DIR, "modify_client.exe")
READER_BIN = os.path.join(BUILD_DIR, "reader_client.exe")
RESULT_FILE = os.path.join(os.path.dirname(__file__), "result.txt")

HOST = "127.0.0.1"
PORT = 16379
CLIENT_TIMEOUT = 30  # 秒


def wait_for_server(host, port, timeout=10):
    """等待服务端就绪"""
    import socket
    start = time.time()
    while time.time() - start < timeout:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1)
            sock.connect((host, port))
            sock.close()
            return True
        except (ConnectionRefusedError, OSError):
            time.sleep(0.1)
    return False


def run_test():
    print("=" * 60)
    print("D2D Integration Test")
    print("=" * 60)
    
    # 检查可执行文件
    for name, path in [("server", SERVER_BIN), ("writer", WRITER_BIN),
                       ("burst", BURST_BIN), ("steady", STEADY_BIN),
                       ("delete", DELETE_BIN), ("modify", MODIFY_BIN),
                       ("reader", READER_BIN)]:
        if not os.path.exists(path):
            print(f"ERROR: {name} not found at {path}")
            print("Run 'make build' first.")
            return False
    
    server_proc = None
    client_procs = []
    
    try:
        # 1. 启动服务端
        print("\n[1/6] Starting server...")
        server_proc = subprocess.Popen(
            [SERVER_BIN, "-p", str(PORT), "-d", "./data_d2d"],
            cwd=os.path.dirname(SERVER_BIN),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        
        if not wait_for_server(HOST, PORT):
            print("ERROR: Server failed to start")
            return False
        print(f"  Server started on {HOST}:{PORT}")
        
        # 2. 预写入数据
        print("\n[2/6] Pre-writing 100 keys...")
        result = subprocess.run(
            [WRITER_BIN, HOST, str(PORT)],
            capture_output=True, text=True, timeout=10
        )
        print(f"  {result.stdout.strip()}")
        if result.returncode != 0:
            print(f"  ERROR: {result.stderr.strip()}")
            return False
        
        # 3. 并发启动 4 个客户端
        print("\n[3/6] Starting 4 clients...")
        
        clients = [
            ("burst", BURST_BIN, []),
            ("steady", STEADY_BIN, []),
            ("delete", DELETE_BIN, []),
            ("modify", MODIFY_BIN, []),
        ]
        
        for name, bin_path, extra_args in clients:
            proc = subprocess.Popen(
                [bin_path, HOST, str(PORT)] + extra_args,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )
            client_procs.append((name, proc))
            print(f"  {name}_client started (pid={proc.pid})")
        
        # 4. 等待所有客户端完成
        print("\n[4/6] Waiting for clients to finish...")
        for name, proc in client_procs:
            try:
                proc.wait(timeout=CLIENT_TIMEOUT)
                output = proc.stdout.read().decode().strip()
                print(f"  {output}")
                if proc.returncode != 0:
                    print(f"  WARNING: {name}_client exited with code {proc.returncode}")
            except subprocess.TimeoutExpired:
                proc.kill()
                print(f"  {name}_client TIMEOUT (killed)")
        
        # 5. 停止服务端（触发 WAL 刷盘）
        print("\n[5/6] Stopping server...")
        server_proc.kill()
        server_proc.wait()
        server_proc = None
        print("  Server stopped")
        
        # 6. 重启服务端，验证持久化
        print("\n[6/6] Restarting server for persistence check...")
        server_proc = subprocess.Popen(
            [SERVER_BIN, "-p", str(PORT), "-d", "./data_d2d"],
            cwd=os.path.dirname(SERVER_BIN),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        
        if not wait_for_server(HOST, PORT):
            print("ERROR: Server failed to restart")
            return False
        print(f"  Server restarted on {HOST}:{PORT}")
        
        # 读取结果
        print("\n  Reading all keys after restart...")
        result = subprocess.run(
            [READER_BIN, HOST, str(PORT), RESULT_FILE, "10"],
            capture_output=True, text=True, timeout=15
        )
        print(f"  {result.stdout.strip()}")
        
        # 验证结果
        print("\n" + "=" * 60)
        print("Verification (after restart)")
        print("=" * 60)
        return verify_result()
        
    except Exception as e:
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return False
    finally:
        for name, proc in client_procs:
            try:
                proc.kill()
            except:
                pass
        
        if server_proc:
            server_proc.kill()
            server_proc.wait()
            print("\nServer stopped")


def verify_result():
    """验证 result.txt"""
    if not os.path.exists(RESULT_FILE):
        print("FAIL: result.txt not found")
        return False
    
    # 预期值:
    # key_00~49: burst_XX
    # key_50~79: steady_XX
    # key_80~89: del_XX
    # key_90~99: modify_XX
    expected = {}
    for i in range(50):
        expected[f"key_{i:02d}"] = f"burst_{i}"
    for i in range(50, 80):
        expected[f"key_{i:02d}"] = f"steady_{i}"
    for i in range(80, 90):
        expected[f"key_{i:02d}"] = f"del_{i}"
    for i in range(90, 100):
        expected[f"key_{i:02d}"] = f"modify_{i}"
    
    total = 0
    valid = 0
    invalid = 0
    missing = 0
    wrong_value = []
    
    with open(RESULT_FILE, 'r') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            
            total += 1
            if '=' not in line:
                invalid += 1
                print(f"  INVALID LINE: {line}")
                continue
            
            key, value = line.split('=', 1)
            
            if value == 'MISSING':
                missing += 1
                continue
            
            if key in expected:
                if value == expected[key]:
                    valid += 1
                else:
                    invalid += 1
                    wrong_value.append((key, value, expected[key]))
            else:
                invalid += 1
                print(f"  UNEXPECTED KEY: {key}")
    
    print(f"  Total keys: {total}")
    print(f"  Valid:       {valid}")
    print(f"  Missing:     {missing}")
    print(f"  Invalid:     {invalid}")
    
    if wrong_value:
        print(f"\n  Wrong values:")
        for key, actual, exp in wrong_value[:10]:
            print(f"    {key}: got '{actual}', expected '{exp}'")
        if len(wrong_value) > 10:
            print(f"    ... and {len(wrong_value) - 10} more")
    
    # 验证结果
    success = True
    
    if total != 100:
        print(f"\n  FAIL: Expected 100 keys, got {total}")
        success = False
    
    if invalid > 0:
        print(f"\n  FAIL: {invalid} invalid values")
        success = False
    
    if missing > 0:
        print(f"\n  FAIL: {missing} missing keys")
        success = False
    
    # 检查 key 范围
    expected_keys = {f"key_{i:02d}" for i in range(100)}
    actual_keys = set()
    with open(RESULT_FILE, 'r') as f:
        for line in f:
            line = line.strip()
            if '=' in line:
                key = line.split('=', 1)[0]
                actual_keys.add(key)
    
    if actual_keys != expected_keys:
        print(f"\n  FAIL: Key set mismatch")
        success = False
    
    if success:
        print(f"\n  PASS: All {total} keys have correct values!")
    else:
        print(f"\n  FAIL: Some checks failed")
    
    return success


if __name__ == "__main__":
    success = run_test()
    if os.path.exists(RESULT_FILE):
        os.remove(RESULT_FILE)
    sys.exit(0 if success else 1)
