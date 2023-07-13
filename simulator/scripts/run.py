import os
import subprocess
import matplotlib.pyplot as plt
import re

def get_file_paths(base_dir):
    if not os.path.isdir(base_dir):
        return base_dir
    folders = os.listdir(base_dir)
    files_list = []
    for folder in folders:
        result = get_file_paths(os.path.join(base_dir, folder))
        if isinstance(result, list):
            files_list += result
        else:
            files_list.append(result)
    return files_list

def get_special_paths(files_paths, keys):
    final_files_paths = []
    found = False
    for path in files_paths:
        key_list = path.split("/")
        for k in keys:
            if not(k in key_list):
                break
        else:
            final_files_paths.append(path)
        
    return final_files_paths

def run_workloads(pu, file):
    f = open(file, "a")
    for w in workloads:
        all_files_paths = get_file_paths("config_files/{0}/{1}/16/".format(pu,w))
        for path in all_files_paths:
            f.write("./build/opt/zsim "+path + "| grep -q \"Max total (aggregate) instructions reached\" \n")
    f.close()

def get_stat_file(pu, file):
    f = open(file, "a")
    a = get_file_paths("zsim_stats/{0}/16/".format(pu))
    for p in a:
        if ".out" in p[-4:]:
            f.write("echo \"{},{}\" >> result.txt \n".format(p.split("/")[-1].split(".")[0], pu))
            f.write("python scripts/get_stats_per_app.py "+p + ">> result.txt \n")
    f.close()

def get_stat_value(pu,file):
    f = open(file, "a")
    a = get_file_paths("zsim_stats/{0}/16/".format(pu))
    for p in a:
        if "ramulator.stats" in p:
            f.write("echo \"{},{}\" >> result_latency.txt \n".format(p.split("/")[-1].split(".")[0], pu))
            f.write("grep 'read_latency_avg' "+ p + ">> result_latency.txt \n")
    f.close()

def draw_stat_file(file):
    f = open(file, 'r')
    Lines = f.readlines()
    count = 0
    x = []
    y = [[],[],[],[],[],[],[],[]]
    
    for line in Lines:
        if count % 12 == 0:
            y_index = 0
            x.append(line.strip())
        if ":" in line:
            y[y_index].append(float(line.strip().split(':')[1]))
            y_index += 1
        count += 1
    f.close()
    # plt.bar(x, y[0])
    # plt.show()   
    
    return x,y

def isfloat(num):
    try:
        float(num)
        return True
    except ValueError:
        return False

def read_value_result_file(file):
    f = open(file, 'r')
    Lines = f.readlines()
    count = 0
    x = []
    y = []
    for l in Lines:
        if count % 2 == 0:
            x.append(l.strip())
        else:
            if "ramulator.read_latency_avg" in l:
                o = re.findall(r'\d+.\d+', l)
                if len(o) != 0 and isfloat(o[0]):
                    y.append(float(o[0]))
                else:
                    y.append(0)
            else:
                count -= 1            
        count += 1
    f.close()
    return x, y



workloads = ["bwa","chai","darknet" ,"hardware-effects", "hashjoin" , "hpcc", "hpcg" , "ligra", "parboil", 
             "parsec", "phoenix",  "polybench", "rodinia", "stream", "splash-2"]
# run_workloads("host_accelerator/no_prefetch", "run_workloads.sh")
# run_workloads("host_accelerator/prefetch","run_workloads.sh")
run_workloads("pim_accelerator","run_workloads.sh")


# get_stat_file("host_accelerator/no_prefetch", "get_stat.sh")
# get_stat_file("host_accelerator/prefetch",  "get_stat.sh")
get_stat_file("pim_accelerator",  "get_stat.sh")
get_stat_value("pim_accelerator", "get_stat_latency.sh")

draw_stat_file("result.txt")
read_value_result_file("result_latency.txt")