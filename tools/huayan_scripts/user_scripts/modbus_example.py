str_1 = cc_client.getModbus('Modbus_3', 'HR000')
cc_client.sendVarValue('str_1', str_1)

cc_client.setModbus('Modbus_3', 'HR001', 255)

values = cc_client.getModbusDatas('Modbus_3', 'HR000', 5)
HR000 = int(values[0])
HR001 = int(values[1])
cc_client.sendVarValue('HR000', HR000)
cc_client.sendVarValue('HR001', HR001)

coils = [1, 0, 0, 1]
cc_client.setModbusDatas('Modbus_3', 'M_3_1', coils)