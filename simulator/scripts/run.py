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
        all_files_paths = get_file_paths("config_files/{0}/{1}/{3}/".format(pu,w, core_number))
        for path in all_files_paths:
            f.write("./build/opt/zsim "+path + "\n")
    f.close()

def get_stat_file(pu, file):
    f = open(file, "a")
    a = get_file_paths("zsim_stats/{0}/{1}/".format(pu,core_number))
    for p in a:
        if ".out" in p[-4:]:
            f.write("echo \"{},{}\" >> result.txt \n".format(p.split("/")[-1].split(".")[0], pu))
            f.write("python scripts/get_stats_per_app.py "+p + ">> result.txt \n")
    f.close()

def get_stat_value(pu,file, storage_file, value):
    f = open(file, "a")
    a = get_file_paths("zsim_stats/{0}/{1}/".format(pu,core_number))
    for p in a:
        if "ramulator.stats" in p:
            f.write("echo \"{},{}\" >> {} \n".format(p.split("/")[-1].split(".")[0], pu, storage_file))
            f.write("grep '{0}' ".format(value)+ p + ">> {0} \n".format(storage_file))
    f.close()



def print_stat_file(file):
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

def print_value_file(file, value):
    f = open(file, 'r')
    Lines = f.readlines()
    # count = 0
    x = []
    y = []
    saw = False
    for l in Lines:
        if value in l:
            saw = True
            o = re.findall(r'\d+.\d+', l)
            if len(o) != 0 and isfloat(o[0]):
                y.append(float(o[0]))
            else:
                y.append(0)
        else:
            if (not saw) and len(x) != 0:
                x.pop()
            saw = False
            x.append(l.strip())
    f.close()
    return x, y

def print_value_file_whole_number(file, value):
    f = open(file, 'r')
    Lines = f.readlines()
    # count = 0
    x = []
    y = []
    saw = False
    for l in Lines:
        if value in l:
            saw = True
            o = re.findall(r'\d+', l)
            if len(o) != 0 and isfloat(o[0]):
                y.append(float(o[0]))
            else:
                y.append(0)
        else:
            if (not saw) and len(x) != 0:
                x.pop()
            saw = False
            x.append(l.strip())
    f.close()
    return x, y


core_number = 64


workloads = ["bwa","chai","darknet" ,"hardware-effects", "hashjoin" , "hpcc", "hpcg" , "ligra", "parboil", 
             "parsec", "phoenix",  "polybench", "rodinia", "stream", "splash-2"]


run_workloads("pim_accelerator","run_workloads_{}.sh".format(core_number))


# get stats 
get_stat_file("pim_accelerator",  "get_stat_{}.sh".format(core_number))
print_stat_file("result.txt")


# get specific measure
get_stat_value("pim_accelerator", "get_stat_latency_{}.sh".format(core_number), "result_latency.txt", "read_latency_avg")
print_value_file("result_latency.txt", "ramulator.read_latency_avg")


get_stat_value("pim_accelerator", "get_stat_latency_network_{}.sh".format(core_number), "result_latency_network.txt", "read_network_latency_avg")
print_value_file("result_latency_network.txt", "ramulator.read_network_latency_avg")

get_stat_value("pim_accelerator", "get_stat_latency_queue_{}.sh".format(core_number), "result_latency_queue.txt", "read_queue_latency_avg")
print_value_file_whole_number("result_latency_queue.txt", "ramulator.read_queue_latency_avg")


get_stat_value("pim_accelerator", "get_stat_latency_queue_o_{}.sh".format(core_number), "result_latency_queue_o.txt", "queueing_latency_avg")
print_value_file("result_latency_queue_o.txt", "ramulator.queueing_latency_avg")
