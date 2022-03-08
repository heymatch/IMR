#ifndef IMR_BASE_H
#define IMR_BASE_H

#define MAP_MAPPING

#include <cstdlib>
#include <cstring>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <iomanip>

struct Evaluation{
    size_t append_trace_size = 0;
    size_t trace_total_size = 0;
    size_t trace_total_requests = 0;
    size_t processing = 0;

    size_t shifting_address;
    size_t max_LBA = 0;
    size_t max_request_write_size = 0;

    size_t update_times = 0;
    // 1 ~ 1024 ~ up
    size_t update_dist[11];
    std::vector<size_t> track_load_count;

    size_t inplace_update_count = 0;
    size_t outplace_update_count = 0;
    size_t direct_update_bottom_count = 0;
    size_t direct_update_top_count = 0;

    size_t hot_write_sector_count = 0;
    size_t hot_write_request_count = 0;
    size_t hot_update_times = 0;

    size_t cold_write_sectors = 0;
    size_t cold_write_request_count = 0;
    size_t cold_update_times = 0;

    size_t buffer_update_times = 0;

    size_t cache_check_times = 0;
    size_t cache_load_times = 0;
    size_t cache_write_times = 0;

    size_t write_buffer_times = 0;
    size_t write_buffer_requests = 0;

    size_t append_count = 0;

    size_t total_sector_used = 0;

    size_t hot_after_hot_partition = 0;
    size_t hot_after_cold_partition = 0;
    size_t cold_after_hot_partition = 0;
    size_t cold_after_cold_partition = 0;

    void insert_update_dist(const size_t &length){
        if(length == 0) return;

        for(int i = 0; i < 11; ++i){
            if(length <= (1 << i)){
                update_dist[i] += 1;
                return;
            }
        }
    }
};

enum Update_Method{
    IN_PLACE,
    OUT_PLACE
};

enum Trace_Type{
    SYSTOR17,
    MSR
};

struct Options{
    Options(
        size_t sectors_per_bottom_track = 2050,
        size_t sectors_per_top_track = 1845,
        size_t total_bottom_track = 106501,
        size_t total_top_track = 106500,

        Update_Method update_method = Update_Method::IN_PLACE,

        size_t hot_data_def_size = 64,
        size_t partition_size = 500,
        size_t max_partition_size = 2000,
        size_t buffer_size = 21,
        size_t mapping_cache_size = 10,
        size_t segment_size = 41
    ) : 
        TOTAL_TRACKS(total_top_track + total_bottom_track),
        SECTORS_PER_BOTTOM_TRACK(sectors_per_bottom_track),
        SECTORS_PER_TOP_TRACK(sectors_per_top_track),
        TOTAL_BOTTOM_TRACK(total_bottom_track),
        TOTAL_TOP_TRACK(total_top_track),

        UPDATE_METHOD(update_method),

        HOT_DATA_DEF_SIZE(hot_data_def_size),
        BASE_PARTITION_TRACK_SIZE(partition_size),
        MAX_PARTITION_SIZE(max_partition_size),
        BASE_BUFFER_TRACK_SIZE(buffer_size),
        MAPPING_CACHE_SIZE(mapping_cache_size),
        SEGMENT_SIZE(segment_size),

        TOTAL_BUFFER_SECTOR_SIZE((BASE_BUFFER_TRACK_SIZE - BASE_BUFFER_TRACK_SIZE / 2) * SECTORS_PER_BOTTOM_TRACK + (BASE_BUFFER_TRACK_SIZE / 2 * SECTORS_PER_TOP_TRACK)),
        TOTAL_SECTORS(SECTORS_PER_BOTTOM_TRACK * TOTAL_BOTTOM_TRACK + SECTORS_PER_TOP_TRACK * TOTAL_TOP_TRACK)
    {}

    // * parameters for disk

    size_t TOTAL_TRACKS;
    const size_t SECTORS_PER_BOTTOM_TRACK;
    const size_t SECTORS_PER_TOP_TRACK;
    size_t TOTAL_BOTTOM_TRACK;
    size_t TOTAL_TOP_TRACK;

    // * parameters for Partition

    size_t HOT_DATA_DEF_SIZE;
    const size_t BASE_PARTITION_TRACK_SIZE;
    const size_t MAX_PARTITION_SIZE;
    const size_t BASE_BUFFER_TRACK_SIZE;           // in tracks
    const size_t MAPPING_CACHE_SIZE;
    const size_t SEGMENT_SIZE;
    
    

    // * translator options

    Update_Method UPDATE_METHOD = Update_Method::IN_PLACE;
    Trace_Type TRACE_TYPE;
    std::string INPUT_FILE;     // input trace
    std::string OUTPUT_FILE;    // output trace
    std::string EVAL_FILE;      // evaluation
    std::string DIST_FILE;      // distribution
    std::string PARAM_FILE;     // parameters, settings

    // ** trace append

    double APPEND_COLD_SIZE = 0.2;
    uint16_t APPEND_PARTS = 1;

    // ** trace partition

    uint16_t TRACE_PARTS = 1;

    // * calculated

    const size_t TOTAL_BUFFER_SECTOR_SIZE;
    size_t TOTAL_SECTORS;

};

struct Request{
    Request(
        double time = 0,
        char iotype = 0,
        size_t address = 0,
        size_t size = 0,
        size_t device = 0
    ) :
        timestamp(time),
        iotype(iotype),
        address(address),
        size(size),
        device(0)
    {}

    inline bool operator<(const Request &right) const{
        return !(timestamp < right.timestamp);
    }

    double timestamp;
    double response;
    char iotype;
    size_t address;
    size_t size;
    size_t device;
};

extern std::priority_queue<Request> order_queue;

static std::ostream& operator<<(std::ostream &out, const Request &right){
    out 
    << right.timestamp << "\t" 
    << right.device << "\t"
    << right.address << "\t" 
    << right.size << "\t";

    if(right.iotype == 'W')
        out << '0';
    else if(right.iotype == 'R')
        out << '1';

    // out << std::endl;
    out << '\n';
    
    return out;
}

class IMR_Base{
public:
    // * main common functions

    virtual void initialize(std::ifstream &);
    virtual void run(std::ifstream &, std::ofstream &) = 0;
    virtual void evaluation(std::string &);
    // virtual void verification();

    void read(const Request &request, std::ostream &output_file);
    virtual void write(const Request &, std::ostream &) = 0;
    void wirte_append(const Request &, std::ostream &);

    // * for I/O
    void write_requests_file(const std::vector<Request> &requests, std::ostream &out_file);
    void read_file(std::istream &);
    void write_file(std::ostream &);

    // * sub-functions
    #ifdef MAP_MAPPING
    inline size_t get_PBA(const size_t &LBA) { return LBA_to_PBA.count(LBA) ? LBA_to_PBA[LBA] : -1; }
    inline size_t get_LBA(const size_t &PBA) { return PBA_to_LBA.count(PBA) ? PBA_to_LBA[PBA] : -1; }
    inline void set_LBA_to_PBA(const size_t &LBA, const size_t &PBA) { LBA_to_PBA[LBA] = PBA; PBA_to_LBA[PBA] = LBA; }
    inline void set_PBA_to_LBA(const size_t &PBA, const size_t &LBA) { PBA_to_LBA[PBA] = LBA; LBA_to_PBA[LBA] = PBA; }
    #endif
    
    #ifdef VECTOR_MAPPING
    inline size_t get_PBA(const size_t &LBA);
    inline size_t get_LBA(const size_t &PBA);
    inline void set_LBA_to_PBA(const size_t &LBA, const size_t &PBA);
    #endif

    inline size_t get_track(const size_t &PBA) const;
    inline size_t get_track_head_sector(const size_t &track) const;
    inline size_t get_track_tail_sector(const size_t &track) const;

    // * BOTTOM is even, TOP is odd
    inline bool isTop(const size_t &track) const;

    // * For evaluation
    inline size_t get_LBA_size();

    Evaluation eval;
    Options options;
    std::vector<bool> track_written;
    std::priority_queue<Request> order_queue;

    void set_evaluation_stream(std::string &s) {evaluation_stream.open(s);}
    void set_distribution_stream(std::string &s) {distribution_stream.open(s);}

    std::ofstream evaluation_stream;
    std::ofstream distribution_stream;

private:
    #ifdef MAP_MAPPING
    std::unordered_map<size_t, size_t> LBA_to_PBA;
    std::unordered_map<size_t, size_t> PBA_to_LBA;
    #endif

    #ifdef VECTOR_MAPPING
    std::vector<size_t> LBA_to_PBA;
    std::vector<size_t> PBA_to_LBA;
    #endif
};

#ifdef VECTOR_MAPPING
inline size_t IMR_Base::get_PBA(const size_t &LBA) { 
    return LBA_to_PBA[LBA];
}

inline size_t IMR_Base::get_LBA(const size_t &PBA) { 
    return PBA_to_LBA[PBA];
}

inline void IMR_Base::set_LBA_to_PBA(const size_t &LBA, const size_t &PBA){
    if(LBA_to_PBA[LBA] == -1) eval.total_sector_used += 1; 
    LBA_to_PBA[LBA] = PBA;
    PBA_to_LBA[PBA] = LBA; 
}
#endif

inline size_t IMR_Base::get_track(const size_t &PBA) const{
    if (PBA == -1) return -1;

    size_t n = PBA / (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK);
    size_t r = PBA % (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK);

    if (r >= options.SECTORS_PER_BOTTOM_TRACK)
        return 2 * n + 1;
    else
        return 2 * n;
}

inline size_t IMR_Base::get_track_head_sector(const size_t &track) const{
    return isTop(track) ? 
        track / 2 * (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK) + options.SECTORS_PER_BOTTOM_TRACK :
        track / 2 * (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK);
}

// TODO
inline size_t IMR_Base::get_track_tail_sector(const size_t &track) const{
    return isTop(track) ? 
        track / 2 * (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK) + options.SECTORS_PER_BOTTOM_TRACK + (options.SECTORS_PER_TOP_TRACK - 1):
        track / 2 * (options.SECTORS_PER_BOTTOM_TRACK + options.SECTORS_PER_TOP_TRACK) + (options.SECTORS_PER_BOTTOM_TRACK - 1);
}

// * even is bottom, odd is top
inline bool IMR_Base::isTop(const size_t &track) const{ 
    return track % 2; 
}

inline size_t IMR_Base::get_LBA_size(){ 
    return std::max(LBA_to_PBA.size(), PBA_to_LBA.size()); 
}

#endif