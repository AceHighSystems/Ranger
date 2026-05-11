




data = ace_read(NODE_ID, RAMGER_PARAMETER)  # functional up to 10kHz?

data = ace_write(NODE_ID, RANGER_PARAMETER, data_to_write) 

data = ace_scan_nodes() # Returns all nodes available on the CAN bus scans from 1 to 127

ace_program(NODE_ID, path_to_bin_file)


data = 8-byte array with ace CAN frame