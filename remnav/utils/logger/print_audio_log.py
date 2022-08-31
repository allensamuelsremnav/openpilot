import sys
import capnp
import json

sys.path.append("../logger")
import log_capnp

print(sys.argv[1])
fname = sys.argv[1]

f = open(fname, "rb")
dat = f.read()
msgs = log_capnp.Message.read_multiple_bytes(dat)

for msg in msgs:
    print(json.dumps(msg.to_dict()))

f.close()