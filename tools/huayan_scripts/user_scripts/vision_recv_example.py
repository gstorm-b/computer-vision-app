# Huayan robot script edit

from CCClient import CCClient
cc_client = CCClient()

# Define coils name: Coil0000,...
# Define input status name: DI000,...
# Define holding register name: HR000,...
# Define input register name: IR000,...

# Camera selection      HR000
# Pattern selection     HR001
# Camera trigger        Coil0000
# Error reset           Coil0001

# Camera ready          DI000
# Pattern ready         DI001
# Task ready            DI002
# Task fault            DI003
# Busy                  DI004
# Fisished              DI005
# Detected              DI006
# Low area              DI007

# Detected number       IR100
# Fault code            IR101

# Vision output data layout
#  +0            count      number of valid positions; 0 while a publish is in progress
#  +1            sequence   1..32767, increments on every publish, wraps to 1
#  +2            flags      bit0 = low area, bit1 = truncated
#  +3            reserved   written as 0
#  +4 .. +4+12N  positions  N positions, 12 registers each

##### Robot global variables
# vsModbusDevice        string : modbus device name
# initStatus           int : vision initialize state
# taskReady            int : vision task ready state
# visionError          int : vision error state 
# visionDetected       int : vision workspiece detected state
# recv_pos_count        int : total received position
# recv_sequence         int : sequence stamp of every receiving time, 
#                           should be different every time
# recv_pos_x            double : x axis of picking position
# recv_pos_y            double : y axis of picking position
# recv_pos_z            double : z axis of picking position
# recv_pos_rx           double : rx axis of picking position
# recv_pos_ry           double : ry axis of picking position
# recv_pos_rz           double : rz axis of picking position
# vs_trigger_request    int : request visionTrigger function to trigger 
#                           vision task
# vs_request_done       int : visionTrigger function handle done 

##### Robot vision thread workflow
# 1. If (using variable condition) : check request, request should be set from main_function
# 2. Call script "VisionTrigger"
# 3. If (using variable condition) : to check vision error
# 4. SetValue  : to mark vision done
# 5. Call script "visionResetTrigger" should be called every time trigger on
# 6. SetValue  : to reset vision request, to prevent continuous trigger

# conventional variables
vsModbusDevice = "Modbus_1"

def visionStartInit():
    import time

    start_hr = 'HR0000'
    start_is = 'DI0000'

    while True:
        # set camera index = 1 and pattern selection = 1
        setValues = [1, 1]
        cc_client.setModbusDatas(vsModbusDevice, start_hr, setValues)
        # wait for server to confirm pattern and camera valid, sleep 2s
        time.sleep(2)
        values = cc_client.getModbusDatas(vsModbusDevice, start_is, 8)
        # check modbus read status
        if len(values) != 8:
            initStatus = 0
            break
        # check task ready state
        taskReady = values[2]
        if taskReady == 0:
            initStatus = 0
            break
        # task ready, exit function
        initStatus = 1
        break
    # update global variables 
    cc_client.sendVarValue(taskReady, 'taskReady')
    cc_client.sendVarValue(initStatus, 'initStatus')

# vision trigger should be called in a program thread
# using command block to check request and mark request done
def visionTrigger():
    import time
    import struct

    # vsModbusDevice = 'Modbus_1'
    start_hr = 'HR0000'
    start_is = 'DI0000'
    start_ir = 'IR0000'
    # vs_results_bytes_num = 100 not actually need to read all
    vs_results_bytes_num = 24
    addr_vs_trigger = 'Coil0000'
    addr_vs_busy = 'DI0004'
    addr_vs_finished = 'DI0005'
    visionError = 0

    def convert_float32(registers, start):
        raw = struct.pack(">HH", registers[start], registers[start + 1])
        return struct.unpack(">f", raw)[0]

    while True:
        values = cc_client.getModbusDatas(vsModbusDevice, start_is, 8)
        taskReady = values[2]

        if taskReady == 0:
            # task not ready, then exit function
            break

        cc_client.setModbus(vsModbusDevice, addr_vs_trigger, 1)        
        vision_busy = 0
        vision_finished = 0

        # wait for vision busy state ON
        start_tick = time.perf_counter()
        while vision_busy == 0:
            # timeout check
            end_tick = time.perf_counter()
            if (end_tick - start_tick) > 1.0:
                visionError = 1
                break
            # get vision busy state
            vision_busy = cc_client.getModbus(vsModbusDevice, addr_vs_busy)
            time.sleep(0.0005)

        if visionError > 0:
            # vision error, exit function
            break

        # wait for vision finished state ON
        start_tick = time.perf_counter()
        while vision_finished == 0:
            # timeout check
            end_tick = time.perf_counter()
            if (end_tick - start_tick) > 1.0:
                visionError = 1
                break
            # get vision busy state
            vision_finished = cc_client.getModbus(vsModbusDevice, addr_vs_finished)
            time.sleep(0.0005)

        if visionError > 0:
            # vision error, exit function
            break

        values = cc_client.getModbusDatas(vsModbusDevice, start_is, 8)
        visionDetected = values[6]
        if visionDetected == 0:
            # vision deteceted workspiece fail, then exit funtion
            break

        # update picking point
        vision_results = cc_client.getModbusDatas(vsModbusDevice, start_ir, vs_results_bytes_num)
        debug_info = vision_results
        cc_client.sendVarValue(debug_info, 'debug_info')
        if (vision_results[0] < 1) or (vision_results[1] == recv_sequence):
            visionError = 1
            # pos count or sequence number invalid, exit function
            break

        # convert position data to float number
        recv_pos_x = convert_float32(vision_results, 4)
        recv_pos_y = convert_float32(vision_results, 6)
        recv_pos_z = convert_float32(vision_results, 8)
        recv_pos_rx = convert_float32(vision_results, 10)
        recv_pos_ry = convert_float32(vision_results, 12)
        recv_pos_rz = convert_float32(vision_results, 14)

        # write to global variables
        recv_pos_count = vision_results[0]
        recv_sequence = vision_results[1]
        cc_client.sendVarValue(recv_pos_count, 'recv_pos_count')
        cc_client.sendVarValue(recv_sequence, 'recv_sequence')
        cc_client.sendVarValue(recv_pos_x, 'recv_pos_x')
        cc_client.sendVarValue(recv_pos_y, 'recv_pos_y')
        cc_client.sendVarValue(recv_pos_z, 'recv_pos_z')
        cc_client.sendVarValue(recv_pos_rx, 'recv_pos_rx')
        cc_client.sendVarValue(recv_pos_ry, 'recv_pos_ry')
        cc_client.sendVarValue(recv_pos_rz, 'recv_pos_rz')

        # position received, exit function
        break

    # update global variables
    cc_client.sendVarValue(taskReady, 'taskReady')
    cc_client.sendVarValue(visionError, 'visionError')
    cc_client.sendVarValue(visionDetected, 'visionDetected')

# reset vision trigger after received position data
def visionResetTrigger():
    #vsModbusDevice = 'Modbus_1'
    addr_vs_trigger = 'Coil0000'
    cc_client.setModbus(vsModbusDevice, addr_vs_trigger, 0)

def clearVisionGlobalVars():
    recv_pos_x = 0.0
    recv_pos_y = 0.0
    recv_pos_z = 0.0
    recv_pos_rx = 0.0
    recv_pos_ry = 0.0
    recv_pos_rz = 0.0

    recv_pos_count = 0
    cc_client.sendVarValue(recv_pos_count, 'recv_pos_count')
    cc_client.sendVarValue(recv_pos_x, 'recv_pos_x')
    cc_client.sendVarValue(recv_pos_y, 'recv_pos_y')
    cc_client.sendVarValue(recv_pos_z, 'recv_pos_z')
    cc_client.sendVarValue(recv_pos_rx, 'recv_pos_rx')
    cc_client.sendVarValue(recv_pos_ry, 'recv_pos_ry')
    cc_client.sendVarValue(recv_pos_rz, 'recv_pos_rz')
    

