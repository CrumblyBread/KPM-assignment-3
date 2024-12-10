import os
import subprocess
import pandas as pd

num_ue_values = [10, 20, 50, 100] 
distance_values = [100, 500, 1000]  
interval_values = [100, 200, 500]  

output_csv = "simulation_results.csv"

results = []
columns = ["num_ue", "distance", "interval", "throughput", "latency", "pdr"]

def parse_metrics(log_file):
    """Extract metrics from the ns-3 simulation log or output file."""
    throughput, latency, pdr = 0, 0, 0  
    with open(log_file, "r") as file:
        for line in file:
            if "Throughput" in line:
                throughput = float(line.split(":")[1].split(" ")[1].strip())  
            elif "Average Latency" in line:
                latency = float(line.split(":")[1].split(" ")[1].strip()) 
            elif "Packet Delivery Ratio" in line:
                pdr = float(line.split(":")[1].split(" ")[1].strip()) 
    return throughput, latency, pdr

def run_simulation(num_ue, distance, interval):
    """Run the ns-3 simulation with the specified parameters."""
    command = [
        "./waf",
        "--run",
        f"lte-project {num_ue} {distance} {interval}"
    ]
    
    log_file = f"logs/simulation_{num_ue}_{distance}_{interval}.log"
    with open(log_file, "w") as logfile:
        subprocess.run(command, stdout=logfile, stderr=subprocess.STDOUT)
    
    return parse_metrics(log_file)

os.makedirs("logs", exist_ok=True)

for num_ue in num_ue_values:
    for distance in distance_values:
        for interval in interval_values:
            print(f"Running simulation: numUe={num_ue}, distance={distance}, interval={interval}")
            throughput, latency, pdr = run_simulation(num_ue, distance, interval)
            results.append([num_ue, distance, interval, throughput, latency, pdr])

df = pd.DataFrame(results, columns=columns)
df.to_csv(output_csv, index=False)

print(f"Simulation results saved to {output_csv}")

