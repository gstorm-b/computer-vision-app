
# use cc_client.readExModbus(device name, device number)

registers = [0x4248, 0x0000]

file_path = 'usr/local/RobotOS/home/RobotOS/HRCPS/CPS.py'

try:
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()

    cc_client.sendVarValue('str_1', file_path)

except FileNotFoundError:
    cc_client.sendVarValue('str_1', 'not found')
except Exception as e:
    cc_client.sendVarValue('str_1', f"Error: {e}")

cc_client.socket_open('192.168.1.59',8080,'test')
cc_client.socket_send_string(msg_test,'test')

# client.py
import socket

HOST = "192.168.1.100"  # IP server
PORT = 5000

file_path = "big_script.py"

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.connect((HOST, PORT))

    with open(file_path, "rb") as f:
        while True:
            chunk = f.read(4096)

            if not chunk:
                break

            s.sendall(chunk)

print("File sent")

with open(file_path, "r", encoding="utf-8") as f:
    str_1 = f.read()

cc_client.sendVarValue('str_1', str_1)
cc_client.socket_send_string(str_1,'test')

time.sleep(5)
cc_client.socket_close('test')

    with open(file_path, "r", encoding="utf-8") as f:
        while True:
            chunk = f.read(4096)
    
            if not chunk:
                break
    
            #s.sendall(chunk)
            cc_client.socket_send_string(chunk.encode("utf-8"),'test')
        
    time.sleep(10)
    cc_client.socket_close('test')


import time

file_path = '/usr/local/RobotOS/home/RobotOS/HRCPS/script/CPS.py'

cc_client.socket_open('192.168.1.59',8080,'test')

try:

    with open(file_path, "r", encoding="utf-8") as f:
        for line in f:
            cc_client.socket_send_string(line,'test')
        
    time.sleep(10)
    cc_client.socket_close('test')

except FileNotFoundError:
    str_1 = 'not found'
    cc_client.sendVarValue('str_1', str_1)
except Exception as e:
    str_1 = f"Error: {e}"
    cc_client.sendVarValue('str_1', str_1)


##################################################


# read_python_file.py
#cc_client.sendVarValue('str_1', 'test_1')
#cc_client.setModbus('Modbus_3','HR000', 100)
#str_1 = cc_client.getModbus('Modbus_3','HR000')
#cc_client.sendVarValue('str_1', str_1)

import time

file_path = '/usr/local/RobotOS/home/RobotOS/HRCPS/script/UserScript.py'

cc_client.socket_open('192.168.1.59',5000,'test')

try:

    with open(file_path, "r", encoding="utf-8") as f:
        #count = 0
        for line in f:
            #count += 1
            #if count < 726:
            #    continue
            
            cc_client.socket_send_string(line,'test')
        
    time.sleep(10)
    cc_client.socket_close('test')

except FileNotFoundError:
    str_1 = 'not found'
    cc_client.sendVarValue('str_1', str_1)
except Exception as e:
    str_1 = f"Error: {e}"
    cc_client.sendVarValue('str_1', str_1)

