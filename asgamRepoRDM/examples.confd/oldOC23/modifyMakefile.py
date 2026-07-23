#pass as argument the complete new command
#example: sudo python modifyMakefile.py newConfigFIle.xml
import sys
import socket    
with open("Makefile", "r") as in_file:
    buf = in_file.readlines()
with open("Makefile", "w") as out_file:
    for line in buf:
        if line == "\t@echo \'Initialize database\'\n":
        	i=buf.index(line)
        	lineToAdd="cp "+sys.argv[1]+" confd-cdb/"
        	line = line +"\t"+ lineToAdd+"\n"
        	buf.remove(buf[i+1])
        out_file.write(line)


hostname = socket.gethostname()    
IPAddr = socket.gethostbyname(hostname)    
txt="#define GRPC_SERVER_IP  ((const unsigned char *)\""+IPAddr+"\")"

with open("parameters.h", "r") as i_file:
    buf2 = i_file.readlines()
with open("parameters.h", "w") as o_file:
    for line in buf2:
        o_file.write(line)
    o_file.write(txt)
