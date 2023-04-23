# Generate language-appropriate files for port assignments.
import collections
import sys

Assignment = collections.namedtuple('PortAssignment',
                                    ['name', 'number', 'comment'])


# These rules should allow generating names that conform to
# language style guidelines.
# 1. Tokens inside a name should be separated by _.
# 2. Token inside a name should begin with a capital letter.
# 3. Initialisms such as UDP or TCP should be in all caps.

all_assignments = [
("operator station, external",
 [Assignment(
     "Operator_Gpsd_Listener", 6001,
     "Gpsd messages from vehicle gpsdrt to the operator."),
  Assignment(
      "Operator_Trajectory_Listener", 6002,
      "Trajectory-applied messages from trajectory execution to operator."),
  ]),

("operator station, localhost",
 [Assignment(
     "Operator_Gpsd_Display", 7000,
     "Gpsd messages from the gpsd listener to display."),
  Assignment(
      "Operator_Gpsd_Trajectory", 7001,
      "Gpsd messages from the gpsd listener to trajectory planner."),
  Assignment(
      "Operator_Trajectory_Request", 7002,
      "Trajectory messages from trajectory planner to trajectory listener for forwarding."),
  Assignment(
      "Operator_Trajectory_Application", 7003,
      "Trajectory-applied messages from trajectory listener to display."),
  ]),

    
("vehicle, external",
 [
 ]),

("vehicle, localhost",
 [Assignment(
     "Vehicle_Trajectory_Request", 7000,
     "Trajectory requests from trajectory dialer to "
     "trajectory execution"),
  
  Assignment(
      "Vehicle_Trajectory_Application", 7001,
      "trajectory-applied messages from trajectory execution "
      "to trajectory dialer for forwarding."),
  ]),
]

def main():
    with open("port.go", "w") as f:
        print("package net\n", file=f)
        print("// Generated by %s" % (sys.argv[0],), file=f)
        print("", file=f)
        for (tag, assignments) in all_assignments:
            print("//\n// %s\n//" % (tag,), file=f)
            for a in assignments:
                print("// %s" % (a.comment,), file=f)
                
                print("const %s = %d" % (a.name.replace("_", ""),
                                         a.number), file=f)
                print("", file=f)

    with open("port.py", "w") as f:
        print("# Generated by %s" % (sys.argv[0],), file=f)
        print("", file=f)
        for (tag, assignments) in all_assignments:
            print("\n# %s\n#" % (tag,), file=f)
            for a in assignments:
                print("# %s" % (a.comment,), file=f)
                print("%s = %d" % (a.name.upper(), a.number), file=f)
                print("", file=f)

    with open("port.h", "w") as f:
        print("#ifndef REMNAV_NET_PORTS_H_", file=f)
        print("#define  REMNAV_NET_PORTS_H_", file=f)
        print("// Generated by %s." % (sys.argv[0],), file=f)
        print("", file=f)
        for (tag, assignments) in all_assignments:
            print("\n// %s\n//" % (tag,), file=f)
            for a in assignments:
                print("// %s" % (a.comment,), file=f)
                print("const int32_t %s = %d;" % (a.name.upper(),
                                                  a.number), file=f)
                print("", file=f)
        print("#endif  // REMNAV_NET_PORTS_H_", file=f)

if __name__ == "__main__":
    main()
    
