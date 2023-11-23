import sys
import argparse
import pandas as pd

# states of the mapper state machine
CRUISING = 0; SPEEDING = 1; SLOWING = -1;

# dictionary to store limits and default values
inputs = { 
    "current_speed": {"min": 0.0, "max": 100.0}, # mph
    "x_throttle": {"min": 0.0, "max": 1.0, "noise": 0.01},
    "x_brake": {"min": 0.0, "max": 1.0, "noise": 0.01},
    "theta_desired_speed": {"min": 70.0, "max": 100.0, "default": 100.0}, # x_throttle = 1 -> 100mph
    "limit_speedup": {"min": 0.25, "max": 1.0, "default": 0.5}, # speed up with this value of acc
    "limit_slowdown": {"min": -1.0, "max": 0.0, "default": -0.5}, # regenerative braking acc limit
    "theta_slowdown": {"min": -1.0/5, "max": 0.0, "default": -0.5/25}, # slope of regenrative braking
    "threshold_speedup": {"min": -2.0, "max": -0.25, "default": -1.0}, # speed up only if (actual-desired) smaller than this value
    "threshold_slowdown": {"min": 0.25, "max": 2.0, "default": 1.0} # slowdown only if (actual-desired) greater than this value 
}

def default_if_out_of_range (value, spec):
    """
    returns default value in the spec if value is not in min/max range
    """
    if value >= spec["min"] and value <= spec["max"]: 
        return value
    else: 
        return spec["default"]
# end of default_if_out_of_range

def clip_if_out_of_range (value, spec):
    """
    clips the value if not in min/max range
    """
    return min (
        max (value, spec["min"]), 
        spec["max"]
    )
# end of clip_if_out_of_range

def check_input (
    current_speed, 
    x_throttle,
    x_brake,
    theta_desired_speed,
    limit_speedup,
    limit_slowdown,
    theta_slowdown,
    threshold_speedup,
    threshold_slowdown
    ):
    """
    Sets inputs to default value or clips them if out of range
    """

    # no checks or modulation or current_speed

    # clip throttle and brake to min/max as there is no default value
    x_throttle = clip_if_out_of_range (x_throttle, inputs["x_throttle"])
    x_brake = clip_if_out_of_range (x_brake, inputs["x_brake"])
    
    # set rest of the parameters to default if illegate
    theta_desired_speed = default_if_out_of_range (theta_desired_speed, inputs["theta_desired_speed"])
    limit_speedup = default_if_out_of_range (limit_speedup, inputs["limit_speedup"])
    limit_slowdown = default_if_out_of_range (limit_slowdown, inputs["limit_slowdown"])
    theta_slowdown = default_if_out_of_range (theta_slowdown, inputs["theta_slowdown"])
    threshold_speedup = default_if_out_of_range (threshold_speedup, inputs["threshold_speedup"])
    threshold_slowdown = default_if_out_of_range (threshold_slowdown, inputs["threshold_slowdown"])

    return
# end of check_inputs

class PedalMapper: 
    def __init__ (self):
        self.state = CRUISING
    
    def calc_acc(
        self,
        current_speed,
        x_throttle,
        x_brake,
        theta_desired_speed = inputs["theta_desired_speed"]["default"],
        limit_speedup = inputs["limit_speedup"]["default"],
        limit_slowdown = inputs["limit_slowdown"]["default"],
        theta_slowdown =  inputs["theta_slowdown"]["default"],
        threshold_speedup = inputs["threshold_speedup"]["default"],
        threshold_slowdown = inputs["threshold_slowdown"]["default"]
        ):
        """
        returns acc values between -1.0 and +1.0
        Args: 
            documented in inputs dictionary
        Returns:
            acc (float) [-1.0..1.0]
        """
        check_input (
            current_speed, 
            x_throttle,
            x_brake,
            theta_desired_speed,
            limit_speedup,
            limit_slowdown,
            theta_slowdown,
            threshold_speedup,
            threshold_slowdown
        )

        desired_speed = x_throttle * theta_desired_speed
        current_minus_desired = current_speed - desired_speed

        # if break is pressed then ignore throttle
        if x_brake > inputs["x_brake"]["noise"]:
            self.state = SLOWING
            return x_brake * (-1.0)
        # else if desired speed is greater than current by noise amount
        elif current_minus_desired < threshold_speedup: 
            self.state = SPEEDING
            return limit_speedup
        # else if desired speed is smaller than current by noise amount
        elif current_minus_desired > threshold_slowdown: 
            self.state = SLOWING
            return max (limit_slowdown, theta_slowdown * current_minus_desired)
        else: # delta between desired and current is in noise level
            if self.state == SPEEDING:
                if current_minus_desired >= 0.0: # reached desired or slighty higher speed
                    self.state = CRUISING
                    return 0.0
                else:
                    return limit_speedup
            elif self.state == SLOWING:
                if current_minus_desired <= 0.0: # reached desired or slightly slower speed
                    self.state = CRUISING
                    return 0.0
                else:
                    return max (limit_slowdown, theta_slowdown * current_minus_desired)
            else: # self.state == CRUISING
                self.state = CRUISING
                return 0.0
        # end of else: # delta between desired and current is in noise level
    # end of calc_acc

    def calc_from_row(self, row):
        return self.calc_acc (
            row['current_speed'], 
            row['x_throttle'],
            row['x_brake'],
            row['theta_desired_speed'],
            row['limit_speedup'],
            row['limit_slowdown'],
            row['theta_slowdown'],
            row['threshold_speedup'],
            row['threshold_slowdown']
        )
# end of class PedalMapper

def write_csv (fout, index, inputs, state, acc):
    if index == 0: # print header
        for key in inputs.keys(): 
            fout.write (f"{key},")
        fout.write (f"desired_speed, state, acc,")
        fout.write ("\n")
    
    for key in inputs.keys(): 
        fout.write (f"{inputs[key]},")
    fout.write (f"{inputs['x_throttle'] * inputs['theta_desired_speed']}, {state},{acc},")
    fout.write ("\n")
# end of write_csv

if __name__ == "__main__": 
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input_file",
        default="C:/Users/gopal/rn1/remnav/acc/test_in.csv",
        help="test input file")
    parser.add_argument(
        "--output_file",
        default="C:/Users/gopal/rn1/remnav/acc/test_out.csv",
        help="output file")
    args = parser.parse_args()
    
    fout = open (args.output_file, "w")
    ti = pd.read_csv (args.input_file)
    pm = PedalMapper ()

    for i, row in ti.iterrows ():
        acc = pm.calc_from_row(row)
        write_csv (fout, i, row, pm.state, acc)

    fout.close ()
# end of main
