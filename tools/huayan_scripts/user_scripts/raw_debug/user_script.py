#!/usr/bin/env python
# _*_ coding:utf-8 _*_
import time
import threading
import json
import os
import sys
import traceback
import datetime
import queue
from CCClient import modules
_HANSROBOT_SCRIPTV6_INTERNAL_ERROR = "_HANSROBOT_SCRIPTV6_INTERNAL_ERROR"
########### user varlist##############
from SetVarInitValue import *

def ExamineVarLength(var,var_name,cc_client):
    if(isinstance(var,str)):
        var_len = len(var)
        if(var_len >= 5242880):
            msg = "Script Var :  " + var_name + ", length :" + str(var_len) + " bytes has reached the maximum limit of 5M, the script service will exit."
            cc_client.sendScriptAlarm(20566, '', [var_name, 'str', '5M', str(var_len)])
            raise Exception(_HANSROBOT_SCRIPTV6_INTERNAL_ERROR)
        elif(var_len >= 3145728):
            msg = "Script Var :  " + var_name + ", length :" + str(var_len) + " bytes has equal to or exceeds 3M."
            cc_client.sendHRLog(2, msg)
        elif(var_len >= 1048576):
            msg = "Script Var :  " + var_name + ", length :" + str(var_len) + " bytes has equal to or exceeds 1M."
            cc_client.sendHRLog(1, msg)
    elif(isinstance(var, (list, dict, set))):
        var_len = len(var)
        if(var_len >= 20000):
            msg = "Script Var :  " + var_name + ", number :" + str(var_len) + " has reached the maximum limit of 20000, the script service will exit."
            cc_client.sendScriptAlarm(20567, '', [var_name, 'list or dict or set', '20000', str(var_len)])
            raise Exception(_HANSROBOT_SCRIPTV6_INTERNAL_ERROR)
        elif(var_len >= 10000):
            msg = "Script Var :  " + var_name + ", number :" + str(var_len) + " has equal to or exceeds 10000."
            cc_client.sendHRLog(2, msg)
        elif(var_len >= 5000):
            msg = "Script Var :  " + var_name + ", number :" + str(var_len) + " has equal to or exceeds 5000."
            cc_client.sendHRLog(1, msg)

def CheckLength(var, var_name,is_run_forever,cc_client):
    ExamineVarLength(var, var_name, cc_client)
    if(is_run_forever):
        if(isinstance(var,str) and len(var) > 10240 ):
            msg = "The variable["+ var_name +"] size cannot exceed 10K when saving the running value!"
            cc_client.sendScriptAlarm(20568, '', [var_name, 'str', '10K', str(len(var))])
            raise Exception(_HANSROBOT_SCRIPTV6_INTERNAL_ERROR)
    else:
        if(isinstance(var,str) and len(var) > 10240 ):
            var = var[0:10240]
    return var

def CheckRange(cc_client,var,var_name,var_initialtype,max_value,min_value):
    if (var_initialtype=="int"):
        if(min_value != "" and (int(min_value)<-2**63 or int(min_value)>2**63-1)):
            msg = "Script Var : " + var_name + ", min_range:" + str(min_value) + ", out of long long int range "
            cc_client.sendScriptAlarm(20562, '', [var_name, 'int', str(2**63-1), str(-2**63), str(var)])
            return False
        if(max_value != "" and (int(max_value)<-2**63 or int(max_value)>2**63-1)):
            msg = "Script Var : " + var_name + ", max_range:" + str(max_value) + ", out of long long int range "
            cc_client.sendScriptAlarm(20563, '', [var_name, 'int', str(max_value), str(min_value)])
            return False
        if(min_value!="" and max_value !="" and  int(min_value)>int(max_value)):
            msg = "Script Var : " + var_name + ", min_range > max_range"
            cc_client.sendScriptAlarm(20563, '', [var_name, 'int', str(max_value), str(min_value)])
            return False
        if(var<-2**63 or var>2**63-1):
            msg = "Script Var : " + var_name + ", Curent Value:" + str(var) + ", out of long long int range "
            cc_client.sendScriptAlarm(20562, '', [var_name, 'int', str(2**63-1), str(-2**63), str(var)])
            return False
    if(max_value != "" and min_value != ""):
        if(float(max_value) < var or var <float(min_value)):
            msg = "Script Var : " + var_name + ", Curent Value:" + str(var) + ", out of range [ "+min_value + " , " + max_value +" ]"
            cc_client.sendScriptAlarm(20562, '', [var_name, 'int', str(max_value), str(min_value), str(var)])
            return False
    elif(max_value == "" and min_value != ""):
        if(var <float(min_value)):
            msg = "Script Var : " + var_name + ", Curent Value:" + str(var) + ", out of range [ "+min_value + " ,    ]"
            cc_client.sendScriptAlarm(20562, '', [var_name, 'int', str(max_value), str(min_value), str(var)])
            return False
    elif(max_value != "" and min_value == ""):
        if(float(max_value) < var):
            msg = "Script Var : " + var_name + ", Curent Value:" + str(var) + ", out of range [    , "+max_value + " ]"
            cc_client.sendScriptAlarm(20562, '', [var_name, 'int', str(max_value), str(min_value), str(var)])
            return False
    return True

def CheckVarList(cc_client):
    global J1
    global J1_5638
    global J2
    global J2_5637
    global J3
    global J3_5636
    global J4
    global J4_5635
    global J5
    global J5_5634
    global J6
    global J6_5633
    global fsm
    global fsm_5645
    global msg_test
    global point_RX
    global point_RX_5629
    global point_RY
    global point_RY_5628
    global point_RZ
    global point_RZ_5627
    global point_X
    global point_X_5632
    global point_Y
    global point_Y_5631
    global point_Z
    global point_Z_5630
    global rx
    global rx_5642
    global ry
    global ry_5643
    global rz
    global rz_5644
    global speed
    global speed_5647
    global str_1
    global x
    global x_5639
    global y
    global y_5640
    global z
    global z_5641
    if isinstance(J1,float) or isinstance(J1,int):
        bRet = CheckRange(cc_client,J1,"J1","double",J1_5638["max_value"],J1_5638["min_value"])
        if(not bRet):
            return bRet
    if isinstance(J2,float) or isinstance(J2,int):
        bRet = CheckRange(cc_client,J2,"J2","double",J2_5637["max_value"],J2_5637["min_value"])
        if(not bRet):
            return bRet
    if isinstance(J3,float) or isinstance(J3,int):
        bRet = CheckRange(cc_client,J3,"J3","double",J3_5636["max_value"],J3_5636["min_value"])
        if(not bRet):
            return bRet
    if isinstance(J4,float) or isinstance(J4,int):
        bRet = CheckRange(cc_client,J4,"J4","double",J4_5635["max_value"],J4_5635["min_value"])
        if(not bRet):
            return bRet
    if isinstance(J5,float) or isinstance(J5,int):
        bRet = CheckRange(cc_client,J5,"J5","double",J5_5634["max_value"],J5_5634["min_value"])
        if(not bRet):
            return bRet
    if isinstance(J6,float) or isinstance(J6,int):
        bRet = CheckRange(cc_client,J6,"J6","double",J6_5633["max_value"],J6_5633["min_value"])
        if(not bRet):
            return bRet
    if isinstance(fsm,float) or isinstance(fsm,int):
        bRet = CheckRange(cc_client,fsm,"fsm","int",fsm_5645["max_value"],fsm_5645["min_value"])
        if(not bRet):
            return bRet
    if isinstance(point_RX,float) or isinstance(point_RX,int):
        bRet = CheckRange(cc_client,point_RX,"point_RX","double",point_RX_5629["max_value"],point_RX_5629["min_value"])
        if(not bRet):
            return bRet
    if isinstance(point_RY,float) or isinstance(point_RY,int):
        bRet = CheckRange(cc_client,point_RY,"point_RY","double",point_RY_5628["max_value"],point_RY_5628["min_value"])
        if(not bRet):
            return bRet
    if isinstance(point_RZ,float) or isinstance(point_RZ,int):
        bRet = CheckRange(cc_client,point_RZ,"point_RZ","double",point_RZ_5627["max_value"],point_RZ_5627["min_value"])
        if(not bRet):
            return bRet
    if isinstance(point_X,float) or isinstance(point_X,int):
        bRet = CheckRange(cc_client,point_X,"point_X","double",point_X_5632["max_value"],point_X_5632["min_value"])
        if(not bRet):
            return bRet
    if isinstance(point_Y,float) or isinstance(point_Y,int):
        bRet = CheckRange(cc_client,point_Y,"point_Y","double",point_Y_5631["max_value"],point_Y_5631["min_value"])
        if(not bRet):
            return bRet
    if isinstance(point_Z,float) or isinstance(point_Z,int):
        bRet = CheckRange(cc_client,point_Z,"point_Z","double",point_Z_5630["max_value"],point_Z_5630["min_value"])
        if(not bRet):
            return bRet
    if isinstance(rx,float) or isinstance(rx,int):
        bRet = CheckRange(cc_client,rx,"rx","double",rx_5642["max_value"],rx_5642["min_value"])
        if(not bRet):
            return bRet
    if isinstance(ry,float) or isinstance(ry,int):
        bRet = CheckRange(cc_client,ry,"ry","double",ry_5643["max_value"],ry_5643["min_value"])
        if(not bRet):
            return bRet
    if isinstance(rz,float) or isinstance(rz,int):
        bRet = CheckRange(cc_client,rz,"rz","double",rz_5644["max_value"],rz_5644["min_value"])
        if(not bRet):
            return bRet
    if isinstance(speed,float) or isinstance(speed,int):
        bRet = CheckRange(cc_client,speed,"speed","double",speed_5647["max_value"],speed_5647["min_value"])
        if(not bRet):
            return bRet
    if isinstance(x,float) or isinstance(x,int):
        bRet = CheckRange(cc_client,x,"x","double",x_5639["max_value"],x_5639["min_value"])
        if(not bRet):
            return bRet
    if isinstance(y,float) or isinstance(y,int):
        bRet = CheckRange(cc_client,y,"y","double",y_5640["max_value"],y_5640["min_value"])
        if(not bRet):
            return bRet
    if isinstance(z,float) or isinstance(z,int):
        bRet = CheckRange(cc_client,z,"z","double",z_5641["max_value"],z_5641["min_value"])
        if(not bRet):
            return bRet
    return True

def PublishVar2CPS(cc_client):
    global J1
    global J2
    global J3
    global J4
    global J5
    global J6
    global cur_joint
    global cur_pos
    global fsm
    global msg_test
    global point_RX
    global point_RY
    global point_RZ
    global point_X
    global point_Y
    global point_Z
    global rx
    global ry
    global rz
    global speed
    global str_1
    global x
    global y
    global z
    vardict={}
    params=[]
    vardict['cmdname']='scriptvars'
    params.append({'J1':str(J1)})
    params.append({'J2':str(J2)})
    params.append({'J3':str(J3)})
    params.append({'J4':str(J4)})
    params.append({'J5':str(J5)})
    params.append({'J6':str(J6)})
    params.append({'cur_joint':str(cur_joint)})
    params.append({'cur_pos':str(cur_pos)})
    params.append({'fsm':str(fsm)})
    params.append({'msg_test':str(CheckLength(msg_test,"msg_test",False,cc_client))})
    params.append({'point_RX':str(point_RX)})
    params.append({'point_RY':str(point_RY)})
    params.append({'point_RZ':str(point_RZ)})
    params.append({'point_X':str(point_X)})
    params.append({'point_Y':str(point_Y)})
    params.append({'point_Z':str(point_Z)})
    params.append({'rx':str(rx)})
    params.append({'ry':str(ry)})
    params.append({'rz':str(rz)})
    params.append({'speed':str(speed)})
    params.append({'str_1':str(CheckLength(str_1,"str_1",False,cc_client))})
    params.append({'x':str(x)})
    params.append({'y':str(y)})
    params.append({'z':str(z)})
    vardict['parameters']=params
    varjson=json.dumps(vardict)
    return varjson
########### user varlist##############

def Func_main(sleeptime, cc_client, control):
    global J1
    global J2
    global J3
    global J4
    global J5
    global J6
    global cur_joint
    global cur_pos
    global fsm
    global msg_test
    global point_RX
    global point_RY
    global point_RZ
    global point_X
    global point_Y
    global point_Z
    global rx
    global ry
    global rz
    global speed
    global str_1
    global x
    global y
    global z
    try:
        while True:
            control.before('6A9D16789AFCE51178E254EFC1CFB26D','Script_41',cc_client)
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
            control.before('F04A6865BD131533C6B289D806F8B021','Script_26',cc_client)
            cur_pos='151,-442,325,-180,0,123;'
            cc_client.sendVarValue('cur_pos',cur_pos)
            msg_test=cc_client.ReadPointByName('Point_999')
            cc_client.sendVarValue('msg_test',msg_test)
            
            str_1 = cc_client.getModbus('Modbus_3', 'HR000')
            #str_1 = 'hr000'
            cc_client.sendVarValue('str_1', str_1)
            control.before('72D011594D9E244BAD11B975EEB9DB8E','SetValue_30',cc_client)
            point_X = msg_test[7]
            if not (CheckVarList(cc_client)):
                return
            control.before('F9515C326184804F243F7E17ACA9B77C','SetValue_36',cc_client)
            point_Y = msg_test[8]
            if not (CheckVarList(cc_client)):
                return
            control.before('84B3F720367393C7BF542B198CB0303E','SetValue_35',cc_client)
            point_Z = msg_test[9]
            if not (CheckVarList(cc_client)):
                return
            control.before('AFD590F76B1F264935A7F78E18CA08ED','SetValue_34',cc_client)
            point_RX = msg_test[10]
            if not (CheckVarList(cc_client)):
                return
            control.before('5E724E1E35B895C4198D13952540A04C','SetValue_33',cc_client)
            point_RY = msg_test[11]
            if not (CheckVarList(cc_client)):
                return
            control.before('9B9C457AE61DF3BC0CE4FBFF03EC6881','SetValue_32',cc_client)
            point_RZ = msg_test[12]
            if not (CheckVarList(cc_client)):
                return
            #thread sleep
            if control.run_forever:
                time.sleep(sleeptime*0.001)
            else:
                break
    except Exception as e:
        strError = str(traceback.format_exc());
        cc_client.sendScriptAlarm(20560, strError, [strError])
        raise e

def Func_tcpip(sleeptime, cc_client, control):
    global J1
    global J2
    global J3
    global J4
    global J5
    global J6
    global cur_joint
    global cur_pos
    global fsm
    global msg_test
    global point_RX
    global point_RY
    global point_RZ
    global point_X
    global point_Y
    global point_Z
    global rx
    global ry
    global rz
    global speed
    global str_1
    global x
    global y
    global z
    try:
        while True:
            while( (control.before('4B7F0A49321436217D38DE5CA5BCB7B1','Loop_11',cc_client) ) and  True ):
                pass
                control.before('CA15897262429AAB2331025CC9FCC903','SetValue_15',cc_client)
                msg_test = 'msg_ok'
                if not (CheckVarList(cc_client)):
                    return
                control.before('66BECA0A62D78202431B1EC552EBB79A','Script_12',cc_client)
                cc_client.socket_open('192.168.1.59',8080,'test')
                cc_client.socket_send_string(msg_test,'test')
                control.before('BFE522A66D0C5B0363F666AF0D5BE5F0','Wait_14',cc_client)
                time.sleep(0.100000)
            #thread sleep
            break
    except Exception as e:
        strError = str(traceback.format_exc());
        cc_client.sendScriptAlarm(20560, strError, [strError])
        raise e
