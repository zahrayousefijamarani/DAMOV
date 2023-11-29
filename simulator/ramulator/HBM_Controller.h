#ifndef __HBM_CONTROLLER_H
#define __HBM_CONTROLLER_H
#include <cassert>
#include <cstdio>
#include <deque>
#include <fstream>
#include <list>
#include <string>
#include <vector>
#include <unordered_map>
#include "Controller.h"
#include "Scheduler.h"
#include "HBM.h"
using namespace std;
namespace ramulator
{
template <>
class Controller<HBM>
{
public:
    // For counting bandwidth
    ScalarStat* read_transaction_bytes;
    ScalarStat* write_transaction_bytes;
    ScalarStat* row_hits;
    ScalarStat* row_misses;
    ScalarStat* row_conflicts;
    VectorStat* read_row_hits;
    VectorStat* read_row_misses;
    VectorStat* read_row_conflicts;
    VectorStat* write_row_hits;
    VectorStat* write_row_misses;
    VectorStat* write_row_conflicts;
    ScalarStat* read_latency_sum;
    ScalarStat* read_queue_latency_sum;
    ScalarStat* queueing_latency_sum;
    ScalarStat* req_queue_length_sum;
    ScalarStat* read_req_queue_length_sum;
    ScalarStat* write_req_queue_length_sum;
    // DRAM power estimation statistics
    ScalarStat act_energy;
    ScalarStat pre_energy;
    ScalarStat read_energy;
    ScalarStat write_energy;
    ScalarStat act_stdby_energy;
    ScalarStat pre_stdby_energy;
    ScalarStat idle_energy_act;
    ScalarStat idle_energy_pre;
    ScalarStat f_act_pd_energy;
    ScalarStat f_pre_pd_energy;
    ScalarStat s_act_pd_energy;
    ScalarStat s_pre_pd_energy;
    ScalarStat sref_energy;
    ScalarStat sref_ref_energy;
    ScalarStat sref_ref_act_energy;
    ScalarStat sref_ref_pre_energy;
    ScalarStat spup_energy;
    ScalarStat spup_ref_energy;
    ScalarStat spup_ref_act_energy;
    ScalarStat spup_ref_pre_energy;
    ScalarStat pup_act_energy;
    ScalarStat pup_pre_energy;
    ScalarStat IO_power;
    ScalarStat WR_ODT_power;
    ScalarStat TermRD_power;
    ScalarStat TermWR_power;
    ScalarStat read_io_energy;
    ScalarStat write_term_energy;
    ScalarStat read_oterm_energy;
    ScalarStat write_oterm_energy;
    ScalarStat io_term_energy;
    ScalarStat ref_energy;
    ScalarStat total_energy;
    ScalarStat average_power;
    // drampower counter
    // Number of activate commands
    ScalarStat numberofacts_s;
    // Number of precharge commands
    ScalarStat numberofpres_s;
    // Number of reads commands
    ScalarStat numberofreads_s;
    // Number of writes commands
    ScalarStat numberofwrites_s;
    // Number of refresh commands
    ScalarStat numberofrefs_s;
    // Number of precharge cycles
    ScalarStat precycles_s;
    // Number of active cycles
    ScalarStat actcycles_s;
    // Number of Idle cycles in the active state
    ScalarStat idlecycles_act_s;
    // Number of Idle cycles in the precharge state
    ScalarStat idlecycles_pre_s;
    // Number of fast-exit activate power-downs
    ScalarStat f_act_pdns_s;
    // Number of slow-exit activate power-downs
    ScalarStat s_act_pdns_s;
    // Number of fast-exit precharged power-downs
    ScalarStat f_pre_pdns_s;
    // Number of slow-exit activate power-downs
    ScalarStat s_pre_pdns_s;
    // Number of self-refresh commands
    ScalarStat numberofsrefs_s;
    // Number of clock cycles in fast-exit activate power-down mode
    ScalarStat f_act_pdcycles_s;
    // Number of clock cycles in slow-exit activate power-down mode
    ScalarStat s_act_pdcycles_s;
    // Number of clock cycles in fast-exit precharged power-down mode
    ScalarStat f_pre_pdcycles_s;
    // Number of clock cycles in slow-exit precharged power-down mode
    ScalarStat s_pre_pdcycles_s;
    // Number of clock cycles in self-refresh mode
    ScalarStat sref_cycles_s;
    // Number of clock cycles in activate power-up mode
    ScalarStat pup_act_cycles_s;
    // Number of clock cycles in precharged power-up mode
    ScalarStat pup_pre_cycles_s;
    // Number of clock cycles in self-refresh power-up mode
    ScalarStat spup_cycles_s;
    // Number of active auto-refresh cycles in self-refresh mode
    ScalarStat sref_ref_act_cycles_s;
    // Number of precharged auto-refresh cycles in self-refresh mode
    ScalarStat sref_ref_pre_cycles_s;
    // Number of active auto-refresh cycles during self-refresh exit
    ScalarStat spup_ref_act_cycles_s;
    // Number of precharged auto-refresh cycles during self-refresh exit
    ScalarStat spup_ref_pre_cycles_s;
    //libDRAMPower* drampower;
    long update_counter = 0;
public:
    /* Member Variables */
    long clk = 0;
    DRAM<HBM>* channel;
    Scheduler<HBM>* scheduler;  // determines the highest priority request whose commands will be issued
    RowPolicy<HBM>* rowpolicy;  // determines the row-policy (e.g., closed-row vs. open-row)
    RowTable<HBM>* rowtable;  // tracks metadata about rows (e.g., which are open and for how long)
    Refresh<HBM>* refresh;

    long long total_hbm_latency = 0;
    long long total_latency = 0;
    long long total_transfer_latency = 0;
    long long total_in_memory_latency = 0;
    long long total_cycle_waiting_not_ready_request = 0;
    long long total_process_latency = 0;
    long long total_incoming_queuing_latency = 0;
    long long total_outgoing_queuing_latency = 0;
    long long total_bursts = 0;
    long long stalled_cycles = 0;
    function<void(const Request&)> update_parent_with_latency;

    struct Queue {
        list<Request> q;
        list<Request> arrivel_q;
        unsigned int max = 32; // TODO queue qize
        unsigned int size() {return q.size();}
        void update(long clk){
          list<Request> tmp;
          for (auto& i : arrivel_q) {
            // assert(i.hops <= MAX_HOP);
            if(i.hops == 0){
              i.arrive_q_hbm = clk;
              q.push_back(i);
              continue;
            }
            i.hops -= 1;
            tmp.push_back(i);
          }
          arrivel_q = tmp;
        }
        void arrive(Request& req, long clk) {
            if(req.hops == 0) {
                req.arrive_q_hbm = clk;
                q.push_back(req);
            } else {
                arrivel_q.push_back(req);
            }
        }
    };


    Queue readq;  // queue for read requests 
    Queue writeq;  // queue for write requests
    Queue otherq;  // queue for all "other" requests (e.g., refresh)


    deque<Request> pending;  // read requests that are about to receive data from DRAM
    deque<Request> pending_write;  // read requests that are about to receive data from DRAM
    bool write_mode = false;  // whether write requests should be prioritized over reads
    //long refreshed = 0;  // last time refresh requests were generated
    /* Command trace for DRAMPower 3.1 */
    string cmd_trace_prefix = "cmd-trace-";
    vector<ofstream> cmd_trace_files;
    bool record_cmd_trace = false;
    /* Commands to stdout */
    bool print_cmd_trace = false;
    bool with_drampower = false;
    // ideal DRAM
    bool no_DRAM_latency = false;
    bool unlimit_bandwidth = false;
    bool pim_mode_enabled = false;
    void fake_ideal_DRAM(const Config& configs) {
        if (configs["no_DRAM_latency"] == "true") {
        no_DRAM_latency = true;
        scheduler->type = Scheduler<HBM>::Type::FRFCFS;
        }
        if (configs["unlimit_bandwidth"] == "true") {
        unlimit_bandwidth = true;
        printf("nBL: %d\n", channel->spec->speed_entry.nBL);
        channel->spec->speed_entry.nBL = 0;
        channel->spec->read_latency = channel->spec->speed_entry.nCL;
        channel->spec->speed_entry.nCCDS = 1;
        channel->spec->speed_entry.nCCDL = 1;
        }
    }
    /* Constructor */
    Controller(const Config& configs, DRAM<HBM>* channel) :
        channel(channel),
        scheduler(new Scheduler<HBM>(this)),
        rowpolicy(new RowPolicy<HBM>(this)),
        rowtable(new RowTable<HBM>(this)),
        refresh(new Refresh<HBM>(this)),
        cmd_trace_files(channel->children.size())
    {
        record_cmd_trace = configs.record_cmd_trace();
        print_cmd_trace = configs.print_cmd_trace();
        if (record_cmd_trace){
            if (configs["cmd_trace_prefix"] != "") {
              cmd_trace_prefix = configs["cmd_trace_prefix"];
            }
            string prefix = cmd_trace_prefix + "chan-" + to_string(channel->id) + "-rank-";
            string suffix = ".cmdtrace";
            for (unsigned int i = 0; i < channel->children.size(); i++)
                cmd_trace_files[i].open(prefix + to_string(i) + suffix);
        }
        pim_mode_enabled = configs.pim_mode_enabled();
        with_drampower = false;
        fake_ideal_DRAM(configs);
        if (with_drampower) {
          act_energy
              .name("act_energy_" + to_string(channel->id))
              .desc("act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pre_energy
              .name("pre_energy_" + to_string(channel->id))
              .desc("pre_energy_" + to_string(channel->id))
              .precision(6)
              ;
          read_energy
              .name("read_energy_" + to_string(channel->id))
              .desc("read_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_energy
              .name("write_energy_" + to_string(channel->id))
              .desc("write_energy_" + to_string(channel->id))
              .precision(6)
              ;
          act_stdby_energy
              .name("act_stdby_energy_" + to_string(channel->id))
              .desc("act_stdby_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pre_stdby_energy
              .name("pre_stdby_energy_" + to_string(channel->id))
              .desc("pre_stdby_energy_" + to_string(channel->id))
              .precision(6)
              ;
          idle_energy_act
              .name("idle_energy_act_" + to_string(channel->id))
              .desc("idle_energy_act_" + to_string(channel->id))
              .precision(6)
              ;
          idle_energy_pre
              .name("idle_energy_pre_" + to_string(channel->id))
              .desc("idle_energy_pre_" + to_string(channel->id))
              .precision(6)
              ;
          f_act_pd_energy
              .name("f_act_pd_energy_" + to_string(channel->id))
              .desc("f_act_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          f_pre_pd_energy
              .name("f_pre_pd_energy_" + to_string(channel->id))
              .desc("f_pre_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          s_act_pd_energy
              .name("s_act_pd_energy_" + to_string(channel->id))
              .desc("s_act_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          s_pre_pd_energy
              .name("s_pre_pd_energy_" + to_string(channel->id))
              .desc("s_pre_pd_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_energy
              .name("sref_energy_" + to_string(channel->id))
              .desc("sref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_energy
              .name("sref_ref_energy_" + to_string(channel->id))
              .desc("sref_ref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_act_energy
              .name("sref_ref_act_energy_" + to_string(channel->id))
              .desc("sref_ref_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          sref_ref_pre_energy
              .name("sref_ref_pre_energy_" + to_string(channel->id))
              .desc("sref_ref_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_energy
              .name("spup_energy_" + to_string(channel->id))
              .desc("spup_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_energy
              .name("spup_ref_energy_" + to_string(channel->id))
              .desc("spup_ref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_act_energy
              .name("spup_ref_act_energy_" + to_string(channel->id))
              .desc("spup_ref_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          spup_ref_pre_energy
              .name("spup_ref_pre_energy_" + to_string(channel->id))
              .desc("spup_ref_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pup_act_energy
              .name("pup_act_energy_" + to_string(channel->id))
              .desc("pup_act_energy_" + to_string(channel->id))
              .precision(6)
              ;
          pup_pre_energy
              .name("pup_pre_energy_" + to_string(channel->id))
              .desc("pup_pre_energy_" + to_string(channel->id))
              .precision(6)
              ;
          IO_power
              .name("IO_power_" + to_string(channel->id))
              .desc("IO_power_" + to_string(channel->id))
              .precision(6)
              ;
          WR_ODT_power
              .name("WR_ODT_power_" + to_string(channel->id))
              .desc("WR_ODT_power_" + to_string(channel->id))
              .precision(6)
              ;
          TermRD_power
              .name("TermRD_power_" + to_string(channel->id))
              .desc("TermRD_power_" + to_string(channel->id))
              .precision(6)
              ;
          TermWR_power
              .name("TermWR_power_" + to_string(channel->id))
              .desc("TermWR_power_" + to_string(channel->id))
              .precision(6)
              ;
          read_io_energy
              .name("read_io_energy_" + to_string(channel->id))
              .desc("read_io_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_term_energy
              .name("write_term_energy_" + to_string(channel->id))
              .desc("write_term_energy_" + to_string(channel->id))
              .precision(6)
              ;
          read_oterm_energy
              .name("read_oterm_energy_" + to_string(channel->id))
              .desc("read_oterm_energy_" + to_string(channel->id))
              .precision(6)
              ;
          write_oterm_energy
              .name("write_oterm_energy_" + to_string(channel->id))
              .desc("write_oterm_energy_" + to_string(channel->id))
              .precision(6)
              ;
          io_term_energy
              .name("io_term_energy_" + to_string(channel->id))
              .desc("io_term_energy_" + to_string(channel->id))
              .precision(6)
              ;
          ref_energy
              .name("ref_energy_" + to_string(channel->id))
              .desc("ref_energy_" + to_string(channel->id))
              .precision(6)
              ;
          total_energy
              .name("total_energy_" + to_string(channel->id))
              .desc("total_energy_" + to_string(channel->id))
              .precision(6)
              ;
          average_power
              .name("average_power_" + to_string(channel->id))
              .desc("average_power_" + to_string(channel->id))
              .precision(6)
              ;
          numberofacts_s
              .name("numberofacts_s_" + to_string(channel->id))
              .desc("Number of activate commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofpres_s
              .name("numberofpres_s_" + to_string(channel->id))
              .desc("Number of precharge commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofreads_s
              .name("numberofreads_s_" + to_string(channel->id))
              .desc("Number of reads commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofwrites_s
              .name("numberofwrites_s_" + to_string(channel->id))
              .desc("Number of writes commands_" + to_string(channel->id))
              .precision(0)
              ;
          numberofrefs_s
              .name("numberofrefs_s_" + to_string(channel->id))
              .desc("Number of refresh commands_" + to_string(channel->id))
              .precision(0)
              ;
          precycles_s
              .name("precycles_s_" + to_string(channel->id))
              .desc("Number of precharge cycles_" + to_string(channel->id))
              .precision(0)
              ;
          actcycles_s
              .name("actcycles_s_" + to_string(channel->id))
              .desc("Number of active cycles_" + to_string(channel->id))
              .precision(0)
              ;
          idlecycles_act_s
              .name("idlecycles_act_s_" + to_string(channel->id))
              .desc("Number of Idle cycles in the active state_" + to_string(channel->id))
              .precision(0)
              ;
          idlecycles_pre_s
              .name("idlecycles_pre_s_" + to_string(channel->id))
              .desc("Number of Idle cycles in the precharge state_" + to_string(channel->id))
              .precision(0)
              ;
          f_act_pdns_s
              .name("f_act_pdns_s_" + to_string(channel->id))
              .desc("Number of fast-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          s_act_pdns_s
              .name("s_act_pdns_s_" + to_string(channel->id))
              .desc("Number of slow-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          f_pre_pdns_s
              .name("f_pre_pdns_s_" + to_string(channel->id))
              .desc("Number of fast-exit precharged power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          s_pre_pdns_s
              .name("s_pre_pdns_s_" + to_string(channel->id))
              .desc("Number of slow-exit activate power-downs_" + to_string(channel->id))
              .precision(0)
              ;
          numberofsrefs_s
              .name("numberofsrefs_s_" + to_string(channel->id))
              .desc("Number of self-refresh commands_" + to_string(channel->id))
              .precision(0)
              ;
          f_act_pdcycles_s
              .name("f_act_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in fast-exit activate power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          s_act_pdcycles_s
              .name("s_act_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in slow-exit activate power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          f_pre_pdcycles_s
              .name("f_pre_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in fast-exit precharged power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          s_pre_pdcycles_s
              .name("s_pre_pdcycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in slow-exit precharged power-down mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_cycles_s
              .name("sref_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          pup_act_cycles_s
              .name("pup_act_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in activate power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          pup_pre_cycles_s
              .name("pup_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in precharged power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          spup_cycles_s
              .name("spup_cycles_s_" + to_string(channel->id))
              .desc("Number of clock cycles in self-refresh power-up mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_ref_act_cycles_s
              .name("sref_ref_act_cycles_s_" + to_string(channel->id))
              .desc("Number of active auto-refresh cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          sref_ref_pre_cycles_s
              .name("sref_ref_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of precharged auto-refresh cycles in self-refresh mode_" + to_string(channel->id))
              .precision(0)
              ;
          spup_ref_act_cycles_s
              .name("spup_ref_act_cycles_s_" + to_string(channel->id))
              .desc("Number of active auto-refresh cycles during self-refresh exit_" + to_string(channel->id))
              .precision(0)
              ;
          spup_ref_pre_cycles_s
              .name("spup_ref_pre_cycles_s_" + to_string(channel->id))
              .desc("Number of precharged auto-refresh cycles during self-refresh exit_" + to_string(channel->id))
              .precision(0)
              ;
        }
    }
    ~Controller(){
        delete scheduler;
        delete rowpolicy;
        delete rowtable;
        delete channel;
        delete refresh;
        for (auto& file : cmd_trace_files)
            file.close();
        cmd_trace_files.clear();
    }
    void finish(long dram_cycles) {
      channel->finish(dram_cycles);
    }
    /* Member Functions */
    Queue& get_queue(Request::Type type)
    {
        switch (int(type)) {
            case int(Request::Type::READ): return readq;
            case int(Request::Type::WRITE): return writeq;
            default: return otherq;
        }
    }
    bool enqueue(Request& req)
    {
        Queue& queue = get_queue(req.type);
        if (queue.max == queue.size())
            return false;
        req.arrive = clk;
        // queue.q.push_back(req);
        queue.arrive(req,clk);
        // shortcut for read requests, if a write to same addr exists
        // necessary for coherence
//        if (req.type == Request::Type::READ && find_if(writeq.q.begin(), writeq.q.end(),
//                [req](Request& wreq){ return req.addr == wreq.addr && req.coreid == wreq.coreid;}) != writeq.q.end()){
//            req.depart = clk + 1;
//            pending.push_back(req);
//            readq.q.pop_back();
//cd        }
        return true;
    }
    void tick()
    {
        clk++;
        (*req_queue_length_sum) += readq.size() + writeq.size() + pending.size();
        (*read_req_queue_length_sum) += readq.size() + pending.size();
        (*write_req_queue_length_sum) += writeq.size();

        readq.update(clk);
        writeq.update(clk);
        otherq.update(clk);

        /*** 1. Serve completed reads ***/
        if (pending.size()) {
            Request& req = pending[0];
            if (req.depart <= clk) {
                if (req.depart - req.arrive > 1) { // this request really accessed a row (when a read accesses the same address of a previous write, it directly returns. See how this is handled in enqueue function)
                    (*read_latency_sum) += req.depart - req.arrive;// + req.hops;
                    // (*read_queue_latency_sum) += req.depart - req.arrive_q_hbm; 
                    
                    if(false){
                        ofstream myfile;
                        myfile.open ("zahra_read_latency.txt", ios::app);
                        myfile << req.depart - req.arrive;// + req.hops;
                        myfile << ", ";
                        switch(int(req.type)){
                            case int(Request::Type::READ): myfile << "read"; break;
                            case int(Request::Type::WRITE): myfile << "write"; break;
                            case int(Request::Type::REFRESH): myfile << "refresh"; break;
                            case int(Request::Type::POWERDOWN) : myfile << "powerdown"; break;
                            case int(Request::Type::SELFREFRESH) : myfile << "selfrefresh"; break;
                            case int(Request::Type::EXTENSION): myfile << "extension"; break;
                            case int(Request::Type::MAX): myfile << "max"; break;
                        }
                        //myfile << req.Type;
                        myfile << ", ";
                        myfile << req.addr;
                        myfile << ", ";
                        myfile << channel->spec->standard_name;
                        myfile << ", bank:";  
                        int bank_id = req.addr_vec[int(HBM::Level::Bank)];
                        bank_id += req.addr_vec[int(HBM::Level::Bank) - 1] * channel->spec->org_entry.count[int(HBM::Level::Bank)];
                            
                        myfile << bank_id;
                        myfile << ", channel: " ;
                        myfile << channel->id;
                        myfile << ", rank:";
                        myfile << req.addr_vec[int(HBM::Level::Rank)];
                        myfile << ", column:";
                        myfile << req.addr_vec[int(HBM::Level::Column)];
                        myfile << ", row:";  
                        myfile << req.addr_vec[int(HBM::Level::Row)];
                        myfile << ", bankgroup:";
                        myfile << req.addr_vec[int(HBM::Level::BankGroup)];
                        myfile << "-bank:";  
                        myfile << req.addr_vec[int(HBM::Level::Bank)];
                        myfile << ", req.childid: ";
                        myfile << req.childid;
                        myfile << ", req.coreid: ";
                        myfile << req.coreid;
                        myfile << "\n";
                        myfile.close();
                    }
                  
		        channel->update_serving_requests(
                  req.addr_vec.data(), -1, clk);
                }
                if (req.type == Request::Type::READ || req.type == Request::Type::WRITE) {
                  req.callback(req);
                  pending.pop_front();
               }
            }
        }
        /*** 1.1. Serve completed writes ***/
        if (pending_write.size()) {
            Request& req = pending_write[0];
            if (req.depart <= clk) {
                req.callback(req);
                pending_write.pop_front();
            }
        }
        /*** 2. Refresh scheduler ***/
        refresh->tick_ref();
        /*** 3. Should we schedule writes? ***/
        if (!write_mode) {
            // yes -- write queue is almost full or read queue is empty
            if (writeq.size() >= int(0.8 * writeq.max) || readq.size() == 0)
                write_mode = true;
        }
        else {
            // no -- write queue is almost empty and read queue is not empty
            if (writeq.size() <= int(0.2 * writeq.max) && readq.size() != 0)
                write_mode = false;
        }
        /*** 4. Find the best command to schedule, if any ***/
        Queue* queue = !write_mode ? &readq : &writeq;
        if (otherq.size())
            queue = &otherq;  // "other" requests are rare, so we give them precedence over reads/writes
        auto req = scheduler->get_head(queue->q);
        if (req == queue->q.end() || !is_ready(req)) {
          if (!no_DRAM_latency) {
            // we couldn't find a command to schedule -- let's try to be speculative
            auto cmd = HBM::Command::PRE;
            vector<int> victim = rowpolicy->get_victim(cmd);
            if (!victim.empty()){
                issue_cmd(cmd, victim);
            }
            return;  // nothing more to be done this cycle
          } else {
            return;
          }
        }
        if (req->is_first_command) {
            req->is_first_command = false;
            int coreid = req->coreid;
            if (req->type == Request::Type::READ || req->type == Request::Type::WRITE) {
              channel->update_serving_requests(req->addr_vec.data(), 1, clk);
            }
            // FIXME: easy to make mistakes when calculating tx. TODO move tx calculation during initialization
            int tx = (channel->spec->prefetch_size * channel->spec->channel_width / 8) * req->burst_count; // req->burst_count is the initial value because req->is_first_command is true
            if (req->type == Request::Type::READ) {
                (*queueing_latency_sum) += clk - req->arrive;
                (*read_queue_latency_sum) += clk - req->arrive_q_hbm; 
                if (is_row_hit(req)) {
                    ++(*read_row_hits)[coreid];
                    ++(*row_hits);
                } else if (is_row_open(req)) {
                    ++(*read_row_conflicts)[coreid];
                    ++(*row_conflicts);
                } else {
                    ++(*read_row_misses)[coreid];
                    ++(*row_misses);
                }
              (*read_transaction_bytes) += tx;
            } else if (req->type == Request::Type::WRITE) {
              if (is_row_hit(req)) {
                  ++(*write_row_hits)[coreid];
                  ++(*row_hits);
              } else if (is_row_open(req)) {
                  ++(*write_row_conflicts)[coreid];
                  ++(*row_conflicts);
              } else {
                  ++(*write_row_misses)[coreid];
                  ++(*row_misses);
              }
              (*write_transaction_bytes) += tx;
            }
        }
        // issue command on behalf of request
        auto cmd = get_first_cmd(req);
        issue_cmd(cmd, get_addr_vec(cmd, req));
        // check whether this is the last command (which finishes the request)
        if (cmd != channel->spec->translate[int(req->type)])
            return;
        // set a future completion time for read requests
        if (req->type == Request::Type::READ) {
            req->depart = clk + channel->spec->read_latency;
            pending.push_back(*req);
        }
        if (req->type == Request::Type::WRITE) {
            channel->update_serving_requests(req->addr_vec.data(), -1, clk);
            req->depart = clk + channel->spec->write_latency;
            pending_write.push_back(*req);
        }
        // remove request from queue
        queue->q.erase(req);
    }
    bool is_ready(list<Request>::iterator req)
    {
        typename HBM::Command cmd = get_first_cmd(req);
        return channel->check(cmd, req->addr_vec.data(), clk);
    }
    bool is_ready(typename HBM::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check(cmd, addr_vec.data(), clk);
    }
    bool is_row_hit(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename HBM::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_hit(cmd, req->addr_vec.data());
    }
    bool is_row_hit(typename HBM::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_hit(cmd, addr_vec.data());
    }
    bool is_row_open(list<Request>::iterator req)
    {
        // cmd must be decided by the request type, not the first cmd
        typename HBM::Command cmd = channel->spec->translate[int(req->type)];
        return channel->check_row_open(cmd, req->addr_vec.data());
    }
    bool is_row_open(typename HBM::Command cmd, const vector<int>& addr_vec)
    {
        return channel->check_row_open(cmd, addr_vec.data());
    }
    void update_temp(ALDRAM::Temp current_temperature)
    {
    }
    // For telling whether this channel is busying in processing read or write
    bool is_active() {
      return (channel->cur_serving_requests > 0);
    }
    // For telling whether this channel is under refresh
    bool is_refresh() {
      return clk <= channel->end_of_refreshing;
    }
    void record_core(int coreid) {
    }


    void attach_parent_update_latency_function(function<void(const Request&)> partent_function) {
        update_parent_with_latency = partent_function;
    }
private:
    typename HBM::Command get_first_cmd(list<Request>::iterator req)
    {
        typename HBM::Command cmd = channel->spec->translate[int(req->type)];
        if (!no_DRAM_latency) {
          return channel->decode(cmd, req->addr_vec.data());
        } else {
          return cmd;
        }
    }
    void issue_cmd(typename HBM::Command cmd, const vector<int>& addr_vec)
    {
        assert(is_ready(cmd, addr_vec));
        if (with_drampower) {
          int bank_id = addr_vec[int(HBM::Level::Bank)];
          if (channel->spec->standard_name == "DDR4" || channel->spec->standard_name == "GDDR5" || channel->spec->standard_name == "HBM") {
              // if has bankgroup
              bank_id += addr_vec[int(HBM::Level::Bank) - 1] * channel->spec->org_entry.count[int(HBM::Level::Bank)];
          }
          update_counter++;
          if (update_counter == 1000000) {
              update_counter = 0;
          }
        }
        if (!no_DRAM_latency) {
          channel->update(cmd, addr_vec.data(), clk);
          rowtable->update(cmd, addr_vec, clk);
        } else {
          // still have bandwidth restriction (update timing for RD/WR requets)
          channel->update_timing(cmd, addr_vec.data(), clk);
        }
        if (record_cmd_trace){
            // select rank
            auto& file = cmd_trace_files[addr_vec[1]];
            string& cmd_name = channel->spec->command_name[int(cmd)];
            file<<clk<<','<<cmd_name;
            // TODO bad coding here
            if (cmd_name == "PREA" || cmd_name == "REF")
                file<<endl;
            else{
                int bank_id = addr_vec[int(HBM::Level::Bank)];
                if (channel->spec->standard_name == "DDR4" || channel->spec->standard_name == "GDDR5" || channel->spec->standard_name == "HBM") {
                    bank_id += addr_vec[int(HBM::Level::Bank) - 1] * channel->spec->org_entry.count[int(HBM::Level::Bank)];
                }
                file<<','<<bank_id<<endl;
            }
        }
        if (print_cmd_trace){
            printf("%5s %10ld:", channel->spec->command_name[int(cmd)].c_str(), clk);
            for (int lev = 0; lev < int(HBM::Level::MAX); lev++)
                printf(" %5d", addr_vec[lev]);
            printf("\n");
        }
    }
    vector<int> get_addr_vec(typename HBM::Command cmd, list<Request>::iterator req){
        return req->addr_vec;
    }
};
} /*namespace ramulator*/
#endif /*__HBM_CONTROLLER_H*/