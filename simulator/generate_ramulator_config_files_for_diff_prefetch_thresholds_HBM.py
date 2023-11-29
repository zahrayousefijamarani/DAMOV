import sys
import os
import errno
from batch_prefetcher_generator import get_hops_thresholds, get_count_thresholds, get_debug_flags, get_prefetcher_types, get_core_numbers, get_core_to_mem_config_hbm

hops_thresholds = get_hops_thresholds()
count_thresholds = get_count_thresholds()
prefetcher_types = get_prefetcher_types()
core_numbers = get_core_numbers()
debug_flags = get_debug_flags()
ramulator_config_dir = os.path.join(os.getcwd(), "ramulator-configs")
template_dir = os.path.join(ramulator_config_dir, "HBM-prefetcher-config-template.cfg")

def mkdir_p(directory):
    try:
        os.makedirs(directory)
    except OSError as exc:  # Python >2.5
        if exc.errno == errno.EEXIST and os.path.isdir(directory):
            pass
        else:
            raise

mkdir_p(os.path.join(ramulator_config_dir, "prefetcher_HBM"))
#mkdir_p(os.path.join(ramulator_config_dir, "pim_HBM"))

core_config = get_core_to_mem_config_hbm()

def generate_config_file(prefetcher_type, debug_flag, count_threshold, hops_threshold, adapative_flag, policy_name, core_number):
    debug_tag = "DebugOn" if debug_flag == "true" else "DebugOff"
    output_dir = os.path.join(ramulator_config_dir, "prefetcher_HBM", "HBM-SubscriptionPF-"+prefetcher_type+"-"+debug_tag+"-"+policy_name+"-va"+str(core_number)+"-config.cfg")
    with open(template_dir, "r") as ins:
        config_file = open(output_dir,"w")
        for line in ins:
            line = line.replace("PREFETCHER_TYPE", prefetcher_type)
            line = line.replace("DEBUG_FLAG", debug_flag)
            line = line.replace("COUNT_THRESHOLD_NUMBER", count_threshold)
            line = line.replace("HOPS_THRESHOLD_NUMBER", hops_threshold)
            line = line.replace("ADAPTIVE_THRESHOLD_FLAG", adapative_flag)
            if core_number != 1:
                line = line.replace("HBM_4GB", core_config[core_number])
            config_file.write(line)
        config_file.close()
    ins.close()

for hops in hops_thresholds:
    for count in count_thresholds:
        for debug_flag in debug_flags:
            for prefetcher_type in prefetcher_types:
                for core_number in core_numbers:
                    generate_config_file(prefetcher_type, debug_flag, str(count), str(hops), "false", str(hops)+"h"+str(count)+"c", core_number)

for debug_flag in debug_flags:
    for prefetcher_type in prefetcher_types:
        for core_number in core_numbers:
            generate_config_file(prefetcher_type, debug_flag, "0", "1", "true", "adaptive", core_number)

# no_pf_template_dir = os.path.join(ramulator_config_dir, "HBM-config.cfg")
# for debug_flag in debug_flags:
#     for core_number in core_numbers:
#         debug_tag = "DebugOn" if debug_flag == "true" else "DebugOff"
#         output_dir = os.path.join(ramulator_config_dir, "pim", "HMC-NoPF-"+debug_tag+"-va"+str(core_number)+"-config.cfg")
#         with open(no_pf_template_dir, "r") as ins:
#             config_file = open(output_dir,"w")
#             for line in ins:
#                 if core_number != 32:
#                     line = line.replace("HMC_4GB", core_config[core_number])
#                 config_file.write(line)
#             config_file.close()
#         ins.close()
