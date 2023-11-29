# We made a file to generate pim_prefetch separately due to it's difference in configuration
import sys
import os
import errno
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds, get_debug_flags, get_prefetcher_types, get_core_numbers

hops_thresholds = get_hops_thresholds()
count_thresholds = get_count_thresholds()
prefetcher_types = get_prefetcher_types()
debug_flags = get_debug_flags()
debug_tags = []
for debug_flag in debug_flags:
    debug_tags.append("DebugOn" if debug_flag == "true" else "DebugOff")

os.chdir("../workloads")
PIM_ROOT = os.getcwd() +"/"
os.chdir("../simulator") 
ROOT = os.getcwd() +"/"

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise

def create_pim_prefetch_configs(benchmark, application, function, command):
    version = "prefetch"
    number_of_cores = get_core_numbers()
    postfixes = ["netoh", "memtrace"]
    prefetch_policy_names = ["adaptive"]
    for hops_threshold in hops_thresholds:
        for count_threshold in count_thresholds:
            prefetch_policy_names.append(str(hops_threshold)+"h"+str(count_threshold)+"c")
    

    for cores in number_of_cores:
        for prefetcher_type in prefetcher_types:
            for postfix in postfixes:
                for prefetcher_policy_name in prefetch_policy_names:
                    for debug_tag in debug_tags:
                        mkdir_p(ROOT+"config_files/pim_HBM_"+version+"_"+postfix+"_"+prefetcher_type.lower()+"/"+prefetcher_policy_name+"_"+debug_tag.lower()+"/"+benchmark+"/"+str(cores)+"/")

    for cores in number_of_cores:
        for prefetcher_type in prefetcher_types:
            for postfix in postfixes:
                for prefetcher_policy_name in prefetch_policy_names:
                    for debug_tag in debug_tags:
                        mkdir_p(ROOT+"zsim_stats/pim_HBM_"+version+"_"+postfix+"_"+prefetcher_type.lower()+"/"+prefetcher_policy_name+"_"+debug_tag.lower()+"/"+str(cores)+"/")

    for cores in number_of_cores:
        for prefetcher_type in prefetcher_types:
            for postfix in postfixes:
                for prefetcher_policy_name in prefetch_policy_names:
                    for debug_tag in debug_tags:
                        with open(ROOT+"templates/template_pim_"+version+".cfg", "r") as ins:
                            config_file = open(ROOT+"config_files/pim_HBM_"+version+"_"+postfix+"_"+prefetcher_type.lower()+"/"+prefetcher_policy_name+"_"+debug_tag.lower()+"/"+benchmark+"/"+str(cores)+"/"+application+"_"+function+".cfg","w")
                            for line in ins:
                                line = line.replace("NUMBER_CORES", str(cores))
                                line = line.replace("STATS_PATH", "zsim_stats/pim_HBM_"+version+"_"+postfix+"_"+prefetcher_type.lower()+"/"+prefetcher_policy_name+"_"+debug_tag.lower()+"/"+str(cores)+"/"+benchmark+"_"+application+"_"+function)
                                line = line.replace("COMMAND_STRING", "\"" + command + "\";")
                                line = line.replace("THREADS", str(cores))
                                line = line.replace("PIM_ROOT",PIM_ROOT)
                                line = line.replace("RAMULATOR_CONFIG_FILENAME", "HBM-SubscriptionPF-"+prefetcher_type+"-"+debug_tag+"-"+prefetcher_policy_name+"-va"+str(cores)+"-config.cfg")
                                line = line.replace("RECORD_MEMORY_TRACE_SWITCH", "true" if postfix == "memtrace" else "false")
                                line = line.replace("gmMBytes = 8192;", "gmMBytes = "+str(128*cores)+";")
                                config_file.write(line)
                            config_file.close()
                        ins.close()


if(len(sys.argv) < 2):
    print("Usage python generate_config_files.py command_file")
    print("command_file: benckmark,applicationm,function,command")
    exit(1)

with open(sys.argv[1], "r") as command_file:
    for line in command_file:
        line = line.split(",")
        benchmark = line[0]
        application = line[1]
        function = line[2]
        command = line[3]
        print(line)
        command = command.replace('\n','')

        ### Fixed LLC Size 
        create_pim_prefetch_configs(benchmark, application, function, command)

