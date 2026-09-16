#!/usr/bin/env python
# _*_ coding:utf-8 _*_

import threading
import time
import xmlrpc.client
import socket
import os
import ScriptDefine
from Socket import SocketMng
from shutil import copy
import base64
import re
import errno
import importlib
import paho.mqtt.client as mqtt
from MqttBase import hrMqtt
import json
from ScriptDefine import ErCode
import traceback

def find_file_by_pattern(base,pattern):
 re_type = re.compile(pattern)
 re_plugin = re.compile(r'.+plugin.+')

 final_file_list = []

 if not os.path.exists(base):
  return final_file_list

 cur_list = os.listdir(base)

 for item in cur_list:
  full_path = os.path.join(base, item)
  nextpath=base+'/'+str(item)
  bfile = os.path.isfile(nextpath)
  if bfile:
   if re_plugin.match(full_path) and re_type.match(full_path):
     final_file_list.append(full_path)
  else:
   final_file_list += find_file_by_pattern(nextpath,pattern)
 
 return final_file_list

modules={}
def LoadScriptPlugin():
 filetype=r'.+\.py$'
 file_list=find_file_by_pattern('/usr/local/bin/Robot/HRCPS/script/',filetype)
 for item in file_list:
  os.remove(item)

 file_list=find_file_by_pattern('/var/www/html/dist/plugin',filetype)
 for item in file_list:
  copy(item,'/usr/local/bin/Robot/HRCPS/script/')

 file_list=find_file_by_pattern('/usr/local/bin/Robot/HRCPS/script/',filetype)
 for item in file_list:
  moduleName=os.path.basename(item)
  moduleName=moduleName[:-3]
  modules[moduleName]=importlib.import_module(moduleName)


import modbus_tk.defines as cst
import modbus_tk.modbus_tcp as modbus_tcp

class MBMaster:
    #Modbus_Master = modbus_tcp.TcpMaster(host="127.0.0.1",port=502)
    def __init__(self):
        self.Modbus_Master.set_timeout(5)

import struct

def ReadFloat(*args,reverse=False):
    for n,m in args:
        n,m = '%04x'%n,'%04x'%m
    if reverse:
        v = n + m
    else:
        v = m + n
    y_bytes = bytes.fromhex(v)
    y = struct.unpack('!f',y_bytes)[0]
    y = round(y,6)
    return y

def WriteFloat(value,reverse=False):
    y_bytes = struct.pack('!f',value)
    y_hex = ''.join(['%02x' % i for i in y_bytes])
    n,m = y_hex[:-4],y_hex[-4:]
    n,m = int(n,16),int(m,16)
    if reverse:
        v = [n,m]
    else:
        v = [m,n]
    return v

def ReadDint(*args,reverse=False):
    for n,m in args:
        n,m = '%04x'%n,'%04x'%m
    if reverse:
        v = n + m
    else:
        v = m + n
    y_bytes = bytes.fromhex(v)
    y = struct.unpack('!i',y_bytes)[0]
    return y

def WriteDint(value,reverse=False):
    y_bytes = struct.pack('!i',value)
    # y_hex = bytes.hex(y_bytes)
    y_hex = ''.join(['%02x' % i for i in y_bytes])
    n,m = y_hex[:-4],y_hex[-4:]
    n,m = int(n,16),int(m,16)
    if reverse:
        v = [n,m]
    else:
        v = [m,n]
    return v
#################################################################################################
class CCClient(object):
    clientIP = '0.0.0.0'
    clientPort = 1789
    xmlrpcAddr='http://127.0.0.1:20000'
    params=[]
    readSleep = 0.05
    scriptStatus=0
    # set speed percent
    # vel: double type
    # vel>0 && vel < 1
    #
    # @output if it is work OK ,
    #         it will retrun "SetOverride,OK,;" ,
    #         
    #         if it work error, it will ret "SetOverride,FAIL,20011,;", 20011 is a errorcode example
    def update_status(self, status):
        self.scriptStatus=status

    def SetOverride(self, vel):
        command = 'SetOverride,0,' + str(vel) + ',;'
        time.sleep(0.1)
        ret = self.sendAndRecv(command)
        time.sleep(0.1)
        return ret
    
    def SetPayload(self, mass,Center_X,Center_Y,Center_Z):
        command = 'SetPayload,0,'
        command += str(mass)
        command += ','
        command += str(Center_X)
        command += ','
        command += str(Center_Y)
        command += ','
        command += str(Center_Z)
        command += ',;'
        ret = self.sendAndRecv(command)
        return ret
    
    # 对应界面的1~6级
    def SetCollideLevel(self, level):
        command = 'SetCollideLevel,0,'
        command += str(level)
        command += ',;'
        ret = self.sendAndRecv(command)
        time.sleep(1)
        return ret

    # setTCP
    def setTCP(self, TCP):
        command = 'SetTCPByName,0,'
        command += str(TCP) + ',;'
        return self.sendAndRecv(command)

    def SetCurTCP(self,TCP):
        command = 'SetCurTCP,0,'
        for i in range(0, 6):
            command += str(TCP[i]) + ','
        command += ';'
        return self.sendAndRecv(command)

    # set UCS
    def setUCS(self, UCS):
        command = 'SetUCSByName,0,'
        command += str(UCS) + ',;'
        return self.sendAndRecv(command)
    
    def SetCurUCS(self,UCS):
        command = 'SetCurUCS,0,'
        for i in range(0, 6):
            command += str(UCS[i]) + ','
        command += ';'
        return self.sendAndRecv(command)

    # setTCP
    def ReadTCP(self, TCP):
        command = 'ReadTCPByName,0,'
        command += str(TCP) + ',;'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    def ConfigTCP(self,name,TCP):
        command = 'ConfigTCP,0,'
        command += str(name)
        command += ','
        for i in range(0, 6):
            command += str(TCP[i]) + ','
        command += ';'
        return self.sendAndRecv(command)  

    # set UCS
    def ReadUCS(self, UCS):
        command = 'ReadUCSByName,0,'
        command += str(UCS) + ',;'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    def ConfigUCS(self,name,UCS):
        command = 'ConfigUCS,0,'
        command += str(name)
        command += ','
        for i in range(0, 6):
            command += str(UCS[i]) + ','
        command += ';'
        return self.sendAndRecv(command)

    #SetMaxPcsRange
    def SetMaxPcsRange(self, pMax, pMin,pUcs):
        command = 'SetMaxPcsRange,0,'
        command += str(pMax[0])
        command += ','
        command += str(pMax[1])
        command += ','
        command += str(pMax[2])
        command += ','
        command += str(180)
        command += ','
        command += str(180)
        command += ','
        command += str(180)
        command += ','
        command += str(pMin[0])
        command += ','
        command += str(pMin[1])
        command += ','
        command += str(pMin[2])
        command += ','
        command += str(-180)
        command += ','
        command += str(-180)
        command += ','
        command += str(-180)
        command += ','
        for i in range(0, 6):
            command += str(pUcs[i]) + ','
        command += ';'
        return self.sendAndRecv(command)
        
    #SetMaxPcsRange
    def SetMaxAcsRange(self, pMax, pMin):
        command = 'SetMaxAcsRange,0,'
        command += str(pMax[0])
        command += ','
        command += str(pMax[1])
        command += ','
        command += str(pMax[2])
        command += ','
        command += str(pMax[3])
        command += ','
        command += str(pMax[4])
        command += ','
        command += str(pMax[5])
        command += ','
        command += str(pMin[0])
        command += ','
        command += str(pMin[1])
        command += ','
        command += str(pMin[2])
        command += ','
        command += str(pMin[3])
        command += ','
        command += str(pMin[4])
        command += ','
        command += str(pMin[5])
        command += ',;'
        return self.sendAndRecv(command)

    #UcsTcp2Base
    def UcsTcp2Base(self, UcsTcp, TCP, UCS):
        command = 'UcsTcp2Base,0,'
        for i in range(0, 6):
            command += str(UcsTcp[i]) + ','
        for i in range(0, 6):
            command += str(TCP[i]) + ','
        for i in range(0, 6):
            command += str(UCS[i]) + ','
        command += ';'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    #Base2UcsTcp
    def Base2UcsTcp(self, Base, TCP, UCS):
        command = 'Base2UcsTcp,0,'
        for i in range(0, 6):
            command += str(Base[i]) + ','
        for i in range(0, 6):
            command += str(TCP[i]) + ','
        for i in range(0, 6):
            command += str(UCS[i]) + ','
        command += ';'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    #PCS2ACS
    def PCS2ACS(self, rawPCS, rawACS, tcp, ucs):
        command = 'PCS2ACS,0,'
        for i in range(0, 6):
            command += str(rawPCS[i]) + ','
        for i in range(0, 6):
            command += str(rawACS[i]) + ','
        for i in range(0, 6):
            command += str(tcp[i]) + ','
        for i in range(0, 6):
            command += str(ucs[i]) + ','
        command += ';'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    def MovePath(self, trajectName):
        command = 'MovePath,0,'
        command += str(trajectName) + ','
        command += ';'
        return self.sendAndRecv(command)

    def InitMovePathL(self,strCmd,vel,acc,jerk,ucs,tcp):
        command = 'InitMovePathL,0,'
        command += strCmd + ','
        command += str(vel) + ','
        command += str(acc) + ','
        command += str(jerk) + ','
        command += ucs + ','
        command += tcp + ','
        command += ';'
        return self.sendAndRecv(command)

    def PushMovePaths(self, strName, nMoveType, pointsCNT, points):
        command = 'PushMovePaths,0,'
        command += strName
        command += ','
        command += str(nMoveType)
        command += ','
        command += str(pointsCNT)
        command += ','
        for pos in points:
            command += str(pos) + ','

        command += ';'
        return self.sendAndRecv(command)

    def MovePathL(self,strCmd):
        command = 'MovePathL,0,'
        command += strCmd
        command += ',;'
        return self.sendAndRecv(command)
    #moveType 
    #"MoveJ" 0   "MoveL" 1   "MoveC" 2
    #relMoveType 0 绝对值, 1 相对值
    
    def WayPointRel(self, type, usePointList, Point_PCS, Point_ACS, relMoveType, nAxisMask, Axis0,Axis1,Axis2,Axis3,Axis4,Axis5, tcp, ucs, speed, Acc, radius,isJoint, isSeek, bit, state, cmdID):
        command = 'WayPointRel,0,'
        command += str(type) + ','
        command += str(usePointList) + ','
        for i in range(0, 6):
            command += str(Point_PCS[i]) + ','
        for i in range(0, 6):
            command += str(Point_ACS[i]) + ','
        command += str(relMoveType) + ','
        for i in range(0, 6):
            command += str(nAxisMask[i]) + ','
        command += str(Axis0) + ','
        command += str(Axis1) + ','
        command += str(Axis2) + ','
        command += str(Axis3) + ','
        command += str(Axis4) + ','
        command += str(Axis5) + ','
        command += str(tcp) + ','
        command += str(ucs) + ','
        command += str(speed) + ','
        command += str(Acc) + ','
        command += str(radius) + ','
        command += str(isJoint) + ','
        command += str(isSeek) + ','
        command += str(bit) + ','
        command += str(state) + ','
        command += str(cmdID) + ','
        command += ';'
        return self.sendAndRecv(command)

    def WayPointEx(self, type, points, RawACSpoints, tcpname, ucs, speed, Acc, radius,isJoint, isSeek, bit, state, cmdID):
        tcp=self.ReadTCP(tcpname)
        command = 'WayPointEx,0,'
        for i in range(0, 6):
            command += str(points[i]) + ','
        for i in range(0, 6):
            command += str(RawACSpoints[i]) + ','
        for i in range(0, 6):
            command += str(ucs[i]) + ','
        for i in range(0, 6):
            command += str(tcp[i]) + ','
        #command += str(tcp) + ','
        command += str(speed) + ','
        command += str(Acc) + ','
        command += str(radius) + ','
        command += str(type) + ','
        command += str(isJoint) + ','
        command += str(isSeek) + ','
        command += str(bit) + ','
        command += str(state) + ','
        command += str(cmdID) + ','
        command += ';'
        return self.sendAndRecv(command)

    def WayPoint(self, type, points, RawACSpoints, tcp, ucs, speed, Acc, radius,isJoint, isSeek, bit, state, cmdID):
        command = 'WayPoint,0,'
        for i in range(0, 6):
            command += str(points[i]) + ','
        for i in range(0, 6):
            command += str(RawACSpoints[i]) + ','
        command += str(tcp) + ','
        command += str(ucs) + ','
        command += str(speed) + ','
        command += str(Acc) + ','
        command += str(radius) + ','
        command += str(type) + ','
        command += str(isJoint) + ','
        command += str(isSeek) + ','
        command += str(bit) + ','
        command += str(state) + ','
        command += str(cmdID) + ','
        command += ';'
        return self.sendAndRecv(command)

    def wayPoint2(self, EndPos, AuxPos, AcsPos, Tcp, Ucs, Vel, Acc, Radius, Type, nIsUseJoint, nIsSeek, nBit, nDIType, strPointGuid, StartPos=None, fixedPosure=None):
        command = 'WayPoint2,0,'
        for i in range(0, 6):
            command += str(EndPos[i]) + ','
        for i in range(0, 6):
            command += str(AcsPos[i]) + ','
        command += Tcp + ','
        command += Ucs + ','
        command += str(Vel) + ','
        command += str(Acc) + ','
        command += str(Radius) + ','
        command += str(Type) + ','
        command += str(nIsUseJoint) + ','
        command += str(nIsSeek) + ','
        command += str(nBit) + ','
        command += str(nDIType) + ','
        for i in range(0, 6):
            command += str(AuxPos[i]) + ','
        command += strPointGuid + ','
        if StartPos is not None and fixedPosure is not None:
            for i in range(0, 6):
                command += str(StartPos[i]) + ','
            command += str(fixedPosure) + ','
        command += ';'
        return self.sendAndRecv(command)
        
    def SetMoveParamsAO(self, nState, nIndex, dInitAO, dWeldingAO):
        command = 'SetMoveParamsAO,0,'
        command += str(nState) + ','
        command += str(nIndex) + ','
        command += str(dInitAO) + ','
        command += str(dWeldingAO) + ',;'
        ret = self.sendAndRecv(command) 
        time.sleep(0.1)
        return ret

    def MoveC(self, StartPoint, AuxPoint, EndPoint, fixedPosure, nMoveCType, nRadLen,speed, Acc, radius, tcp, ucs, cmdID):
        command = 'MoveC,0,'
        for i in range(0, 6):
            command += str(StartPoint[i]) + ','
        for i in range(0, 6):
            command += str(AuxPoint[i]) + ','
        for i in range(0, 6):
            command += str(EndPoint[i]) + ','
        command += str(fixedPosure) + ','
        command += str(nMoveCType) + ','
        command += str(nRadLen) + ','
        command += str(speed) + ','
        command += str(Acc) + ','
        command += str(radius) + ','
        command += str(tcp) + ','
        command += str(ucs) + ','
        command += str(cmdID) + ','
        command += ';'
        return self.sendAndRecv(command)

    def MoveZ(self, StartPoint, EndPoint, PlanePoint, Speed, Acc, WIdth, Density, EnableDensity, EnablePlane, EnableWaiTime, PosiTime, NegaTime, Radius, tcp, ucs, cmdID):
        command = 'MoveZ,0,'
        for i in range(0, 6):
            command += str(StartPoint[i]) + ','
        for i in range(0, 6):
            command += str(EndPoint[i]) + ','
        for i in range(0, 6):
            command += str(PlanePoint[i]) + ','
        command += str(Speed) + ','
        command += str(Acc) + ','
        command += str(WIdth) + ','
        command += str(Density) + ','
        command += str(EnableDensity) + ','
        command += str(EnablePlane) + ','
        command += str(EnableWaiTime) + ','
        command += str(PosiTime) + ','
        command += str(NegaTime) + ','
        command += str(Radius) + ','
        command += str(tcp) + ','
        command += str(ucs) + ','
        command += str(cmdID) + ','
        command += ';'
        return self.sendAndRecv(command)

    #SetForceControlState
    def SetForceControlState(self, state):
        command = 'SetForceControlState,0,' + str(state) + ',;'
        retData = self.sendAndRecv(command)
        if retData[1] == 'OK':
            while True:
                command = 'ReadFTControlState,0,;'
                retData = self.sendAndRecv(command)
                if state == 1:
                    if int(retData[2]) == 2:
                        break
                elif state == 0:
                    if int(retData[2]) != 2:
                        time.sleep(0.05)
                        break
                time.sleep(0.01)
            #time.sleep(0.05)
        return True

    #SetScriptForceControlState
    def SetScriptForceControlState(self, state,FTMode,UCS,TCP,vel,forces,freedom,PID,Mass,Damp,Stiff):
        command = 'SetScriptForceControlState,0,' + str(state) + ','
        command += str(FTMode) + ','
        command += str(UCS) + ','
        command += str(TCP) + ','
        for i in range(0, 2):
            command += str(vel[i]) + ','
        for i in range(0, 6):
            command += str(forces[i]) + ','
        for i in range(0, 6):
            command += str(freedom[i]) + ','
        for i in range(0, 6):
            command += str(PID[i]) + ','
        for i in range(0, 6):
            command += str(Mass[i]) + ','
        for i in range(0, 6):
            command += str(Damp[i]) + ','
        for i in range(0, 6):
            command += str(Stiff[i]) + ','
        command += ';'
        retData = self.sendAndRecv(command)
        if retData[1] == 'OK':
            while True:
                command = 'ReadFTControlState,0,;'
                retData = self.sendAndRecv(command)
                if state == 1:
                    if int(retData[2]) == 2:
                        break
                elif state == 0:
                    if int(retData[2]) != 2:
                        time.sleep(self.readSleep)
                        break
                time.sleep(self.readSleep)
        return True

    def SetLinearSearchParams(self,direction,distance,velocity,ucs,tcp,threshold):
        command = 'SetLinearSearchParams,0,'
        command += str(direction) + ','
        command += str(distance) + ','
        command += str(velocity) + ','
        command += ucs + ','
        command += tcp + ','
        command += str(threshold) + ',;'
        return self.sendAndRecv(command)
    
    def SetRotationSearchParams(self,direction,maxRotationAngel,velocity,ucs,tcp,forceThresholdToStartLinear,torqueThreshold):
        command = 'SetRotationSearchParams,0,'
        command += str(direction) + ','
        command += str(maxRotationAngel) + ','
        command += str(velocity) + ','
        command += ucs + ','
        command += tcp + ','
        command += str(forceThresholdToStartLinear) + ','
        command += str(torqueThreshold) + ','
        command += ';'
        return self.sendAndRecv(command)

    def SetSpiralSearchParams(self,radiusInc,maxRadus,velocity,ucs,tcp,forceThresholdToStartLinear):
        command = 'SetSpiralSearchParams,0,'
        command += str(radiusInc) + ','
        command += str(maxRadus) + ','
        command += str(velocity) + ','
        command += ucs + ','
        command += tcp + ','
        command += str(forceThresholdToStartLinear) + ','
        command += ';'
        return self.sendAndRecv(command)

    def StartSearchMotion(self,searchType):
        command = 'StartSearchMotion,0,'
        command += str(searchType) + ','
        command += ';'
        return self.sendAndRecv(command)
    
    def WaitSearchMotionDone(self):
        command = 'ReadSearchState,0,;'
        time.sleep(0.2)
        while True:
            ret = self.sendAndRecv(command)
            # 搜索运动中
            if (ret[2] == '0'):
                continue
            # 满足停止条件，搜索成功
            elif (ret[2] == '1'):
                self.SetForceControlState(0)
                self.sendScriptLog(2,'满足停止条件，搜索成功')
                return True
            # 运动完成，但未满足停止状态，搜孔失败失败
            elif (ret[2] == '2'):
                self.SetForceControlState(0)
                self.sendScriptLog(2,'运动完成或力阈值超过，未满足停止状态，搜孔失败失败')
                return False
            elif (ret[2] == '3'):
                self.SetForceControlState(0)
                self.sendScriptLog(2,'直线下插成功')
                return True
    
    def WaitRotationSearchDone(self):
        time.sleep(0.1)
        while True:
            self.sendScriptLog(2,'开始直线下插')
            self.StartSearchMotion(0)
            ret = self.WaitSearchMotionDone()
            if (ret == True):
                # 下插成功，退出搜索
                return True
            time.sleep(0.1)
            self.sendScriptLog(2,'直线下插失败，开始旋转搜索')
            self.StartSearchMotion(1)
            if(self.WaitSearchMotionDone() == True):
                # 搜索成功，继续直线下插
                continue
            else:
                self.sendScriptLog(2,'旋转搜索失败，返回false')
                # 搜索失败，返回false
                return False

    def WaitSpiralSearchDone(self):
        time.sleep(0.1)
        while True:
            self.sendScriptLog(2,'开始直线下插')
            self.StartSearchMotion(0)
            if (self.WaitSearchMotionDone() == True):
                return True
            self.sendScriptLog(2,'直线下插失败，开始螺旋搜索')
            self.StartSearchMotion(2)
            time.sleep(0.1)
            # 读取搜索状态
            if(self.WaitSearchMotionDone() == True):
                self.sendScriptLog(2,'螺旋搜索成功，继续直线下插')
                # 搜索成功，继续直线下插
                continue
            else:
                self.sendScriptLog(2,'螺旋搜索失败，返回false')
                # 搜索失败，返回false
                return False

    #Pose_Add
    def Pose_Add(self, pos1, pos2):
        command = 'PoseAdd,0,'
        for i in range(0, 6):
            command += str(pos1[i]) + ','
        for i in range(0, 6):
            command += str(pos2[i]) + ','
        command += ';'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

	#Pose_Sub
    def Pose_Sub(self, pos1, pos2):
        command = 'PoseSub,0,'
        for i in range(0, 6):
            command += str(pos1[i]) + ','
        for i in range(0, 6):
            command += str(pos2[i]) + ','
        command += ';'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData
    
	#Pose_Trans
    def Pose_Trans(self, pos1, pos2):
        command = 'PoseTrans,0,'
        for i in range(0, 6):
            command += str(pos1[i]) + ','
        for i in range(0, 6):
            command += str(pos2[i]) + ','
        command += ';'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    #Pose_Trans
    def Pose_Inverse(self, pos1):
        command = 'PoseInverse,0,'
        for i in range(0, 6):
            command += str(pos1[i]) + ','
        command += ';'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    #Pose_DefdFrame
    def Pose_DefdFrame(self, pos1, pos2, pos3, pos4, pos5, pos6):
        command = 'PoseDefdFrame,0,'
        for i in range(0, 3):
            command += str(pos1[i]) + ','
        for i in range(0, 3):
            command += str(pos2[i]) + ','
        for i in range(0, 3):
            command += str(pos3[i]) + ','
        for i in range(0, 3):
            command += str(pos4[i]) + ','
        for i in range(0, 3):
            command += str(pos5[i]) + ','
        for i in range(0, 3):
            command += str(pos6[i]) + ','
        command += ';'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    
    # read Robot state
    def readRobotState(self):
        return self.sendAndRecv('ReadRobotState,0,;')
        
    def ReadCurFSM(self):
        retData = self.sendAndRecv('ReadCurFSM,0,;')
        del retData[0]
        del retData[0]
        retData.pop()
        return int(retData[0])

    # stop Moving
    # no parameter
    def stop(self):
        return self.sendAndRecv('GrpStop,0,;')

    #Pose_ReadJoint
    def Pose_ReadJoint(self):
        retData = self.ReadActPos()
        retData.pop()
        retData.pop()
        retData.pop()
        retData.pop()
        retData.pop()
        retData.pop()
        return retData
		
    #Pose_ReadPos
    def Pose_ReadPos(self):
        retData = self.ReadActPos()
        del retData[0]
        del retData[0]
        del retData[0]
        del retData[0]
        del retData[0]
        del retData[0]
        return retData

    # ReadActPos
    # 
    # @output = [J1,J2,J3,J4,J5,J6,X,Y,Z,RX,RY,RZ]
    # !! the points in Cartesian coordinates is based on the base coordinates (not user coordinates)
    #
    def ReadActPos(self):
        retData = self.sendAndRecv('ReadActPos,0,;')
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    # readPointList
    # 
    # @output = [J1,J2,J3,J4,J5,J6,X,Y,Z,RX,RY,RZ,TCP_X,TCP_Y,TCP_Z,TCP_RX,TCP_RY,TCP_RZ,UCS_X,UCS_Y,UCS_Z,UCS_RX,UCS_RY,UCS_RZ]
    # !! the points in Cartesian coordinates is based on the base coordinates (not user coordinates)
    #
    def ReadPointByName(self,point_name):
        command = 'ReadPointByName,0,'+ str(point_name) + ',;'
        retData = self.sendAndRecv(command)        
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    # send the finish code 
    # if py has running finish 
    # py should send a finish message to CC
    # No output 
    def sendScriptFinish(self,errorcode):
        command = 'SendScriptFinish,0,'+ str(errorcode) + ',;'
        self.sendAndRecv(command)        
    
    # config IO
    def readCI(self, bit):
        command = 'ReadBoxCI,' + str(bit) + ',;'
        retData = self.sendAndRecv(command)
        #time.sleep(self.readSleep)
        return int(retData[2])

    def readCO(self, bit):
        command = 'ReadBoxCO,' + str(bit) + ',;'
        retData = self.sendAndRecv(command)
        #time.sleep(self.readSleep)
        return int(retData[2])

    def setCO(self, bit, state):
        command = 'SetBoxCO,' + str(bit) + ',' + str(state) + ',;'
        return self.sendAndRecv(command)
    
    def cdsSetIO(self, nEndDOMask,nEndDOVal,nBoxDOMask,nBoxDOVal,nBoxCOMask,nBoxCOVal,nBoxAOCH0_Mask,nBoxAOCH0_Mode,nBoxAOCH1_Mask,nBoxAOCH1_Mode,dbBoxAOCH0_Val,dbBoxAOCH1_Val):
        command = 'cdsSetIO,'
        command += str(nEndDOMask) + ','
        command += str(nEndDOVal) + ','
        command += str(nBoxDOMask) + ','
        command += str(nBoxDOVal) + ','
        command += str(nBoxCOMask) + ','
        command += str(nBoxCOVal) + ','
        command += str(nBoxAOCH0_Mask) + ','
        command += str(nBoxAOCH0_Mode) + ','
        command += str(nBoxAOCH1_Mask) + ','
        command += str(nBoxAOCH1_Mode) + ','
        command += str(dbBoxAOCH0_Val) + ','
        command += str(dbBoxAOCH1_Val) + ','
        command += ';'
        return self.sendAndRecv(command)
    
    def SetTrackingState(self, state):
        command = 'SetTrackingState,0,' + str(state) + ',;'
        return self.sendAndRecv(command)

    def HRApp(self,name,param):
        command = 'HRAppCmd,'
        command += str(name) + ','
        if isinstance(param,list):
            for item in param:
               command += str(item) + ','
        else:
            command += str(param) + ','
        command += ';'
        return self.sendAndRecv(command)

    #  
    def readDO(self, bit):
        command = 'ReadBoxDO,' + str(bit) + ',;'
        retData = self.sendAndRecv(command)
        #time.sleep(self.readSleep)
        return int(retData[2])

    def readDI(self, bit):
        command = 'ReadBoxDI,' + str(bit) + ',;'
        retData = self.sendAndRecv(command)
        #time.sleep(self.readSleep)
        return int(retData[2])

    def setDO(self, bit, state):
        command = 'SetBoxDO,' + str(bit) + ',' + str(state) + ',;'
        return self.sendAndRecv(command)
    
    def readBoxAI(self, bit):
        command = 'ReadBoxAI,' + str(bit) + ',;'
        retData = self.sendAndRecv(command)
        #time.sleep(self.readSleep)
        return float(retData[2])

    def readAO(self, bit):
        command = 'ReadBoxAO,' + str(bit) + ',;'
        retData = self.sendAndRecv(command)
        #time.sleep(self.readSleep)
        return float(retData[3])

    def SetBoxAO(self,index,value,pattern):
        command = 'SetBoxAO,' + str(index) + ',' + str(value) + ',' + str(pattern) + ',;'
        return self.sendAndRecv(command)

    #  end io
    def readEI(self, bit):
        command = 'ReadEI,0,' + str(bit) + ',;'
        retData = self.sendAndRecv(command)
        #time.sleep(self.readSleep)
        return int(retData[2])

    def readEO(self, bit):
        command = 'ReadEO,0,' + str(bit) + ',;'
        retData = self.sendAndRecv(command)
        #time.sleep(self.readSleep)
        return int(retData[2])

    def setEO(self, bit, state):
        command = 'SetEndDO,0,' + str(bit) + ',' + str(state) + ',;'
        retData =  self.sendAndRecv(command)
        time.sleep(1)
        return retData

    def readEAI(self, bit):
        command = 'ReadEAI,0,' + str(bit) + ',;'
        retData = self.sendAndRecv(command)
        #time.sleep(self.readSleep)
        return float(retData[2])

    def readEndModbusRegisters(self, nSlaveID, nFunction, nRegAddr, nRegCount):
        command = 'ReadHoldingRegisters,0,'
        command += str(nSlaveID) + ','
        command += str(nFunction) + ','
        command += str(nRegAddr) + ','
        command += str(nRegCount) + ',;'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    def writeEndModbusRegisters(self, nSlaveID, nFunction, nRegAddr, nRegCount, data):
        command = 'WriteHoldingRegisters,0,'
        command += str(nSlaveID) + ','
        command += str(nFunction) + ','
        command += str(nRegAddr) + ','
        command += str(nRegCount) + ','
        for i in range(0, nRegCount):
            command += str(data[i]) + ','
        command += ';'
        return self.sendAndRecv(command)

    def SetMoveParamsAO(self, nState, nIndex, dInitAO, dWeldingAO):
        command = 'SetMoveParamsAO,0,'
        command += str(nState) + ','
        command += str(nIndex) + ','
        command += str(dInitAO) + ','
        command += str(dWeldingAO) + ',;'
        return self.sendAndRecv(command)


    def CalDistance(self, FirstX, FirstY, FirstZ, FirstRx, FirstRy, FirstRz, SecondX, SecondY, SecondZ, SecondRx, SecondRy, SecondRz):
        command = 'CalPointDistance,0,'
        command += str(FirstX) + ','
        command += str(FirstY) + ','
        command += str(FirstZ) + ','
        command += str(FirstRx) + ','
        command += str(FirstRy) + ','
        command += str(FirstRz) + ','
        command += str(SecondX) + ','
        command += str(SecondY) + ','
        command += str(SecondZ) + ','
        command += str(SecondRx) + ','
        command += str(SecondRy) + ','
        command += str(SecondRz) + ',;'
        return self.sendAndRecv(command)


    def SetMoveTraceParams(self, nState, dDistance,dAwayVelocity,dGobackVelocity):
        command = 'SetMoveTraceParams,0,'
        command += str(nState) + ','
        command += str(dDistance) + ','
        command += str(dAwayVelocity) + ','
        command += str(dGobackVelocity) + ',;'
        return self.sendAndRecv(command)

    def SetMoveTraceInitParams(self, dK, dB,dMaxLimit, dMinLimit):
        command = 'SetMoveTraceInitParams,0,'
        command += str(dK) + ','
        command += str(dB) + ','
        command += str(dMaxLimit) + ','
        command += str(dMinLimit) + ',;'
        return self.sendAndRecv(command)

    def SetMoveTraceUcs(self, dX, dY, dZ, dRx, dRy, dRz):
        command = 'SetMoveTraceUcs,0,'
        command += str(dX) + ','
        command += str(dY) + ','
        command += str(dZ) + ','
        command += str(dRx) + ','
        command += str(dRy) + ','
        command += str(dRz) + ',;'
        return self.sendAndRecv(command)
    
    def SetPoseTrackingMaxMotionLimit(self, dMaxLineVel, dMaxOriVel):
        command = 'SetPoseTrackingMaxMotionLimit,0,'
        command += str(dMaxLineVel) + ','
        command += str(dMaxOriVel) + ',;'
        return self.sendAndRecv(command)
    
    def SetPoseTrackingPIDParams(self, dPosPID1, dPosPID2, dPosPID3, dOriPID1, dOriPID2, dOriPID3):
        command = 'SetPoseTrackingPIDParams,0,'
        command += str(dPosPID1) + ','
        command += str(dPosPID2) + ','
        command += str(dPosPID3) + ','
        command += str(dOriPID1) + ','
        command += str(dOriPID2) + ','
        command += str(dOriPID3) + ',;'
        return self.sendAndRecv(command)
    
    def SetPoseTrackingTargetPos(self, dX, dY, dZ, dRx, dRy, dRz):
        command = 'SetPoseTrackingTargetPos,0,'
        command += str(dX) + ','
        command += str(dY) + ','
        command += str(dZ) + ','
        command += str(dRx) + ','
        command += str(dRy) + ','
        command += str(dRz) + ',;'
        return self.sendAndRecv(command)
    
    def SetPoseTrackingState(self, nState):
        command = 'SetPoseTrackingState,0,'
        command += str(nState) + ',;'
        return self.sendAndRecv(command)
    
    def SetUpdateTrackingPose(self, dX, dY, dZ, dRx, dRy, dRz):
        command = 'SetUpdateTrackingPose,0,'
        command += str(dX) + ','
        command += str(dY) + ','
        command += str(dZ) + ','
        command += str(dRx) + ','
        command += str(dRy) + ','
        command += str(dRz) + ',;'
        return self.sendAndRecv(command)
    
    ###################################################################################################
    # modbus 
    #setModbus
    #ModbusCoil value range : 0~255
    #ModbusHoldingRegister value range :0~65535
    #支持二、八、十、十六进制输入值
    def setModbus(self,deviceName,varName,value):
        val = 0
        if isinstance(value,(list,tuple)):
            if not value:
                raise ValueError(f"Invalid type for value:{type(value).__name__}.Value must be a integer.")
            val = value[0]
        else:
            val = value
        if not isinstance(val,(int)):
            raise ValueError(f"Invalid type for value:{type(val).__name__}.Value must be a integer.")
        command = 'SetExDeviceData,' + str(deviceName) + ',' + str(varName) + ',' + str(val) + ',;'
        return self.sendAndRecv(command)

    def getModbus(self,deviceName,varName):
        command = 'ReadExDeviceData,' + str(deviceName) + ',' + str(varName) + ',;'
        retData = self.sendAndRecv(command)
        #time.sleep(self.readSleep)
        return int(retData[2])

    def SetECATAO(self, nRbtID, nIndex, dVal):
        command = 'SetECATAO,0,'
        command += str(nIndex) + ','
        command += str(dVal) + ',;'
        return self.sendAndRecv(command)
    #setModbusDatas
    #values Maximum quantity:125
    #ModbusCoil value range : 0~255
    #ModbusHoldingRegister value range :0~65535
    #支持二、八、十、十六进制输入值
    def setModbusDatas(self,deviceName,varName,values):
        if isinstance(values, (list, tuple)) and len(values) > 0 and all(isinstance(x, (int)) for x in values):
            pass  # Validate that all elements in the list or tuple are integers
        else:
            raise TypeError(f"Invalid type for values: {type(values).__name__}. Values must be a non-empty list/tuple of integer.")

        command = 'SetExDeviceDatas,' + str(deviceName) + ',' + str(varName) +','
        for item in values:
            command += str(item) + ','
        command += ';'
        return self.sendAndRecv(command)

    def getModbusDatas(self,deviceName,varName,nCNT):
        command = 'ReadExDeviceDatas,' + str(deviceName) + ',' + str(varName) +',' + str(nCNT) + ',;'
        retData = self.sendAndRecv(command)
        del retData[0]
        del retData[0]
        retData.pop()
        return retData

    ###################################################################################################
    #MBSlave_ReadCoils
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_ReadCoils(self,addr,nb):
        # 与机器人主站进行通信，读取多个线圈寄存器
        #time.sleep(self.readSleep)
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. address must be an integer.")
        if addr < 0 or addr > 1499:
            raise ValueError(f"Invalid value for address: {addr},The input address range needs to be between 0 and 1499.")
        if not isinstance(nb, int):
            raise TypeError(f"Invalid type for nb: {type(nb).__name__}. nb must be an integer.")
        if nb <= 0:
            raise ValueError(f"Invalid value for nb: {nb}. nb must be a positive integer.")
        if (nb + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to read {nb} coil registers starting at address {addr} exceeds the allowed range [0, 1499].")
        return MBMaster.Modbus_Master.execute(1, cst.READ_COILS, addr, nb)

    #MBSlave_ReadInputCoils
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_ReadInputCoils(self,addr,nb):
        # 与机器人主站进行通信，读取输入线圈寄存器
        #time.sleep(self.readSleep)
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. address must be an integer.")
        if addr < 0 or addr > 1499:
            raise ValueError(f"Invalid value for address: {addr},The input address range needs to be between 0 and 1499.")
        if not isinstance(nb, int):
            raise TypeError(f"Invalid type for nb: {type(nb).__name__}. nb must be an integer.")
        if nb <= 0:
            raise ValueError(f"Invalid value for nb: {nb}. nb must be a positive integer.")
        if (nb + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to read {nb} input coil registers starting at address {addr} exceeds the allowed range [0, 1499].")
        return MBMaster.Modbus_Master.execute(1, cst.READ_DISCRETE_INPUTS, addr, nb)

    #MBSlave_ReadHoldingRegisters
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_ReadHoldingRegisters(self,addr,nb):
        # 与机器人主站进行通信，读取保持寄存器
        #time.sleep(self.readSleep)
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. address must be an integer.")
        if addr < 0 or addr > 1499:
            raise ValueError(f"Invalid value for address: {addr},The input address range needs to be between 0 and 1499.")
        if not isinstance(nb, int):
            raise TypeError(f"Invalid type for nb: {type(nb).__name__}. nb must be an integer.")
        if nb <= 0:
            raise ValueError(f"Invalid value for nb: {nb}. nb must be a positive integer.")
        if (nb + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to read {nb} holding registers starting at address {addr} exceeds the allowed range [0, 1499].")
        return MBMaster.Modbus_Master.execute(1, cst.READ_HOLDING_REGISTERS, addr, nb)

    #MBSlave_ReadInputRegisters
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_ReadInputRegisters(self,addr,nb):
        # 与机器人主站进行通信，读取输入寄存器
        #time.sleep(self.readSleep)
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. address must be an integer.")
        if addr < 0 or addr > 1499:
            raise ValueError(f"Invalid value for address: {addr},The input address range needs to be between 0 and 1499.")
        if not isinstance(nb, int):
            raise TypeError(f"Invalid type for nb: {type(nb).__name__}. nb must be an integer.")
        if nb <= 0:
            raise ValueError(f"Invalid value for nb: {nb}. nb must be a positive integer.")
        if (nb + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to read {nb} input registers starting at address {addr} exceeds the allowed range [0, 1499].")
        return MBMaster.Modbus_Master.execute(1, cst.READ_INPUT_REGISTERS, addr, nb)
    ###################################################################################################
    #MBSlave_WriteCoils
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_WriteCoils(self,addr,val):
        # 与机器人主站进行通信，写入多个线圈寄存器
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. Address must be an integer.")
        if addr < 0 or addr > 1499:
            raise ValueError(f"Invalid value for address: {addr}. The input address range needs to be between 0 and 1499.")
        if isinstance(val, (list, tuple)) and len(val) > 0 and all(isinstance(x, (int,float)) for x in val):
            pass  # Validate that all elements in the list or tuple are integers
        else:
            raise TypeError(f"Invalid type for value: {type(val).__name__}. Value must be a non-empty list/tuple of integers.")
        if (len(val) + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to write {len(val)} coil registers starting at address {addr} exceeds the allowed range [0, 1499].")
        return MBMaster.Modbus_Master.execute(1, cst.WRITE_MULTIPLE_COILS, addr, output_value=val)
    
    #MBSlave_WriteCoil
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_WriteCoil(self,addr,val):
        # 与机器人主站进行通信，写入单个线圈寄存器
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. Address must be an integer.")
        if addr < 0 or addr > 1499:
            raise ValueError(f"Invalid value for address: {addr}. The input address range needs to be between 0 and 1499.")
        valCoil=0
        if isinstance(val,(list,tuple)):
            if not val:  # Check if the list or tuple is empty
                raise ValueError("Value list is empty. A non-empty list or tuple of integers is required.")
            valCoil=val[0]
        else:
            valCoil=val
        if not isinstance(valCoil, (int,float)):
            raise TypeError(f"Invalid type for value: {type(valCoil).__name__}. Value must be an integer.")
        if (1 + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to write 1 coil register starting at address {addr} exceeds the allowed range [0, 1499].")
        return MBMaster.Modbus_Master.execute(1, cst.WRITE_SINGLE_COIL, addr, output_value=valCoil)
    ###################################################################################################
    #MBSlave_WriteHoldingRegisters
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_WriteHoldingRegisters(self,addr,val):
        # 与机器人主站进行通信，写入多个保持寄存器
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. Address must be an integer.")
        if addr < 0 or addr > 1499:
            raise ValueError(f"Invalid value for address: {addr}. The input address range needs to be between 0 and 1499.")
        if isinstance(val, (list, tuple)) and all(isinstance(x, (int)) for x in val) and len(val) > 0:
            pass  # Validate that all elements in the list or tuple are integers
        else:
            raise TypeError(f"Invalid type for value: {type(val).__name__}. Value must be a non-empty list/tuple of integers.")
        if (len(val) + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to write {len(val)} holding registers starting at address {addr} exceeds the allowed range [0, 1499].")
        return MBMaster.Modbus_Master.execute(1, cst.WRITE_MULTIPLE_REGISTERS, addr, output_value=val)
    #MBSlave_WriteHoldingRegister
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_WriteHoldingRegister(self,addr,val):
        # 与机器人主站进行通信，写入单个保持寄存器
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. Address must be an integer.")
        if addr < 0 or addr > 1499:
            raise ValueError(f"Invalid value for address: {addr}. The input address range needs to be between 0 and 1499.")
        valCoil=0
        if isinstance(val,(list,tuple)):
            if not val:  # Check if the list or tuple is empty
                raise ValueError("Value list is empty. A non-empty list or tuple of integers is required.")
            valCoil=val[0]
        else:
            valCoil=val
        if not isinstance(valCoil, (int)):
            raise TypeError(f"Invalid type for value: {type(valCoil).__name__}. Value must be an integer.")
        if (1 + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to write 1 holding register starting at address {addr} exceeds the allowed range [0, 1499].")
        return MBMaster.Modbus_Master.execute(1, cst.WRITE_SINGLE_REGISTER, addr, output_value=valCoil)
    ###################################################################################################
    #MBSlave_WriteHoldingRegisters_Float
    #addr value range : 0~1499
    #支持二、八、十、十六进制整数型输入值、支持十进制浮点型输入值
    def MBSlave_WriteHoldingRegisters_Float(self, addr, val):
        # 与机器人主站进行通信，写入多个浮点型保持寄存器
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. Address must be an integer.")
        if addr < 0 or addr > 1498:
            raise ValueError(f"Invalid value for address: {addr}. The input address range needs to be between 0 and 1498.")
        if isinstance(val, (list, tuple)) and all(isinstance(x, (int,float)) for x in val) and len(val) > 0:
            pass
        else:
            raise ValueError("Value list is empty. A non-empty list of floats is required.")
        if(addr + 2*len(val)) > 1500:
            raise ValueError(f"Invalid range: Attempting to write {2*len(val)} holding registers starting at address {addr} exceeds the allowed range [0, 1499].")
        valsend = []
        for item in val:
            valsend.extend(WriteFloat(item))
        return self.MBSlave_WriteHoldingRegisters(addr, valsend)

    #MBSlave_ReadHoldingRegisters_Float
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_ReadHoldingRegisters_Float(self,addr,nb):
        # 与机器人主站进行通信，读取多个浮点型保持寄存器
        #time.sleep(self.readSleep)
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. address must be an integer.")
        if addr < 0 or addr > 1498:
            raise ValueError(f"Invalid value for address: {addr},The input address range needs to be between 0 and 1498.")
        if not isinstance(nb, int):
            raise TypeError(f"Invalid type for nb: {type(nb).__name__}. nb must be an integer.")
        if nb <= 0:
            raise ValueError(f"Invalid value for nb: {nb}. nb must be a positive integer.")
        if (2*nb + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to read {2*nb} holding registers starting at address {addr} exceeds the allowed range [0, 1499].")
        valsend=[]
        for item in range(0,nb):
         nval=ReadFloat(self.MBSlave_ReadHoldingRegisters(item*2+addr,2))
         valsend.append(nval)
        return valsend
    ###################################################################################################
    #MBSlave_WriteHoldingRegisters_Int
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_WriteHoldingRegisters_Int(self,addr,val):
        # 与机器人主站进行通信，写入多个整型保持寄存器
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. Address must be an integer.")
        if addr < 0 or addr > 1498:
            raise ValueError(f"Invalid value for address: {addr}. The input address range needs to be between 0 and 1498.")
        if isinstance(val, (list, tuple)) and all(isinstance(x, int) for x in val) and len(val) > 0:
            pass
        else:
            raise ValueError("Value list is empty. A non-empty list of floats is required.")
        if(addr + 2*len(val)) > 1500:
            raise ValueError(f"Invalid range: Attempting to write {2*len(val)} holding registers starting at address {addr} exceeds the allowed range [0, 1499].")
        valsend=[]
        for item in val:
         valsend.extend(WriteDint(item))
        return self.MBSlave_WriteHoldingRegisters(addr,valsend)

    #MBSlave_ReadHoldingRegisters_Int
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_ReadHoldingRegisters_Int(self,addr,nb):
        # 与机器人主站进行通信，读取多个整型保持寄存器
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. address must be an integer.")
        if addr < 0 or addr > 1498:
            raise ValueError(f"Invalid value for address: {addr},The input address range needs to be between 0 and 1498.")
        if not isinstance(nb, int):
            raise TypeError(f"Invalid type for nb: {type(nb).__name__}. nb must be an integer.")
        if nb <= 0:
            raise ValueError(f"Invalid value for nb: {nb}. nb must be a positive integer.")
        if (2*nb + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to read {2*nb} holding registers starting at address {addr} exceeds the allowed range [0, 1499].")
        valsend=[]
        for item in range(0,nb):
         nval=ReadDint(self.MBSlave_ReadHoldingRegisters(item*2+addr,2))
         valsend.append(nval)
        return valsend
    ###################################################################################################
    #MBSlave_ReadInputRegisters_Float
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_ReadInputRegisters_Float(self,addr,nb):
        # 与机器人主站进行通信，读取多个浮点型输入寄存器
        #time.sleep(self.readSleep)
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. address must be an integer.")
        if addr < 0 or addr > 1498:
            raise ValueError(f"Invalid value for address: {addr},The input address range needs to be between 0 and 1498.")
        if not isinstance(nb, int):
            raise TypeError(f"Invalid type for nb: {type(nb).__name__}. nb must be an integer.")
        if nb <= 0:
            raise ValueError(f"Invalid value for nb: {nb}. nb must be a positive integer.")
        if (2*nb + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to read {2*nb} input registers starting at address {addr} exceeds the allowed range [0, 1499].")
        valsend=[]
        for item in range(0,nb):
         nval=ReadFloat(self.MBSlave_ReadInputRegisters(item*2+addr,2))
         valsend.append(nval)
        return valsend

    #MBSlave_ReadInputRegisters_Int
    #addr value range : 0~1499
    #支持二、八、十、十六进制输入值
    def MBSlave_ReadInputRegisters_Int(self,addr,nb):
        # 与机器人主站进行通信，读取多个整型输入寄存器
        #time.sleep(self.readSleep)
        if not isinstance(addr, int):
            raise TypeError(f"Invalid type for address: {type(addr).__name__}. address must be an integer.")
        if addr < 0 or addr > 1498:
            raise ValueError(f"Invalid value for address: {addr},The input address range needs to be between 0 and 1498.")
        if not isinstance(nb, int):
            raise TypeError(f"Invalid type for nb: {type(nb).__name__}. nb must be an integer.")
        if nb <= 0:
            raise ValueError(f"Invalid value for nb: {nb}. nb must be a positive integer.")
        if (2*nb + addr) > 1500:
            raise ValueError(f"Invalid range: Attempting to read {2*nb} input registers starting at address {addr} exceeds the allowed range [0, 1499].")
        valsend=[]
        for item in range(0,nb):
         nval=ReadDint(self.MBSlave_ReadInputRegisters(item*2+addr,2))
         valsend.append(nval)
        return valsend
        
    def SetEncoderDriverIOParameters(self,nRbtID,TriggerType,DetectionTime):
        command = 'SetEncoderDriverIOParameters,'
        command+=str(nRbtID)+','
        for i in range(0, 8):
            command += str(TriggerType[i]) + ','
        command+=str(DetectionTime)+','
        command += ';'
        return self.sendAndRecv(command)

    def SetTrackingCompensation(self,nRbtID,Switch,Compensation):
        command = 'SetTrackingCompensation,'
        command+=str(nRbtID)+','
        command += str(Switch) + ','
        command += str(Compensation) + ','
        command += ';'
        return self.sendAndRecv(command)
    
    def SetUpperLimitCompensation(self,nRbtID,dis_Compensation):
        command = 'SetUpperLimitCompensation,'
        command+=str(nRbtID)+','
        command += str(dis_Compensation) + ','
        command += ';'
        return self.sendAndRecv(command)

    def ReadEncoderDriverIOStatus(self,nRbtID,IONum):
        command = 'ReadEncoderDriverIOStatus,'
        command+=str(nRbtID)+','
        command += str(IONum) + ','
        command += ';'
        return self.sendAndRecv(command)
    
    def ReadConveyor(self,nRbtID):
        command = 'ReadConveyor,'
        command+=str(nRbtID)+','
        command += ';'
        return self.sendAndRecv(command)
        
    ###################################################################################################
    # sendCmdID
    # No output 
    def sendCmdID(self, CmdID,showName,ThreadID):
        command = 'SetCurCmdID,'
        command += str(CmdID) + ','
        command += str(showName) + ','
        command += str(ThreadID) + ',;'
        return self.sendAndRecv(command)

    def sendCallstackCmdID(self, CmdID,showName,ThreadID):
        command = 'SetCurCallstackCmdID,'
        command += str(CmdID) + ','
        command += str(showName) + ','
        command += str(ThreadID) + ',;'
        return self.sendAndRecv(command)

    def sendExitCallstackCmdID(self, CmdID,showName,ThreadID):
        command = 'SetCurExitCallstackCmdID,'
        command += str(CmdID) + ','
        command += str(showName) + ','
        command += str(ThreadID) + ',;'
        return self.sendAndRecv(command)
    ###################################################################################################

    def ___robust_send(self, cmd):
        total_sent = 0
        data = cmd.encode()
        while total_sent < len(data):
            try:
                sent = self.tcp.send(data[total_sent:])
                if 0 == sent:
                    raise RuntimeError("___robust_send The socket connection has been closed")
                total_sent += sent
            except socket.error as e:
                if e.errno == errno.EINTR:      # 系统中断
                    continue
                elif e.errno in (errno.EAGAIN, errno.EWOULDBLOCK):   # 资源不可用
                    time.sleep(0.01)            # 休眠10毫秒再次尝试
                    continue
                else:
                    # 其他错误, 直接抛出异常
                    raise
            except Exception:
                raise
        return total_sent

    def __robust_recv(self, buffer_size, end_marker=",;"):
        complete_data = ""              # 用于存储接收到的完整数据
        if not end_marker:
            end_marker = ",;"
        while True:
            try:
                data = self.tcp.recv(buffer_size)
                if not data:
                    raise RuntimeError('The socket connection has been closed')
                
                # 解码数据并拼接
                complete_data += data.decode("utf-8", "ignore")

                # 检查是否接收到以 end_marker 结尾的数据
                if complete_data.endswith(end_marker):
                    return complete_data
            except socket.error as e:
                # 系统中断
                if e.errno == errno.EINTR:
                    continue
                elif e.errno in (errno.EAGAIN, errno.EWOULDBLOCK):
                    time.sleep(0.01)
                    continue
                else:
                    raise
            except Exception as e:
                raise

    def sendAndRecv(self, cmd):
        self.IFlock.acquire()
        ret = ""
        retData = ""
        try:
            self.___robust_send(cmd)
            ret = self.__robust_recv(4096)
            retData = ret.split(',')
            if len(retData) < 3:
                self.sendScriptLog(2,'[script]sendAndRecv exit with ServerReturnError')
                if(self.scriptStatus!=3):
                    self.sendScriptAlarm(ErCode.REC_OSInternalError, "[script]sendAndRecv exit with ServerReturnError", [])
                    os._exit(0)
                else:
                    raise Exception("sendAndRecv exit with ServerReturnError")

            if retData[0] == "errorcmd":
                self.sendScriptLog(2,'[script]sendAndRecv exit with errorcmd')
                if retData[2] != "20018":
                    if(self.scriptStatus!=3):
                        self.sendScriptAlarm(ErCode.REC_CmdExecuteFailed, "", [str(retData[2]),cmd])
                        os._exit(0)
                    else:
                        raise Exception('function sendAndRecv command [' + cmd + '] failed, return: ' + str(retData[2]))
            if retData[1] == "Fail":
                self.sendScriptLog(3,'[script]sendAndRecv exit with Fail['+retData[2]+']'+'and the cmd is '+cmd)
                if retData[2] != "20018":
                    if(self.scriptStatus!=3):
                        self.sendScriptAlarm(ErCode.REC_CmdExecuteFailed, "", [str(retData[2]),cmd])
                        os._exit(0)
                    else:
                        raise Exception('function sendAndRecv command [' + cmd + '] failed, return: ' + str(retData[2]))
        except Exception as e:
            strError = str(traceback.format_exc())
            self.sendScriptAlarm(ErCode.REC_ScriptServiceExitedAbnormally, strError, [strError])
            os._exit(0)
        finally:
            # 修改完成，释放锁
            self.IFlock.release()
        return retData

    # Socket
    def socket_open(self,strIp,nPort,strName,connect_cnt=1):
        return self.socket.Socket_open(strIp,nPort,connect_cnt,strName)

    def socket_close(self,strName):
        return self.socket.Socket_close(strName)

    def socket_send_string(self,strInfo,strName):
        return self.socket.Socket_send_string(strInfo,strName)

    #strInfo:字符串，eg:"477270456E61626C652C302C3B",该字符串每两位将被解析为一个16进制数，发送出去
    def socket_send_hex(self,strInfo,strName):
        return self.socket.Socket_send_hex(strInfo,strName)

    def socket_send_int(self,nValue,strName):
        return self.socket.Socket_send_int(nValue,strName)

    #nNb:返回的数组长度，strName:socket Name，调用此接口将收到形如[b'73', b'74', b'61', b'72']的数据
    # 若recv到的内容比nNb规定的长，则截取nNb长度返回。若比nNb规定的短，则用None补足nNb长度返回,例如：[b'73', b'74', b'61', b'72', b'74', None, None]
    def socket_read_hex(self,nNb,strName,timeout=0):
        return self.socket.Socket_read_hex(nNb,strName,timeout)

    def socket_read_string(self,strName,timeout=0):
        return self.socket.Socket_read_string(strName,timeout)
    
    def str2base64(self,strSrc):
       code = (base64.b64encode(strSrc.encode('utf-8'))).decode('utf-8')    #base64编码
       return code
    
    # No output
    def sendVarValue(self, VarName, ValueStr):
        return
    
    def __robust_send_var_value(self, var_name, value, max_retries=3, retry_delay=0.01):
        for attempt in range(max_retries):
            try:
                return self.rpcClient.SendVarValue(var_name,value)
            except socket.error as e:
                if e.errno in (errno.EINTR, errno.EAGAIN, errno.EWOULDBLOCK):
                    time.sleep(retry_delay)
                    continue
                else:
                    raise       # 其他网络错误直接抛出
            except Exception as e:
                raise
        else:
            raise RuntimeError(f"__robust_send_var_value failed after {max_retries} times")

    # saveValue 将自定义变量显示在界面上
    # No output 
    def saveValue(self, VarName, Value):
        if isinstance(Value,list):
           # 使用英文逗号拼接列表元素
           ValueStr = "[" + ",".join(repr(item) for item in Value) + "]"
        else:
           ValueStr=str(Value)
        ValueStr = self.str2base64(str(ValueStr))
        self.rpclock.acquire()
        try:
            self.__robust_send_var_value(str(VarName),ValueStr)
        finally:
            # 修改完成，释放锁
            self.rpclock.release()

    def __robust_send_script_error(self, msg, max_retries=3, retry_delay=0.01):
        for attempt in range(max_retries):
            try:
                self.rpcClient.SendScriptError(msg,str(""))
            except socket.error as e:
                if e.errno in (errno.EINTR, errno.EAGAIN, errno.EWOULDBLOCK):
                    time.sleep(retry_delay)
                    continue
                else:
                    raise       # 其他网络错误直接抛出
            except Exception as e:
                raise
        else:
            raise RuntimeError(f"__robust_send_script_error failed after {max_retries} times")

    def sendScriptError(self,msg):
        self.rpclock.acquire()
        try:
            self.__robust_send_script_error(str(msg), 3, 0.01)
        finally:
            # 修改完成，释放锁
            self.rpclock.release()

    def __robust_send_script_alarm(self, alarm_info, max_retries=3, retry_delay=0.01):
        for attempt in range(max_retries):
            try:
                return self.rpcClient.SendScriptAlarm(alarm_info)
            except socket.error as e:
                if e.errno in (errno.EINTR, errno.EAGAIN, errno.EWOULDBLOCK):
                    time.sleep(retry_delay)
                    continue
                else:
                    raise       # 其他网络错误直接抛出
            except Exception as e:
                raise
        else:
            raise RuntimeError(f"__robust_send_script_alarm failed after {max_retries} times")

    def sendScriptAlarm(self, errCode, stackInfo='', localParaList=None):
        """
        构建 Script Alarm Info JSON 字符串, 并发送到 CPS

        :param errCode: 错误码 (unsigned int)
        :param stackInfo: 堆栈信息 (str)
        :param localParaList: 定位参数列表 (list of str)
        """
        rlt = False
        if localParaList is None:
            localParaList = []
    
        # 检查 errCode 是否为无符号整型
        if not (isinstance(errCode, int) and errCode >= 0):
            return rlt

        # 检查 stackInfo 是否为字符串
        if not isinstance(stackInfo, str):
            return rlt

        # 检查 localParaList 是否为字符串列表
        if not (isinstance(localParaList, list) and all(isinstance(item, str) for item in localParaList)):
            return rlt

        # 构建字典结构
        json_dict = {
            "ErrCode": errCode,
            "StackInfo": stackInfo,
            "LocalParamArr": localParaList
        }

        # 转换为 JSON 字符串
        alarmInfoStr = json.dumps(json_dict, indent = 4)
        self.rpclock.acquire()
        try:
            self.__robust_send_script_alarm(alarmInfoStr)
            rlt = True
        finally:
            # 修改完成，释放锁
            self.rpclock.release()
        return rlt

    def sendHRLog(self,nLevel,msg):
        hrMqtt.sendHRLog(nLevel,msg)

    def sendScriptLog(self,nLevel,msg):
        hrMqtt.sendScriptLog("HRLog",nLevel,msg)

    def __init__(self):
        self.rpcClient = xmlrpc.client.ServerProxy(self.xmlrpcAddr)
        self.tcp = socket.socket()
        self.socket = SocketMng()
        self.rpclock = threading.RLock()
        self.IFlock = threading.RLock()
        self.tcp.connect((self.clientIP, self.clientPort))
        return

    def CloseTCP(self):
        self.tcp.close()
        self.socket.Socket_CloseAll()
