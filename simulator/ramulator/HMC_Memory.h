#ifndef __HMC_MEMORY_H
#define __HMC_MEMORY_H

#include "HMC.h"
#include "LogicLayer.h"
#include "LogicLayer.cc"
#include "Memory.h"
#include "Packet.h"
#include "Statistics.h"
#include <fstream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <array>
#include <climits>
#include <bitset>
#include <algorithm>
#include <functional>

using namespace std;

namespace ramulator
{

template<>
class Memory<HMC, Controller> : public MemoryBase
{
protected:
  long max_address;

  long instruction_counter = 0;
  bool profile_this_epoch = true;
  bool get_memory_addresses = false;
  string application_name;
  ofstream memory_addresses;
  ofstream adaptive_thresholds;
  ofstream latencies_each_epoch;
  ofstream feedback_reg_epoch;
  ofstream adaptive_thresholds_record;
  ofstream set_sampling_result;



  long capacity_per_stack;
  ScalarStat dram_capacity;
  ScalarStat num_dram_cycles;
  VectorStat num_read_requests;
  VectorStat num_write_requests;
  ScalarStat ramulator_active_cycles;
  ScalarStat memory_footprint;
  VectorStat incoming_requests_per_channel;
  VectorStat incoming_read_reqs_per_channel;
  ScalarStat physical_page_replacement;
  ScalarStat maximum_internal_bandwidth;
  ScalarStat maximum_link_bandwidth;
  ScalarStat read_bandwidth;
  ScalarStat write_bandwidth;

  ScalarStat read_latency_avg;
  ScalarStat read_latency_ns_avg;
  ScalarStat read_latency_sum;
  ScalarStat queueing_latency_avg;
  ScalarStat queueing_latency_ns_avg;
  ScalarStat queueing_latency_sum;
  ScalarStat request_packet_latency_avg;
  ScalarStat request_packet_latency_ns_avg;
  ScalarStat request_packet_latency_sum;
  ScalarStat response_packet_latency_avg;
  ScalarStat response_packet_latency_ns_avg;
  ScalarStat response_packet_latency_sum;

  // shared by all Controller objects
  ScalarStat read_transaction_bytes;
  ScalarStat write_transaction_bytes;
  ScalarStat row_hits;
  ScalarStat row_misses;
  ScalarStat row_conflicts;
  VectorStat read_row_hits;
  VectorStat read_row_misses;
  VectorStat read_row_conflicts;
  VectorStat write_row_hits;
  VectorStat write_row_misses;
  VectorStat write_row_conflicts;

  ScalarStat req_queue_length_avg;
  ScalarStat req_queue_length_sum;
  ScalarStat read_req_queue_length_avg;
  ScalarStat read_req_queue_length_sum;
  ScalarStat write_req_queue_length_avg;
  ScalarStat write_req_queue_length_sum;

  VectorStat record_read_hits;
  VectorStat record_read_misses;
  VectorStat record_read_conflicts;
  VectorStat record_write_hits;
  VectorStat record_write_misses;
  VectorStat record_write_conflicts;

  long mem_req_count = 0;
  long long total_hops = 0;
  long total_memory_accesses = 0;
  int num_cores;
  int max_block_col_bits;
  bool warmup_finished = false;
  long clk_at_end_of_warmup = 0;
  long warmup_reqs = 0;
  int network_width = 6;
  int network_height = 6;
  int central_vault = 14;
  int max_hops = 70;
public:
    long clk = 0;
    bool pim_mode_enabled = false;
    bool network_overhead = false;
    int per_hop_overhead = 1;

    int calculate_hops_travelled(int src_vault, int dst_vault, int length) {
      assert(src_vault >= 0);
      assert(dst_vault >= 0);
      assert(length > 0);
      int hops = calculate_hops_travelled(src_vault, dst_vault)*length;
      assert(hops <= max_hops);
      return hops;
    }

    int calculate_hops_travelled(int src_vault, int dst_vault) {
      assert(src_vault >= 0);
      assert(dst_vault >= 0);
      int vault_destination_x = dst_vault/network_width;
      int vault_destination_y = dst_vault%network_width;

      int vault_origin_x = src_vault/network_width;
      int vault_origin_y = src_vault%network_width;

      int hops = abs(vault_destination_x - vault_origin_x) + abs(vault_destination_y - vault_origin_y);
      assert(hops <= max_hops);
      return hops*per_hop_overhead;
    }

    int calculate_broadcast_hops_travelled(int src_vault) {
      assert(src_vault >= 0);

      int vault_origin_x = src_vault/network_width;
      int vault_origin_y = src_vault%network_width;

      int min_x = 0;
      int min_y = 0;

      int max_x = network_width - 1;
      int max_y = network_width - 1;

      int max_x_hops = max(abs(max_x - vault_origin_x), abs(vault_origin_x - min_x));
      int max_y_hops = max(abs(max_y - vault_origin_y), abs(vault_origin_y - min_y));

      int hops = max_x_hops + max_y_hops;
      assert(hops <= max_hops);
      return hops;
    }

    void set_address_recorder (){
      get_memory_addresses = false;
      string to_open = application_name + ".memory_addresses.csv";
      std::cout << "Recording memory trace at " << to_open << "\n";
      memory_addresses.open(to_open.c_str(), std::ofstream::out);
      memory_addresses << "CLK,RawADDR,ADDR,CoreID,Hops,NoPFHops,W|R,Vault,BankGroup,Bank,Row,Column \n";
    }

    void set_adaptive_threshold_recorder () {
      string thresholds_to_open = application_name + ".adaptive_thresholds.csv";
      adaptive_thresholds.open(thresholds_to_open.c_str(), std::ofstream::out);
      adaptive_thresholds << "Epoch Start Cycle,";
      for(int i = 0; i < ctrls.size(); i++) {
        adaptive_thresholds << "Core " << i << ",";
      }
      adaptive_thresholds << "\n";

      string latencies_to_open = application_name + ".latencies_per_epoch.csv";
      latencies_each_epoch.open(latencies_to_open.c_str(), std::ofstream::out);
      latencies_each_epoch << "Epoch Start Cycle,";
      for(int i = 0; i < ctrls.size(); i++) {
        latencies_each_epoch << "Core " << i << ",";
      }
      latencies_each_epoch << "\n";

      string feedback_regs_to_open = application_name + ".feedback_regs_per_epoch.csv";
      feedback_reg_epoch.open(feedback_regs_to_open.c_str(), std::ofstream::out);
      feedback_reg_epoch << "Epoch Start Cycle,";
      for(int i = 0; i < ctrls.size(); i++) {
        feedback_reg_epoch << "Core " << i << ",";
      }
      feedback_reg_epoch << "\n";
      string set_sampling_result_to_open = application_name+".set_sampling_result.csv";
      set_sampling_result.open(set_sampling_result_to_open.c_str(), std::ofstream::out);
      string to_open = application_name + ".adaptive_thresholds_record.csv";
      adaptive_thresholds_record.open(to_open.c_str(), std::ofstream::out);
      adaptive_thresholds_record << "Epoch Start Cycle,";
      for(int i = 0; i < ctrls.size(); i++) {
        adaptive_thresholds_record << "Feedback Register for Core " << i << ",";
        adaptive_thresholds_record << "Latency Magnitute for Core " << i << ",";
        adaptive_thresholds_record << "Last Change for Core " << i << ",";
        adaptive_thresholds_record << "Threshold for Core " << i << ",";
      }
      adaptive_thresholds_record << "\n";
    }

    void set_application_name(string _app){
      application_name = _app;
    }

    enum class Type {
        RoCoBaVa, // XXX The specification doesn't define row/column addressing
        RoBaCoVa,
        RoCoBaBgVa,
        MAX,
    } type = Type::RoCoBaVa;

    std::map<std::string, Type> name_to_type = {
      {"RoCoBaVa", Type::RoCoBaVa},
      {"RoBaCoVa", Type::RoBaCoVa},
      {"RoCoBaBgVa", Type::RoCoBaBgVa}};

    enum class Translation {
      None,
      Random,
      MAX,
    } translation = Translation::None;

    std::map<string, Translation> name_to_translation = {
      {"None", Translation::None},
      {"Random", Translation::Random},
    };

    vector<int> free_physical_pages;
    long free_physical_pages_remaining;
    map<pair<int, long>, long> page_translation;

    vector<list<int>> tags_pools;

    vector<Controller<HMC>*> ctrls;
    vector<LogicLayer<HMC>*> logic_layers;
    HMC * spec;

    enum SubscriptionPrefetcherType {
      None, // Baseline configuration (no prefetching)
      Swap, // Swap with remote vault's same address
      Allocate, // Allocate from local vault's reserved address. To be implemented
      Copy // Copy to local vault's reserved address. To be implemented
    } subscription_prefetcher_type = SubscriptionPrefetcherType::None;

    std::map<string, SubscriptionPrefetcherType> name_to_prefetcher_type = {
      {"None", SubscriptionPrefetcherType::None},
      {"Swap", SubscriptionPrefetcherType::Swap},
      {"Allocate", SubscriptionPrefetcherType::Allocate},
    };

    // A subscription based prefetcher
    class SubscriptionPrefetcherSet {
    private:
      // Subscription task. Denoting where it is subscribing from, and where to, and how many cycles of latency
      struct SubscriptionTask {
        enum Type {
          SubReq, // Request a data to be sent from "to_vault" to "from_vault"
          SubReqAck, // Acknowledge "SubReq". Incorporated into "SubXfer" in real scenario
          SubReqNAck, // Negatively acknowledge "SubReq". Used when "from_vault" has no space so "to_vault" can rollback the subscription
          SubXfer, // Actually transfer the data from "from_vault" to "to_vault"
          SubXferAck, // Acknowledge "SubXferAck"
          UnsubReq, // Request a data currently in "to_vault" be returned to "from_vault"
          UnsubReqAck, // Acknowledge "UnsubReq". Incorporated into "UnsubXfer" in real scenario
          UnsubXfer, // Actually transfer the datat from "from_vault" to "to_vault"
          UnsubXferAck, // Acknowledge "UnsubXfer"
          ResubReq, // Request a data to be sent from "to_vault" to "from_vault", but does not trigger swap
          ResubXfer, // Actually transfer the data from "from_vault" to "to_vault"
          ResubXferAck, // Acknowledge "ResubXfer"
          UpdateOnWrite, // Set dirty bit on write
          IncThreshold, // Happens in adapative policy. Broadcast to all vaults to increase threshold
          DecThreshold, // Same as above, but decrease threshold
          UpdateAdap,
          UpdateThreshold,
        } type;
        long addr;
        int from_vault;
        int to_vault;
        int hops;
        bool dirty = false;
        bool from_buffer = false;
        int count = 0;
        int64_t feedback = 0;
        long latencies = 0;
        int num_requests = 0;
        unordered_map<int, long> latencies_for_diff_thresholds;
        unordered_map<int, int> requests_completed_for_diff_thresholds;
        unordered_map<int, int64_t> feedbacks_for_diff_thresholds;
        SubscriptionTask(long addr, int from_vault, int to_vault, int hops, Type type):addr(addr),from_vault(from_vault),to_vault(to_vault),hops(hops),type(type){}
        SubscriptionTask(long addr, int from_vault, int to_vault, int hops, Type type, bool dirty):addr(addr),from_vault(from_vault),to_vault(to_vault),hops(hops),type(type),dirty(dirty){}
        SubscriptionTask(long addr, int from_vault, int to_vault, int hops, Type type, int count):addr(addr),from_vault(from_vault),to_vault(to_vault),hops(hops),type(type),count(count){}
        SubscriptionTask(long addr, int from_vault, int to_vault, int hops, Type type, bool dirty, bool from_buffer):addr(addr),from_vault(from_vault),to_vault(to_vault),hops(hops),type(type),dirty(dirty),from_buffer(from_buffer){}
        SubscriptionTask(long addr, int from_vault, int to_vault, int hops, Type type, int64_t feedback, long latencies, int num_requests, unordered_map<int, long> latencies_for_thresholds, unordered_map<int, int> requests_for_thresholds, unordered_map<int, int64_t> feedbacks_for_thresholds)
          :addr(addr),from_vault(from_vault),to_vault(to_vault),hops(hops),type(type),feedback(feedback),
          latencies(latencies),num_requests(num_requests), latencies_for_diff_thresholds(latencies_for_thresholds), requests_completed_for_diff_thresholds(requests_for_thresholds), feedbacks_for_diff_thresholds(feedbacks_for_thresholds){}
        SubscriptionTask(){}
      };
      class LRUUnit;
      class LFUUnit;
      class SubscriptionBuffer;
      // The actual subscription table. Translates an address to its subscribed vault
      // Some variables just to save the vaule before initialization
      size_t subscription_table_size = SIZE_MAX;
      size_t subscription_table_ways = subscription_table_size;
      size_t subscription_table_sets = subscription_table_size / subscription_table_ways;
      size_t receiving_buffer_size = 32;
      class SubscriptionTable{
        private:
        bool initialized = false; // Each subscription table can be only initialized once
        int controller = -1;
        // Specs for Subscription table
        size_t subscription_table_size = SIZE_MAX;
        size_t subscription_table_ways = subscription_table_size;
        size_t subscription_table_sets = subscription_table_size / subscription_table_ways;
        size_t receiving_buffer_size = 32;
        // To reserve location in receiving buffer and make sure there is not too many pending subscription/unsubscription at the same time
        size_t receiving = 0;
        LRUUnit* lru_unit = nullptr;
        LFUUnit* lfu_unit = nullptr;
        Memory<HMC, Controller>* mem_ptr = nullptr;
        SubscriptionBuffer* subscription_buffer = nullptr;
        int count_threshold = 0;
        long total_in_table = 0;
        struct SubscriptionTableEntry {
          int vault;
          enum SubscriptionStatus {
            PendingSubscription,
            Subscribed,
            PendingRemoval,
            PendingResubscription,
            Invalid,
          } status = SubscriptionStatus::PendingSubscription;
          bool dirty = false;
          long finished_subscription = 0;
          SubscriptionTableEntry(){}
          SubscriptionTableEntry(int vault):vault(vault){}
          SubscriptionTableEntry(int vault, SubscriptionTableEntry::SubscriptionStatus status):vault(vault),status(status){}
          SubscriptionTableEntry(int vault, SubscriptionTableEntry::SubscriptionStatus status, bool dirty):vault(vault),status(status),dirty(dirty){}
        };
        // Actual data structure for those tables
        unordered_map<long, SubscriptionTableEntry> address_translation_table; // Subscribe remote address (1st val) to local address (2nd address)
        vector<size_t> virtualized_table_sets; // Used for limiting the number of ways in each "set" in each subscription table
        public:
        unordered_map<long, int> unused_subscriptions;
        SubscriptionTable(){}
        SubscriptionTable(size_t size, size_t ways, size_t receiving_buffer_size, Memory<HMC, Controller>* mem_ptr):subscription_table_size(size),subscription_table_ways(ways),receiving_buffer_size(receiving_buffer_size), mem_ptr(mem_ptr){initialize();} // Only set from table size
        void propogate_count_threshold(int threshold) {
          count_threshold = threshold;
        }
        void set_controller(int c) {controller = c;}
        unordered_map<long, SubscriptionTableEntry>::iterator begin(){return address_translation_table.begin();}
        unordered_map<long, SubscriptionTableEntry>::iterator end(){return address_translation_table.end();}
        void clear(){address_translation_table.clear();}
        void set_subscription_table_size(size_t size) {
          subscription_table_size = size;
          // If we have not set the table ways, we make it fully associative to prevent any issues
          if(subscription_table_ways == SIZE_MAX){
            subscription_table_ways = size;
          }
        }
        void set_subscription_table_ways(size_t ways) {subscription_table_ways = ways;}
        size_t get_subscription_table_size()const{return subscription_table_size;}
        size_t get_subscription_table_ways()const{return subscription_table_ways;}
        size_t get_subscription_table_sets()const{return subscription_table_sets;}
        void attach_lru_unit(LRUUnit* ptr) {
          lru_unit = ptr;
        }
        void attach_lfu_unit(LFUUnit* ptr) {
          lfu_unit = ptr;
        }
        void attach_subscription_buffer(SubscriptionBuffer* ptr) {
          subscription_buffer = ptr;
        }
        void insert_lru_lfu(long addr) {
          if(lru_unit != nullptr) {
            lru_unit -> insert(addr);
          }
          if(lfu_unit != nullptr) {
            lfu_unit -> insert(addr);
          }
        }
        void erase_lfu_lru(long addr) {
          if(lru_unit != nullptr) {
            lru_unit -> erase(addr);
          }
          if(lfu_unit != nullptr) {
            lfu_unit -> erase(addr);
          }
        }
        // We split initialize() function from constructor as it might be called after constructor. It can be only exec'ed once
        void initialize(){
          // We can only initialize once
          assert(!initialized);
          // Initialize the subscription table
          assert(subscription_table_size % subscription_table_ways == 0);
          subscription_table_sets = subscription_table_size / subscription_table_ways;
          cout << "Subscription Table Size: " << (subscription_table_size == SIZE_MAX ? "Unlimited" : to_string(subscription_table_size)) << endl;
          cout << "Subscription Table Ways: " << (subscription_table_ways == SIZE_MAX ? "Unlimited" : to_string(subscription_table_ways)) << endl;
          cout << "Subscription Table Sets: " << subscription_table_sets << endl;
          // One subscription to table per vault
          virtualized_table_sets.assign(subscription_table_sets, 0);
          initialized = true;
        }
        size_t get_set(long addr)const{return addr % subscription_table_sets;}
        bool subscription_table_is_free(long addr, size_t required_space) const {
          return virtualized_table_sets[get_set(addr)] + required_space <= subscription_table_ways;
        }
        bool receive_buffer_is_free()const{return receiving < receiving_buffer_size;}
        void submit_subscription(int req_vault, long addr){
          if(has(addr)) {
            cout << "address " << addr << " already exists in table. its status is " << get_status(addr);
          }
          assert(!has(addr));
          virtualized_table_sets[get_set(addr)]++;
          assert(virtualized_table_sets[get_set(addr)] <= subscription_table_ways);
          insert_lru_lfu(addr);
          address_translation_table.insert({addr, SubscriptionTableEntry(req_vault, SubscriptionTableEntry::SubscriptionStatus::PendingSubscription)});
          unused_subscriptions.insert({addr, 0});
        }
        void update_valid_bit(long addr){
          if(subscription_buffer != nullptr){
            subscription_buffer -> update_valid_bit(addr);
          }
        }
        void complete_subscription(long addr) {
          if(!is_pending_subscription(addr) && !is_pending_removal(addr)) {
            cout << "address " << addr << " is not pending subscription, it is " << get_status(addr);
          }
          assert(is_pending_subscription(addr) || is_pending_removal(addr));
          if(has(addr)) {
            address_translation_table.at(addr).finished_subscription = mem_ptr -> clk;
            address_translation_table.at(addr).status = SubscriptionTableEntry::SubscriptionStatus::Subscribed;
          }
        }
        void rollback_subscription(long addr) {
          if(!is_pending_subscription(addr) && !is_pending_removal(addr)) {
            cout << "address " << addr << " is not pending subscription, it is " << get_status(addr);
          }
          assert(is_pending_subscription(addr) || is_pending_removal(addr) || !has(addr));
          remove_table_entry(addr);
        }
        void submit_unsubscription(long addr) {
          if(!is_pending_subscription(addr) && !is_subscribed(addr)) {
            cout << "address " << addr << " is not pending subscription or subscribed, it is " << get_status(addr);
          }
          assert(is_pending_subscription(addr) || is_subscribed(addr));
          if(has(addr)) {
            address_translation_table.at(addr).status = SubscriptionTableEntry::SubscriptionStatus::PendingRemoval;
          }
        }
        void submit_resubscription(int vault, long addr) {
          if(!is_subscribed(addr)) {
            cout << "address " << addr << " is not subscribed, it is " << get_status(addr);
          }
          assert(is_subscribed(addr));
          if(has(addr)) {
            address_translation_table.at(addr).status = SubscriptionTableEntry::SubscriptionStatus::PendingResubscription;
            address_translation_table.at(addr).vault = vault;
          }
        }
        void modify_subscription(int vault, long addr) {
          if(!is_subscribed(addr)) {
            cout << "address " << addr << " is not subscribed, it is " << get_status(addr);
          }
          assert(is_subscribed(addr));
          if(has(addr)) {
            address_translation_table.at(addr).vault = vault;
          }
        }
        int get_status(long addr)const{
          if(has(addr)) {
            return (int)address_translation_table.at(addr).status;
          }
          return -1;
        }
        void set_dirty(long addr){
          if(has(addr)) {
            address_translation_table.at(addr).dirty = true;
          }
        }
        bool is_dirty(long addr)const{
          if(has(addr)){
            return address_translation_table.at(addr).dirty;
          }
          return false;
        }
        void start_receiving() {
          receiving++;
        }
        void stop_receiving() {
          receiving--;
        }
        void complete_unsubscription(long addr) {
          remove_table_entry(addr);
        }
        void complete_resubscription(long addr) {
          assert(is_pending_resubscription(addr));
          remove_table_entry(addr);
        }
        void remove_table_entry(long addr) {
          if(has(addr)) {
            erase_lfu_lru(addr);
            if(address_translation_table[addr].finished_subscription > 0) {
              total_in_table += (mem_ptr -> clk - address_translation_table[addr].finished_subscription);
            }
            // Actually remove the address from the table
            address_translation_table.erase(addr);
            if(unused_subscriptions.count(addr) > 0) {
              unused_subscriptions.erase(addr);
            }
            // Decrease the "virtual" set's content count for subscription from the original vault
            if(virtualized_table_sets[get_set(addr)] > 0) {
              virtualized_table_sets[get_set(addr)]--;
              assert(virtualized_table_sets[get_set(addr)] < subscription_table_ways);
            }
            update_valid_bit(addr);
          }
        }
        bool has(long addr) const{return address_translation_table.count(addr) > 0;}
        bool has(long addr, int vault)const{
          if(has(addr)){
            return address_translation_table.at(addr).vault == vault;
          }
          return false;
        }
        bool is_subscribed(long addr, int vault)const {
          if(is_subscribed(addr)) {
            return address_translation_table.at(addr).vault == vault;
          }
          return false;
        }
        bool is_subscribed(long addr) const{
          if(has(addr)){
            return address_translation_table.at(addr).status == SubscriptionTableEntry::SubscriptionStatus::Subscribed;
          }
          return false;
        }
        bool is_pending_subscription(long addr) const{
          if(has(addr)){
            return address_translation_table.at(addr).status == SubscriptionTableEntry::SubscriptionStatus::PendingSubscription;
          }
          return false;
        }
        bool is_pending_subscription(long addr, int vault) const{
          if(is_pending_subscription(addr)){
            return address_translation_table.at(addr).vault == vault;
          }
          return false;
        }
        bool is_pending_removal(long addr) const{
          if(has(addr)){
            return address_translation_table.at(addr).status == SubscriptionTableEntry::SubscriptionStatus::PendingRemoval;
          }
          return false;
        }
        bool is_pending_removal(long addr, int vault) const{
          if(is_pending_removal(addr)){
            return address_translation_table.at(addr).vault == vault;
          }
          return false;
        }
        bool is_pending_resubscription(long addr) const{
          if(has(addr)){
            return address_translation_table.at(addr).status == SubscriptionTableEntry::SubscriptionStatus::PendingResubscription;
          }
          return false;
        }
        bool is_pending_resubscription(long addr, int vault) const{
          if(is_pending_resubscription(addr)){
            return address_translation_table.at(addr).vault == vault;
          }
          return false;
        }
        int& operator[](const long& addr){
          return address_translation_table[addr].vault;
        }
        void touch(bool is_original_vault, long addr) {
          if(has(addr)) {
            if(is_original_vault) {
              unused_subscriptions[addr]--;
            } else {
              unused_subscriptions[addr]++;
            }
          }
        }
        size_t count(long addr) const{return address_translation_table.count(addr);}
        void finish() {
          for(auto i:address_translation_table) {
            if(i.second.finished_subscription > 0) {
              total_in_table += (mem_ptr -> clk - i.second.finished_subscription);
              assert(total_in_table > 0);
            }
          }
        }
        long get_total_in_table()const {return total_in_table;}
      };
      vector<SubscriptionTable> subscription_tables;

      // Structures used to evict entry from subscription table when it's full. We can choose from LRU and LFU and "dirty" LFU.
      enum SubscriptionPrefetcherReplacementPolicy {
        LRU,
        LFU,
        DirtyLFU,
      } subscription_table_replacement_policy = SubscriptionPrefetcherReplacementPolicy::LRU; // We default it to LRU
      std::map<string, SubscriptionPrefetcherReplacementPolicy> name_to_prefetcher_rp = {
        {"LRU", SubscriptionPrefetcherReplacementPolicy::LRU},
        {"LFU", SubscriptionPrefetcherReplacementPolicy::LFU},
        {"DirtyLFU", SubscriptionPrefetcherReplacementPolicy::DirtyLFU},
      };
      class LRUUnit {
        private:
        bool initialized = false;
        size_t address_access_history_size = SIZE_MAX; // Currently the size cannot be less than the corresponding table size or it may deadlock (see touch() function below)
        size_t address_access_history_used = 0; // Used for LRU
        size_t corresponding_table_sets = 1;
        vector<list<long>> address_access_history;
        unordered_map<long, typename list<long>::iterator> address_access_history_map;
        public:
        LRUUnit(){}
        LRUUnit(size_t set):corresponding_table_sets(set){initialize();}
        LRUUnit(size_t size, size_t sets):address_access_history_size(size),corresponding_table_sets(sets){initialize();}
        void set_address_access_history_size(size_t size){address_access_history_size = size;}
        void set_corresponding_table_sets(size_t sets){corresponding_table_sets = sets;}
        size_t get_address_access_history_size()const{return address_access_history_size;}
        size_t get_address_access_history_used()const{return address_access_history_used;}
        size_t get_corresponding_table_sets()const{return corresponding_table_sets;}
        size_t get_set(long addr)const{return addr % corresponding_table_sets;}
        void initialize(){
          // We can only initialize once
          assert(!initialized);
          cout << "Address access history size: " << (address_access_history_size == SIZE_MAX ? "Unlimited" : to_string(address_access_history_size));
          cout << " Corresponding table sets: " << corresponding_table_sets << endl;
          address_access_history.assign(corresponding_table_sets, list<long>());
          initialized = true;
        }
        void erase(long addr){
          if(address_access_history_map.count(addr)){
            address_access_history[get_set(addr)].erase(address_access_history_map[addr]);
            address_access_history_map.erase(addr);
            address_access_history_used--;
          }
        }
        void touch(long addr) {
          if(address_access_history_map.count(addr) == 0) {
            return;
          }
          insert(addr);
        }
        void insert(long addr){
          // If there exists the address in access history, we first remove it
          erase(addr);
          // Then if the address access history table is still larger than maximum minus one, we make some space
          // TODO: Make this global
          while(!address_access_history[get_set(addr)].empty() && address_access_history_used >= address_access_history_size) {
            long last = address_access_history[get_set(addr)].back();
            address_access_history[get_set(addr)].pop_back();
            address_access_history_map.erase(last);
            address_access_history_used--;
          }
          // Last, we insert the new address to the front of the history (i.e. most recently accessed)
          address_access_history[get_set(addr)].push_front(addr);
          address_access_history_used++;
          address_access_history_map[addr] = address_access_history[get_set(addr)].begin();
        }
        long find_victim(long addr)const{
          assert(initialized);
          return address_access_history[get_set(addr)].back();
        }
      };
      class LFUUnit {
        private:
        bool initialized = false;
        size_t count_priority_queue_size = SIZE_MAX; // Currently the size cannot be less than the corresponding table size or it may deadlock (see touch() function below)
        size_t count_priority_queue_used = 0;
        size_t corresponding_table_sets = 1;
        struct LFUPriorityQueueItem {
          long addr;
          uint64_t count;
          LFUPriorityQueueItem(){}
          LFUPriorityQueueItem(long addr, uint64_t count):addr(addr),count(count){}
          bool operator< (const LFUPriorityQueueItem& lfupqi)const {
            return this -> count < lfupqi.count;
          }
        };
        vector<multiset<LFUPriorityQueueItem>> count_priority_queue;
        unordered_map<long, typename multiset<LFUPriorityQueueItem>::iterator> count_priority_queue_map;
        public:
        LFUUnit(){}
        LFUUnit(size_t set):corresponding_table_sets(set){initialize();}
        LFUUnit(size_t size, size_t sets):count_priority_queue_size(size),corresponding_table_sets(sets){initialize();}
        void set_count_priority_queue_size(size_t size){count_priority_queue_size = size;}
        void set_corresponding_table_sets(size_t sets){corresponding_table_sets = sets;}
        size_t get_count_priority_queue_size()const{return count_priority_queue_size;}
        size_t get_count_priority_queue_used()const{return count_priority_queue_used;}
        size_t get_corresponding_table_sets()const{return corresponding_table_sets;}
        size_t get_set(long addr)const{return addr % corresponding_table_sets;}
        void initialize(){
          // We can only initialize once
          assert(!initialized);
          cout << "Count Priority Queue size: " << (count_priority_queue_size == SIZE_MAX ? " Unlimited" : to_string(count_priority_queue_size));
          cout << " Corresponding table sets: " << corresponding_table_sets << endl;
          count_priority_queue.assign(corresponding_table_sets, multiset<LFUPriorityQueueItem>()); // Used for LFU
          initialized = true;
        }
        void touch(long addr) {
          if(count_priority_queue_map.count(addr) == 0) {
            return;
          }
          insert(addr);
        }
        void insert(long addr){
          // First we set the count to 0, in case we are handling new entry
          LFUPriorityQueueItem item(addr, 0);
          // Then, we try to find the existing entry with same address, take it's data to item, increase the count, then remove it from the table pending reinsertion
          if(count_priority_queue_map.count(addr)) {
            auto it = count_priority_queue_map[addr];
            item = *it;
            item.count++;
            count_priority_queue[get_set(addr)].erase(it);
            count_priority_queue_map.erase(addr);
            count_priority_queue_used--;
          }
          // Also, we check if the table has free space, and try make one if it doesn't
          // TODO: Make it global
          while(!count_priority_queue[get_set(addr)].empty() && count_priority_queue_used >= count_priority_queue_size) {
            auto it = count_priority_queue[get_set(addr)].begin();
            long top_addr = it -> addr;
            count_priority_queue[get_set(addr)].erase(it);
            count_priority_queue_map.erase(top_addr);
            count_priority_queue_used--;
          }
          // Finally, we (re)insert the entry into the table
          count_priority_queue_map[addr] = count_priority_queue[get_set(addr)].insert(item);
          count_priority_queue_used++;
        }
        void erase(long addr){
          if(count_priority_queue_map.count(addr)) {
            count_priority_queue[get_set(addr)].erase(count_priority_queue_map[addr]);
            count_priority_queue_map.erase(addr);
            count_priority_queue_used--;
          }
        }
        long find_victim(long addr)const{
          assert(initialized);
          // if(count_priority_queue[get_set(addr)].begin() -> count > 0 || count_priority_queue[get_set(addr)].end() -> count > 0){
            // cout << "The least used address is " << count_priority_queue[get_set(addr)].begin() -> addr << " with count " << count_priority_queue[get_set(addr)].begin() -> count << endl;
            // for(auto& i: count_priority_queue[get_set(addr)]) {
            //   cout << i.addr << " " << i.count << endl;
            // }
            // cout << "The most used address is" << prev(count_priority_queue[get_set(addr)].end()) -> addr << " with count " << prev(count_priority_queue[get_set(addr)].end()) -> count << endl;
          // }
          return count_priority_queue[get_set(addr)].begin() -> addr; // LFU Logic
        }
      };
      vector<LRUUnit> lru_units;
      vector<LFUUnit> lfu_units;

      // Count table. Tracks the # of accesses for each memory location so we can submit for subscription when it reaches threshold
      class CountTable {
        public:
        struct CountTableEntry{
          uint64_t tag;
          uint64_t count;
          CountTableEntry(){}
          CountTableEntry(uint64_t tag, uint64_t count):tag(tag),count(count){}
        };
        private:
        bool initialized = false;
        size_t counter_table_size = 1024;
        int counter_bits = 6;
        int tag_bits = 26;
        int controllers;
        vector<vector<CountTableEntry>> count_tables;
        long evictions = 0;
        long insertions = 0;
        uint64_t maximum_count = 0;
        uint64_t total_count_at_eviction = 0;
        public:
        CountTable(){}
        CountTable(int controllers, size_t size, int counter_bits, int tag_bits):controllers(controllers),counter_table_size(size),counter_bits(counter_bits),tag_bits(tag_bits){initialize();}
        void set_counter_table_size(size_t size){counter_table_size = size;}
        void set_counter_bits(int bits){counter_bits = bits;}
        void set_tag_bits(int bits){tag_bits = bits;}
        void set_controllers(int controllers){this -> controllers = controllers;}
        size_t get_counter_table_size()const{return counter_table_size;}
        int get_counter_bits()const{return counter_bits;}
        int get_tag_bits()const{return tag_bits;}
        int get_controllers()const{return controllers;}
        void initialize(){
          assert(!initialized);
          cout << "Counter table size: " << counter_table_size << " We use " << counter_bits << " bits for counter and " << tag_bits << " bits for tag" << endl;
          count_tables.assign(controllers, vector<CountTableEntry>(counter_table_size, CountTableEntry(0, 0)));
          initialized = true;
        }
        uint64_t calculate_tag(long addr)const{
          const int total_bits = 8*sizeof(uint64_t);
          const int higher_bits = total_bits - tag_bits;
          uint64_t tag = 0;
          while (addr != 0) {
            tag ^= addr;
            addr = addr >> tag_bits;
          }
          tag = (tag << (higher_bits)) >> (higher_bits);
          return tag;
        }
        uint64_t update_counter_table_and_get_count(int req_vault, long addr) {
          uint64_t tag = calculate_tag(addr);
          int index = addr % counter_table_size;
          if(count_tables[req_vault][index].tag != tag){
            // cout << "Inserting " << addr << " address into vault " << req_vault << "'s counter table location " << index << endl;
            if(count_tables[req_vault][index].tag != 0) {
              total_count_at_eviction += count_tables[req_vault][index].count;
              evictions++;
            }
            count_tables[req_vault][index].tag = tag;
            count_tables[req_vault][index].count = 0;
          } else {
            count_tables[req_vault][index].count++;
            if(count_tables[req_vault][index].count > get_count_upper_limit()) {
              count_tables[req_vault][index].count = get_count_upper_limit();
            }
            // cout << "Updating " << addr << " in counter table. It now has count " << count_tables[req_vault][index].count << endl;
          }
          insertions++;
          if(count_tables[req_vault][index].count > maximum_count) {
            maximum_count = count_tables[req_vault][index].count;
          }
          return count_tables[req_vault][index].count;
        }
        int get_count(int req_vault, long addr) {
          int count = -1;
          uint64_t tag = calculate_tag(addr);
          int index = addr % counter_table_size;
          if(count_tables[req_vault][index].tag == tag){
            count = (int)count_tables[req_vault][index].count;
          }
          return count;
        }
        vector<CountTableEntry>& operator[](const size_t& i) {return count_tables[i];}
        void print_stats(){
          cout << "We have accessed " << insertions << " times to the counter table " << " and evicted " << evictions << " from it. The number of accesses without eviction is " << (insertions - evictions) << endl;
          cout << "The maximum count we have stored is " << maximum_count << endl;
        }
        long get_insertions()const {return insertions;}
        long get_evictions()const {return evictions;}
        long get_total_count_at_eviction()const {return total_count_at_eviction;}
        double get_avg_count_at_eviction()const {return (double)(total_count_at_eviction) / (double)(evictions);}
        int get_maximum_count()const {return maximum_count;}
        uint64_t get_count_upper_limit() const{return ((uint64_t)1 << counter_bits) - 1;}
        uint64_t get_count_lower_limit() const{return 0;}
      };
      CountTable count_table;

      // Actually dicatates when prefetch happens.
      uint64_t prefetch_hops_threshold = 5;
      uint64_t prefetch_count_threshold = 1;
      vector<int> available_thresholds = {0, 64};
      unordered_map<int, int> set_to_thresholds;
      int sample_set_size = 64;
      vector<unordered_map<int, long>> latencies_for_diff_thresholds;
      vector<unordered_map<int, int>> requests_completed_for_diff_thresholds;
      vector<unordered_map<int, long>> max_latencies_for_diff_thresholds;
      unordered_map<int, long> global_latencies_for_diff_thresholds;
      unordered_map<int, int> global_requests_completed_for_diff_thresholds;
      unordered_map<int, int64_t> global_feedbacks_for_diff_thresholds;
      uint64_t threshold_change_epoch = 100000;
      vector<int> total_unsub_epochs;
      vector<uint64_t> prefetch_count_thresholds;
      int64_t next_prefetch_count_threshold = -1;
      uint64_t prefetch_maximum_count_threshold = 0;
      // Adaptively change the above numbers as the program executes
      bool adaptive_threshold_changes = false;
      bool bimodel_adaptive_on = true;
      bool magnitude_adaptive = true;
      bool set_sampling_on = false;
      bool use_global_adaptive = false;
      bool use_maximum_latency = false;
      double update_factor = 0.9;
      long update_at = lround(threshold_change_epoch*update_factor);
      long process_latency = 1000;
      int invert_latency_variance_threshold = 40;
      vector<int64_t> feedbacks;
      int64_t global_feedback;
      vector<unordered_map<int, int64_t>> feedbacks_for_diff_thresholds;
      vector<long> latencies_for_current_epoch;
      long global_latencies;
      vector<int> requests_completed_for_current_epoch;
      long global_requests;
      vector<double> previous_latencies;
      double global_previous_latencies;
      vector<int> last_threshold_change;
      vector<long> maximum_latency_for_current_epoch;
      long feedback_bits = 20;
      // Assuming we are using a 2's comp register with given # of bits to store this info
      int64_t feedback_maximum = (long)(1<<(feedback_bits-1)) - 1;
      int64_t feedback_minimum = -(feedback_maximum+1);
      long positive_feedback_threshold = 10000;
      long negative_feedback_threshold = -10000;
      long feedback_threshold = (long)(1<<(feedback_bits-1)) / ((long)count_table.get_count_upper_limit()+1);
      long total_positive_feedback = 0;
      long total_negative_feedback = 0;
      long total_threshold_increases = 0;
      long total_threshold_decreases = 0;

      // A pointer so we can easily access Memory members
      Memory<HMC, Controller>* mem_ptr = nullptr;

      // Variables used for statistic purposes
      long total_memory_accesses = 0;
      long total_submitted_subscriptions = 0;
      long total_successful_subscriptions = 0;
      long total_unsuccessful_subscriptions = 0;
      long total_unsubscriptions = 0;
      long total_buffer_successful_insertation = 0;
      long total_buffer_unsuccessful_insertation = 0;
      long total_subscription_from_buffer = 0;
      long total_resubscriptions = 0;
      long long total_hops = 0;
      long total_subscribed_accesses = 0;
      long total_subscribed_local_accesses = 0;
      double total_in_table = 0;
      double avg_in_table_per_subscription = 0;

      // Tasks being communicated via the network
      list<SubscriptionTask> pending;
      // We can only send one packet at a time so this is to show how many flits are pending to be sent
      vector<int> pending_send;

      // Buffer to be used when the subscription table is "full". Tasks in this queue is actually at its destination
      size_t subscription_buffer_size = 32; // Anything too large may take long time (days to weeks) to execute for some benchmarks, it shouldn't be too large either as it's a fully-associative queue
      bool subscription_buffer_valid_bit_in_use = true; // EXPERIMENTAL FLAG: We have two different implementations of subscription buffer, one is a FIFO queue and only process first element, and the other is update valid bit on subscription table changes and only process valid bit = 1 entries
      class SubscriptionBuffer {
        private:
        unordered_map<long, typename list<SubscriptionTask>::iterator> map; // To ensure that we don't put two addresses in the same buffer twice
        size_t buffer_size = 32;
        list<SubscriptionTask> buffer;
        SubscriptionTable* subscription_table = nullptr;
        bool valid_bit_in_use = false;
        unordered_map<long, typename list<typename list<SubscriptionTask>::iterator>::iterator> ready_map;
        public:
        list<typename list<SubscriptionTask>::iterator> ready_tasks;
        SubscriptionBuffer(){}
        SubscriptionBuffer(size_t buffer_size, bool valid_bit_in_use):buffer_size(buffer_size), valid_bit_in_use(valid_bit_in_use){}
        bool is_free(long addr)const{return buffer.size() < buffer_size && !has(addr);}
        bool is_not_empty()const{return buffer.size() > 0;}
        bool has(long addr)const{return map.count(addr) > 0;}
        void attach_subscription_table(SubscriptionTable* table) {subscription_table = table;}
        void set_valid_bit_in_use(bool val){valid_bit_in_use = val;}
        SubscriptionTask& front(){return buffer.front();}
        void pop_front(){buffer.pop_front();}
        void update_valid_bit(long addr){
          if(!valid_bit_in_use || subscription_table == nullptr){
            return;
          }
          size_t free_set = subscription_table -> get_set(addr);
          for(auto i = buffer.begin(); i != buffer.end(); i++) {
            if(subscription_table -> get_set(i -> addr) == free_set && ready_map.count(i-> addr) == 0){
              // cout << "Push " << i -> addr << " into the ready queue" << endl;
              ready_tasks.push_back(i);
              ready_map[i -> addr] = prev(ready_tasks.end());
            }
          }
        }
        void clear_valid_bit() {
          ready_tasks.clear();
          ready_map.clear();
        }
        void erase(long addr){
          buffer.erase(map[addr]);
          map.erase(addr);
        }
        void push_back(const SubscriptionTask& task) {
          if(buffer.size() < buffer_size && !has(task.addr)){
            // cout << "Inserting address " << task.addr << " into buffer " << endl;;
            buffer.push_back(task);
            map[task.addr] = prev(end());
          }
        }
        typename list<SubscriptionTask>::iterator begin(){return buffer.begin();}
        typename list<SubscriptionTask>::iterator end(){return buffer.end();}
      };
      vector<SubscriptionBuffer> subscription_buffers;
      int controllers; // Record how many vaults we have
      // Control if we swap the subscribed address with its "mirror" address. Subscription
      // SubscriptionPrefetcherType::Swap set this to true, SubscriptionPrefetcherType::Allocate set this to false
      bool swap = false;
      int tailing_zero = 1;
      bool debug = false; // Used to controll debug dump
      bool dirty_bit_used = true; // EXPERIMENTAL FLAG - Use dirty bit that changes to true when writing to a memory location, and when a subscription table entry is not dirty it will just send a notification instead of the full package on unsubscription
    public:
      explicit SubscriptionPrefetcherSet(int controllers, Memory<HMC, Controller>* mem_ptr):controllers(controllers),mem_ptr(mem_ptr) {
        cout << "Initialize subscription table with " << controllers << " vaults." << endl;
        count_table.set_controllers(controllers);
      }
      void print_debug_info(const string& info) {
        if(debug) {
          cout << "[Subscription Debug] " << info << endl;
        }
      }
      void set_debug_flag(bool flag) {
        debug = flag;
        print_debug_info("Debug Mode On.");
      }
      vector<int> address_to_address_vector(long addr){
        addr <<= tailing_zero;
        return mem_ptr -> address_to_address_vector(addr);
      }
      long address_vector_to_address(const vector<int> addr_vec) {
        long addr = mem_ptr -> address_vector_to_address(addr_vec);
        addr >>= tailing_zero;
        return addr;
      }
      int find_original_vault_of_address(long addr){
        vector<int> addr_vec = address_to_address_vector(addr);
        // print_debug_info("The original vault of address "+to_string(addr)+" is "+to_string(addr_vec[int(HMC::Level::Vault)]));
        return addr_vec[int(HMC::Level::Vault)];
      }
      long find_victim_for_unsubscription(int vault, long addr) const {
        long victim_addr = 0;
        if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
          victim_addr = lru_units[vault].find_victim(addr); // LRU Logic
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU) {
          // cout << "We found victim " << lfu_unit.find_victim(addr) << endl;
          victim_addr = lfu_units[vault].find_victim(addr); // LFU Logic
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::DirtyLFU){
          assert(false);
        } else {
          // If our replacement policy is unknown, we fail deliberately
          assert(false);
        }
        return victim_addr;
      }
      void set_prefetch_hops_threshold(int threshold) {
        prefetch_hops_threshold = threshold;
        cout << "Prefetcher hops threshold: " << prefetch_hops_threshold << endl;
      }
      void set_prefetch_count_threshold(int threshold) {
        prefetch_count_threshold = threshold;
        cout << "Prefetcher count threshold: " << prefetch_count_threshold << endl;
      }
      void set_threshold_change_epoch(int epoch) {
        threshold_change_epoch = epoch;
        cout << "Threshold change epoch: " << threshold_change_epoch << endl;
      }
      void set_adaptive_threshold_changes(bool flag) {
        adaptive_threshold_changes = flag;
        if(adaptive_threshold_changes) {
          cout << "Adaptive Threshold Change is on" << endl;
        } else {
          cout << "Adaptive Threshold Change is off" << endl;
        }
      }
      void set_invert_latency_variance_threshold(int threshold) {
        invert_latency_variance_threshold = threshold;
        cout << "Invert latency variance threshold: " << invert_latency_variance_threshold << endl;
      }
      void set_bimodel_adaptive(bool flag) {
        bimodel_adaptive_on = flag;
        if(bimodel_adaptive_on) {
          cout << "Bimodel Adaptive is on" << endl;
        } else {
          cout << "Bimodel Adaptive is off" << endl;
        }
      }
      void set_magnitude_adaptive(bool flag) {
        magnitude_adaptive = flag;
        if(magnitude_adaptive) {
          cout << "Magnitude adaptive is on" << endl;
        } else {
          cout << "Magnitude adaptive is off" << endl;
        }
      }
      void set_set_sampling(bool flag) {
        set_sampling_on = flag;
        if(set_sampling_on) {
          cout << "Set Sampling is on" << endl;
        } else {
          cout << "Set Sampling is off" << endl;
        }
      }
      void set_use_global_adaptive(bool flag) {
        use_global_adaptive = flag;
        if(use_global_adaptive) {
          cout << "Global adaptive is on" << endl;
        } else {
          cout << "Global adaptive is off" << endl;
        }
      }
      bool get_use_global_adaptive() const{return use_global_adaptive;}
      void set_use_maximum_latency(bool flag) {
        use_maximum_latency = flag;
        if(use_maximum_latency) {
          cout << "Using Maximum Latency" << endl;
        } else {
          cout << "Using Average Latency" << endl;
        }
      }
      void set_update_factor(double factor) {
        update_factor = factor;
        update_at = lround(threshold_change_epoch*update_factor);
        cout << "Update factor: " << update_factor << ". we will update at " << update_at << " cycle after the last epoch." << endl;
      }
      void set_process_latency(long latency) {
        process_latency = latency;
        cout << "Process latency: " << process_latency << endl;
      }
      void set_positive_feedback_threshold(long threshold) {
        // Ignore threshold if the flag is turned off
        if(adaptive_threshold_changes) {
          assert(threshold >= 0);
          positive_feedback_threshold = threshold;
          cout << "Setting Positive Feedback Threshold as " << positive_feedback_threshold << endl;
        }
      }
      void set_negative_feedback_threshold(long threshold) {
        // Ignore threshold if the flag is turned off
        if(adaptive_threshold_changes) {
          assert(threshold <= 0);
          negative_feedback_threshold = threshold;
          cout << "Setting Negative Feedback Threshold as " << negative_feedback_threshold << endl;
        }
      }
      void set_subscription_table_size(size_t size) {
        if(subscription_table_ways == SIZE_MAX){
          subscription_table_ways = size;
        }
        subscription_table_size = size;
      }
      void set_subscription_table_ways(size_t ways) {subscription_table_ways = ways;}
      void set_subscription_buffer_size(size_t size) {subscription_buffer_size = size;}
      void set_subscription_recv_buffer_size(size_t size) {receiving_buffer_size = size;}
      void set_subscription_table_replacement_policy(const string& policy) {
        subscription_table_replacement_policy = name_to_prefetcher_rp[policy];
        cout << "Subscription table replacement policy is: " << policy << endl;
      }
      void set_counter_table_size(size_t size) {count_table.set_counter_table_size(size);}
      void set_counter_bits(int bits) {count_table.set_counter_bits(bits);}
      void set_tag_bits(int bits) {count_table.set_tag_bits(bits);}
      void set_swap_switch(bool val) {swap = val;}
      size_t get_set(const Request& req) {
        long addr = req.addr;
        pre_process_addr(addr);
        return subscription_tables[0].get_set(addr);
      }
      void initialize_sets(){
        subscription_tables.assign(controllers, SubscriptionTable(subscription_table_size, subscription_table_ways, SIZE_MAX, mem_ptr));
        for(int i = 0; i < controllers; i++) {
          subscription_tables[i].set_controller(i);
        }
        subscription_table_sets = subscription_table_size / subscription_table_ways;
        count_table.initialize();
        if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
          lru_units.assign(controllers, LRUUnit(subscription_table_sets));
          for(int c = 0; c < controllers; c++) {
            subscription_tables[c].attach_lru_unit(&lru_units[c]);
          }
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU) {
          lfu_units.assign(controllers, LFUUnit(subscription_table_sets));
          for(int c = 0; c < controllers; c++) {
            subscription_tables[c].attach_lfu_unit(&lfu_units[c]);
          }
        } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::DirtyLFU) {
          cout << "DirtyLFU is no longer supported due to it causing starving in the buffer" << endl;
          assert(false);
        } else {
          cout << "Unknown replacement policy!" << endl;
          assert(false); // We fail early if the policy is not known.
        }
        cout << "Subscription buffer size: " << subscription_buffer_size << endl;
        subscription_buffers.assign(controllers, SubscriptionBuffer(subscription_buffer_size, subscription_buffer_valid_bit_in_use));
        for(int c = 0; c < controllers; c++) {
          subscription_tables[c].attach_subscription_buffer(&subscription_buffers[c]);
          subscription_buffers[c].attach_subscription_table(&subscription_tables[c]);
        }
        global_feedback = 0;
        global_latencies = 0;
        global_previous_latencies = 0.0;
        global_requests = 0;
        pending_send.assign(controllers, 0);
        feedbacks.assign(controllers, 0);
        latencies_for_current_epoch.assign(controllers, 0);
        requests_completed_for_current_epoch.assign(controllers, 0);
        previous_latencies.assign(controllers, 0.0);
        prefetch_count_thresholds.assign(controllers, prefetch_count_threshold);
        total_unsub_epochs.assign(controllers, 0);
        last_threshold_change.assign(controllers, -1);
        maximum_latency_for_current_epoch.assign(controllers, 0);
        latencies_for_diff_thresholds.assign(controllers, unordered_map<int, long>());
        requests_completed_for_diff_thresholds.assign(controllers, unordered_map<int, int>());
        max_latencies_for_diff_thresholds.assign(controllers, unordered_map<int, long>());
        feedbacks_for_diff_thresholds.assign(controllers, unordered_map<int, int64_t>());
        for(int c = 0; c < controllers; c++) {
          subscription_tables[c].propogate_count_threshold(prefetch_count_threshold);
        }
        feedback_threshold = (long)(1<<(feedback_bits-1)) / ((long)count_table.get_count_upper_limit()+1);
        int sampling_set = subscription_table_sets / 8;
        // int sampling_set = 0;
        for(auto threshold:available_thresholds) {
          for(int i = 0; i < sample_set_size; i++) {
            set_to_thresholds[sampling_set] = threshold;
            sampling_set++;
          }
        }

        if(set_sampling_on) {
          mem_ptr -> set_sampling_result << "Epoch End Cycle,";
          for(int i = 0; i < controllers; i++) {
            for(auto threshold:available_thresholds) {
              mem_ptr -> set_sampling_result << "Core " << i << " Threshold " << threshold << ",";
            }
          }
          mem_ptr -> set_sampling_result << "\n";
        }
      }

      bool check_prefetch(uint64_t hops, uint64_t count, int vault, vector<int> addr_vec) {
        if(set_sampling_on) {
          long addr = address_vector_to_address(addr_vec);
          int set = subscription_tables[0].get_set(addr);
          if(set_to_thresholds.count(set) > 0) {
            // cout << "Set " << set << " is a leader set which has threshold " << set_to_thresholds.at(set) << endl;
            return hops >= prefetch_hops_threshold && count >= set_to_thresholds.at(set);
          }
        }
        return hops >= prefetch_hops_threshold && count >= prefetch_count_thresholds[vault];
      }
      long find_mirror_address(int vault, long addr) {
        vector<int> victim_vec = address_to_address_vector(addr);
        victim_vec[int(HMC::Level::Vault)] = vault;
        long victim_addr = address_vector_to_address(victim_vec);
        return victim_addr;
      }
      // Start the entire process by allocating subscription table locally, and push a subscription request into the network or buffer
      void subscribe_address(int req_vault, long addr) {
        // Starting by reserving space in the subscription table. For swap we need 2 entries (one for the actual subscription, one for the swapped out address)
        int required_space = swap ? 2 : 1;
        // Calculate vaules needed for next steps
        int original_vault = find_original_vault_of_address(addr);
        int hops = mem_ptr -> calculate_hops_travelled(req_vault, original_vault);
        // Generate a "Subscription Request" task
        int count = count_table.get_count(req_vault, addr);
        // cout << "Triggering subscription for address " << addr << " from vault " << req_vault << " to " << original_vault << " with count " << count << endl;
        SubscriptionTask task = SubscriptionTask(addr, req_vault, original_vault, hops, SubscriptionTask::Type::SubReq, count);
        // cout << "SubReq task generated from " << task.from_vault << " to " << task.to_vault << " addr " << task.addr << endl;
        // If the original vault of the address is the current vault (i.e. we have an address subscribed elsewhere and we want it back), we need to process unsubscription
        if(original_vault == req_vault) {
          // cout << "addr " << addr << " is currently in the same to vault as subscribe vault" << endl;
          unsubscribe_address(req_vault, addr);
        // If we have space to insert it into the subscription table and to receive the data, we push it into the network
        } else if(subscription_tables[req_vault].receive_buffer_is_free()
          && subscription_tables[req_vault].subscription_table_is_free(addr, required_space)){
          // cout << "we push address " << addr << " into the network" << endl;
          push_subscribe_request_into_network(task);
        // Otherwise, we push it into the subscription buffer of the requesting vault, pending unsubscription that frees up the table
        } else {
          // cout << "either the receive buffer is full or subscription table is full. We wait..." << endl;  
          // If this failure of pushing into network is due to lack of subscription table space, we try to make some space
          if(!subscription_tables[req_vault].subscription_table_is_free(addr, required_space)) {
            // We find a victim address to free up subscription table
            long victim_addr = find_victim_for_unsubscription(req_vault, addr);
            // We then submit the address for unsubscribe
            unsubscribe_address(req_vault, victim_addr);
          }
          // If we have space, we insert it into the buffer
          if(subscription_buffers[req_vault].is_free(task.addr)){
            total_buffer_successful_insertation++;
            print_debug_info("Pushing task "+to_string(task.addr)+" from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" into the buffer at sender");
            subscription_buffers[req_vault].push_back(task);
          // Otherwise, there is nothing we can do
          } else {
            total_buffer_unsuccessful_insertation++;
          }
        }
      }
      // Finish pushing the subscription request into the network
      void push_subscribe_request_into_network(const SubscriptionTask& input_task) {
        assert(input_task.type == SubscriptionTask::Type::SubReq);
        // Make a copy for modification
        SubscriptionTask task = input_task;
        // We then insert the subscription request into the table, and allocate space in receiving buffer for receiving
        print_debug_info("Submitting address "+to_string(task.addr)+" into "+to_string(task.from_vault)+" to "+to_string(task.to_vault));
        subscription_tables[task.from_vault].submit_subscription(task.from_vault, task.addr);
        subscription_tables[task.from_vault].start_receiving();
        print_debug_info("After submission, entry "+to_string(task.addr)+" exists in from vault? "+to_string(subscription_tables[task.from_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.from_vault].get_status(task.addr))+" To which vault? "+to_string(subscription_tables[task.from_vault][task.addr]));
        print_debug_info("After submission, entry "+to_string(task.addr)+" exists in to vault? "+to_string(subscription_tables[task.to_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.to_vault].get_status(task.addr)));
        // Actually put the request into the network
        print_debug_info("We are pushing subscription task from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" with addr "+to_string(task.addr));
        // Only count actual data sent without network latencies
        total_hops += task.from_buffer ? task.hops : 0;
        task.hops += pending_send[task.from_vault];
        pending.push_back(task);
        pending_send[task.from_vault]+=1;
        total_submitted_subscriptions++;
        // If we want to swap, we reserve the space for swapped out address as well
        if(swap) {
          long mirror_addr = find_mirror_address(task.from_vault, task.addr);
          subscription_tables[task.from_vault].submit_subscription(task.to_vault, mirror_addr);
        }
      }
      // Process received subscription request. Start data transfer or push it into the buffer or return failure
      void receive_subscribe_request(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubReq);
        // Starting by reserving space in the subscription table. For swap we need 2 entries (one for the actual subscription, one for the swapped out address)
        // If the address is already subscribed (to somewhere else), we do not need any space for it as we can use the existing entry
        int required_space = subscription_tables[task.to_vault].is_subscribed(task.addr) ? (swap ? 1 : 0) : (swap ? 2 : 1);
        // If we have space in subscription table to put it in, and receiving buffer to receive the swapped out address in the case of swap, we proceed with subscription
        if(subscription_tables[task.to_vault].subscription_table_is_free(task.addr, required_space)
          && subscription_tables[task.to_vault].receive_buffer_is_free()) {
          process_subscribe_request(task);
        // If not, but we have some space in the buffer, we insert it into the buffer
        } else {
          if(subscription_buffers[task.to_vault].is_free(task.addr)) {
            total_buffer_successful_insertation++;
            print_debug_info("Pushing task "+to_string(task.addr)+" from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" into the buffer at receiver");
            subscription_buffers[task.to_vault].push_back(task);
          // Otherwise, there is nothing we can do
          } else {
            total_buffer_unsuccessful_insertation++;
            print_debug_info("We are pushing SubReqNack task from "+to_string(task.to_vault)+" to "+to_string(task.from_vault)+" with addr "+to_string(task.addr));
            int hops = mem_ptr -> calculate_hops_travelled(task.to_vault, task.from_vault);
            total_hops += task.from_buffer ? hops : 0;
            pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops+pending_send[task.to_vault], SubscriptionTask::Type::SubReqNAck, task.dirty, task.from_buffer));
            pending_send[task.to_vault]+=1;
          }
          if(!subscription_tables[task.to_vault].subscription_table_is_free(task.addr, required_space)
            || !subscription_tables[task.to_vault].receive_buffer_is_free()) {
            long victim_addr = find_victim_for_unsubscription(task.to_vault, task.addr);
            unsubscribe_address(task.to_vault, victim_addr);
          }
        }
      }
      // Finish processing subscription request
      void process_subscribe_request(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubReq);
        // We calculate how many hops it required to transfer the data
        int hops = mem_ptr -> calculate_hops_travelled(task.to_vault, task.from_vault);
        // If this address is not currently subscribed anywhere, we insert the subscription request into the table, and allocate space in receiving buffer for receiving swapped out address
        if(!subscription_tables[task.to_vault].has(task.addr)){
          print_debug_info("Submitting address "+to_string(task.addr)+" from "+to_string(task.from_vault)+" into "+to_string(task.to_vault));
          subscription_tables[task.to_vault].submit_subscription(task.from_vault, task.addr);
          print_debug_info("After submission, entry "+to_string(task.addr)+" exists in from vault? "+to_string(subscription_tables[task.from_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.from_vault].get_status(task.addr)));
          print_debug_info("After submission, entry "+to_string(task.addr)+" exists in to vault? "+to_string(subscription_tables[task.to_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.to_vault].get_status(task.addr))+" To which vault? "+to_string(subscription_tables[task.to_vault][task.addr]));
          // We send the acknowledgement and data through the network
          print_debug_info("We are pushing SubReqAck and SubXfer task from "+to_string(task.to_vault)+" to "+to_string(task.from_vault)+" with addr "+to_string(task.addr));
          total_hops += task.from_buffer ? hops*DATA_LENGTH : 0;
          // SubReqAck and SubReqXfer are one in real case
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops+pending_send[task.to_vault], SubscriptionTask::Type::SubReqAck));
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops*DATA_LENGTH+pending_send[task.to_vault], SubscriptionTask::Type::SubXfer, task.dirty, task.from_buffer));
          pending_send[task.to_vault]+=DATA_LENGTH;
          // If swap is on, we compute the "mirror" address and swap it
          if(swap) {
            long mirror_addr = find_mirror_address(task.from_vault, task.addr);
            subscription_tables[task.to_vault].submit_subscription(task.to_vault, mirror_addr);
            subscription_tables[task.to_vault].start_receiving();
          }
        // If it is already subscribed, and not in the process of removal or subscription, we submit re-subscription request to ask the current subscribed vault to send it
        } else {
          if(subscription_tables[task.to_vault].is_subscribed(task.addr)){
            int value_vault = subscription_tables[task.to_vault][task.addr];
            int to_vaule_hops = mem_ptr -> calculate_hops_travelled(task.from_vault, value_vault);
            // We send acknowledgement to the requester, and ask the vault vault to send the vaule back to the requester
            print_debug_info("We are pushing ResubReq task from "+to_string(task.from_vault)+" to "+to_string(value_vault)+" with addr "+to_string(task.addr));
            total_hops += task.from_buffer ? to_vaule_hops : 0;
            pending.push_back(SubscriptionTask(task.addr, task.from_vault, value_vault, to_vaule_hops+pending_send[task.to_vault], SubscriptionTask::Type::ResubReq, task.dirty, task.from_buffer));
            pending_send[task.to_vault]+=1;
            if(swap) {
              // In the case of swap, we also return the swapped out data of the original subscriber as it is no longer needed
              long mirror_addr = find_mirror_address(value_vault, task.addr);
              pending.push_back(SubscriptionTask(task.addr, task.to_vault, value_vault, hops*DATA_LENGTH+pending_send[task.to_vault], SubscriptionTask::Type::UnsubXfer));
            }
          // If it is in the process of removal or subscription and we are not in the process of subscribing from or removing to the requester vault, we cannot really subscribe it
          } else if(!subscription_tables[task.to_vault].is_subscribed(task.addr) && subscription_tables[task.to_vault][task.addr] != task.from_vault) {
            print_debug_info("We are pushing SubReqNAck task from "+to_string(task.to_vault)+" to "+to_string(task.from_vault)+" with addr "+to_string(task.addr));
            total_hops += task.from_buffer ? hops : 0;
            pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops+pending_send[task.to_vault], SubscriptionTask::Type::SubReqNAck, task.dirty, task.from_buffer));
            pending_send[task.to_vault]+=1;
          }
        }
      }
      // Used only when swap is set as true. For find swapout data and actually swap it out
      void start_mirror_subscription_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubReqAck);
        if(swap) {
          long mirror_addr = find_mirror_address(task.to_vault, task.addr);
          int hops = mem_ptr -> calculate_hops_travelled(task.to_vault, task.from_vault, WRITE_LENGTH);
          total_hops += hops;
          pending.push_back(SubscriptionTask(mirror_addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::SubXfer));
        }
      }
      // Upon receiving negative acknowledgement, rollback the pending subscription
      void subscription_rollback(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubReqNAck);
        print_debug_info("Rollback address "+to_string(task.addr));
        subscription_tables[task.to_vault].rollback_subscription(task.addr);
        subscription_tables[task.to_vault].stop_receiving();
        total_unsuccessful_subscriptions++;
      }
      // Upon receiving transferred data, make the subscription final and provide acknowledgement (for client side)
      void receive_subscription_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubXfer || task.type == SubscriptionTask::Type::ResubXfer);
        if(!subscription_tables[task.to_vault].has(task.addr)) {
          print_debug_info("Subscription of "+to_string(task.addr)+" is interrupted due to it being unsubscribed before finishing");
          return;
        } else if(subscription_tables[task.to_vault].is_subscribed(task.addr)) {
          print_debug_info("Subscription of "+to_string(task.addr)+" is already completed.");
          return;
        }
        // Copy the status into a boolean vaule as it would be overriden upon complete_subscription() call
        bool is_pending_removal = subscription_tables[task.to_vault].is_pending_removal(task.addr);
        // Mark the local entry of the subscription table as completed and free up the receiving buffer
        print_debug_info("Complete subscription "+to_string(task.addr)+" from "+to_string(task.from_vault)+" into "+to_string(task.to_vault)+" its current status is "+to_string(subscription_tables[task.to_vault].get_status(task.addr)));
        subscription_tables[task.to_vault].complete_subscription(task.addr);
        print_debug_info("After completion, entry "+to_string(task.addr)+" exists in from vault? "+to_string(subscription_tables[task.from_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.from_vault].get_status(task.addr)));
        print_debug_info("After completion, entry "+to_string(task.addr)+" exists in to vault? "+to_string(subscription_tables[task.to_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.to_vault].get_status(task.addr)));
        subscription_tables[task.to_vault].stop_receiving();
        // Calculate the hops to original and from vault (they are not the same in the case of resubscription)
        int original_vault = find_original_vault_of_address(task.addr);
        int original_vault_hops = mem_ptr -> calculate_hops_travelled(original_vault, task.to_vault);
        int hops = mem_ptr -> calculate_hops_travelled(task.to_vault, task.from_vault);
        // Send the acknowledgement to the original vault so it can update its entry accordingly
        print_debug_info("We are pushing SubXferAck task from "+to_string(task.to_vault)+" to "+to_string(original_vault)+" with addr "+to_string(task.addr));
        if(is_pending_removal) {
          unsubscribe_address(task.to_vault, task.addr);
        } else {
          total_hops += task.from_buffer ? original_vault_hops : 0;
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, original_vault, original_vault_hops+pending_send[task.to_vault], SubscriptionTask::Type::SubXferAck, task.dirty, task.from_buffer));
          pending_send[task.to_vault]+=1;
        }
        // If it is resubscription, we also need to notify the from vault (where the data is actually from) to ensure that it can remove that entry from its table
        if(task.type == SubscriptionTask::Type::ResubXfer) {
          if(task.from_buffer) {
            total_hops += hops > original_vault_hops ? hops - original_vault_hops : 0;
          }
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops+pending_send[task.to_vault], SubscriptionTask::Type::ResubXferAck, task.dirty, task.from_buffer));
          if(task.dirty) {
            subscription_tables[task.to_vault].set_dirty(task.addr);
          }
        }
        total_successful_subscriptions++;
      }
      // Upon receiving acknowledgement, make the subscription final/process resubscription based on the reply (for value side)
      void finish_subscription_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::SubXferAck);
        // If we already have this address subscribed (which is resubscription), we modify existing subscription entry to re-direct the 
        if(subscription_tables[task.to_vault].is_subscribed(task.addr)) {
          subscription_tables[task.to_vault].modify_subscription(task.from_vault, task.addr);
        // If not, we change it to completed
        } else if(subscription_tables[task.to_vault].is_pending_subscription(task.addr)){
          print_debug_info("Complete subscription "+to_string(task.addr)+" from "+to_string(task.from_vault)+" into "+to_string(task.to_vault));
          subscription_tables[task.to_vault].complete_subscription(task.addr);
          print_debug_info("After completion, entry "+to_string(task.addr)+" exists in from vault? "+to_string(subscription_tables[task.from_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.from_vault].get_status(task.addr)));
          print_debug_info("After completion, entry "+to_string(task.addr)+" exists in to vault? "+to_string(subscription_tables[task.to_vault].has(task.addr))+" status? "+to_string(subscription_tables[task.to_vault].get_status(task.addr)));
        }
      }
      // Start the unsubscription process by determining if the unsubscription is made by the holder of the address, and act accordingly
      void unsubscribe_address(int caller_vault, long addr) {
        if(!subscription_tables[caller_vault].has(addr)) {
          cout << ("We have addr "+to_string(addr)+" for subscription from vault "+to_string(caller_vault)+" but we do not have it");
        }
        assert(subscription_tables[caller_vault].has(addr));
        // Find out where the addressis currently at and where its original vault is
        int current_vault = subscription_tables[caller_vault][addr];
        int original_vault = find_original_vault_of_address(addr);
        // The from_vault might be either the original vault or current vault, so we need to calculate both the hops from from vault to current vault
        // And from from vault to original vault. One of those hop counts will be 0
        int hops = mem_ptr -> calculate_hops_travelled(caller_vault, current_vault);
        int reverse_hops = mem_ptr -> calculate_hops_travelled(caller_vault, original_vault);
        SubscriptionTask task = SubscriptionTask(addr, caller_vault, current_vault, hops, SubscriptionTask::Type::UnsubReq);
        // If we are calling from the original vault, we need to send UnsubReq through the network to the current vault so it can send data back
        if(caller_vault == original_vault) {
          print_debug_info("We are pushing UnsubReq task from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" with addr "+to_string(task.addr));
          total_hops += task.hops;
          task.hops+=pending_send[caller_vault];
          pending.push_back(task);
          pending_send[caller_vault]+=1;
          // But at the same time, we also send the swapped out data back to its original vault
          if(swap) {
            long mirror_addr = find_mirror_address(addr, current_vault);
            process_unsubscribe_request(SubscriptionTask(mirror_addr, caller_vault, original_vault, reverse_hops, SubscriptionTask::Type::UnsubReq));
          }
        // If we're calling from the current vault, we can process the unsubscription immediately by sending the data back to its original vault
        } else if(caller_vault == current_vault) {
          process_unsubscribe_request(task);
          // And we also request swapped vault back
          if(swap) {
            total_hops += reverse_hops;
            long mirror_addr = find_mirror_address(addr, original_vault);
            pending.push_back(SubscriptionTask(mirror_addr, caller_vault, original_vault, reverse_hops, SubscriptionTask::Type::UnsubReq));
          }
        // When we are dealing with resubscription, the current vault is set to the future subscribed vault. We do not need to do anything as it would unsubscribe in a few cycles
        } else if(!(subscription_tables[caller_vault].is_pending_resubscription(addr) && caller_vault != original_vault)) {
          cout << "we are unsubscribing " << to_string(task.addr) << " with original vault " << to_string(original_vault)+" and vault " << to_string(current_vault) << " this is requested by " << to_string(caller_vault) << " and the requester is neither original or the subscribed vault";
          assert(false); // The vault requesting unsubscription should either be the original vault or current vault
        }
      }
      // Actually process the unsubscription process (either by receiving UnsubReq or by starting unsubscription from current vault)
      void process_unsubscribe_request(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::UnsubReq);
        int original_vault = find_original_vault_of_address(task.addr);
        // If the address is currently subscribed or pending subscription, we just start unsubscription
        if(subscription_tables[task.to_vault].is_subscribed(task.addr)) {
          // We first mark the subscription table as "pending removal"
          subscription_tables[task.to_vault].submit_unsubscription(task.addr);
          // We again find the original vault of the address to send the data back
          // The to_vault will always be the "current vault" holding the address
          int hops = mem_ptr -> calculate_hops_travelled(task.to_vault, original_vault);
          // It seems we do not need to send acknowledgement in the case of unsubscription request
          // if(task.to_vault != task.from_vault) {
          //   pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::UnsubReqAck));
          // }
          // Last, we actually send the data back to the original vault
          int transfer_length = hops*DATA_LENGTH;
          if(dirty_bit_used) {
            transfer_length = subscription_tables[task.to_vault].is_dirty(task.addr) ? hops*DATA_LENGTH : hops;
          }
          print_debug_info("We are pushing UnsubXfer task from "+to_string(task.to_vault)+" to "+to_string(original_vault)+" with addr "+to_string(task.addr));
          total_hops += transfer_length;
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, original_vault, transfer_length+pending_send[task.to_vault], SubscriptionTask::Type::UnsubXfer));
          pending_send[task.to_vault]+=transfer_length/hops;
        // If it is pending subscription, we mark it as pending unsubscription pending the transfer of the data
        } else if(subscription_tables[task.to_vault].is_pending_subscription(task.addr)){
          subscription_tables[task.to_vault].submit_unsubscription(task.addr);
        // If it is the address is currently being resubscribed (i.e. moved from this vault to another vault)
        // We do nothing in the case of local unsubscription (requested by current vault) and forward the subscription request to the new location if it is remote unsubscription (requested by the original vault)
        } else if(subscription_tables[task.to_vault].is_pending_resubscription(task.addr)
            && task.from_vault == original_vault) {
          int future_subscribed_vault = subscription_tables[task.to_vault][task.addr];
          int hops = mem_ptr -> calculate_hops_travelled(task.to_vault, future_subscribed_vault);
          print_debug_info("We are pushing UnsubReq task from "+to_string(original_vault)+" to "+to_string(future_subscribed_vault)+" with addr "+to_string(task.addr));
          // We deliberately delay the unsubscription request until the resubscription likely completes
          total_hops += hops;
          pending.push_back(SubscriptionTask(task.addr, original_vault, future_subscribed_vault, hops+pending_send[task.to_vault], SubscriptionTask::Type::UnsubReq));
          pending_send[task.to_vault]+=1;
        } else {
          print_debug_info("We are unable to unsubscribe address "+to_string(task.addr)+" from "+to_string(task.from_vault)+" to "+to_string(task.to_vault));
          print_debug_info("Do we have this address "+to_string(subscription_tables[task.to_vault].has(task.addr)));
          print_debug_info("Its status: "+to_string(subscription_tables[task.to_vault].get_status(task.addr)));
        }
      }
      // Process unsubscription data transfer
      void process_unsubscribe_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::UnsubXfer);
        // Then we complete unsubscription
        int hops = mem_ptr -> calculate_hops_travelled(task.to_vault, task.from_vault);
        print_debug_info("Complete unsubscription of address "+to_string(task.addr));
        subscription_tables[task.to_vault].complete_unsubscription(task.addr);
        // And send an acknowledgement back to the "current vault" so it knows it can safely erase this entry from the subscription table
        total_hops += hops;
        pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops+pending_send[task.to_vault], SubscriptionTask::Type::UnsubXferAck));
        pending_send[task.to_vault]+=1;
      }
      // Completes unsubscription data transfer
      void complete_unsubscribe_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::UnsubXferAck);
        // Then update subscription table
        print_debug_info("Complete unsubscription of address "+to_string(task.addr));
        subscription_tables[task.to_vault].complete_unsubscription(task.addr);
        total_unsubscriptions++;
      }
      // Process resubscription request
      void process_resubscription_request(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::ResubReq);
        int hops = mem_ptr -> calculate_hops_travelled(task.to_vault, task.from_vault);
        // If this entry is anything other than subscribed, we cannot resubscribe it, so we negatively acknowledgement this resubscription request to ask the sender to try later
        if(!subscription_tables[task.to_vault].is_subscribed(task.addr)) {
          total_hops += task.from_buffer ? hops : 0;
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops+pending_send[task.to_vault], SubscriptionTask::Type::SubReqNAck, task.dirty, task.from_buffer));
          pending_send[task.to_vault]+=1;
        // Otherwise, we can transfer the data to the current requester
        } else {
          subscription_tables[task.to_vault].submit_resubscription(task.from_vault, task.addr);
          total_hops += task.from_buffer ? hops*DATA_LENGTH : 0;
          // These two are one in real hardware
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops, SubscriptionTask::Type::SubReqAck));
          pending.push_back(SubscriptionTask(task.addr, task.to_vault, task.from_vault, hops*DATA_LENGTH+pending_send[task.to_vault], SubscriptionTask::Type::ResubXfer, subscription_tables[task.to_vault].is_dirty(task.addr), task.from_buffer));
          pending_send[task.to_vault]+=DATA_LENGTH;
        }
      }
      // Finish resubscription request
      void finish_resubscription_transfer(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::ResubXferAck);
        // Upon receiving Resubscription transfer acknowledgement, we can savely remove this entry from our subscription table as it is no longer needed
        print_debug_info("Complete unsubscription of address "+to_string(task.addr));
        subscription_tables[task.to_vault].complete_resubscription(task.addr);
        total_resubscriptions++;
      }
      // Go into the buffer and check if we can process the subscription request, and if so, completes such request
      bool process_task_from_buffer(const SubscriptionTask& task) {
        // We first calculate the required space if we are inserting it
        int required_space = swap ? 2 : 1;
        // If hops is not 0, it means this task is from the sender's end
        if(task.hops != 0) {
          // If this task is already completed and the entry is there, we do nothing and ask the call stack to remove this task
          if(subscription_tables[task.from_vault].is_subscribed(task.addr, task.from_vault)
            || subscription_tables[task.from_vault].is_pending_subscription(task.addr, task.from_vault)
            || subscription_tables[task.from_vault].is_pending_removal(task.addr, task.from_vault)) {
            return true;
          } else if(subscription_tables[task.from_vault].is_pending_subscription(task.addr)
            || subscription_tables[task.from_vault].is_pending_removal(task.addr) 
            || subscription_tables[task.from_vault].is_pending_resubscription(task.addr)){
            return false;
          // Otherwise, we see if we can insert this task into the subscription table
          } else if(subscription_tables[task.from_vault].receive_buffer_is_free()
            && subscription_tables[task.from_vault].subscription_table_is_free(task.addr, required_space)) {
            print_debug_info("Processing task addr "+to_string(task.addr)+" from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" with hop "+to_string(task.hops));
            // Then we insert it
            SubscriptionTask new_task = task;
            new_task.from_buffer = true;
            push_subscribe_request_into_network(new_task);
            // And ask the it to be removed
            return true;
          }
        // Otherwise, it is in the receiver's end
        } else if(task.hops == 0) {
          // If the address is already subscribed (to somewhere else), we do not need any space for it as we can use the existing entry
          required_space = subscription_tables[task.to_vault].is_subscribed(task.addr) ? (swap ? 1 : 0) : (swap ? 2 : 1);
          // Same, check if it is completed
          if(subscription_tables[task.to_vault].is_subscribed(task.addr, task.from_vault)
            || subscription_tables[task.to_vault].is_pending_subscription(task.addr, task.from_vault)
            || subscription_tables[task.to_vault].is_pending_removal(task.addr, task.from_vault)) {
            return true;
          }
          else if(subscription_tables[task.to_vault].is_pending_resubscription(task.addr)
            || subscription_tables[task.to_vault].is_pending_subscription(task.addr)
            || subscription_tables[task.to_vault].is_pending_removal(task.addr)) {
            return false;
          }
          // Check for insertion condition and insert
          else if(subscription_tables[task.to_vault].subscription_table_is_free(task.addr, required_space) 
            && subscription_tables[task.to_vault].receive_buffer_is_free()) {
            print_debug_info("Processing task addr "+to_string(task.addr)+" from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" with hop "+to_string(task.hops));
            SubscriptionTask new_task = task;
            new_task.from_buffer = true;
            process_subscribe_request(new_task);
            return true;
          }
        }
        return false;
      }
      // Process tasks currently in the network depends on the request type
      void process_task_from_network(const SubscriptionTask& task) {
        switch(task.type){
          case SubscriptionTask::Type::SubReq:
            print_debug_info("Recv SubReq from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            receive_subscribe_request(task);
            break;
          case SubscriptionTask::Type::SubReqAck:
            print_debug_info("Recv SubReqAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            start_mirror_subscription_transfer(task);
            break;
          case SubscriptionTask::Type::SubReqNAck:
            print_debug_info("Recv SubReqNAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            subscription_rollback(task);
            break;
          case SubscriptionTask::Type::SubXfer:
            print_debug_info("Recv SubXfer from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            receive_subscription_transfer(task);
            break;
          case SubscriptionTask::Type::SubXferAck:
            print_debug_info("Recv SubXferAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            finish_subscription_transfer(task);
            break;
          case SubscriptionTask::Type::UnsubReq:
            print_debug_info("Recv UnsubReq from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            process_unsubscribe_request(task);
            break;
          case SubscriptionTask::Type::UnsubReqAck:
            print_debug_info("Recv UnsubReqAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
          // It seems we do not need to do anything in the case of unsubscription request acknowledgement
            break;
          case SubscriptionTask::Type::UnsubXfer:
            print_debug_info("Recv UnsubXfer from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            process_unsubscribe_transfer(task);
            break;
          case SubscriptionTask::Type::UnsubXferAck:
            print_debug_info("Recv UnsubXferAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            complete_unsubscribe_transfer(task);
            break;
          case SubscriptionTask::Type::ResubReq:
            print_debug_info("Recv ResubReq from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            process_resubscription_request(task);
            break;
          case SubscriptionTask::Type::ResubXfer:
            print_debug_info("Recv ResubXfer from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            receive_subscription_transfer(task);
            break;
          case SubscriptionTask::Type::ResubXferAck:
            print_debug_info("Recv ResubXferAck from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            finish_resubscription_transfer(task);
            break;
          case SubscriptionTask::Type::UpdateOnWrite:
            print_debug_info("Recv UpdateOnWrite from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            update_on_write(task);
            break;
          case SubscriptionTask::Type::IncThreshold:
            print_debug_info("Recv IncThreshold from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            process_increase_threshold(task);
            break;
          case SubscriptionTask::Type::DecThreshold:
            print_debug_info("Recv DecThreshold from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            process_decrease_threshold(task);
            break;
          case SubscriptionTask::Type::UpdateAdap:
            print_debug_info("Recv UpdateAdap from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            process_update_adaptive(task);
            break;
          case SubscriptionTask::Type::UpdateThreshold:
            print_debug_info("Recv UpdateThreshold from "+to_string(task.from_vault)+" to "+to_string(task.to_vault)+" addr "+to_string(task.addr));
            process_update_threshold(task);
            break;
          default:
            cout << "Unknown subscription task type " << task.type << endl;
            assert(false);
        }
      }

      void tick() {
        // First, we check if there is any subscription buffer in pending (i.e. arrived but cannot be subscribed due to subscription table space constraints)
        for(int c = 0; c < controllers; c++){
          if(subscription_buffers[c].is_not_empty()) {
            if(subscription_buffer_valid_bit_in_use) {
              for(auto i:subscription_buffers[c].ready_tasks) {
                SubscriptionTask task = *i;
                print_debug_info("Before processing task"+to_string(task.addr)); 
                if(process_task_from_buffer(task)) {
                  print_debug_info("Try erase task "+to_string(task.addr));
                  subscription_buffers[c].erase(task.addr);
                  total_subscription_from_buffer++;
                }
              }
              subscription_buffers[c].clear_valid_bit();  
            } else {
              SubscriptionTask task = subscription_buffers[c].front();
              if(process_task_from_buffer(task)) {
                subscription_buffers[c].pop_front();
                total_subscription_from_buffer++;
              }
            }
          }
        }
        // Then, we process the transfer of subscription requests in the network
        list<SubscriptionTask> new_pending;
        for(auto& i:pending) {
          if(i.hops == 0) {
            process_task_from_network(i);
          } else {
            i.hops -= 1;
            new_pending.push_back(i);
          }
        }
        pending = new_pending;
        for(int c = 0; c < controllers; c++){
          if(pending_send[c] > 0) {
            pending_send[c]--;
          }
        }
        
        // For global adaptive only - every vault send the stats to the central vault for processing
        if(mem_ptr -> clk % threshold_change_epoch == update_at && adaptive_threshold_changes && use_global_adaptive) {
          for(int c = 0; c < controllers; c++) {
            submit_update_adaptive(c);
          }
        }

        // Finally, we try to process threshold changes
        if(mem_ptr -> clk % threshold_change_epoch == 0 && mem_ptr -> clk > 0) {
          mem_ptr -> adaptive_thresholds << mem_ptr -> clk << ",";
          mem_ptr -> latencies_each_epoch << mem_ptr -> clk << ",";
          mem_ptr -> feedback_reg_epoch << mem_ptr -> clk << ",";
          mem_ptr -> adaptive_thresholds_record << mem_ptr -> clk << ",";
          mem_ptr -> set_sampling_result << mem_ptr -> clk << ",";
          double global_average_latency = 0;
          if(global_requests) {
            global_average_latency = ((double)global_latencies) / ((double)global_requests);
          }
          
          for(int c = 0; c < controllers; c++) {
            mem_ptr -> feedback_reg_epoch << (use_global_adaptive ? global_feedback : feedbacks[c]) << ",";
            mem_ptr -> adaptive_thresholds_record << (use_global_adaptive ? global_feedback : feedbacks[c]) << ",";
            double current_latency = 0;
            if(!use_maximum_latency) {
              if(requests_completed_for_current_epoch[c]) {
                current_latency = ((double)latencies_for_current_epoch[c]) / ((double)requests_completed_for_current_epoch[c]);
              }
            } else {
              current_latency = maximum_latency_for_current_epoch[c];
            }
            mem_ptr -> latencies_each_epoch << (use_global_adaptive ? global_average_latency : current_latency) << ",";

            int new_count_threshold = -1;
            int exammined_thresholds = 0;
            if(set_sampling_on && adaptive_threshold_changes) {
              if(invert_latency_variance_threshold <= 0) {
                // Use feedback register to do set_sampling
                int64_t max_feedback = LONG_MIN;
                // Iterate through all available thresholds
                for(auto threshold:available_thresholds) {
                  // Initialize current feedback to 0
                  int64_t current_feedback = 0;
                  // If we have feedback, we use that
                  unordered_map<int, int64_t>& feedbacks_map = use_global_adaptive ? global_feedbacks_for_diff_thresholds : feedbacks_for_diff_thresholds[c];
                  if(feedbacks_map.count(threshold)) {
                    current_feedback = feedbacks_map[threshold];
                  }
                  // If the feedback is higher than the maximum, we use that feedback and threshold
                  if(current_feedback >= max_feedback) {
                    max_feedback = current_feedback;
                    new_count_threshold = threshold;
                  }

                  // cout << "At cycle " << mem_ptr -> clk << " Core " << c << " threshold " << threshold << " has feedback " << current_feedback << endl;
                  mem_ptr -> set_sampling_result << current_feedback << ",";
                }
              } else {
                // Use latency for set sampling
                double min_avg_latency = (double)LONG_MAX;
                // Iterative through all thresholds
                for(auto threshold:available_thresholds) {
                  double current_threshold_latency = (double)LONG_MAX;
                  unordered_map<int, int>& requests_map = use_global_adaptive ? global_requests_completed_for_diff_thresholds : requests_completed_for_diff_thresholds[c];
                  unordered_map<int, long> latencies_map = use_global_adaptive ? global_latencies_for_diff_thresholds : latencies_for_diff_thresholds[c];
                  if(requests_map.count(threshold)) {
                    exammined_thresholds++;
                    if(!use_maximum_latency) {
                      current_threshold_latency = ((double)(latencies_map[threshold])) / ((double)(requests_map[threshold]));
                    } else {
                      current_threshold_latency = (double)(max_latencies_for_diff_thresholds[c][threshold]);
                    }
                  }
                  if(current_threshold_latency < min_avg_latency) {
                    min_avg_latency = current_threshold_latency;
                    new_count_threshold = threshold;
                  }
                  mem_ptr -> set_sampling_result << current_threshold_latency << ",";
                }
                if(exammined_thresholds <= 1) {
                  cout << "We exammined less than 2 thresholds for core " << c << ". Keeping the old threshold" << endl;
                  new_count_threshold = -1;
                }
                if(new_count_threshold != -1) {
                  // cout << "Threshold " << new_count_threshold << " for core " << c << " has latency " << min_avg_latency << " which is lower so we are using it" << endl;
                }
              }
            }

            double magnitude = 0.0;
            if(!set_sampling_on || (new_count_threshold == -1 && magnitude_adaptive) || invert_latency_variance_threshold <= 0) {
              new_count_threshold = prefetch_count_thresholds[c];
              // cout << "Before increase, the threshold of vault " << c << " is " << new_count_threshold << endl;
              if((use_global_adaptive ? global_previous_latencies : previous_latencies[c]) == 0 || bimodel_adaptive_on || invert_latency_variance_threshold <= 0) {
                // if(feedbacks[c] > positive_feedback_threshold) {
                //   // cout << "Increase the count threshold of vault " << c << " by one due to hops" << endl;
                //   new_count_threshold++;
                // } else if(feedbacks[c] < negative_feedback_threshold) {
                //   // cout << "Decrease the count threshold of vault " << c << " by one due to hops" << endl;
                //   new_count_threshold--;
                // }
                new_count_threshold += -1*((use_global_adaptive ? global_feedback : feedbacks[c]) / feedback_threshold);
              }

              if(previous_latencies[c] > 0) {
                if(use_global_adaptive){
                  magnitude = global_average_latency / global_previous_latencies;
                } else {
                  magnitude = current_latency / previous_latencies[c];
                }
                // 1/invert_latency_variance_threshold% difference = 1 hop change
                int change = floor((magnitude-1)*invert_latency_variance_threshold)*(-1)*last_threshold_change[c];
                // cout << "The magnitude is " << magnitude << " our last change is " << last_threshold_change[c] << endl;
                // if(change != 0) {
                //   cout << "Change count threshold of vault " << c << " by " << change << " due to latencies" << endl;
                // }
                new_count_threshold += change;
              }
            }
            mem_ptr -> adaptive_thresholds_record << magnitude << ",";
            mem_ptr -> adaptive_thresholds_record << last_threshold_change[c] << ",";
            
            if(!use_global_adaptive) {
              previous_latencies[c] = current_latency;
              latencies_for_current_epoch[c] = 0;
              requests_completed_for_current_epoch[c] = 0;
              maximum_latency_for_current_epoch[c] = 0;
              feedbacks[c] = 0;
              requests_completed_for_diff_thresholds[c] = unordered_map<int, int>();
              latencies_for_diff_thresholds[c] = unordered_map<int, long>();
              max_latencies_for_diff_thresholds[c] = unordered_map<int, long>();
              feedbacks_for_diff_thresholds[c] = unordered_map<int, int64_t>();
            }

            if(adaptive_threshold_changes) {
              if(new_count_threshold == -1) {
                new_count_threshold = prefetch_count_thresholds[c];
              }
              if(new_count_threshold < (int)count_table.get_count_lower_limit()) {
                // cout << "Pending new count threshold is " << new_count_threshold << " and it is lower than the lower limit of " << count_table.get_count_lower_limit() << endl;
                new_count_threshold = (int)count_table.get_count_lower_limit();
              }
              if(new_count_threshold > (int)count_table.get_count_upper_limit()) {
                // cout << "Pending new count threshold is " << new_count_threshold << " and it is higher than the higher limit of " << count_table.get_count_upper_limit() << endl;
                new_count_threshold = (int)count_table.get_count_upper_limit()+1;
              }
              if(new_count_threshold > prefetch_count_thresholds[c]) {
                // cout << "In total we are increasing the threshold by " << (new_count_threshold - prefetch_count_thresholds[c]) << endl;;
                total_threshold_increases += (new_count_threshold - prefetch_count_thresholds[c]);
                last_threshold_change[c] = 1;
                new_count_threshold = count_table.get_count_upper_limit()+1;
              } else if(new_count_threshold < prefetch_count_thresholds[c] || new_count_threshold == 0) {
                // cout << "In total we are decreasing the threshold by " << (prefetch_count_thresholds[c] - new_count_threshold) << endl;;
                total_threshold_decreases += (prefetch_count_thresholds[c] - new_count_threshold);
                last_threshold_change[c] = -1;
                new_count_threshold = count_table.get_count_lower_limit();
              } else {
                last_threshold_change[c] = 0;
              }
              if(new_count_threshold > prefetch_maximum_count_threshold) {
                prefetch_maximum_count_threshold = new_count_threshold;
              }

              // // Go back to subscribe when we are non subscribe for 10 straight epochs to detect when subscription is helpful
              // // It causes about 5-10% performance cost depends on the bm 
              // if(new_count_threshold >= count_table.get_count_upper_limit()+1) { // If we're using no subscription for this epoch...
              //   // We increase the counter
              //   total_unsub_epochs[c]++;
              //   // If we have more than 10 epoch with no subscription...
              //   if(total_unsub_epochs[c] > 10) {
              //     // We force this epoch to become subscription
              //     last_threshold_change[c] = -1;
              //     new_count_threshold = count_table.get_count_lower_limit();
              //     total_unsub_epochs[c] = 0;
              //   }
              // } else {
              //   // otherwise we reset the counter to 0
              //   total_unsub_epochs[c] = 0;
              // }

              mem_ptr -> adaptive_thresholds << new_count_threshold << ",";
              mem_ptr -> adaptive_thresholds_record << new_count_threshold << ",";
              if(!use_global_adaptive) {
                prefetch_count_thresholds[c] = (uint64_t)new_count_threshold;
                subscription_tables[c].propogate_count_threshold(new_count_threshold);
                for(auto addr:subscription_tables[c].unused_subscriptions) {
                  if(subscription_tables[c][addr.first] == c && (addr.second <= 0 || new_count_threshold >= count_table.get_count_upper_limit())) {
                    unsubscribe_address(c, addr.first);
                  }
                }
              } else if(c == mem_ptr -> central_vault) {
                next_prefetch_count_threshold = (int64_t)new_count_threshold;
                
              }
              // cout << "The count threshold for vault " << c << " is " << prefetch_count_thresholds[c] << endl;
            }
          }
          global_previous_latencies = global_average_latency;
          global_feedback = 0;
          global_latencies = 0;
          global_requests = 0;
          mem_ptr -> adaptive_thresholds << "\n";
          mem_ptr -> latencies_each_epoch <<  "\n";
          mem_ptr -> feedback_reg_epoch <<  "\n";
          mem_ptr -> adaptive_thresholds_record << "\n";
          mem_ptr -> set_sampling_result << "\n";
        }

        if(adaptive_threshold_changes && mem_ptr -> clk % threshold_change_epoch == process_latency && next_prefetch_count_threshold >= 0 && use_global_adaptive) {
          for(int c = 0; c < controllers; c++) {
            submit_update_threshold(c, (uint64_t)next_prefetch_count_threshold);
          }
          next_prefetch_count_threshold = -1;
        }
      }
      void pre_process_addr(long& addr) {
        mem_ptr -> clear_lower_bits(addr, mem_ptr -> tx_bits + tailing_zero);
      }
      // A rewritten function that updates LRU entries, translates old address to the correct vault, and then update counter table and check for prefetch
      void access_address(Request& req) {
        // Preprocess the address as we need to drop some lowest bits
        long addr = req.addr;
        pre_process_addr(addr);
        // We take the requester's vault # for easier processing
        int req_vault_id = req.coreid;
        int original_vault_id = req.addr_vec[int(HMC::Level::Vault)];
        int val_vault_id = original_vault_id;
        // If we have the address in the subscription table, we have it in LRU or LFU unit, and we send it to the unit for counting
        if(subscription_tables[original_vault_id].is_subscribed(addr) || subscription_tables[original_vault_id].is_pending_removal(addr)) {
          val_vault_id = subscription_tables[original_vault_id][addr];
          if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LRU) {
            // Update the LRU entries of the address in from table (located in the original vault) and to table (located in the current subscribed vault)
            lru_units[original_vault_id].touch(addr);
            lru_units[val_vault_id].touch(addr);
          } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::LFU) {
            // Update the LFU entries of the address in from table (located in the original vault) and to table (located in the current subscribed vault)
            lfu_units[original_vault_id].touch(addr);
            lfu_units[val_vault_id].touch(addr);
          } else if(subscription_table_replacement_policy == SubscriptionPrefetcherReplacementPolicy::DirtyLFU) {
            assert(false);
          }
          subscription_tables[original_vault_id].touch(req.coreid == original_vault_id, addr);
          subscription_tables[val_vault_id].touch(req.coreid == original_vault_id, addr);
          total_subscribed_accesses++;
          if(req.coreid == val_vault_id) {
            total_subscribed_local_accesses++;
          }
          print_debug_info("Redirecting "+to_string(addr)+" to vault "+to_string(subscription_tables[original_vault_id][addr])+" because it is subscribed");
          // Also, we set the address vector's vault to the subscribed vault so it can be sent to the correct vault for processing.
          req.addr_vec[int(HMC::Level::Vault)] = val_vault_id;
        }
        total_memory_accesses++;
      }
      void update_count_table_and_submit_subscription(const Request& req) {
        int req_vault_id = req.coreid;
        int val_vault_id = req.addr_vec[int(HMC::Level::Vault)];
        long addr = req.addr;
        pre_process_addr(addr);
        // Calculate hops and count for prefetch policy check and implementation
        uint64_t hops = (uint64_t)(mem_ptr -> calculate_hops_travelled(req_vault_id, val_vault_id));
        uint64_t count = count_table.update_counter_table_and_get_count(req_vault_id, addr);
        // If the policy says that we should subscribe this address, we subscribe it to the requester's vault so it is closer when accessed in the future
        // We do not subscribe in the case that the original vault has the address pending subscription or removal, or when the subscription is already done to the requester vault
        if(check_prefetch(hops, count, req_vault_id, req.addr_vec)) {
          print_debug_info("Checking subscription at vault "+to_string(req_vault_id)+" and address "+to_string(addr));
          print_debug_info("Is pending subscription? "+to_string(subscription_tables[req_vault_id].is_pending_subscription(addr)));
          print_debug_info("Is pending resubscription? "+to_string(subscription_tables[req_vault_id].is_pending_resubscription(addr)));
          print_debug_info("Is pending removal? "+to_string(subscription_tables[req_vault_id].is_pending_removal(addr)));
          print_debug_info("Is subscribed to "+to_string(req_vault_id)+"?"+to_string(subscription_tables[req_vault_id].is_subscribed(addr, req_vault_id)));
        }
        if(check_prefetch(hops, count, req_vault_id, req.addr_vec) && !(
            subscription_tables[req_vault_id].is_pending_subscription(addr)
            || subscription_tables[req_vault_id].is_pending_resubscription(addr) 
            || subscription_tables[req_vault_id].is_pending_removal(addr) 
            || subscription_tables[req_vault_id].is_subscribed(addr, req_vault_id))) {
          // cout << "Address " << addr << " with hop " << hops << " and count " << count << " and originally in vault " << original_vault_id << " meets subscription threshold. We now subscribe it from " << val_vault_id << " to " << req_vault_id << endl;
          subscribe_address(req_vault_id, addr);
        }
      }
      void process_write_request(const Request& req) {
        if(!dirty_bit_used) {
          return;
        }
        long addr = req.addr;
        pre_process_addr(addr);
        int req_vault = req.coreid;
        int original_vault = find_original_vault_of_address(addr);
        if(subscription_tables[original_vault].is_subscribed(addr)
          || subscription_tables[original_vault].is_pending_removal(addr)) {
          int subscribed_vault = subscription_tables[original_vault][addr];
          int hops_req_original = mem_ptr -> calculate_hops_travelled(req_vault, original_vault);
          int hops_original_subscribed = mem_ptr -> calculate_hops_travelled(original_vault, subscribed_vault);
          // These two are one in real hardware
          pending.push_back(SubscriptionTask(addr, req_vault, original_vault, hops_req_original+pending_send[req_vault], SubscriptionTask::Type::UpdateOnWrite));
          pending.push_back(SubscriptionTask(addr, req_vault, subscribed_vault, hops_req_original+hops_original_subscribed+pending_send[req_vault], SubscriptionTask::Type::UpdateOnWrite));
          pending_send[req_vault]+=1;
        }
      }
      void update_on_write(const SubscriptionTask& task) {
        if(!dirty_bit_used) {
          return;
        }
        assert(task.type == SubscriptionTask::Type::UpdateOnWrite);
        if(subscription_tables[task.to_vault].is_subscribed(task.addr)
          || subscription_tables[task.to_vault].is_pending_removal(task.addr)) {
          subscription_tables[task.to_vault].set_dirty(task.addr);
        }
      }
      bool is_subscribed_or_pending_removal_at_requester_vault(const Request& req) {
        long addr = req.addr;
        pre_process_addr(addr);
        int req_vault = req.coreid;
        return subscription_tables[req_vault].is_subscribed(addr) || subscription_tables[req_vault].is_pending_removal(addr);
      }
      void record_positive_feedback(int vault, int hops_diff, int cycles, const Request& req) {
        if(!adaptive_threshold_changes) {
          return;
        }
        assert(hops_diff >= 0);
        // cout << "Increasing feedback vaule by " << hops_diff << ", from " << feedbacks[vault];
        feedbacks[vault]+=hops_diff;
        if(feedbacks[vault] > feedback_maximum) {
          feedbacks[vault] = feedback_maximum;
        }
        global_feedback += hops_diff;
        if(global_feedback > feedback_maximum) {
          global_feedback = feedback_maximum;
        }
        // cout << " to " << feedbacks[vault] << endl;
        // cout << "The maximum feedback is " << feedback_maximum << " the minimum is" << feedback_minimum << endl;
        total_positive_feedback++;

        if(set_sampling_on) {
          long addr = req.addr;
          pre_process_addr(addr);
          int set = subscription_tables[0].get_set(addr);
          int threshold_for_this_set = -1;
          if(set_to_thresholds.count(set) != 0) {
            threshold_for_this_set = set_to_thresholds[set];
          }
          if(feedbacks_for_diff_thresholds[vault].count(threshold_for_this_set) <= 0) {
            feedbacks_for_diff_thresholds[vault][threshold_for_this_set] = 0;
          }
          feedbacks_for_diff_thresholds[vault][threshold_for_this_set] += hops_diff;
          if(feedbacks_for_diff_thresholds[vault][threshold_for_this_set] > feedback_maximum) {
            feedbacks_for_diff_thresholds[vault][threshold_for_this_set] = feedback_maximum;
          }
        }
        // if(feedbacks[vault] >= positive_feedback_threshold) {
        //   feedbacks[vault] = 0;
        //   submit_decrease_threshold(vault, cycles);
        // }
      }
      void record_negative_feedback(int vault, int hops_diff, int cycles, const Request& req) {
        if(!adaptive_threshold_changes) {
          return;
        }
        assert(hops_diff >= 0);
        // cout << "Decrasing feedback vaule, from " << feedbacks[vault];
        feedbacks[vault]-=hops_diff;
        if(feedbacks[vault] < feedback_minimum) {
          feedbacks[vault] = feedback_minimum;
        }
        global_feedback -= hops_diff;
        if(global_feedback < feedback_minimum) {
          global_feedback = feedback_minimum;
        }
        // cout << " to " << feedbacks[vault] << endl; 
        total_negative_feedback++;

        if(set_sampling_on) {
          long addr = req.addr;
          pre_process_addr(addr);
          int set = subscription_tables[0].get_set(addr);
          int threshold_for_this_set = -1;
          if(set_to_thresholds.count(set) != 0) {
            threshold_for_this_set = set_to_thresholds[set];
          }
          if(feedbacks_for_diff_thresholds[vault].count(threshold_for_this_set) <= 0) {
            feedbacks_for_diff_thresholds[vault][threshold_for_this_set] = 0;
          }
          feedbacks_for_diff_thresholds[vault][threshold_for_this_set] -= hops_diff;
          if(feedbacks_for_diff_thresholds[vault][threshold_for_this_set] < feedback_minimum) {
            feedbacks_for_diff_thresholds[vault][threshold_for_this_set] = feedback_minimum;
          }
        }
        // if(feedbacks[vault] <= negative_feedback_threshold) {
        //   feedbacks[vault] = 0;
        //   submit_increase_threshold(vault, cycles);
        // }
      }
      void submit_update_adaptive(int vault) {
        int hops = mem_ptr -> calculate_hops_travelled(vault, mem_ptr -> central_vault);
        pending.push_back(SubscriptionTask(0, vault, mem_ptr -> central_vault, hops+pending_send[vault], SubscriptionTask::Type::UpdateAdap,
          feedbacks[vault], latencies_for_current_epoch[vault], requests_completed_for_current_epoch[vault], latencies_for_diff_thresholds[vault],
          requests_completed_for_diff_thresholds[vault], feedbacks_for_diff_thresholds[vault]));
        // cout << "submitting update adaptive for vault " << vault << " it has latency map of size " << latencies_for_diff_thresholds[vault].size() << endl;
        pending_send[vault]+=1;
        feedbacks[vault] = 0;
        previous_latencies[vault] = latencies_for_current_epoch[vault];
        latencies_for_current_epoch[vault] = 0;
        requests_completed_for_current_epoch[vault] = 0;
        latencies_for_diff_thresholds[vault] = unordered_map<int, long>();
        requests_completed_for_diff_thresholds[vault] = unordered_map<int, int>();
        feedbacks_for_diff_thresholds[vault] = unordered_map<int, int64_t>();

      }
      void process_update_adaptive(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::UpdateAdap);
        // cout << "Process update adaptive..." << endl;
        global_latencies+=task.latencies;
        global_requests+=task.num_requests;
        global_feedback += task.feedback;
        if(global_feedback > feedback_maximum) {
          global_feedback = feedback_maximum;
        } else if(global_feedback < feedback_minimum) {
          global_feedback = feedback_minimum;
        }
        for(auto i : task.latencies_for_diff_thresholds) {
          // cout << "vault " << task.from_vault << " and its threshold " << i.first << " has latency " << i.second << endl;
          if(global_latencies_for_diff_thresholds.count(i.first) <= 0) {
            global_latencies_for_diff_thresholds[i.first] = 0;
          }
          global_latencies_for_diff_thresholds[i.first] += i.second;
        }
        for(auto i : task.requests_completed_for_diff_thresholds) {
          if(global_requests_completed_for_diff_thresholds.count(i.first) <= 0) {
            global_requests_completed_for_diff_thresholds[i.first] = 0;
          }
          global_requests_completed_for_diff_thresholds[i.first] += i.second;
        }
      }
      void submit_update_threshold(int vault, uint64_t threshold) {
        int hops = mem_ptr -> calculate_hops_travelled(vault, mem_ptr -> central_vault);
        SubscriptionTask task = SubscriptionTask((long)threshold, mem_ptr -> central_vault, vault, hops+pending_send[mem_ptr -> central_vault], SubscriptionTask::Type::UpdateThreshold);
        if(vault == mem_ptr -> central_vault) {
          process_update_threshold(task);
        } else {
          pending.push_back(task);
        }
      }
      void process_update_threshold(const SubscriptionTask& task) {
        assert(task.type == SubscriptionTask::Type::UpdateThreshold);
        int new_count_threshold = (int)task.addr;
        prefetch_count_thresholds[task.to_vault] = (uint64_t)new_count_threshold;
        subscription_tables[task.to_vault].propogate_count_threshold(new_count_threshold);
        for(auto addr:subscription_tables[task.to_vault].unused_subscriptions) {
          if(subscription_tables[task.to_vault][addr.first] == task.to_vault && (addr.second <= 0 || (uint64_t)new_count_threshold >= count_table.get_count_upper_limit())) {
            unsubscribe_address(task.to_vault, addr.first);
          }
        }
      }
      void submit_increase_threshold(int vault, int cycles) {
        // cout << "Broadcasting increase request" << endl;
        pending.push_back(SubscriptionTask(0, vault, vault, cycles, SubscriptionTask::Type::IncThreshold));
      }
      void submit_decrease_threshold(int vault, int cycles) {
        // cout << "Broadcasting decrease request" << endl;
        pending.push_back(SubscriptionTask(0, vault, vault, cycles, SubscriptionTask::Type::DecThreshold));
      }
      void process_increase_threshold(const SubscriptionTask& task) {
        if(prefetch_count_thresholds[task.to_vault] < count_table.get_count_upper_limit()) {
          // cout << "Increasing threshold from " << prefetch_count_threshold;
          prefetch_count_thresholds[task.to_vault]++;
          // cout << " to " << prefetch_count_threshold << endl;
          if(prefetch_count_thresholds[task.to_vault] > prefetch_maximum_count_threshold) {
            prefetch_maximum_count_threshold = prefetch_count_thresholds[task.to_vault];
          }
          total_threshold_increases++;
        }
      }
      void process_decrease_threshold(const SubscriptionTask& task) {
        if(prefetch_count_thresholds[task.to_vault] > count_table.get_count_lower_limit()) {
          // cout << "Decreasing threshold from " << prefetch_count_threshold;
          prefetch_count_thresholds[task.to_vault]--;
          // cout << " to " << prefetch_count_threshold << endl;
          total_threshold_decreases++;
        }
      }
      void update_latency(const Request& req) {
        if(!adaptive_threshold_changes) {
          return;
        }
        long latency = req.depart_hmc - req.arrive_hmc;
        int from_vault = req.coreid;
        latencies_for_current_epoch[from_vault] += latency;
        // global_latencies += latency;
        requests_completed_for_current_epoch[from_vault]++;
        // global_requests++;
        if(latency > maximum_latency_for_current_epoch[from_vault]) {
          maximum_latency_for_current_epoch[from_vault] = latency;
        }
        if(set_sampling_on) {
          long addr = req.addr;
          pre_process_addr(addr);
          int set = subscription_tables[0].get_set(addr);
          int threshold_for_this_set = -1;
          if(set_to_thresholds.count(set) != 0) {
            threshold_for_this_set = set_to_thresholds[set];
          }
          if(latencies_for_diff_thresholds[from_vault].count(threshold_for_this_set) > 0) {
            latencies_for_diff_thresholds[from_vault][threshold_for_this_set] += latency;
          } else {
            latencies_for_diff_thresholds[from_vault][threshold_for_this_set] = latency;
          }
          if(requests_completed_for_diff_thresholds[from_vault].count(threshold_for_this_set) > 0) {
            requests_completed_for_diff_thresholds[from_vault][threshold_for_this_set]++;
          } else {
            requests_completed_for_diff_thresholds[from_vault][threshold_for_this_set] = 1;
          }
          if(max_latencies_for_diff_thresholds[from_vault].count(threshold_for_this_set) <= 0 || latency > max_latencies_for_diff_thresholds[from_vault][threshold_for_this_set]) {
            max_latencies_for_diff_thresholds[from_vault][threshold_for_this_set] = latency;
          }
        }
      }
      void finish() {
        for(int c = 0; c < controllers; c++) {
          subscription_tables[c].finish();
          total_in_table += (double)subscription_tables[c].get_total_in_table();
        }
        total_in_table = total_in_table / 2;
        avg_in_table_per_subscription = 0;
        if(avg_in_table_per_subscription > 0) {
          avg_in_table_per_subscription = total_in_table / (double)avg_in_table_per_subscription;
        }
      }
      void print_stats(){
        cout << "-----Prefetcher Stats-----" << endl;
        cout << "Total memory accesses: " << total_memory_accesses << endl;
        cout << "Total subscribed accesses: " << total_subscribed_accesses << endl;
        cout << "Total subscribed local accesses: " << total_subscribed_local_accesses << endl;
        cout << "Total submitted subscriptions: " << total_submitted_subscriptions << endl;
        cout << "Total Successful Subscription: " << total_successful_subscriptions << endl;
        cout << "Total Unsuccessful Subscription: " << total_unsuccessful_subscriptions << endl;
        cout << "Total Successful Subscription from Subscription Buffer: " << total_subscription_from_buffer << endl;
        cout << "Total Unsubscription: " << total_unsubscriptions << endl;
        cout << "Total Resubscription: " << total_resubscriptions << endl;
        cout << "Total Successful Insertation into the Subscription Buffer: " << total_buffer_successful_insertation << endl;
        cout << "Total Unsuccessful Insertation into the Subscription Buffer: " << total_buffer_unsuccessful_insertation << endl;
        cout << "Total hops travelled by Subscription Table packets: " << total_hops << endl;
        cout << "Total positive feedbacks: " << total_positive_feedback << endl;
        cout << "Total negative feedbacks: " << total_negative_feedback << endl;
        cout << "Total threshold increases: " << total_threshold_increases << endl;
        cout << "Total threshold decreases: " << total_threshold_decreases << endl;
        cout << "Maximum count threshold: " << prefetch_maximum_count_threshold << endl;
        cout << "Total subscription lifespan in table: " << total_in_table << endl;
        cout << "Average subscription lifespan: " << avg_in_table_per_subscription << endl;
        count_table.print_stats();
      }
      long get_total_memory_accesses()const {return total_memory_accesses;}
      long get_total_subscribed_accesses()const {return total_subscribed_accesses;}
      long get_total_subscribed_local_accesses()const {return total_subscribed_local_accesses;}
      long get_total_submitted_subscriptions()const {return total_submitted_subscriptions;}
      long get_total_successful_subscriptions()const {return total_successful_subscriptions;}
      long get_total_unsuccessful_subscriptions()const {return total_unsuccessful_subscriptions;}
      long get_total_subscription_from_buffer()const {return total_subscription_from_buffer;}
      long get_total_unsubscriptions()const {return total_unsubscriptions;}
      long get_total_resubscriptions()const {return total_resubscriptions;}
      long get_total_buffer_successful_insertation()const {return total_buffer_successful_insertation;}
      long get_total_buffer_unsuccessful_insertation()const {return total_buffer_unsuccessful_insertation;}
      long get_total_hops()const {return total_hops;}
      long get_count_table_insertions()const{return count_table.get_insertions();}
      long get_count_table_evictions()const{return count_table.get_evictions();}
      long get_count_table_total_count_at_eviction()const{return count_table.get_total_count_at_eviction();}
      double get_count_table_avg_count_at_eviction()const{return count_table.get_avg_count_at_eviction();}
      int get_count_table_maximum_count()const{return count_table.get_maximum_count();}
      long get_total_positive_feedback()const{return total_positive_feedback;}
      long get_total_negative_feedback()const{return total_negative_feedback;}
      long get_total_threshold_increases()const{return total_threshold_increases;}
      long get_total_threshold_decreases()const{return total_threshold_decreases;}
      long get_prefetch_maximum_count_threshold()const{return prefetch_maximum_count_threshold;}
      double get_total_in_table()const{return total_in_table;}
      double get_avg_in_table_per_subscription()const{return avg_in_table_per_subscription;}
    };
    
    SubscriptionPrefetcherSet prefetcher_set;

    vector<int> addr_bits;
    vector<vector <int> > address_distribution;
    vector<vector <int> > address_distribution_r;
    vector<vector <int> > address_distribution_w;
    vector<vector <int> > address_distribution_o;
    vector<long> hops_distribution;
    vector<long> hops_distribution_r;
    vector<long> hops_distribution_w;
    vector<long> hops_distribution_o;
    vector<long> network_cycle_distribution;
    struct MemoryAccessCountEntry {
      int count;
      int original_vault;
      size_t set;
      MemoryAccessCountEntry(){}
      MemoryAccessCountEntry(int count, int vault, size_t set):count(count),original_vault(vault), set(set){}
    };
    unordered_map<long, MemoryAccessCountEntry> memory_address_count;

    int tx_bits;

    Memory(const Config& configs, vector<Controller<HMC>*> ctrls)
        : ctrls(ctrls),
          spec(ctrls[0]->channel->spec),
          addr_bits(int(HMC::Level::MAX)),
          prefetcher_set(ctrls.size(), this)
    {
        // make sure 2^N channels/ranks
        // TODO support channel number that is not powers of 2
        int *sz = spec->org_entry.count;
        assert((sz[0] & (sz[0] - 1)) == 0);
        assert((sz[1] & (sz[1] - 1)) == 0);
        // validate size of one transaction
        int tx = (spec->prefetch_size * spec->channel_width / 8);
        tx_bits = calc_log2(tx);
        assert((1<<tx_bits) == tx);

        pim_mode_enabled = configs.pim_mode_enabled();
        network_overhead = configs.network_overhead_enabled();

        if(network_overhead) {
          network_width = ceil(sqrt(ctrls.size()));
          network_height = ceil(sqrt(ctrls.size()));
          max_hops = (network_width+network_height)*(DATA_LENGTH+2)*per_hop_overhead;
          central_vault = (network_width - 1) / 2 + ((network_height - 1) / 2 * network_width);
          cout << "We are simulating the network latency of a " << network_width << "x" << network_height << " network" << endl;
        }

        capacity_per_stack = spec->channel_width / 8;

        for (unsigned int lev = 0; lev < addr_bits.size(); lev++) {
          addr_bits[lev] = calc_log2(sz[lev]);
          capacity_per_stack *= sz[lev];
        }
        max_address = capacity_per_stack * configs.get_stacks();

        addr_bits[int(HMC::Level::MAX) - 1] -= calc_log2(spec->prefetch_size);

        // Initiating translation
        if (configs.contains("translation")) {
          translation = name_to_translation[configs["translation"]];
        }
        if (translation != Translation::None) {
          // construct a list of available pages
          // TODO: this should not assume a 4KB page!
          free_physical_pages_remaining = max_address >> 12;

          free_physical_pages.resize(free_physical_pages_remaining, -1);
        }

        // Initiating addressing
        if (configs.contains("addressing_type")) {
          assert(name_to_type.find(configs["addressing_type"]) != name_to_type.end());
          printf("configs[\"addressing_type\"] %s\n", configs["addressing_type"].c_str());
          type = name_to_type[configs["addressing_type"]];
        }

        // HMC
        assert(spec->source_links > 0);
        tags_pools.resize(spec->source_links);
        for (auto & tags_pool : tags_pools) {
          for (int i = 0 ; i < spec->max_tags ; ++i) {
            tags_pool.push_back(i);
          }
        }

        int stacks = configs.get_int_value("stacks");
        for (int i = 0 ; i < stacks ; ++i) {
          logic_layers.emplace_back(new LogicLayer<HMC>(configs, i, spec, ctrls,
              this, std::bind(&Memory<HMC>::receive_packets, this,
                              std::placeholders::_1)));
        }

        cout << "Request type = "<< int(Request::Type::READ) << " is a read \n";
        cout << "Request type = " << int(Request::Type::WRITE) << " is a write \n";

        num_cores = configs.get_core_num();
        cout << "Number of cores in HMC Memory: " << configs.get_core_num() << endl;
        cout << "Read latency of HMC Memory: " << ctrls[0]->channel->spec->read_latency << " cycles" << endl;
        cout << "Write latency of HMC Memory: " << ctrls[0]->channel->spec->write_latency << " cycles" << endl;
        address_distribution.resize(configs.get_core_num());
        address_distribution_r.resize(configs.get_core_num());
        address_distribution_w.resize(configs.get_core_num());
        address_distribution_o.resize(configs.get_core_num());
        hops_distribution.assign(network_width+network_height, 0);
        hops_distribution_r.assign(network_width+network_height, 0);
        hops_distribution_w.assign(network_width+network_height, 0);
        hops_distribution_o.assign(network_width+network_height, 0);
        network_cycle_distribution.assign(max_hops, 0);
        for(int i=0; i < configs.get_core_num(); i++){
            address_distribution[i].resize(ctrls.size());
            address_distribution_r[i].resize(ctrls.size());
            address_distribution_w[i].resize(ctrls.size());
            address_distribution_o[i].resize(ctrls.size());
            for(int j=0; j < ctrls.size(); j++){
                address_distribution[i][j] = 0;
                address_distribution_r[i][j] = 0;
                address_distribution_w[i][j] = 0;
                address_distribution_o[i][j] = 0;
            }
        }

        this -> set_application_name(configs.get_application_name());
        if(configs.get_record_memory_trace()){
          this -> set_address_recorder();
        }
        this -> set_adaptive_threshold_recorder();

        if (configs.contains("subscription_prefetcher")) {
          cout << "Using prefetcher: " << configs["subscription_prefetcher"] << endl;
          subscription_prefetcher_type = name_to_prefetcher_type[configs["subscription_prefetcher"]];
          if(subscription_prefetcher_type == SubscriptionPrefetcherType::Allocate) {
            prefetcher_set.set_swap_switch(false);
          } else if(subscription_prefetcher_type == SubscriptionPrefetcherType::Swap) {
            prefetcher_set.set_swap_switch(true);
          }
        }

        if (configs.contains("prefetcher_count_threshold")) {
          prefetcher_set.set_prefetch_count_threshold(stoi(configs["prefetcher_count_threshold"]));
        }

        if (configs.contains("prefetcher_hops_threshold")) {
          prefetcher_set.set_prefetch_hops_threshold(stoi(configs["prefetcher_hops_threshold"]));
        }

        if (configs.contains("threshold_change_epoch")) {
          prefetcher_set.set_threshold_change_epoch(stol(configs["threshold_change_epoch"]));
        }

        if (configs.contains("prefetcher_subscription_table_size")) {
          prefetcher_set.set_subscription_table_size(stoi(configs["prefetcher_subscription_table_size"]));
        }

        if (configs.contains("prefetcher_subscription_table_way")) {
          prefetcher_set.set_subscription_table_ways(stoi(configs["prefetcher_subscription_table_way"]));
        }

        if (configs.contains("prefetcher_subscription_buffer_size")) {
          prefetcher_set.set_subscription_buffer_size(stoi(configs["prefetcher_subscription_buffer_size"]));
        }

        if (configs.contains("prefetcher_table_replacement_policy")) {
          prefetcher_set.set_subscription_table_replacement_policy(configs["prefetcher_table_replacement_policy"]);
        }

        if (configs.contains("prefetcher_count_table_size")) {
          prefetcher_set.set_counter_table_size(stoi(configs["prefetcher_count_table_size"]));
        }

        if (configs.contains("print_debug_info")) {
          prefetcher_set.set_debug_flag(configs["print_debug_info"] == "true");
        }

        if (configs.contains("warmup_reqs")) {
          warmup_reqs = stol(configs["warmup_reqs"]);
        }

        if (configs.contains("adaptive_threshold_change")) {
          prefetcher_set.set_adaptive_threshold_changes(configs["adaptive_threshold_change"] == "true");
          if (configs.contains("positive_feedback_threshold")) {
            prefetcher_set.set_positive_feedback_threshold(stol(configs["positive_feedback_threshold"]));
          }
          if (configs.contains("negative_feedback_threshold")) {
            prefetcher_set.set_negative_feedback_threshold(stol(configs["negative_feedback_threshold"]));
          }
          if (configs.contains("invert_latency_variance_threshold")) {
            prefetcher_set.set_invert_latency_variance_threshold(stol(configs["invert_latency_variance_threshold"]));
          }
          if (configs.contains("bimodel_adaptive")) {
            prefetcher_set.set_bimodel_adaptive(configs["bimodel_adaptive"] == "true");
          }
          if (configs.contains("magnitude_adaptive")) {
            prefetcher_set.set_magnitude_adaptive(configs["magnitude_adaptive"] == "true");
          }
          if (configs.contains("set_sampling")) {
            prefetcher_set.set_set_sampling(configs["set_sampling"] == "true");
          }
          if (configs.contains("use_maximum_latency")) {
            prefetcher_set.set_use_maximum_latency(configs["use_maximum_latency"] == "true");
          }
          if (configs.contains("use_global_adaptive")) {
            prefetcher_set.set_use_global_adaptive(configs["use_global_adaptive"] == "true");
            if (configs.contains("update_factor")) {
              prefetcher_set.set_update_factor(stod(configs["update_factor"]));
            }
            if (configs.contains("process_latency")) {
              prefetcher_set.set_process_latency(stol(configs["process_latency"]));
            }
          }
        }

        if (configs.contains("per_hop_overhead")) {
          per_hop_overhead = stoi(configs["per_hop_overhead"]);
          cout << "Per hop overhead " << per_hop_overhead << " cycles" << endl;
          network_width = ceil(sqrt(ctrls.size()));
          network_height = ceil(sqrt(ctrls.size()));
          max_hops = (network_width+network_height)*(DATA_LENGTH+2)*per_hop_overhead;
          network_cycle_distribution.assign(max_hops, 0);
        }


        if (subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
          prefetcher_set.initialize_sets();
        }
        max_block_col_bits = spec->maxblock_entry.flit_num_bits - tx_bits;
        cout << "maxblock_entry.flit_num_bits: " << spec->maxblock_entry.flit_num_bits << " tx_bits: " << tx_bits << " max_block_col_bits: " << max_block_col_bits << endl;

        // regStats
        dram_capacity
            .name("dram_capacity")
            .desc("Number of bytes in simulated DRAM")
            .precision(0)
            ;
        dram_capacity = max_address;

        num_dram_cycles
            .name("dram_cycles")
            .desc("Number of DRAM cycles simulated")
            .precision(0)
            ;

        num_read_requests
            .init(configs.get_core_num())
            .name("read_requests")
            .desc("Number of incoming read requests to DRAM")
            .precision(0)
            ;

        num_write_requests
            .init(configs.get_core_num())
            .name("write_requests")
            .desc("Number of incoming write requests to DRAM")
            .precision(0)
            ;

        incoming_requests_per_channel
            .init(sz[int(HMC::Level::Vault)])
            .name("incoming_requests_per_channel")
            .desc("Number of incoming requests to each DRAM channel")
            .precision(0)
            ;

        incoming_read_reqs_per_channel
            .init(sz[int(HMC::Level::Vault)])
            .name("incoming_read_reqs_per_channel")
            .desc("Number of incoming read requests to each DRAM channel")
            .precision(0)
            ;
        ramulator_active_cycles
            .name("ramulator_active_cycles")
            .desc("The total number of cycles that the DRAM part is active (serving R/W)")
            .precision(0)
            ;
        memory_footprint
            .name("memory_footprint")
            .desc("memory footprint in byte")
            .precision(0)
            ;
        physical_page_replacement
            .name("physical_page_replacement")
            .desc("The number of times that physical page replacement happens.")
            .precision(0)
            ;

        maximum_internal_bandwidth
            .name("maximum_internal_bandwidth")
            .desc("The theoretical maximum bandwidth (Bps)")
            .precision(0)
            ;

        maximum_link_bandwidth
            .name("maximum_link_bandwidth")
            .desc("The theoretical maximum bandwidth (Bps)")
            .precision(0)
            ;

        read_bandwidth
            .name("read_bandwidth")
            .desc("Real read bandwidth(Bps)")
            .precision(0)
            ;

        write_bandwidth
            .name("write_bandwidth")
            .desc("Real write bandwidth(Bps)")
            .precision(0)
            ;
        read_latency_sum
            .name("read_latency_sum")
            .desc("The memory latency cycles (in memory time domain) sum for all read requests")
            .precision(0)
            ;
        read_latency_avg
            .name("read_latency_avg")
            .desc("The average memory latency cycles (in memory time domain) per request for all read requests")
            .precision(6)
            ;
        queueing_latency_sum
            .name("queueing_latency_sum")
            .desc("The sum of time waiting in queue before first command issued")
            .precision(0)
            ;
        queueing_latency_avg
            .name("queueing_latency_avg")
            .desc("The average of time waiting in queue before first command issued")
            .precision(6)
            ;
        read_latency_ns_avg
            .name("read_latency_ns_avg")
            .desc("The average memory latency (ns) per request for all read requests in this channel")
            .precision(6)
            ;
        queueing_latency_ns_avg
            .name("queueing_latency_ns_avg")
            .desc("The average of time (ns) waiting in queue before first command issued")
            .precision(6)
            ;
        request_packet_latency_sum
            .name("request_packet_latency_sum")
            .desc("The memory latency cycles (in memory time domain) sum for all read request packets transmission")
            .precision(0)
            ;
        request_packet_latency_avg
            .name("request_packet_latency_avg")
            .desc("The average memory latency cycles (in memory time domain) per request for all read request packets transmission")
            .precision(6)
            ;
        request_packet_latency_ns_avg
            .name("request_packet_latency_ns_avg")
            .desc("The average memory latency (ns) per request for all read request packets transmission")
            .precision(6)
            ;
        response_packet_latency_sum
            .name("response_packet_latency_sum")
            .desc("The memory latency cycles (in memory time domain) sum for all read response packets transmission")
            .precision(0)
            ;
        response_packet_latency_avg
            .name("response_packet_latency_avg")
            .desc("The average memory latency cycles (in memory time domain) per response for all read response packets transmission")
            .precision(6)
            ;
        response_packet_latency_ns_avg
            .name("response_packet_latency_ns_avg")
            .desc("The average memory latency (ns) per response for all read response packets transmission")
            .precision(6)
            ;

        // shared by all Controller objects

        read_transaction_bytes
            .name("read_transaction_bytes")
            .desc("The total byte of read transaction")
            .precision(0)
            ;
        write_transaction_bytes
            .name("write_transaction_bytes")
            .desc("The total byte of write transaction")
            .precision(0)
            ;

        row_hits
            .name("row_hits")
            .desc("Number of row hits")
            .precision(0)
            ;
        row_misses
            .name("row_misses")
            .desc("Number of row misses")
            .precision(0)
            ;
        row_conflicts
            .name("row_conflicts")
            .desc("Number of row conflicts")
            .precision(0)
            ;

        read_row_hits
            .init(configs.get_core_num())
            .name("read_row_hits")
            .desc("Number of row hits for read requests")
            .precision(0)
            ;
        read_row_misses
            .init(configs.get_core_num())
            .name("read_row_misses")
            .desc("Number of row misses for read requests")
            .precision(0)
            ;
        read_row_conflicts
            .init(configs.get_core_num())
            .name("read_row_conflicts")
            .desc("Number of row conflicts for read requests")
            .precision(0)
            ;

        write_row_hits
            .init(configs.get_core_num())
            .name("write_row_hits")
            .desc("Number of row hits for write requests")
            .precision(0)
            ;
        write_row_misses
            .init(configs.get_core_num())
            .name("write_row_misses")
            .desc("Number of row misses for write requests")
            .precision(0)
            ;
        write_row_conflicts
            .init(configs.get_core_num())
            .name("write_row_conflicts")
            .desc("Number of row conflicts for write requests")
            .precision(0)
            ;

        req_queue_length_sum
            .name("req_queue_length_sum")
            .desc("Sum of read and write queue length per memory cycle.")
            .precision(0)
            ;
        req_queue_length_avg
            .name("req_queue_length_avg")
            .desc("Average of read and write queue length per memory cycle.")
            .precision(6)
            ;

        read_req_queue_length_sum
            .name("read_req_queue_length_sum")
            .desc("Read queue length sum per memory cycle.")
            .precision(0)
            ;
        read_req_queue_length_avg
            .name("read_req_queue_length_avg")
            .desc("Read queue length average per memory cycle.")
            .precision(6)
            ;

        write_req_queue_length_sum
            .name("write_req_queue_length_sum")
            .desc("Write queue length sum per memory cycle.")
            .precision(0)
            ;
        write_req_queue_length_avg
            .name("write_req_queue_length_avg")
            .desc("Write queue length average per memory cycle.")
            .precision(6)
            ;

        record_read_hits
            .init(configs.get_core_num())
            .name("record_read_hits")
            .desc("record read hit count for this core when it reaches request limit or to the end")
            ;

        record_read_misses
            .init(configs.get_core_num())
            .name("record_read_misses")
            .desc("record_read_miss count for this core when it reaches request limit or to the end")
            ;

        record_read_conflicts
            .init(configs.get_core_num())
            .name("record_read_conflicts")
            .desc("record read conflict count for this core when it reaches request limit or to the end")
            ;

        record_write_hits
            .init(configs.get_core_num())
            .name("record_write_hits")
            .desc("record write hit count for this core when it reaches request limit or to the end")
            ;

        record_write_misses
            .init(configs.get_core_num())
            .name("record_write_misses")
            .desc("record write miss count for this core when it reaches request limit or to the end")
            ;

        record_write_conflicts
            .init(configs.get_core_num())
            .name("record_write_conflicts")
            .desc("record write conflict for this core when it reaches request limit or to the end")
            ;

        for (auto ctrl : ctrls) {
          ctrl->read_transaction_bytes = &read_transaction_bytes;
          ctrl->write_transaction_bytes = &write_transaction_bytes;

          ctrl->row_hits = &row_hits;
          ctrl->row_misses = &row_misses;
          ctrl->row_conflicts = &row_conflicts;
          ctrl->read_row_hits = &read_row_hits;
          ctrl->read_row_misses = &read_row_misses;
          ctrl->read_row_conflicts = &read_row_conflicts;
          ctrl->write_row_hits = &write_row_hits;
          ctrl->write_row_misses = &write_row_misses;
          ctrl->write_row_conflicts = &write_row_conflicts;

          ctrl->queueing_latency_sum = &queueing_latency_sum;

          ctrl->req_queue_length_sum = &req_queue_length_sum;
          ctrl->read_req_queue_length_sum = &read_req_queue_length_sum;
          ctrl->write_req_queue_length_sum = &write_req_queue_length_sum;

          ctrl->record_read_hits = &record_read_hits;
          ctrl->record_read_misses = &record_read_misses;
          ctrl->record_read_conflicts = &record_read_conflicts;
          ctrl->record_write_hits = &record_write_hits;
          ctrl->record_write_misses = &record_write_misses;
          ctrl->record_write_conflicts = &record_write_conflicts;
          ctrl->attach_parent_update_latency_function(std::bind(&Memory<HMC, Controller>::SubscriptionPrefetcherSet::update_latency, &(this->prefetcher_set), std::placeholders::_1));
        }
    }

    ~Memory()
    {
        for (auto ctrl: ctrls)
            delete ctrl;
        delete spec;
    }

    double clk_ns()
    {
        return spec->speed_entry.tCK;
    }

    void record_core(int coreid) {
      // TODO record multicore statistics
    }

    void tick()
    {
        clk++;
        num_dram_cycles++;

        bool is_active = false;
        for (auto ctrl : ctrls) {
          is_active = is_active || ctrl->is_active();
          ctrl->tick();
        }
        if (is_active) {
          ramulator_active_cycles++;
        }
        // We are using logic layer only when PIM is enabled
        if(!pim_mode_enabled) {
          for (auto logic_layer : logic_layers) {
            logic_layer->tick();
          }
        }
        if (subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
          prefetcher_set.tick();
        }
    }

    int assign_tag(int slid) {
      if (tags_pools[slid].empty()) {
        return -1;
      } else {
        int tag = tags_pools[slid].front();
        tags_pools[slid].pop_front();
        return tag;
      }
    }

    Packet form_request_packet(const Request& req) {
      // All packets sent from host controller are Request packets

      //cout << "Forming request packet with addr " << req.addr << endl;
      long addr = req.addr;
      int cub = addr / capacity_per_stack;
      long adrs = addr;
      int max_block_bits = spec->maxblock_entry.flit_num_bits;
      clear_lower_bits(addr, max_block_bits);
      int slid = addr % spec->source_links;
      int tag = assign_tag(slid); // may return -1 when no available tag // TODO recycle tags when request callback
      int lng = req.type == Request::Type::READ ?
                                                1 : 1 +  spec->payload_flits;
      Packet::Command cmd;
      switch (int(req.type)) {
        case int(Request::Type::READ):
          cmd = read_cmd_map[lng];
        break;
        case int(Request::Type::WRITE):
          cmd = write_cmd_map[lng];
        break;
        default: assert(false);
      }
      Packet packet(Packet::Type::REQUEST, cub, adrs, tag, lng, slid, cmd);
      packet.req = req;
      
      //cout << "Forming a packet to send to memory \n";
      //cout << "ADDR: " << packet.header.ADRS.value << " CUB " << packet.header.CUB.value << " SLID " << packet.tail.SLID.value << " TAG " << packet.header.TAG.value << " LNG " << lng << endl;

      debug_hmc("cub: %d", cub);
      debug_hmc("adrs: %lx", adrs);
      debug_hmc("slid: %d", slid);
      debug_hmc("lng: %d", lng);
      debug_hmc("cmd: %d", int(cmd));
      // DEBUG:
      assert(packet.header.CUB.valid());
      assert(packet.header.ADRS.valid());
      assert(packet.header.TAG.valid()); // -1 also considered valid here...
      assert(packet.tail.SLID.valid());
      assert(packet.header.CMD.valid());
      return packet;
    }

    void receive_packets(Packet packet) {
      debug_hmc("receive response packets@host controller");
      if (packet.flow_control) {
        return;
      }

      assert(packet.type == Packet::Type::RESPONSE);

      tags_pools[packet.header.SLID.value].push_back(packet.header.TAG.value);
      Request& req = packet.req;
      req.depart_hmc = clk;
      if (req.type == Request::Type::READ) {
        read_latency_sum += req.depart_hmc - req.arrive_hmc;
        debug_hmc("read_latency: %ld", req.depart_hmc - req.arrive_hmc);
        request_packet_latency_sum += req.arrive - req.arrive_hmc;
        debug_hmc("request_packet_latency: %ld", req.arrive - req.arrive_hmc);
        response_packet_latency_sum += req.depart_hmc - req.depart;
        debug_hmc("response_packet_latency: %ld", req.depart_hmc - req.depart);

        req.callback(req);


      }
      else if(req.type == Request::Type::WRITE){
        req.callback(req);
      }
    }

    long address_vector_to_address(const vector<int>& addr_vec) {
      long addr = 0;
      long vault = addr_vec[int(HMC::Level::Vault)];
      long bank_group = addr_vec[int(HMC::Level::BankGroup)];
      long bank = addr_vec[int(HMC::Level::Bank)];
      long row = addr_vec[int(HMC::Level::Row)];
      long column = addr_vec[int(HMC::Level::Column)];
      // cout << "Address Vector is in Vault " << addr_vec[int(HMC::Level::Vault)] << " BankGroup " << addr_vec[int(HMC::Level::BankGroup)]
      //   << " Bank " << addr_vec[int(HMC::Level::Bank)] << " Row " << addr_vec[int(HMC::Level::Row)] << " Column " << addr_vec[int(HMC::Level::Column)];
      int column_significant_bits = addr_bits[int(HMC::Level::Column)] - max_block_col_bits;
      switch(int(type)) {
        case int(Type::RoCoBaVa): {
          addr |= row;
          addr <<= column_significant_bits;
          addr |= (column >> max_block_col_bits);
          addr <<= addr_bits[int(HMC::Level::BankGroup)];
          addr |= bank_group;
          addr <<= addr_bits[int(HMC::Level::Bank)];
          addr |= bank;
          addr <<= addr_bits[int(HMC::Level::Vault)];
          addr |= vault;
          addr <<= max_block_col_bits;
          addr |= column & ((1<<max_block_col_bits) - 1);
        }
        break;
        case int(Type::RoBaCoVa): {
          addr |= row;
          addr <<= addr_bits[int(HMC::Level::BankGroup)];
          addr |= bank_group;
          addr <<= addr_bits[int(HMC::Level::Bank)];
          addr |= bank;
          addr <<= column_significant_bits;
          addr |= (column >> max_block_col_bits);
          addr <<= addr_bits[int(HMC::Level::Vault)];
          addr |= vault;
          addr <<= max_block_col_bits;
          addr |= column & ((1<<max_block_col_bits) - 1);
        }
        break;
        case int(Type::RoCoBaBgVa): {
          addr |= row;
          addr <<= column_significant_bits;
          addr |= (column >> max_block_col_bits);
          addr <<= addr_bits[int(HMC::Level::Bank)];
          addr |= bank;
          addr <<= addr_bits[int(HMC::Level::BankGroup)];
          addr |= bank_group;
          addr <<= addr_bits[int(HMC::Level::Vault)];
          addr |= vault;
          addr <<= max_block_col_bits;
          addr |= column & ((1<<max_block_col_bits) - 1);
        }
        break;
        default:
            assert(false);
      }
      // cout << " and after translation, the original address is: " << addr << endl;
      return addr;
    }

    vector<int> address_to_address_vector(const long& addr) {
      long local_addr = addr;
      // cout << "The input address is " << addr;
      vector<int> addr_vec;
      addr_vec.resize(addr_bits.size());
      switch(int(type)) {
          case int(Type::RoCoBaVa): {
            addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(local_addr, max_block_col_bits);
            addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Vault)]);
            addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Bank)]);
            addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::BankGroup)]);
            int column_MSB_bits =
              slice_lower_bits(
                  local_addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            addr_vec[int(HMC::Level::Column)] =
              addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          case int(Type::RoBaCoVa): {
            addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(local_addr, max_block_col_bits);
            addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Vault)]);
            int column_MSB_bits =
              slice_lower_bits(
                  local_addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            addr_vec[int(HMC::Level::Column)] =
              addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Bank)]);
            addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::BankGroup)]);
            addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          case int(Type::RoCoBaBgVa): {
            addr_vec[int(HMC::Level::Column)] =
                slice_lower_bits(local_addr, max_block_col_bits);
            addr_vec[int(HMC::Level::Vault)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Vault)]);
            addr_vec[int(HMC::Level::BankGroup)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::BankGroup)]);
            addr_vec[int(HMC::Level::Bank)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Bank)]);
            int column_MSB_bits =
              slice_lower_bits(
                  local_addr, addr_bits[int(HMC::Level::Column)] - max_block_col_bits);
            addr_vec[int(HMC::Level::Column)] =
              addr_vec[int(HMC::Level::Column)] | (column_MSB_bits << max_block_col_bits);
            addr_vec[int(HMC::Level::Row)] =
                slice_lower_bits(local_addr, addr_bits[int(HMC::Level::Row)]);
          }
          break;
          default:
              assert(false);
        }
        // cout << " And after translation, it is in Vault " << addr_vec[int(HMC::Level::Vault)] << " BankGroup " << addr_vec[int(HMC::Level::BankGroup)]
        //     << " Bank " << addr_vec[int(HMC::Level::Bank)] << " Row " << addr_vec[int(HMC::Level::Row)] << " Column " << addr_vec[int(HMC::Level::Column)] << endl;
        return addr_vec;
    }

    bool send(Request req)
    {
      //  cout << "receive request packets@host controller with address " << req.addr << " from vault " << req.coreid << " at " << clk << endl;
        req._addr = req.addr;
        req.reqid = mem_req_count;
        if(mem_req_count-1 >= warmup_reqs && !warmup_finished) {
          warmup_finished = true;
          clk_at_end_of_warmup = clk;
          string sub_stats_to_open = application_name+".ramulator.subscription_stats.end_of_warmup";
          write_sub_stats_file(sub_stats_to_open);
          write_address_distribution(".end_of_warmup");
        }

        // cout << "Raw address " << req._addr << " Address before bit operation is " << bitset<64>(req.addr) << endl;
        clear_higher_bits(req.addr, max_address-1ll);
        // cout << "Address after clear higher bits is" << bitset<64>(req.addr) << endl;
        long addr = req.addr;
        long coreid = req.coreid;

        // Each transaction size is 2^tx_bits, so first clear the lowest tx_bits bits
        clear_lower_bits(addr, tx_bits);
        // bitset<64> address_in_binary = bitset<64>(req.addr);
        // for(int i = 0; i < tx_bits + 1; i++) {
        //   if(address_in_binary[i]) {
        //     cout << "Address for request with address " << req.addr << " and raw address " << req._addr << " does not have " << tx_bits + 1 << " tailing zeros. It is " << address_in_binary << " in binary. Aborting..." << endl;
        //   }
        //   assert(!address_in_binary[i]);
        // }
        // cout << "Address after clear lower bits is " << bitset<64>(addr);
        vector<int> addr_vec = address_to_address_vector(addr);
        // cout << " with vault " << addr_vec[int(HMC::Level::Vault)] << " bank group " << addr_vec[int(HMC::Level::BankGroup)] << " bank " << addr_vec[int(HMC::Level::Bank)] << " row " << addr_vec[int(HMC::Level::Row)] << " column " << addr_vec[int(HMC::Level::Column)] << endl;
        // assert(address_vector_to_address(addr_vec) == addr); // Test script to make sure the implementation is correct.
        req.addr_vec = addr_vec;
        int original_vault = req.addr_vec[int(HMC::Level::Vault)];
        int requester_vault = req.coreid;
        if (subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
          prefetcher_set.access_address(req);
        } else {
          total_memory_accesses++;
        }
        int subscribed_vault = req.addr_vec[int(HMC::Level::Vault)];
        // if(subscribed_vault != original_vault) {
          // cout << "subscribed_vault: " << subscribed_vault << " original_vault: " << original_vault << " address " << req._addr << endl;
        // }
        

        req.arrive_hmc = clk;

        if(pim_mode_enabled){
            // To model NOC traffic
            //I'm considering 32 vaults. So the 2D mesh will be 36x36
            //To calculate how many hops, check the manhattan distance
            int hops;
            int actual_hops;
            int no_prefetcher_hops;
            if(!network_overhead) {
              hops = 0;
              no_prefetcher_hops = 0;
              actual_hops = 0;
            }
            else if (req.type == Request::Type::READ){
              // If we do not use prefetcher, we calculate hops the traditional way
              no_prefetcher_hops = calculate_hops_travelled(requester_vault, original_vault, READ_LENGTH);
              if(subscription_prefetcher_type == SubscriptionPrefetcherType::None){
                // Let's assume 1 Flit = 128 bytes
                // A read request is 64 bytes
                // One read request will take = 1 Flit*hops + 5*hops
                hops = no_prefetcher_hops;
              } else {
                hops = 0;
                if(requester_vault != subscribed_vault) {
                  // We first check the subscription table of the original vault
                  hops += calculate_hops_travelled(requester_vault, original_vault);
                  // Then the original vault forward this request to the subscribed vault
                  hops += calculate_hops_travelled(original_vault, subscribed_vault);
                  // Then the subscribed vault send the data back to the requester vault
                  hops += calculate_hops_travelled(subscribed_vault, requester_vault, DATA_LENGTH);
                }
              }
            }
            else if (req.type == Request::Type::WRITE){
              no_prefetcher_hops = calculate_hops_travelled(requester_vault, original_vault, WRITE_LENGTH);
              if(requester_vault != subscribed_vault) {
                // We write to the original vault in any case, and let the original vault determine whether to fowraed it or not
                hops = no_prefetcher_hops;
              } else {
                hops = 0;
              }
            // What is "OTHER"?
            } else {
              no_prefetcher_hops = calculate_hops_travelled(requester_vault, original_vault, OTHER_LENGTH);
              if(subscription_prefetcher_type == SubscriptionPrefetcherType::None) {
                hops = no_prefetcher_hops;
              } else {
                if(requester_vault == subscribed_vault) {
                  hops = 0;
                } else {
                  // The requester first send data to the original vault, then the original vault forward it to the subscribed vault
                  hops = calculate_hops_travelled(requester_vault, original_vault)+calculate_hops_travelled(original_vault, subscribed_vault);
                }
              }
            }
            req.hops = hops;
            if(hops != 0) {
              // cout << "The # of hops for request with address " << req._addr << " is " << hops << endl; 
            }

            if(!ctrls[req.addr_vec[int(HMC::Level::Vault)]] -> receive(req)){
              cout << "We are not able to send request with address " << req.addr << endl;
              return false;
            }
            if(network_overhead) {
              if (subscription_prefetcher_type != SubscriptionPrefetcherType::None && hops != 0) {
                prefetcher_set.update_count_table_and_submit_subscription(req);
              }
              actual_hops = req.served_without_hops == 1 ? 0 : hops;
              assert(total_hops >= 0);
              total_hops += actual_hops;
              assert(total_hops >= 0);
              if(actual_hops > no_prefetcher_hops) {
                // cout << "No prefetch hop is " << no_prefetcher_hops << " and prefetch hop is " << hops << " submitting negative feedback" << endl;
                prefetcher_set.record_negative_feedback(requester_vault, actual_hops-no_prefetcher_hops, actual_hops, req);
                if(requester_vault != subscribed_vault && !prefetcher_set.get_use_global_adaptive()) {
                  prefetcher_set.record_negative_feedback(subscribed_vault, actual_hops-no_prefetcher_hops, actual_hops, req);
                }
                // prefetcher_set.record_negative_feedback(requester_vault, 1, actual_hops);
              } else if(actual_hops < no_prefetcher_hops) {
                // cout << "No prefetch hop is " << no_prefetcher_hops << " and prefetch hop is " << hops << " submitting positive feedback" << endl;
                prefetcher_set.record_positive_feedback(requester_vault, no_prefetcher_hops-actual_hops, actual_hops, req);
                // if(requester_vault != subscribed_vault) {
                //   prefetcher_set.record_positive_feedback(subscribed_vault, no_prefetcher_hops-actual_hops, actual_hops, req.addr_vec);
                // }
                // prefetcher_set.record_positive_feedback(requester_vault, 1, actual_hops);
              // } else if(hops == no_prefetcher_hops) {
              //   prefetcher_set.record_positive_feedback(requester_vault, 1, hops);
              }
            }

            if (req.type == Request::Type::READ) {
                ++num_read_requests[coreid];
                ++incoming_read_reqs_per_channel[req.addr_vec[int(HMC::Level::Vault)]];
            }
            if (req.type == Request::Type::WRITE) {
                ++num_write_requests[coreid];
                if(subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
                  prefetcher_set.process_write_request(req);
                }
            }
            ++incoming_requests_per_channel[req.addr_vec[int(HMC::Level::Vault)]];
            ++mem_req_count;
            if(memory_address_count.count(req.addr) == 0) {
              size_t set = -1;
              if(subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
                set = prefetcher_set.get_set(req);
              }
              memory_address_count[req.addr] = MemoryAccessCountEntry(1, original_vault, set);
            } else {
              memory_address_count[req.addr].count++;
            }

            if(req.coreid >= 0 && req.coreid < 256) {
              network_cycle_distribution[hops]++;
              int simplified_hops = calculate_hops_travelled(req.coreid, req.addr_vec[int(HMC::Level::Vault)])/per_hop_overhead;
              hops_distribution[simplified_hops]++;
              assert(address_distribution[req.coreid][req.addr_vec[int(HMC::Level::Vault)]] >= 0);
              address_distribution[req.coreid][req.addr_vec[int(HMC::Level::Vault)]]++;
              assert(address_distribution[req.coreid][req.addr_vec[int(HMC::Level::Vault)]] >= 0);
              if (req.type == Request::Type::WRITE){
                hops_distribution_w[simplified_hops]++;
                address_distribution_w[req.coreid][req.addr_vec[int(HMC::Level::Vault)]]++;
              } else if (req.type == Request::Type::READ) {
                hops_distribution_r[simplified_hops]++;
                address_distribution_r[req.coreid][req.addr_vec[int(HMC::Level::Vault)]]++;
              } else{
                hops_distribution_o[simplified_hops]++;
                address_distribution_o[req.coreid][req.addr_vec[int(HMC::Level::Vault)]]++;
              }
            }
            else
              cerr << "HMC MEMORY: INVALID CORE ID: " << req.coreid << "endl";

            if(get_memory_addresses){
              if (profile_this_epoch){
                memory_addresses << clk << "," << req._addr << "," << req.addr << "," << req.coreid << "," << actual_hops << "," << no_prefetcher_hops << ",";
                if (req.type == Request::Type::WRITE)       memory_addresses << "W,";
                else if (req.type == Request::Type::READ)   memory_addresses << "R,";
                else                                        memory_addresses << "NA,";
                memory_addresses << req.addr_vec[int(HMC::Level::Vault)] << "," << req.addr_vec[int(HMC::Level::BankGroup)] << ","
                                 << req.addr_vec[int(HMC::Level::Bank)] << ","  << req.addr_vec[int(HMC::Level::Row)]       << ","
                                 << req.addr_vec[int(HMC::Level::Column)] << "\n";

                instruction_counter++;
                if(instruction_counter >= 10000){
                  profile_this_epoch = false;
                  instruction_counter = 0;
                }
              }
              else{
                instruction_counter++;
                if(instruction_counter >= 990000){
                  profile_this_epoch = true;
                  instruction_counter = 0;
                }
              }
            }

            memory_addresses << clk << "," << req._addr << "," << req.addr << "," << req.coreid << "," << actual_hops << "," << no_prefetcher_hops << ",";
            if (req.type == Request::Type::WRITE)       memory_addresses << "W,";
            else if (req.type == Request::Type::READ)   memory_addresses << "R,";
            else                                        memory_addresses << "NA,";
            memory_addresses << req.addr_vec[int(HMC::Level::Vault)] << "," << req.addr_vec[int(HMC::Level::BankGroup)] << ","
                             << req.addr_vec[int(HMC::Level::Bank)] << ","  << req.addr_vec[int(HMC::Level::Row)]       << ","
                             << req.addr_vec[int(HMC::Level::Column)] << "\n";
            return true;
        }
        else{
            Packet packet = form_request_packet(req);
            if (packet.header.TAG.value == -1) {
                return false;
            }

            // TODO support multiple stacks
            Link<HMC>* link =
                logic_layers[0]->host_links[packet.tail.SLID.value].get();
            if (packet.total_flits <= link->slave.available_space()) {
              link->slave.receive(packet);
              if (req.type == Request::Type::READ) {
                ++num_read_requests[coreid];
                ++incoming_read_reqs_per_channel[req.addr_vec[int(HMC::Level::Vault)]];
              }
              if (req.type == Request::Type::WRITE) {
                ++num_write_requests[coreid];
              }
              ++incoming_requests_per_channel[req.addr_vec[int(HMC::Level::Vault)]];
              ++mem_req_count;

              if(req.coreid >= 0 && req.coreid < 256) {
                address_distribution[req.coreid][req.addr_vec[int(HMC::Level::Vault)]]++;
                if (req.type == Request::Type::WRITE)       address_distribution_w[req.coreid][req.addr_vec[int(HMC::Level::Vault)]]++;
                else if (req.type == Request::Type::READ)   address_distribution_r[req.coreid][req.addr_vec[int(HMC::Level::Vault)]]++;
                else                                        address_distribution_o[req.coreid][req.addr_vec[int(HMC::Level::Vault)]]++;
              }
              else
                cerr << "HMC MEMORY: INVALID CORE ID: " << req.coreid << "endl";
              return true;
            }
            else {
              return false;
            }
        }

        if(get_memory_addresses){
          cout << "Get memory address \n";
          if (profile_this_epoch){

          memory_addresses << clk << "," << req._addr << "," << req.addr << "," << req.coreid << "," << 0 << "," << 0 << ",";
          if (req.type == Request::Type::WRITE)       memory_addresses << "W,";
          else if (req.type == Request::Type::READ)   memory_addresses << "R,";
          else                                        memory_addresses << "NA,";
          memory_addresses << req.addr_vec[int(HMC::Level::Vault)] << "," << req.addr_vec[int(HMC::Level::BankGroup)] << ","
                            << req.addr_vec[int(HMC::Level::Bank)] << ","  << req.addr_vec[int(HMC::Level::Row)]       << ","
                            << req.addr_vec[int(HMC::Level::Column)] << "\n";

            instruction_counter++;
            if(instruction_counter >= 10000){
              profile_this_epoch = false;
              instruction_counter = 0;
            }
          }
          else{
            instruction_counter++;
            if(instruction_counter >= 990000){
              profile_this_epoch = true;
              instruction_counter = 0;
            }
          }
        }

        memory_addresses << clk << "," << req._addr << "," << req.addr << "," << req.coreid << "," << 0 << "," << 0 << ",";
        if (req.type == Request::Type::WRITE)       memory_addresses << "W,";
        else if (req.type == Request::Type::READ)   memory_addresses << "R,";
        else                                        memory_addresses << "NA,";
        memory_addresses << req.addr_vec[int(HMC::Level::Vault)] << "," << req.addr_vec[int(HMC::Level::BankGroup)] << ","
                          << req.addr_vec[int(HMC::Level::Bank)] << ","  << req.addr_vec[int(HMC::Level::Row)]       << ","
                          << req.addr_vec[int(HMC::Level::Column)] << "\n";
        return true;
    }

    int pending_requests()
    {
        int reqs = 0;
        for (auto ctrl: ctrls)
            reqs += ctrl->readq.size() + ctrl->writeq.size() + ctrl->otherq.size() + ctrl->pending.size();
        return reqs;
    }

    void write_sub_stats_file(string sub_stats_to_open) {
      ofstream sub_stats_ofs(sub_stats_to_open.c_str(), ofstream::out);
      if (subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
        sub_stats_ofs << "-----Prefetcher Stats-----" << "\n";
        sub_stats_ofs << "MemAccesses: " << prefetcher_set.get_total_memory_accesses() << "\n";
        sub_stats_ofs << "SubAccesses: " << prefetcher_set.get_total_subscribed_accesses() << "\n";
        sub_stats_ofs << "SubLocAccesses: " << prefetcher_set.get_total_subscribed_local_accesses() << "\n";
        sub_stats_ofs << "SubmittedSubscriptions: " << prefetcher_set.get_total_submitted_subscriptions() << "\n";
        sub_stats_ofs << "SuccessfulSubscriptions: " << prefetcher_set.get_total_successful_subscriptions() << "\n";
        sub_stats_ofs << "UnsuccessfulSubscriptions: " << prefetcher_set.get_total_unsuccessful_subscriptions() << "\n";
        sub_stats_ofs << "SuccessfulSubscriptionFromBuffer: " << prefetcher_set.get_total_subscription_from_buffer() << "\n";
        sub_stats_ofs << "Unsubscriptions: " << prefetcher_set.get_total_unsubscriptions() << "\n";
        sub_stats_ofs << "Resubscriptions: " << prefetcher_set.get_total_resubscriptions() << "\n";
        sub_stats_ofs << "SuccessfulInsertationToBuffer: " << prefetcher_set.get_total_buffer_successful_insertation() << "\n";
        sub_stats_ofs << "UnsuccessfulInsertationToBuffer: " << prefetcher_set.get_total_buffer_unsuccessful_insertation() << "\n";
        sub_stats_ofs << "SubscriptionPktHopsTravelled: " << prefetcher_set.get_total_hops() << "\n";
        sub_stats_ofs << "CountTableUpdates: " << prefetcher_set.get_count_table_insertions() << "\n";
        sub_stats_ofs << "CountTableEvictions: " << prefetcher_set.get_count_table_evictions() << "\n";
        sub_stats_ofs << "CountTableTotalCount: " << prefetcher_set.get_count_table_total_count_at_eviction() << "\n";
        sub_stats_ofs << "CountTableAvgCount: " << prefetcher_set.get_count_table_avg_count_at_eviction() << "\n";
        sub_stats_ofs << "CountTableUpdatesWithoutEviction: " << (prefetcher_set.get_count_table_insertions() - prefetcher_set.get_count_table_evictions()) << "\n";
        sub_stats_ofs << "CountTableMaxCount: " << prefetcher_set.get_count_table_maximum_count() << "\n";
        sub_stats_ofs << "TotalPosFeedback: " << prefetcher_set.get_total_positive_feedback() << "\n";
        sub_stats_ofs << "TotalNegFeedback: " << prefetcher_set.get_total_negative_feedback() << "\n";
        sub_stats_ofs << "TotalThresholdInc: " << prefetcher_set.get_total_threshold_increases() << "\n";
        sub_stats_ofs << "TotalThresholdDec: " << prefetcher_set.get_total_threshold_decreases() << "\n";
        sub_stats_ofs << "MaxCountThreshold: " << prefetcher_set.get_prefetch_maximum_count_threshold() << "\n";
        sub_stats_ofs << "TotalInTable: "<< prefetcher_set.get_total_in_table() << "\n";
        sub_stats_ofs << "AvgInTable: " << prefetcher_set.get_avg_in_table_per_subscription() << "\n";
        sub_stats_ofs << "-----End Prefetcher Stats-----" << "\n";
      } else {
        sub_stats_ofs << "MemAccesses: " << total_memory_accesses << "\n";
      }
      sub_stats_ofs << "AccessPktHopsTravelled: " << total_hops << "\n";
      long long total_latency = 0;
      long long total_hmc_latency = 0;
      long long total_waiting_ready = 0;
      long long total_readq_pending = 0;
      long long total_writeq_pending = 0;
      long long total_otherq_pending = 0;
      long long total_overflow_pending = 0;
      long long total_transfer_latency = 0;
      long long total_in_memory_latency = 0;
      long long total_process_latency = 0;
      long long total_incoming_queuing_latency = 0;
      long long total_outgoing_queuing_latency = 0;
      long long total_bursts = 0;
      long long stalled_cycles = 0;
      long long total_pending_queue_pending = 0;
      sub_stats_ofs << "-----Controller Stats-----" << "\n";
      for(int c = 0; c < ctrls.size(); c++) {
        sub_stats_ofs << "Controller" << c << "MaxReadQQSize: " << ctrls[c] -> readq.max_q_size << "\n";
        sub_stats_ofs << "Controller" << c << "MaxWriteQQSize: " << ctrls[c] -> writeq.max_q_size << "\n";
        sub_stats_ofs << "Controller" << c << "MaxOtherQQSize: " << ctrls[c] -> otherq.max_q_size << "\n";
        sub_stats_ofs << "Controller" << c << "MaxOverflowQSize: " << ctrls[c] -> overflow.max_q_size << "\n";
        sub_stats_ofs << "Controller" << c << "MaxReadQArrivelQSize: " << ctrls[c] -> readq.max_arrivel_size << "\n";
        sub_stats_ofs << "Controller" << c << "MaxWriteQArrivelQSize: " << ctrls[c] -> writeq.max_arrivel_size << "\n";
        sub_stats_ofs << "Controller" << c << "MaxOtherQArrivelQSize: " << ctrls[c] -> otherq.max_arrivel_size << "\n";
        sub_stats_ofs << "Controller" << c << "MaxOverflowArrivelQSize: " << ctrls[c] -> overflow.max_arrivel_size << "\n";
        sub_stats_ofs << "Controller" << c << "ReadQPending: " << ctrls[c] -> readq.total_pending_task << "\n";
        total_readq_pending += ctrls[c] -> readq.total_pending_task;
        sub_stats_ofs << "Controller" << c << "WriteQPending: " << ctrls[c] -> writeq.total_pending_task << "\n";
        total_writeq_pending += ctrls[c] -> writeq.total_pending_task;
        sub_stats_ofs << "Controller" << c << "OtherQPending: " << ctrls[c] -> otherq.total_pending_task << "\n";
        total_otherq_pending += ctrls[c] -> otherq.total_pending_task;
        sub_stats_ofs << "Controller" << c << "OverflowPending: " << ctrls[c] -> overflow.total_pending_task << "\n";
        total_overflow_pending += ctrls[c] -> overflow.total_pending_task;
        sub_stats_ofs << "Controller" << c << "PendingQPending: " << ctrls[c] -> total_pending_finished_task << "\n";
        total_pending_queue_pending += ctrls[c] -> total_pending_finished_task;
        sub_stats_ofs << "Controller" << c << "WaitingReady: " << ctrls[c]->total_cycle_waiting_not_ready_request << "\n";
        total_waiting_ready += ctrls[c]->total_cycle_waiting_not_ready_request;
        sub_stats_ofs << "Controller" << c << "RequestLatency: " << ctrls[c]->total_latency << "\n";
        total_latency += ctrls[c]->total_latency;
        sub_stats_ofs << "Controller" << c << "HMCLatency: " << ctrls[c]->total_hmc_latency << "\n";
        total_hmc_latency += ctrls[c]->total_hmc_latency;
        sub_stats_ofs << "Controller" << c << "TransferLatency: " << ctrls[c]->total_transfer_latency << "\n";
        total_transfer_latency += ctrls[c]->total_transfer_latency;
        sub_stats_ofs << "Controller" << c << "InMemoryLatency: " << ctrls[c]->total_in_memory_latency << "\n";
        total_in_memory_latency += ctrls[c]->total_in_memory_latency;
        sub_stats_ofs << "Controller" << c << "ProcessLatency: " << ctrls[c]->total_process_latency << "\n";
        total_process_latency += ctrls[c]->total_process_latency;
        sub_stats_ofs << "Controller" << c << "IncomingQueuingLatency: " << ctrls[c]->total_incoming_queuing_latency << "\n";
        total_incoming_queuing_latency += ctrls[c]->total_incoming_queuing_latency;
        sub_stats_ofs << "Controller" << c << "OutgoingQueuingLatency: " << ctrls[c]->total_outgoing_queuing_latency << "\n";
        total_outgoing_queuing_latency += ctrls[c]->total_outgoing_queuing_latency;
        sub_stats_ofs << "Controller" << c << "RequestBursts: " << ctrls[c]->total_bursts << "\n";
        total_bursts += ctrls[c]->total_bursts;
        sub_stats_ofs << "Controller" << c << "StalledCycles: " << ctrls[c]->stalled_cycles << "\n";
        stalled_cycles += ctrls[c]->stalled_cycles;
        assert(total_hmc_latency >= 0);
        assert(total_latency >= 0);
      }
      sub_stats_ofs << "TotalWaitingReady: " << total_waiting_ready << "\n";
      sub_stats_ofs << "TotalRequestLatency: " << total_latency << "\n";
      sub_stats_ofs << "TotalHMCLatency: " << total_hmc_latency << "\n";
      sub_stats_ofs << "TotalTransferLatency: " << total_transfer_latency << "\n";
      sub_stats_ofs << "TotalInMemoryLatency: " << total_in_memory_latency << "\n";
      sub_stats_ofs << "TotalProcessLatency: " << total_process_latency << "\n";
      sub_stats_ofs << "TotalIncomingQueuingLatency: " << total_incoming_queuing_latency << "\n";
      sub_stats_ofs << "TotalOutgoingQueuingLatency: " << total_outgoing_queuing_latency << "\n";
      sub_stats_ofs << "TotalStalledCycles: " << stalled_cycles << "\n";
      sub_stats_ofs << "TotalRequestBursts: " << total_bursts << "\n";
      sub_stats_ofs << "MemoryRequests: " << mem_req_count << "\n";
      sub_stats_ofs << "TotalReadQPending: " << total_readq_pending << "\n";
      sub_stats_ofs << "TotalWriteQPending: " << total_writeq_pending << "\n";
      sub_stats_ofs << "TotalOtherQPending: " << total_otherq_pending << "\n";
      sub_stats_ofs << "TotalOverflowPending: " << total_overflow_pending << "\n";
      sub_stats_ofs << "TotalPendingQPending: " << total_pending_queue_pending << "\n";
      sub_stats_ofs << "-----End Controller Stats-----" << "\n";  
      sub_stats_ofs.close();
    }

    void write_address_distribution(string postfix) {
      string to_open = application_name+".ramulator.address_distribution"+postfix;
      string to_open_r = application_name+".ramulator.address_distribution_r"+postfix;
      string to_open_w = application_name+".ramulator.address_distribution_w"+postfix;
      string to_open_o = application_name+".ramulator.address_distribution_o"+postfix;
      cout << "Address distribution stored at: " << to_open << endl;
      std::ofstream ofs(to_open.c_str(), std::ofstream::out);
      ofs << "CoreID VaultID #Requests\n";
      for(int i=0; i < address_distribution.size(); i++){
        for(int j=0; j < address_distribution.size(); j++){
          ofs << i << " " << j << " " <<  address_distribution[i][j] << "\n";
        }
      }
      ofs.close();
      std::ofstream ofs_r(to_open_r.c_str(), std::ofstream::out);
      ofs_r << "CoreID VaultID #Requests\n";
      for(int i=0; i < address_distribution_r.size(); i++){
        for(int j=0; j < address_distribution_r.size(); j++){
          ofs_r << i << " " << j << " " <<  address_distribution_r[i][j] << "\n";
        }
      }
      ofs_r.close();
      std::ofstream ofs_w(to_open_w.c_str(), std::ofstream::out);
      ofs_w << "CoreID VaultID #Requests\n";
      for(int i=0; i < address_distribution_w.size(); i++){
        for(int j=0; j < address_distribution_w.size(); j++){
          ofs_w << i << " " << j << " " <<  address_distribution_w[i][j] << "\n";
        }
      }
      ofs_w.close();
      std::ofstream ofs_o(to_open_o.c_str(), std::ofstream::out);
      ofs_o << "CoreID VaultID #Requests\n";
      for(int i=0; i < address_distribution_o.size(); i++){
        for(int j=0; j < address_distribution_o.size(); j++){
          ofs_o << i << " " << j << " " <<  address_distribution_o[i][j] << "\n";
        }
      }
      ofs_o.close();
    }

    void finish() {
      std::cout << "[RAMULATOR] Gathering stats \n";

      dram_capacity = max_address;
      int *sz = spec->org_entry.count;
      maximum_internal_bandwidth =
        spec->speed_entry.rate * 1e6 * spec->channel_width * sz[int(HMC::Level::Vault)] / 8;
      maximum_link_bandwidth =
        spec->link_width * 2 * spec->source_links * spec->lane_speed * 1e9 / 8;

      long dram_cycles = num_dram_cycles.value();
      long total_read_req = num_read_requests.total();
      for (auto ctrl : ctrls) {
        ctrl->finish(dram_cycles);
      }
      read_bandwidth = read_transaction_bytes.value() * 1e9 / (dram_cycles * clk_ns());
      write_bandwidth = write_transaction_bytes.value() * 1e9 / (dram_cycles * clk_ns());;
      read_latency_avg = read_latency_sum.value() / total_read_req;
      queueing_latency_avg = queueing_latency_sum.value() / total_read_req;
      request_packet_latency_avg = request_packet_latency_sum.value() / total_read_req;
      response_packet_latency_avg = response_packet_latency_sum.value() / total_read_req;
      read_latency_ns_avg = read_latency_avg.value() * clk_ns();
      queueing_latency_ns_avg = queueing_latency_avg.value() * clk_ns();
      request_packet_latency_ns_avg = request_packet_latency_avg.value() * clk_ns();
      response_packet_latency_ns_avg = response_packet_latency_avg.value() * clk_ns();
      req_queue_length_avg = req_queue_length_sum.value() / dram_cycles;
      read_req_queue_length_avg = read_req_queue_length_sum.value() / dram_cycles;
      write_req_queue_length_avg = write_req_queue_length_sum.value() / dram_cycles;

      cout << "Number of cores: " << num_cores << endl;
      write_address_distribution("");
      
      memory_addresses.close();
      adaptive_thresholds.close();
      adaptive_thresholds_record.close();
      if(set_sampling_result.is_open()) {
        set_sampling_result.close();
      }
      latencies_each_epoch.close();
      feedback_reg_epoch.close();
      string to_open_hops_distribution = application_name+".hops_distribution.csv";
      ofstream hops_distribution_ofs(to_open_hops_distribution.c_str(), ofstream::out);
      hops_distribution_ofs << "Hops,Read,Write,Other,Total" << "\n";
      for(int i = 0; i < network_width+network_height; i++) {
        hops_distribution_ofs << i << "," << hops_distribution_r[i] << "," << hops_distribution_w[i] << "," << hops_distribution_o[i] << "," << hops_distribution[i] << "\n";
      }
      hops_distribution_ofs.close();
      string to_open_network_cycles_distribution = application_name+".network_cycle.csv";
      ofstream network_cycle_ofs(to_open_network_cycles_distribution.c_str(), ofstream::out);
      network_cycle_ofs << "Cycle,# Requests\n";
      for(int i = 0; i < max_hops; i++) {
        network_cycle_ofs << i << "," << network_cycle_distribution[i] << "\n";
      }
      network_cycle_ofs.close();
      string cycle_stats_to_open = application_name+".ramulator.cycle_stats";
      ofstream cycle_stats_ofs(cycle_stats_to_open.c_str(), ofstream::out);
      cycle_stats_ofs << "RamulatorCycleAtFinish: " << clk << "\n";
      cycle_stats_ofs << "WarmupCycles: " << (clk_at_end_of_warmup <= 0 ? clk : clk_at_end_of_warmup)  << "\n";
      cycle_stats_ofs.close();
      if(subscription_prefetcher_type != SubscriptionPrefetcherType::None) {
        prefetcher_set.finish();
      }
      prefetcher_set.print_stats();
      string sub_stats_to_open = application_name+".ramulator.subscription_stats";
      write_sub_stats_file(sub_stats_to_open);
      cout << "Total number of hops travelled: " << total_hops << endl;

      string address_access_count_to_open = application_name+".ramulator.address_access_count.csv";
      ofstream address_access_count_ofs(address_access_count_to_open.c_str(), ofstream::out);
      address_access_count_ofs << "Address,Original Vault,Count,Set\n";
      for(auto const& pair : memory_address_count) {
        address_access_count_ofs << pair.first << "," << pair.second.original_vault << "," << pair.second.count<< "," << pair.second.set << "\n";
      }
    }

    long page_allocator(long addr, int coreid) {
        long virtual_page_number = addr >> 12;

        switch(int(translation)) {
            case int(Translation::None): {
              return addr;
            }
            case int(Translation::Random): {
                auto target = make_pair(coreid, virtual_page_number);
                if(page_translation.find(target) == page_translation.end()) {
                    // page doesn't exist, so assign a new page
                    // make sure there are physical pages left to be assigned

                    // if physical page doesn't remain, replace a previous assigned
                    // physical page.
                    memory_footprint += 1<<12;
                    if (!free_physical_pages_remaining) {
                      physical_page_replacement++;
                      long phys_page_to_read = lrand() % free_physical_pages.size();
                      assert(free_physical_pages[phys_page_to_read] != -1);
                      page_translation[target] = phys_page_to_read;
                    } else {
                        // assign a new page
                        long phys_page_to_read = lrand() % free_physical_pages.size();
                        // if the randomly-selected page was already assigned
                        if(free_physical_pages[phys_page_to_read] != -1) {
                            long starting_page_of_search = phys_page_to_read;

                            do {
                                // iterate through the list until we find a free page
                                // TODO: does this introduce serious non-randomness?
                                ++phys_page_to_read;
                                phys_page_to_read %= free_physical_pages.size();
                            }
                            while((phys_page_to_read != starting_page_of_search) && free_physical_pages[phys_page_to_read] != -1);
                        }

                        assert(free_physical_pages[phys_page_to_read] == -1);

                        page_translation[target] = phys_page_to_read;
                        free_physical_pages[phys_page_to_read] = coreid;
                        --free_physical_pages_remaining;
                    }
                }

                // SAUGATA TODO: page size should not always be fixed to 4KB
                return (page_translation[target] << 12) | (addr & ((1 << 12) - 1));
            }
            default:
                assert(false);
        }

    }


private:
    int calc_log2(int val){
        int n = 0;
        while ((val >>= 1))
            n ++;
        return n;
    }
    int slice_lower_bits(long& addr, int bits)
    {
        int lbits = addr & ((1<<bits) - 1);
        addr >>= bits;
        return lbits;
    }
    void clear_lower_bits(long& addr, int bits)
    {
        addr >>= bits;
    }
    void clear_higher_bits(long& addr, long mask) {
        addr = (addr & mask);
    }
    long lrand(void) {
        if(sizeof(int) < sizeof(long)) {
            return static_cast<long>(rand()) << (sizeof(int) * 8) | rand();
        }

        return rand();
    }
};

} /*namespace ramulator*/

#endif /*__HMC_MEMORY_H*/